#!/usr/bin/env python3
"""Run the real launch screen against a checked GX stream, in both screen shapes.

The Library renderer is compiled as test_gameflow_gx_stream.py compiles it,
with the launch screen's own state and ui_launch, and driven through a launch:
Game Detail, A, Swiss's progress messages, the hand-off, or a failure that
goes back to the Library. The tests then check that:

- the screen dims to the frame's edges in 4:3 and in 16:9, and the cover,
  the ring and the text sit where they do in both;
- the cover glides from Detail's pose to the centre, from every layout, and
  keeps its poster when the pack closes for the hand-off;
- the ring is a track and a fill within its vertex budget, the fill never
  falls as boxes come and go, and one caption replaces the file names;
- Animations Off draws a still ring that steps with the launch;
- a failed launch fades out and the Library comes back as it was.
"""

import math
from pathlib import Path
import subprocess
import tempfile
import unittest

from test_gameflow_gx_stream import GUI, build, covers, frames

DRIVER = r"""
static drawGameflowEvent_t *eventData;
static uiDrawObj_t event;
static uiGameflowRenderSnapshot_t snapshot;
static uint32_t generation;
/* Y: the cards are apps, as Apps shows them. */
static bool apps;

/* L layout count selected | K (Detail's snapshot) | W wide | M motion
 * | D mode | S message (\n and \205 escaped) | C packClosed | E (hand-off)
 * | N frames dt -- the log has one "F" per frame. */
static void publish(int layout, uint32_t count, uint32_t selected)
{
	uiGameflowLibraryWindowSlot_t slots[25];
	size_t n;
	memset(&snapshot, 0, sizeof(snapshot));
	snapshot.selection.generation = ++generation;
	snapshot.selection.itemCount = count;
	snapshot.selection.selectedIndex = selected;
	snapshot.layout = (u8)layout;
	if(layout == UI_GAMEFLOW_LAYOUT_GRID) {
		snapshot.columns = 5;
		n = UIGameflowLibrary_BuildGridWindow(count, selected, 5u,
			UI_GAMEFLOW_DIRECTION_NONE, slots);
	}
	else {
		n = UIGameflowLibrary_BuildWindow(count, selected,
			UI_GAMEFLOW_DIRECTION_NONE, slots);
	}
	snapshot.recordCount = (u32)n;
	for(size_t i = 0; i < n; ++i) {
		uiGameflowCardSnapshot_t *record = &snapshot.records[i];
		record->flags = UI_GAMEFLOW_CARD_VALID;
		record->libraryIndex = slots[i].index;
		record->relativeSlot = slots[i].relativeSlot;
		record->column = slots[i].column;
		snprintf(record->gameId, sizeof(record->gameId), "G%03uE0", slots[i].index);
		snprintf(record->title, sizeof(record->title), "Game number %u", slots[i].index);
		snprintf(record->company, sizeof(record->company), "Company %u", slots[i].index % 9u);
		if(apps) {
			record->flags |= UI_GAMEFLOW_CARD_APP;
			memset(record->gameId, 0, sizeof(record->gameId));
			snprintf(record->title, sizeof(record->title), "App %u", slots[i].index);
		}
	}
	CHECK(_GameflowSnapshotValid(&snapshot));
	memset(eventData, 0, sizeof(*eventData));
	UIGameflow_Init(&eventData->state);
	UIGameflow_SetColumns(&eventData->state, snapshot.columns);
	CHECK(UIGameflow_ApplySnapshot(&eventData->state, &snapshot.selection, motionMode));
	_GameflowCopySnapshot(eventData, &snapshot);
}

/* Detail's snapshot for the focused game, as DrawUpdateGameflowDetail
 * publishes it. */
static void detail(void)
{
	const uiGameflowFrame_t *frame = UIGameflow_Frame(&eventData->state);
	const uiGameflowCardSnapshot_t *record = _GameflowFindRecord(
		&eventData->snapshot, frame->focusIndex, NULL);
	uiGameflowDetailSnapshot_t *d = &eventData->detail;
	CHECK(record != NULL);
	memset(d, 0, sizeof(*d));
	d->flags = UI_GAMEFLOW_DETAIL_VALID;
	d->generation = frame->generation;
	d->focusIndex = frame->focusIndex;
	memcpy(d->gameId, record->gameId, 6);
	snprintf(d->title, sizeof(d->title), "Detail title %s", record->gameId);
	snprintf(d->company, sizeof(d->company), "Detail publisher");
	snprintf(d->launchLabel, sizeof(d->launchLabel), "A  LAUNCH GAME");
	snprintf(d->primaryActions, sizeof(d->primaryActions), "A  LAUNCH   B  LIBRARY");
	_GameflowPrepareDetailPresentation(eventData);
}

static void unescape(char *text)
{
	char *out = text;
	while(*text) {
		if(text[0] == '\\' && text[1] == 'n') { *out++ = '\n'; text += 2; }
		else if(strncmp(text, "\\205", 4) == 0) { *out++ = '\205'; text += 4; }
		else *out++ = *text++;
	}
	*out = '\0';
}

int main(void)
{
	char line[256];
	out = stdout;
	eventData = calloc(1, sizeof(*eventData));
	CHECK(eventData != NULL);
	event.data = eventData;
	sceneFrame.scene = UI_SCENE_LIBRARY;
	sceneFrame.chromeProgress = 1.0f;
	sceneFrame.libraryReveal = 1.0f;
	while(fgets(line, sizeof(line), stdin)) {
		unsigned a, b; int c; float dt;
		line[strcspn(line, "\n")] = '\0';
		if(sscanf(line, "L %d %u %u", &c, &a, &b) == 3) publish(c, a, b);
		else if(strcmp(line, "K") == 0) detail();
		else if(strcmp(line, "Y") == 0) apps = true;
		else if(sscanf(line, "W %d", &c) == 1) UIStage_SetWide(c != 0);
		else if(sscanf(line, "M %d", &c) == 1) motionMode = (uiMotionMode_t)c;
		else if(sscanf(line, "C %d", &c) == 1) packClosed = c != 0;
		else if(sscanf(line, "D %d", &c) == 1) {
			/* DrawSetGameflowMode, without its lock. */
			UIGameflow_SetMode(&eventData->state, (uiGameflowMode_t)c, motionMode);
			_GameflowSetLaunch(eventData, UIGameflow_Frame(&eventData->state)->mode ==
				UI_GAMEFLOW_MODE_LAUNCH);
			sceneFrame.scene = c ? UI_SCENE_GAME_DETAIL : UI_SCENE_LIBRARY;
		}
		else if(strncmp(line, "S ", 2) == 0) {
			unescape(line + 2);
			/* DrawLaunchStep, without its lock. */
			if(launchActive) _GameflowLaunchMessage(line + 2);
		}
		else if(strcmp(line, "E") == 0) UILaunch_Finish(&launchState);
		else if(sscanf(line, "N %u %f", &a, &dt) == 2) {
			for(b = 0; b < a; ++b) {
				animDelta = dt; animSeconds += dt;
				fprintf(out, "F\n");
				_DrawGameflow(&event);
			}
		}
		else { fprintf(stderr, "bad command: %s\n", line); return 64; }
	}
	return 0;
}
"""

