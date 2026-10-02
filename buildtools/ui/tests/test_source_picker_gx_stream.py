#!/usr/bin/env python3
"""Run the real Source picker renderer against a checked GX stream.

_DrawDeviceSelector and its tile helpers are compiled out of
FrameBufferMagic.c with the real springs (ui_motion.c) and screen shapes
(ui_stage.c). GX, the font and the device pictures are stand-ins that check
every primitive is declared and emitted in full, that the drawing state is
set up before each kind of draw, and log what is drawn. The tests then check,
in 4:3 and in Menu Widescreen:

- at rest the row shows each listed device once, the focused one in the
  middle and drawn last, the others two a side at most, with its name;
- a step slides the row without a tile jumping, also across the wrap;
- Copy/Move's page reaches the screen's edges and nothing else leaves the
  640 x 480 stage;
- the row rises into view as the cube shrinks out of its way;
- Motion Off and a new list (Z) snap; the renderer never reads a handler.
"""

import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

from test_cheats_gx_stream import extract_function


ROOT = Path(__file__).resolve().parents[3]
GUI = ROOT / "cube/swiss/source/gui"
HANDLER_H = ROOT / "cube/swiss/source/devices/deviceHandler.h"


def between(source: str, start: str, end: str, inclusive: bool = False) -> str:
    first = source.index(start)
    last = source.index(end, first)
    return source[first:last + (len(end) if inclusive else 0)]


PRELUDE = r"""
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui_motion.h"
#include "ui_stage.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef struct { u8 r, g, b, a; } GXColor;
typedef struct uiDrawObj { int type; void *data; struct uiDrawObj *child; bool disposed; } uiDrawObj_t;
#define ALIGN_LEFT 0
#define ALIGN_CENTER 1
#define ALIGN_RIGHT 2
enum { GX_QUADS = 0x80, GX_VTXFMT0 = 0 };
/* The time and the dial in their default corner, the right, leave the left;
 * test_clock_corner.py covers the others. */
static int _FreeCorner(float *inset) { *inset = 0.0f; return -1; }

#define CHECK(c) do { if(!(c)) { fprintf(stderr, "failed %d: %s\n", __LINE__, #c); exit(73); } } while(0)
/* What the last call left GX ready for: drawInit's textured pipeline, the
 * raster pipeline over it, or neither (after a picture or text). */
enum { DIRTY, TEXTURED, RASTER };
static int pipeline = DIRTY;
static bool active;
static int declared, emitted, phase;
static void drawInit(void) { CHECK(!active); pipeline = TEXTURED; }
static void _SetupRasterColor(void) { CHECK(!active && pipeline == TEXTURED); pipeline = RASTER; }
static void UIColor_Apply(u8 *r, u8 *g, u8 *b) { (void)r; (void)g; (void)b; }
static void GX_Begin(int primitive, int format, int count)
{
	CHECK(!active && pipeline == RASTER && primitive == GX_QUADS &&
		format == GX_VTXFMT0 && count > 0 && count % 4 == 0);
	active = true; declared = count; emitted = 0; phase = 0;
	printf("B %d\n", count);
}
static void GX_Position3f32(float x, float y, float z)
{
	CHECK(active && phase == 0 && z == 0.0f && isfinite(x) && isfinite(y));
	phase = 1; printf("P %.2f %.2f\n", x, y);
}
static void GX_Color4u8(u8 r, u8 g, u8 b, u8 a)
{
	(void)r; (void)g; (void)b;
	CHECK(active && phase == 1); phase = 2; printf("C %u\n", a);
}
static void GX_TexCoord2f32(float s, float t)
{
	CHECK(active && phase == 2 && emitted < declared && s == 0.0f && t == 0.0f);
	phase = 0; emitted++;
}
static void GX_End(void) { CHECK(active && phase == 0 && emitted == declared); active = false; }
static void _DrawImageNow(int textureId, int x, int y, int width, int height,
	int depth, float s1, float s2, float t1, float t2, int centered, u8 opacity)
{
	CHECK(!active && pipeline == TEXTURED && depth == 0 && centered == 0);
	CHECK(s1 == 0.0f && s2 == 1.0f && t1 == 0.0f && t2 == 1.0f);
	CHECK(width > 0 && height > 0);
	printf("I %d %d %d %d %d %u\n", textureId, x, y, width, height, opacity);
	pipeline = DIRTY;
}
static void drawStringMedium(int x, int y, const char *text, float scale,
	int align, GXColor color)
{
	CHECK(!active && text != NULL && scale > 0.0f);
	printf("S %d %d %d %u %s\n", x, y, align, color.a, text);
	pipeline = DIRTY;
}
static void _DrawHintText(int x, int y, const char *text, float scale,
	int align, GXColor color)
{
	CHECK(!active && text != NULL && scale > 0.0f);
	printf("H %d %d %d %u %s\n", x, y, align, color.a, text);
	pipeline = DIRTY;
}
typedef struct { float chromeProgress; float cubeScale; } uiSceneFrame_t;
static uiSceneFrame_t sceneFrame = {1.0f, 0.5f};
static const uiSceneFrame_t *UIScene_Frame(void) { return &sceneFrame; }
static float animDelta, animSeconds;
static float UIAnim_Delta(void) { return animDelta; }
static float UIAnim_Seconds(void) { return animSeconds; }
static uiMotionMode_t motionMode;
static uiMotionMode_t _CurrentMotionMode(void) { return motionMode; }
"""

DRIVER = r"""
static drawDeviceSelectorEvent_t selector;
static uiDrawObj_t event = {0, &selector, NULL, false};

/* Device i draws picture 100 + i, in one of the shapes Swiss's pictures have,
 * and every fourth one was not detected. */
static void publish(int count, int travel, int all, int advanced, int destination)
{
	static const u16 shapes[][4] = {
		{84, 84, 84, 84}, {59, 78, 64, 80}, {140, 64, 140, 64},
		{115, 72, 120, 76}, {76, 84, 80, 92}, {102, 56, 104, 58}, {116, 40, 120, 40}
	};
	drawDeviceSelectorSnapshot_t *s = &selector.snapshot;
	CHECK(count > 0 && count <= MAX_DEVICES);
	memset(s, 0, sizeof(*s));
	for(int i = 0; i < count; ++i) {
		drawDeviceTile_t *tile = &s->tiles[i];
		tile->picture.textureId = 100 + i;
		tile->picture.width = shapes[i % 7][0];
		tile->picture.height = shapes[i % 7][1];
		tile->picture.realWidth = shapes[i % 7][2];
		tile->picture.realHeight = shapes[i % 7][3];
		snprintf(tile->name, sizeof(tile->name), "Device %d", i);
		snprintf(tile->facts, sizeof(tile->facts), "FILES %d", i);
		snprintf(tile->status, sizeof(tile->status), "STATUS %d", i);
		tile->nameScale = 0.76f;
		tile->detected = i % 4 != 3;
	}
	s->count = count;
	s->travel = travel;
	s->showAllDevices = all != 0;
	s->inAdvanced = advanced != 0;
	strcpy(s->hint, "STICK / D-PAD  CHANGE   A  OPEN   Z  ALL   B  BACK");
	s->hintScale = 0.46f;
	selector.destination = destination != 0;
}

int main(void)
{
	char line[128];
	while(fgets(line, sizeof(line), stdin)) {
		int a, b, c, d, e;
		float dt;
		if(sscanf(line, "L %d %d %d %d %d", &a, &b, &c, &d, &e) == 5) {
			publish(a, b, c, d, e);
		}
		else if(sscanf(line, "T %d", &a) == 1) selector.snapshot.travel = a;
		else if(sscanf(line, "W %d", &a) == 1) UIStage_SetWide(a != 0);
		else if(sscanf(line, "M %d", &a) == 1) motionMode = (uiMotionMode_t)a;
		else if(sscanf(line, "K %f", &dt) == 1) sceneFrame.cubeScale = dt;
		else if(line[0] == 'R') memset(&selector, 0, sizeof(selector));
		else if(sscanf(line, "N %d %f", &a, &dt) == 2) {
			for(b = 0; b < a; ++b) {
				animDelta = dt;
				animSeconds += dt;
				printf("F\n");
				drawInit();	/* as videoDrawEvent does before each event */
				_DrawDeviceSelector(&event);
				/* No state leaks to the next event. */
				CHECK(!active && pipeline == TEXTURED);
			}
		}
		else { fprintf(stderr, "bad command: %s", line); return 64; }
	}
	return 0;
}
"""


def picker(frame_c: str) -> str:
    return "\n".join([
        between(frame_c, "/* One device the Source picker lists",
                "} drawDeviceSelectorEvent_t;", True),
        extract_function(frame_c, "static void _putFlatVertex("),
        extract_function(frame_c, "static void _putFlatRect("),
        between(frame_c, "/* The Source picker: the focused device's tile",
                "/* What a device can do,"),
    ])


def parse(log: str) -> list[dict]:
    frames = []
    for line in log.splitlines():
        tag, _, rest = line.partition(" ")
        if tag == "F":
            frames.append({"quads": [], "images": [], "strings": [], "hints": []})
        elif tag == "B":
            frames[-1]["quads"].append({"count": int(rest), "points": [], "alpha": []})
        elif tag == "P":
            frames[-1]["quads"][-1]["points"].append(tuple(map(float, rest.split())))
        elif tag == "C":
            frames[-1]["quads"][-1]["alpha"].append(int(rest))
        elif tag == "I":
            ident, x, y, w, h, opacity = map(int, rest.split())
            frames[-1]["images"].append({"id": ident, "cx": x + w / 2, "cy": y + h / 2,
                                         "w": w, "h": h, "opacity": opacity})
        elif tag in ("S", "H"):
            x, y, align, alpha, text = rest.split(" ", 4)
            frames[-1]["strings" if tag == "S" else "hints"].append(
                {"x": int(x), "y": int(y), "alpha": int(alpha), "text": text})
    return frames