CENTRE = (320.0, 188.0)
RADIUS = 136.0
FRAME = 1 / 60
# Game Detail for game 18 of 40, then A.
LAUNCH = ["K", "D 1", "N 40 0.0167", "D 2"]


def primitives(frame: str) -> list:
    """Each primitive in a frame: its kind, texture, points and alphas."""
    found, texture = [], None
    for line in frame.splitlines():
        if line.startswith("X "):
            texture = line[2:]
        elif line.startswith("B "):
            fields = line.split()
            found.append({"kind": "strip" if fields[-1] == "strip" else "quads",
                          "count": int(fields[1]), "texture": texture,
                          "points": [], "alphas": []})
            texture = None
        elif line.startswith("P "):
            found[-1]["points"].append(tuple(map(float, line.split()[1:])))
        elif line.startswith("C "):
            found[-1]["alphas"].append(int(line.split()[4]))
    return found


def strings(frame: str) -> list:
    """(x, y, text) of every line of text a frame draws."""
    return [(int(l.split()[1]), int(l.split()[2]), l.split(" ", 6)[6])
            for l in frame.splitlines() if l.startswith("S ")]


def turn(point) -> float:
    """Where a point sits on the ring: 0 at twelve o'clock, clockwise."""
    angle = math.atan2(point[1] - CENTRE[1], point[0] - CENTRE[0])
    return (angle / (2 * math.pi) + 0.25) % 1.0