def tiles(frame: dict) -> list[dict]:
    """Each tile is one 28-vertex batch: its body, inset, frame and highlight."""
    found = []
    for quad in frame["quads"]:
        if quad["count"] == 28:
            xs = [x for x, _ in quad["points"][:4]]
            ys = [y for _, y in quad["points"][:4]]
            found.append({"cx": (min(xs) + max(xs)) / 2, "width": max(xs) - min(xs),
                          "height": max(ys) - min(ys)})
    return found


class SourcePickerGXStreamTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.frame_c = (GUI / "FrameBufferMagic.c").read_text()
        handler = HANDLER_H.read_text()
        cls.types = "\n".join([
            between(handler, "typedef struct {\n\tint textureId;", "} textureImage;", True),
            re.search(r"#define MAX_DEVICES \d+", handler).group(0),
        ])
        cls.source = picker(cls.frame_c)
        cls.directory = tempfile.TemporaryDirectory(prefix="swiss-picker-gx-")
        cls.program = cls.build(cls.source, "picker")

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    @classmethod
    def build(cls, source: str, name: str) -> Path:
        work = Path(cls.directory.name)
        harness = work / f"{name}.c"
        program = work / name
        harness.write_text("\n".join([PRELUDE, cls.types, source, DRIVER]))
        command = shlex.split(os.environ.get("CC", "cc")) + [
            "-std=c99", "-Wall", "-Wextra", "-pedantic", f"-I{GUI}", str(harness),
            str(GUI / "ui_motion.c"), str(GUI / "ui_stage.c"), "-o", str(program), "-lm",
        ]
        compiled = subprocess.run(command, capture_output=True, text=True, timeout=60)
        assert compiled.returncode == 0, compiled.stdout + compiled.stderr
        return program

    def run_script(self, script: str, program: Path | None = None) -> list[dict]:
        result = subprocess.run([str(program or self.program)], input=script,
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        return parse(result.stdout)

    def assert_rest(self, frame: dict, count: int, focus: int) -> None:
        shown = min(count, 5)
        ids = [image["id"] for image in frame["images"]]
        self.assertEqual(len(tiles(frame)), shown)
        self.assertEqual(sorted(ids), sorted(set(ids)), "a device shows twice")
        self.assertEqual(len(ids), shown)
        # The focused device is in the middle, drawn last, on the largest tile.
        self.assertEqual(ids[-1], 100 + focus)
        middle = tiles(frame)[-1]
        self.assertAlmostEqual(middle["cx"], 320.0, places=1)
        self.assertTrue(all(tile["width"] < middle["width"] for tile in tiles(frame)[:-1]))
        offsets = sorted(round(abs(tile["cx"] - 320.0)) for tile in tiles(frame))
        self.assertEqual(offsets, [0, 158, 158, 238, 238][:shown])
        names = [s for s in frame["strings"] if s["text"].startswith("Device ")]
        self.assertEqual([(s["text"], s["alpha"]) for s in names], [(f"Device {focus}", 255)])
        self.assertEqual(len(frame["hints"]), 1)

    def test_rest_shows_each_listed_device_once(self):
        for count, travel in ((1, 0), (2, 1), (3, -1), (4, 6), (5, 2), (7, -3), (26, 30)):
            with self.subTest(count=count, travel=travel):
                frames = self.run_script(f"L {count} {travel} 0 0 0\nN 90 0.0166667\n")
                self.assert_rest(frames[-1], count, travel % count)

    def test_a_step_slides_the_row_across_the_wrap(self):
        for start, step in ((4, 1), (5, -1)):
            with self.subTest(step=step):
                frames = self.run_script(
                    f"L 5 {start} 0 0 0\nN 90 0.0166667\nT {start + step}\nN 150 0.0166667\n")[90:]
                focus = (start + step) % 5
                path = [next(i["cx"] for i in f["images"] if i["id"] == 100 + focus)
                        for f in frames if any(i["id"] == 100 + focus for i in f["images"])]
                self.assertEqual(len(path), len(frames), "the arriving device vanished")
                self.assertGreater(abs(path[0] - 320.0), 60.0, "the step did not slide")
                moves = [b - a for a, b in zip(path, path[1:])]
                self.assertTrue(all(-40.0 < move * step < 1.0 for move in moves),
                                "a tile jumped or slid the wrong way")
                for frame in frames:
                    self.assertLessEqual(len(tiles(frame)), 5)
                    self.assertLessEqual(len(frame["images"]), 5)
                    self.assertLessEqual(len(frame["strings"]), 4)
                self.assert_rest(frames[-1], 5, focus)

    def test_both_screen_shapes(self):
        for wide, left, right in ((0, 0.0, 640.0), (1, -320.0 / 3.0, 640.0 + 320.0 / 3.0)):
            for destination in (0, 1):
                with self.subTest(wide=wide, destination=destination):
                    frames = self.run_script(
                        f"W {wide}\nL 9 3 1 0 {destination}\nN 60 0.0166667\n"
                        "T 5\nN 30 0.0166667\nT 2\nN 150 0.0166667\n")
                    for frame in frames:
                        pages = [q for q in frame["quads"] if q["count"] == 4]
                        self.assertEqual(len(pages), destination)
                        for page in pages:
                            xs = [x for x, _ in page["points"]]
                            ys = [y for _, y in page["points"]]
                            self.assertAlmostEqual(min(xs), left, places=1)
                            self.assertAlmostEqual(max(xs), right, places=1)
                            self.assertEqual((min(ys), max(ys)), (0.0, 480.0))
                        for quad in frame["quads"]:
                            if quad["count"] == 4:
                                continue
                            for x, y in quad["points"]:
                                self.assertTrue(0.0 <= x <= 640.0 and 0.0 <= y <= 480.0,
                                                f"({x}, {y}) leaves the stage")
                        for image in frame["images"]:
                            self.assertTrue(0.0 <= image["cx"] - image["w"] / 2 and
                                            image["cx"] + image["w"] / 2 <= 640.0)
                    self.assert_rest(frames[-1], 9, 2)

    def test_motion_off_and_a_new_list_snap(self):
        frames = self.run_script("M 2\nL 6 0 0 0 0\nN 1 0.0166667\nT 1\nN 1 0.0166667\n")
        self.assert_rest(frames[0], 6, 0)
        self.assert_rest(frames[1], 6, 1)
        frames = self.run_script("L 4 1 0 0 0\nN 90 0.0166667\nL 12 9 1 0 0\nN 1 0.0166667\n")
        self.assert_rest(frames[-1], 12, 9)

    def test_the_row_rises_as_the_cube_shrinks(self):
        frames = self.run_script("K 0.95\nL 5 0 0 0 0\nN 1 0.0166667\nK 0.77\n"
                                 "N 1 0.0166667\nK 0.5\nN 90 0.0166667\n")
        self.assertEqual(frames[0], {"quads": [], "images": [], "strings": [], "hints": []})
        self.assertEqual(len(frames[1]["images"]), 5)
        self.assertLess(frames[1]["images"][-1]["opacity"], 160)
        self.assertGreater(frames[1]["images"][-1]["cy"], frames[-1]["images"][-1]["cy"])
        self.assert_rest(frames[-1], 5, 0)

    def test_renderer_never_reads_a_handler(self):
        draw = extract_function(self.frame_c, "static void _DrawDeviceSelector(") + \
            extract_function(self.frame_c, "static void _DrawDeviceTile(")
        for probe in ("DEVICEHANDLER_INTERFACE", "devices[", "deviceHandler_", "->details",
                      "->info", "alloc(", "free("):
            self.assertNotIn(probe, draw)
        records = re.sub(r"/\*.*?\*/", "", between(
            self.frame_c, "/* One device the Source picker lists",
            "} drawDeviceSelectorSnapshot_t;"), flags=re.S)
        self.assertNotIn("*", records, "a snapshot record points at live data")
        tile = extract_function(self.frame_c, "static void _DeviceSelectorTile(")
        self.assertIn("tile->picture = device->deviceTexture;", tile)

    def test_mutations_are_rejected(self):
        mutations = {
            "no edge fade": ("presence <= 0.001f || tiles", "tiles"),
            "painter's order reversed": ("fabsf(row[i - 1].place) < fabsf(place)",
                                         "fabsf(row[i - 1].place) > fabsf(place)"),
            "a new list slides": ("!data->started || data->shownAll != s->showAllDevices",
                                  "!data->started"),
        }
        script = ("L 7 3 0 0 0\nN 90 0.0166667\nL 12 9 1 0 0\nN 1 0.0166667\n")
        for name, (old, new) in mutations.items():
            with self.subTest(mutation=name):
                self.assertIn(old, self.source)
                program = self.build(self.source.replace(old, new, 1), "mutant")
                frames = self.run_script(script, program)
                with self.assertRaises(AssertionError):
                    self.assert_rest(frames[89], 7, 3)
                    self.assert_rest(frames[-1], 12, 9)


if __name__ == "__main__":
    unittest.main(verbosity=2)