def dim(frame: str) -> int:
    """The alpha of the dim, the frame's first quad when it spans the stage."""
    first = primitives(frame)[0]
    xs = [p[0] for p in first["points"]]
    return first["alphas"][0] if first["kind"] == "quads" and xs and \
        min(xs) <= 0.0 and max(xs) >= 640.0 else 0


def ring(frame: str) -> tuple:
    """The ring's strips, and how far its fill reaches (0 when empty)."""
    strips = [p for p in primitives(frame) if p["kind"] == "strip"]
    fill = turn(strips[3]["points"][-1]) if len(strips) > 3 else 0.0
    return strips, fill


class LaunchGxStream(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="swiss-launch-gx-")
        cls.frame_c = (GUI / "FrameBufferMagic.c").read_text()
        cls.frame_h = (GUI / "FrameBufferMagic.h").read_text()
        cls.binary = build(Path(cls.tmp.name), GUI, cls.frame_c, cls.frame_h, True,
                           "launch", DRIVER)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_script(self, script):
        # Detail's text holds IPL font bytes such as its "\267" dot.
        result = subprocess.run([str(self.binary)],
                                input=("\n".join(script) + "\n").encode(),
                                capture_output=True, timeout=120)
        self.assertEqual(result.returncode, 0, result.stderr[-2000:])
        return frames(result.stdout.decode("latin-1"))

    def check_launch_frames(self):
        """A launch frame in 4:3 and in 16:9."""
        rests = {}
        for wide, edges in ((0, (0.0, 640.0)), (1, (-320 / 3, 640 + 320 / 3))):
            rest = self.run_script([f"W {wide}", "L 0 40 18"] + LAUNCH + [
                "S Checking Game\\205", "N 60 0.0167"])[-1]
            rests[wide] = rest
            drawn = primitives(rest)
            # First the dim, one quad from edge to edge of the frame.
            veil = drawn[0]
            self.assertEqual((veil["kind"], veil["count"]), ("quads", 4))
            xs, ys = [p[0] for p in veil["points"]], [p[1] for p in veil["points"]]
            self.assertAlmostEqual(min(xs), edges[0], places=2)
            self.assertAlmostEqual(max(xs), edges[1], places=2)
            self.assertEqual((min(ys), max(ys)), (0.0, 480.0))
            self.assertEqual(set(veil["alphas"]), {240})
            # Then the ring: a track and a fill, three bands each, within
            # the budget and on the circle.
            strips, fill = ring(rest)
            self.assertEqual(len(strips), 6)
            self.assertLessEqual(sum(s["count"] for s in strips), 6 * 98)
            for strip in strips:
                for point in strip["points"]:
                    self.assertLess(abs(math.dist(point, CENTRE) - RADIUS), 3.0 + 0.01)
            self.assertAlmostEqual(turn(strips[0]["points"][0]), 0.0, places=3)
            self.assertGreater(fill, 0.14 - 0.01)
            self.assertLess(fill, 0.24)
            # The cover, centred inside the ring, clear of it.
            cover = covers(rest)["G018E0"]
            self.assertEqual(len(cover), 1)
            x0, y0, x1, y1 = cover[0]
            self.assertAlmostEqual((x0 + x1) / 2, 320.0, delta=0.5)
            self.assertAlmostEqual((y0 + y1) / 2, 188.0, delta=0.5)
            self.assertLess(math.dist((x0, y0), CENTRE), RADIUS - 3.0)
            # The title and publisher, and one caption: no Detail text
            # and no hint line are left.
            self.assertEqual(strings(rest), [
                (320, 360, "Detail title G018E0"), (320, 384, "Detail publisher"),
                (320, 414, "Checking game")])
            self.assertNotIn("\nH ", "\n" + rest)
        # The same launch screen in both shapes, but for the dim's reach: its
        # "B 4" and four vertices of three lines are the first 13 lines.
        self.assertNotEqual(rests[0], rests[1])
        self.assertEqual(rests[0].split("\n", 13)[13], rests[1].split("\n", 13)[13])

    def test_a_launch_frame_in_both_screen_shapes(self):
        self.check_launch_frames()

    def check_glides(self):
        """From every layout, Detail's cover glides to the centre."""
        for layout in (0, 1, 2, 3):
            shots = self.run_script([f"L {layout} 40 18"] + LAUNCH + ["N 60 0.0167"])
            boxes = [covers(f)["G018E0"][0] for f in shots[39:]]
            # Detail's pose, then a glide that never jumps.
            self.assertAlmostEqual(boxes[0][0], 48 + 6, delta=1.0)
            for a, b in zip(boxes, boxes[1:]):
                self.assertLess(max(abs(p - q) for p, q in zip(a, b)), 40.0)
            self.assertAlmostEqual((boxes[-1][0] + boxes[-1][2]) / 2, 320.0, delta=0.5)

    def test_the_cover_glides_to_the_centre_from_every_layout(self):
        self.check_glides()

    def test_autoboot_launches_from_the_library(self):
        # Boot without prompts: no Detail, and the Library's own title.
        rest = self.run_script(["L 0 40 18", "N 10 0.0167", "D 2", "N 60 0.0167"])[-1]
        self.assertAlmostEqual((covers(rest)["G018E0"][0][0] +
                                covers(rest)["G018E0"][0][2]) / 2, 320.0, delta=0.5)
        self.assertEqual(strings(rest), [(320, 360, "Game number 18"),
                                         (320, 384, "Company 0"),
                                         (320, 414, "Starting game")])

    def test_autoboot_from_spotlight_glides_from_its_picture(self):
        # Boot without prompts in Spotlight: the cover leaves the middle of
        # the picture panel and glides to the centre without a jump.
        shots = self.run_script(["L 3 40 18", "N 10 0.0167", "D 2", "N 60 0.0167"])
        boxes = [covers(f)["G018E0"][0] for f in shots[10:] if "G018E0" in covers(f)]
        self.assertAlmostEqual(boxes[0][0], 106 + 6, delta=12.0)
        for a, b in zip(boxes, boxes[1:]):
            self.assertLess(max(abs(p - q) for p, q in zip(a, b)), 40.0)
        self.assertAlmostEqual((boxes[-1][0] + boxes[-1][2]) / 2, 320.0, delta=0.5)

    def test_the_fill_never_falls_and_the_caption_follows(self):
        messages = ["Saving recent list\\205", "Video Mode: NTSC 480p", "Applied 2 cheats",
                    "Checking Game\\205", "Reading File 1/2\\nmain.dol [3012KB]",
                    "Patching File 1/2\\nmain.dol [3012KB]",
                    "Writing File 1/2\\nmain.dol [3012KB]",
                    "Reading File 1/2\\nmain.dol [3012KB]",
                    "Reading File 2/2\\nstage.rel [220KB]",
                    "One moment, setting up audio streaming.",
                    "Loading DOL\\nDo not remove SD Card - SD2SP2", "Loading DOL"]
        script = ["L 0 40 18"] + LAUNCH + ["N 30 0.0167"]
        for message in messages:
            script += [f"S {message}", "N 8 0.0167"]
        shots = self.run_script(script + ["N 30 0.0167"])[40:]
        fills = [ring(f)[1] for f in shots]
        for a, b in zip(fills, fills[1:]):
            self.assertGreaterEqual(b, a - 1e-4)
        self.assertGreater(fills[-1], 0.78)
        captions = [s[2] for f in shots for s in strings(f) if s[1] == 414]
        self.assertEqual(list(dict.fromkeys(captions)), [
            "Starting game", "Saving recent games", "Setting the video mode",
            "Applying cheats", "Checking game", "Preparing game files",
            "Setting up audio streaming", "Loading game"])
        for text in ("main.dol", "KB", "Reading File"):
            self.assertNotIn(text, "".join(shots))
        # "Do not remove" stays up to the hand-off, under the caption.
        self.assertIn((320, 438, "Do not remove SD Card - SD2SP2"), strings(shots[-1]))

    def check_the_cover_outlasts_the_poster_pack(self):
        # The hand-off closes the pack; Detail's cover stays until the end.
        rest = self.run_script(["L 0 40 18"] + LAUNCH + ["N 30 0.0167", "C 1",
                                                         "S Loading DOL", "E", "N 20 0.0167"])
        self.assertEqual(set(covers(rest[-1])), {"G018E0"})
        self.assertGreater(ring(rest[-1])[1], 0.99)

    def test_the_cover_outlasts_the_poster_pack(self):
        self.check_the_cover_outlasts_the_poster_pack()

    def test_animations_off_still_ring_that_steps(self):
        shots = self.run_script(["M 2", "L 0 40 18"] + LAUNCH + [
            "N 1 0.0167", "S Checking Game\\205", "N 1 0.0167", "N 30 0.0167"])
        # No glide, no glint: the screen is the same from the first frame.
        self.assertEqual(shots[-31], shots[-1])
        self.assertAlmostEqual(ring(shots[-1])[1], 0.14, delta=0.005)
        self.assertAlmostEqual((covers(shots[-1])["G018E0"][0][0] +
                                covers(shots[-1])["G018E0"][0][2]) / 2, 320.0, delta=0.5)

    def test_an_app_launches_from_apps(self):
        """A on an app launches it from the cards, with no Detail: its own
        poster flies to the centre, the others fade, the launch says app,
        and boot_dol's "Loading DOL" moves the ring on."""
        shots = self.run_script(["Y", "L 0 12 4", "N 20 0.0167", "D 2", "N 40 0.0167",
                                 "S Loading DOL", "N 40 0.0167"])
        start, loading = shots[55], shots[-1]
        self.assertTrue(any(text == "Starting app" for _, _, text in strings(start)))
        self.assertTrue(any(text == "Loading app" for _, _, text in strings(loading)))
        for frame in (start, loading):
            self.assertFalse([text for _, _, text in strings(frame) if "game" in text.lower()])
        self.assertEqual(dim(loading), 240)
        self.assertEqual(set(covers(loading)), {"APP004"})
        self.assertGreater(ring(loading)[1], ring(start)[1])

    def test_a_failed_launch_goes_back_to_the_library(self):
        shots = self.run_script(["L 0 40 18"] + LAUNCH + [
            "N 30 0.0167", "S Checking Game\\205", "N 30 0.0167", "D 0", "N 120 0.0167"])
        before = self.run_script(["L 0 40 18", "N 60 0.0167"])[-1]
        # The launch screen fades out, and the Library is as it was.
        fades = [dim(f) for f in shots[99:]]
        self.assertEqual(fades[0], 240)
        self.assertTrue(all(a >= b for a, b in zip(fades, fades[1:])))
        self.assertEqual(fades[-1], 0)
        self.assertEqual(shots[-1], before)

    def test_mutants_fail(self):
        mutants = {
            "the dim stops at the stage": ("{{{UIStage_Left(), 0.0f},\n\t\t{UIStage_Right(), 0.0f}, "
                                           "{UIStage_Right(), 480.0f},\n\t\t{UIStage_Left(), 480.0f}}}",
                                           "{{{0.0f, 0.0f},\n\t\t{640.0f, 0.0f}, "
                                           "{640.0f, 480.0f},\n\t\t{0.0f, 480.0f}}}"),
            "the pack's closing takes the cover": ("posterTexture = launchActive && launchHasPoster &&",
                                                   "posterTexture = false && launchHasPoster &&"),
            "Detail stays under the launch screen": ("if(frame->launchProgress < 0.999f) {",
                                                     "if(frame->launchProgress < 2.0f) {"),
            "the cover stays at Detail's pose": ("gameflowLaunchPose.point[vertex],\n",
                                                 "card->quad.point[vertex],\n"),
        }
        tests = (self.check_launch_frames, self.check_the_cover_outlasts_the_poster_pack,
                 self.check_glides)
        original = self.binary
        try:
            for index, (name, (old, new)) in enumerate(mutants.items()):
                with self.subTest(mutant=name):
                    self.assertEqual(self.frame_c.count(old), 1, name)
                    self.binary = build(Path(self.tmp.name), GUI,
                                        self.frame_c.replace(old, new), self.frame_h,
                                        True, f"mutant{index}", DRIVER)
                    failed = False
                    for test in tests:
                        try:
                            test()
                        except AssertionError:
                            failed = True
                            break
                    self.assertTrue(failed, f"mutant escaped: {name}")
        finally:
            self.binary = original


if __name__ == "__main__":
    unittest.main(verbosity=2)
