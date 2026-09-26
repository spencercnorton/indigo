#!/usr/bin/env python3
"""Run the real Library renderer against a checked GX stream, in every layout.

_DrawGameflow and its helpers are compiled out of FrameBufferMagic.c with the
real ring/grid state, window and navigation code. GX, the font and the poster
cache are stand-ins that check every primitive is declared and emitted in full
and log what is drawn. The tests then check:

- Horizontal draws exactly what the base commit's renderer drew, frame for
  frame, through moves, wraps, pages, Detail and every motion mode: only the
  Library's command line is new.
- Vertical and Grid place their cards where the layouts say, show every card
  a user can see with its cover, never draw a card twice at rest, and move
  without a card jumping when the selection changes (a small grid wraps round
  the screen, so a row can leave at one edge while it arrives at the other).
- From every layout the focused card flies to Detail's pose.
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
# The layouts were added on top of this commit, and its renderer is the
# reference the Horizontal layout must reproduce. Move it forward on purpose,
# and only when the carousel's own drawing changes.
BASE = "v1.22.0"
PURE = ("ui_gameflow.c", "ui_motion.c", "ui_gameflow_library.c",
        "ui_command_rail.c", "ui_gameflow_detail.c", "ui_game_history.c")


def between(source: str, start: str, end: str, inclusive: bool = False) -> str:
    first = source.index(start)
    last = source.index(end, first)
    return source[first:last + (len(end) if inclusive else 0)]


def renderer(frame_c: str, frame_h: str) -> str:
    return "\n".join([
        between(frame_h, "typedef struct uiDrawObj {", "} uiDrawObj_t;", True),
        between(frame_h, "#define UI_GAMEFLOW_RENDER_SLOTS", "} uiGameflowRenderSnapshot_t;", True),
        between(frame_c, "typedef struct drawGameflowDetailPresentation {",
                "} drawGameflowEvent_t;", True),
        between(frame_c, "typedef struct gameflowPoint {", "static void _DrawHomeText("),
        extract_function(frame_c, "static bool _GameflowSnapshotValid("),
        extract_function(frame_c, "static void _GameflowCopySnapshot("),
    ])


PRELUDE = r"""
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define UI_ASSETS_HOST_BUILD 1
#include "ui_assets.h"
#include "ui_gameflow.h"
#include "ui_gameflow_detail.h"
#include "ui_gameflow_library.h"
#include "ui_command_rail.h"
#include "ui_scene.h"

typedef int8_t s8;
typedef struct { u8 r, g, b, a; } GXColor;
#define BNR_PIXELDATA_LEN (96*32*2)
#define ALIGN_LEFT 0
#define ALIGN_CENTER 1
#define ALIGN_RIGHT 2
#define UI_COLOR_INDIGO 0
enum { GX_QUADS = 0x80, GX_VTXFMT0 = 0, GX_TEXMAP0 = 0, GX_BM_BLEND = 1,
	GX_BL_SRCALPHA = 4, GX_BL_INVSRCALPHA = 5, GX_LO_CLEAR = 0,
	GX_TF_RGB5A3 = 5, GX_CLAMP = 0, GX_FALSE = 0, GX_LINEAR = 1, GX_NEAR = 0 };
static struct { int uiColor; } swissSettings;

#define CHECK(c) do { if(!(c)) { fprintf(stderr, "failed %d: %s\n", __LINE__, #c); exit(73); } } while(0)
static FILE *out;
static bool active;
static int declared, emitted, phase;
static void drawInit(void) { CHECK(!active); }
static void _SetupRasterColor(void) { CHECK(!active); }
static void UIColor_Apply(u8 *r, u8 *g, u8 *b) { (void)r; (void)g; (void)b; }
static void GX_SetNumTevStages(int n) { CHECK(!active); (void)n; }
static void GX_SetBlendMode(int a, int b, int c, int d) { CHECK(!active); (void)a; (void)b; (void)c; (void)d; }
static void GX_InvalidateTexAll(void) { CHECK(!active); }
static void GX_LoadTexObj(GXTexObj *texture, int map) { CHECK(!active); (void)map; fprintf(out, "X %s\n", (const char *)texture->data); }
static void GX_Begin(int primitive, int format, int count)
{
	CHECK(!active && primitive == GX_QUADS && format == GX_VTXFMT0 && count > 0 && count % 4 == 0);
	active = true; declared = count; emitted = 0; phase = 0;
	fprintf(out, "B %d\n", count);
}
static void GX_Position3f32(float x, float y, float z)
{
	CHECK(active && phase == 0 && z == 0.0f && isfinite(x) && isfinite(y));
	phase = 1; fprintf(out, "P %.2f %.2f\n", x, y);
}
static void GX_Color4u8(u8 r, u8 g, u8 b, u8 a) { CHECK(active && phase == 1); phase = 2; fprintf(out, "C %u %u %u %u\n", r, g, b, a); }
static void GX_TexCoord2f32(float s, float t)
{
	CHECK(active && phase == 2 && emitted < declared); phase = 0; emitted++;
	fprintf(out, "T %.4f %.4f\n", s, t);
}
static void GX_End(void) { CHECK(active && phase == 0 && emitted == declared); active = false; }
static void DCFlushRange(void *p, u32 n) { (void)p; (void)n; }
static void GX_InitTexObj(GXTexObj *t, void *d, int w, int h, int f, int s, int tt, int m)
{ (void)w; (void)h; (void)f; (void)s; (void)tt; (void)m; t->data = d; }
static void GX_InitTexObjFilterMode(GXTexObj *t, int a, int b) { (void)t; (void)a; (void)b; }
static void drawStringMedium(int x, int y, const char *text, float scale, int align, GXColor c)
{ CHECK(!active); fprintf(out, "S %d %d %.3f %d %u %s\n", x, y, scale, align, c.a, text); }
static void _DrawHintText(int x, int y, const char *text, float scale, int align, GXColor c)
{ CHECK(!active); fprintf(out, "H %d %d %.3f %d %u %s\n", x, y, scale, align, c.a, text); }
static void _HintRoundRect(float cx, float cy, float w, float h, float r, GXColor c)
{ CHECK(!active); fprintf(out, "R %.2f %.2f %.2f %.2f %.2f %u\n", cx, cy, w, h, r, c.a); }
static int GetTextSizeInPixels(const char *text) { return 11 * (int)strlen(text); }
static float GetTextScaleToFitInWidthWithMax(const char *text, int width, float maximum)
{
	float size = (float)GetTextSizeInPixels(text);
	float scale = size < (float)width ? 1.0f : (float)width / size;
	return scale < maximum ? scale : maximum;
}
/* Every seventh game has no cover, so the fallback art is drawn too. */
static GXTexObj covers[1000];
static char coverIds[1000][8];
uiPosterResult_t UIAssets_Query(const char *id, size_t length, bool bnr, uiPosterHandle_t *handle)
{
	int n = atoi(id + 1);
	(void)bnr;
	if(length < 6 || id[0] != 'G' || n % 7 == 3) return UI_POSTER_PROCEDURAL_CARD;
	handle->slot = (u16)n; handle->generation = 1u;
	return UI_POSTER_EXACT;
}
GXTexObj *UIAssets_Peek(uiPosterHandle_t handle)
{
	memcpy(coverIds[handle.slot], "", 1);
	snprintf(coverIds[handle.slot], sizeof(coverIds[0]), "G%03uE0", handle.slot);
	covers[handle.slot].data = coverIds[handle.slot];
	return &covers[handle.slot];
}
bool UIAssets_DominantColor(const char *id, size_t length, u8 *r, u8 *g, u8 *b)
{ (void)id; (void)length; (void)r; (void)g; (void)b; return false; }
static uiSceneFrame_t sceneFrame;
const uiSceneFrame_t *UIScene_Frame(void) { return &sceneFrame; }
static float animDelta, animSeconds;
static float UIAnim_Delta(void) { return animDelta; }
static float UIAnim_Seconds(void) { return animSeconds; }
static uiMotionMode_t motionMode;
static uiMotionMode_t _CurrentMotionMode(void) { return motionMode; }
"""

DRIVER = r"""
static drawGameflowEvent_t *eventData;
static uiDrawObj_t event;
static uiGameflowRenderSnapshot_t snapshot;
static uint32_t generation;

/* L layout count selected | P selected hint rowDirection snap
 * | M motion | D mode | N frames dt  -- the log has one "F" per frame. */
static void publish(int layout, uint32_t count, uint32_t selected, int hint,
	int rowDirection, int snap, bool first)
{
	uiGameflowLibraryWindowSlot_t slots[25];
	size_t n;
	memset(&snapshot, 0, sizeof(snapshot));
	snapshot.selection.generation = ++generation;
	snapshot.selection.itemCount = count;
	snapshot.selection.selectedIndex = selected;
	snapshot.selection.directionHint = (uiGameflowDirection_t)hint;
	snapshot.selection.snapTransition = snap != 0;
#if LAYOUTS
	snapshot.layout = (u8)layout;
	if(layout == UI_GAMEFLOW_LAYOUT_GRID) {
		snapshot.columns = 5;
		n = UIGameflowLibrary_BuildGridWindow(count, selected, 5u,
			(uiGameflowDirection_t)rowDirection, slots);
	}
	else
#else
	(void)layout; (void)rowDirection;
#endif
	n = UIGameflowLibrary_BuildWindow(count, selected,
		(uiGameflowDirection_t)hint, slots);
	snapshot.recordCount = (u32)n;
	for(size_t i = 0; i < n; ++i) {
		uiGameflowCardSnapshot_t *record = &snapshot.records[i];
		record->flags = UI_GAMEFLOW_CARD_VALID |
			(slots[i].index % 5u == 1u ? UI_GAMEFLOW_CARD_CUSTOM : 0u);
		record->libraryIndex = slots[i].index;
		record->relativeSlot = slots[i].relativeSlot;
#if LAYOUTS
		record->column = slots[i].column;
#endif
		snprintf(record->gameId, sizeof(record->gameId), "G%03uE0", slots[i].index);
		snprintf(record->title, sizeof(record->title), "Game number %u", slots[i].index);
		snprintf(record->company, sizeof(record->company), "Company %u", slots[i].index % 9u);
		snprintf(record->facts, sizeof(record->facts), "%s  |  1.4 GB", record->gameId);
	}
	CHECK(_GameflowSnapshotValid(&snapshot));
	if(first) {
		memset(eventData, 0, sizeof(*eventData));
		UIGameflow_Init(&eventData->state);
#if LAYOUTS
		UIGameflow_SetColumns(&eventData->state, snapshot.columns);
#endif
	}
	CHECK(UIGameflow_ApplySnapshot(&eventData->state, &snapshot.selection, motionMode));
	_GameflowCopySnapshot(eventData, &snapshot);
}

int main(void)
{
	char line[128];
	int layout = 0;
	uint32_t count = 0;
	out = stdout;
	eventData = calloc(1, sizeof(*eventData));
	CHECK(eventData != NULL);
	event.data = eventData;
	sceneFrame.scene = UI_SCENE_LIBRARY;
	sceneFrame.chromeProgress = 1.0f;
	sceneFrame.libraryReveal = 1.0f;
	while(fgets(line, sizeof(line), stdin)) {
		unsigned a, b; int c, d, e; float dt;
		if(sscanf(line, "L %d %u %u", &layout, &a, &b) == 3) {
			count = a; publish(layout, count, b, 0, 0, 0, true);
		}
		else if(sscanf(line, "P %u %d %d %d", &a, &c, &d, &e) == 4) {
			publish(layout, count, a, c, d, e, false);
		}
		else if(sscanf(line, "M %d", &c) == 1) motionMode = (uiMotionMode_t)c;
		else if(sscanf(line, "D %d", &c) == 1) {
			UIGameflow_SetMode(&eventData->state, (uiGameflowMode_t)c, motionMode);
			sceneFrame.scene = c ? UI_SCENE_GAME_DETAIL : UI_SCENE_LIBRARY;
		}
		else if(sscanf(line, "N %u %f", &a, &dt) == 2) {
			for(b = 0; b < a; ++b) {
				animDelta = dt; animSeconds += dt;
				fprintf(out, "F\n");
				_DrawGameflow(&event);
			}
		}
		else { fprintf(stderr, "bad command: %s", line); return 64; }
	}
	return 0;
}
"""


def build(work: Path, gui: Path, frame_c: str, frame_h: str, layouts: bool, name: str) -> Path:
    source = work / f"{name}.c"
    source.write_text(PRELUDE + renderer(frame_c, frame_h) + DRIVER)
    binary = work / name
    flags = ["-std=gnu11", "-O1", "-Wall", "-Wextra", "-Wno-unused-function",
             "-Wno-unused-parameter", "-fsanitize=address,undefined",
             "-fno-sanitize-recover=all", f"-DLAYOUTS={1 if layouts else 0}",
             "-I" + str(gui)]
    if os.uname().sysname == "Linux":
        flags += ["-fno-pie", "-no-pie"]
    result = subprocess.run(shlex.split(os.environ.get("CC", "cc")) + flags +
                            ["-o", str(binary), str(source)] +
                            [str(gui / pure) for pure in PURE] + ["-lm"],
                            capture_output=True, text=True, timeout=180)
    if result.returncode:
        raise AssertionError(result.stderr[-4000:])
    return binary


def frames(log: str) -> list:
    """Each frame's lines, after the first 'F'."""
    return log.split("F\n")[1:]


def covers(frame: str) -> dict:
    """gameId -> list of drawn cover quads (x0, y0, x1, y1) in one frame."""
    found = {}
    lines = frame.splitlines()
    for i, line in enumerate(lines):
        if line.startswith("X "):
            points = [tuple(map(float, l.split()[1:])) for l in lines[i + 1:i + 14]
                      if l.startswith("P ")][:4]
            xs = [p[0] for p in points]
            ys = [p[1] for p in points]
            found.setdefault(line[2:], []).append((min(xs), min(ys), max(xs), max(ys)))
    return found


def centre(box):
    return ((box[0] + box[2]) / 2, (box[1] + box[3]) / 2)


# Horizontal through moves, wraps, page snaps, Detail and back, in each
# motion mode and small libraries; the same script runs on both renderers.
HORIZONTAL = []
for mode in (0, 1, 2):
    for count, start in ((40, 1), (2, 0), (3, 2), (7, 6), (1, 0)):
        HORIZONTAL += [f"M {mode}", f"L 0 {count} {start}", "N 2 0.0167"]
        selected = start
        for move, (hint, snap) in enumerate(((1, 0), (1, 0), (-1, 0), (1, 1), (-1, 1), (-1, 0))):
            if snap:
                selected = 0 if selected == count - 1 else min(count - 1, selected + 9) \
                    if hint > 0 else (count - 1 if selected == 0 else max(0, selected - 9))
            else:
                selected = (selected + hint) % count
            HORIZONTAL += [f"P {selected} {hint} 0 {snap}", "N 3 0.0167"]
            if move == 2:
                HORIZONTAL += ["N 1 0.004", "D 1", "N 20 0.0167", "D 0", "N 20 0.0167"]
        HORIZONTAL += ["N 30 0.0167"]


class GameflowGxStream(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="swiss-gameflow-gx-")
        work = Path(cls.tmp.name)
        cls.frame_c = (GUI / "FrameBufferMagic.c").read_text()
        cls.frame_h = (GUI / "FrameBufferMagic.h").read_text()
        cls.binary = build(work, GUI, cls.frame_c, cls.frame_h, True, "layouts")
        base = work / "base-tree"
        base.mkdir()
        archive = subprocess.run(["git", "-C", str(ROOT), "archive", BASE,
                                  "cube/swiss/source/gui"], capture_output=True)
        if archive.returncode:
            raise AssertionError(archive.stderr.decode()[-2000:])
        subprocess.run(["tar", "-x", "-C", str(base)], input=archive.stdout, check=True)
        base_gui = base / "cube/swiss/source/gui"
        cls.base = build(work, base_gui, (base_gui / "FrameBufferMagic.c").read_text(),
                         (base_gui / "FrameBufferMagic.h").read_text(), False, "base")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_script(self, script, binary=None):
        result = subprocess.run([str(binary or self.binary)], input="\n".join(script) + "\n",
                                capture_output=True, text=True, timeout=120)
        self.assertEqual(result.returncode, 0, result.stderr[-2000:])
        return result.stdout

    def test_horizontal_draws_what_the_carousel_always_drew(self):
        now = self.run_script(HORIZONTAL)
        before = self.run_script(HORIZONTAL, self.base)
        self.assertGreater(len(frames(now)), 600)
        new_hint = "H 320 428 0.460 1 {} D-PAD  BROWSE   A  OPEN   Y  SETTINGS   X  BACK   B  HOME"
        old_hint = "H 320 428 0.460 1 {} D-PAD  BROWSE   A  OPEN   X  BACK   B  HOME"
        hints = 0
        for number, (a, b) in enumerate(zip(frames(now), frames(before))):
            a_lines, b_lines = a.splitlines(), b.splitlines()
            self.assertEqual(len(a_lines), len(b_lines), f"frame {number}")
            for x, y in zip(a_lines, b_lines):
                if x != y:
                    alpha = x.split()[5]
                    self.assertEqual((x, y), (new_hint.format(alpha), old_hint.format(alpha)),
                                     f"frame {number}")
                    hints += 1
        self.assertEqual(len(frames(now)), len(frames(before)))
        self.assertGreater(hints, 300)

    def test_vertical_column(self):
        log = self.run_script(["L 1 40 12", "N 40 0.0167"])
        rest = covers(frames(log)[-1])
        # The selected cover and its two neighbours show their covers.
        self.assertEqual(set(rest), {"G011E0", "G012E0", "G013E0"})
        # The selected cover is large at the left, a neighbour above and one below.
        selected = rest["G012E0"][0]
        self.assertEqual(len(rest["G012E0"]), 1)
        self.assertLess(selected[2], 240)
        self.assertGreater(selected[3] - selected[1], 200)
        above, below = rest["G011E0"][0], rest["G013E0"][0]
        self.assertLess(above[3], selected[1])
        self.assertGreater(below[1], selected[3])
        self.assertLess(above[3] - above[1], 70)
        for boxes in rest.values():
            for box in boxes:
                self.assertTrue(0 <= box[1] and box[3] <= 430, box)
        # Its title, publisher and facts sit to the right of the cover.
        text = [l for l in frames(log)[-1].splitlines() if l.startswith("S ")]
        self.assertIn("S 262 206", "\n".join(text))
        self.assertTrue(any(l.startswith("S 262 ") and l.endswith("Game number 12") for l in text))
        self.assertTrue(any(l.startswith("S 262 ") and l.endswith("G012E0  |  1.4 GB") for l in text))
        # Up steps back one card without a jump, and the ring wraps.
        log = self.run_script(["L 1 40 0", "N 40 0.0167", "P 39 -1 0 0", "N 1 0.0",
                               "N 40 0.0167"])
        before, after, rest = (covers(f) for f in (frames(log)[39], frames(log)[40],
                                                   frames(log)[-1]))
        for game, boxes in before.items():
            if game in after:
                self.assertEqual(boxes, after[game], game)
        self.assertLess(rest["G039E0"][0][2], 240)
        self.assertGreater(rest["G039E0"][0][3] - rest["G039E0"][0][1], 200)

    def test_grid_rows_and_highlight(self):
        log = self.run_script(["L 2 40 18", "N 40 0.0167"])
        rest = covers(frames(log)[-1])
        # Fifteen cards are drawn at all: the rows two away are gone.
        self.assertEqual(frames(log)[-1].splitlines()[0], "B 60")
        # Three rows of five on screen, every card with a cover showing it.
        expected = {f"G{i:03d}E0" for i in range(10, 25) if i % 7 != 3}
        self.assertEqual(set(rest), expected)
        for game, boxes in rest.items():
            self.assertEqual(len(boxes), 1, game)
            x, y = centre(boxes[0])
            index = int(game[1:4])
            self.assertAlmostEqual(x, 120 + (index % 5) * 100, delta=1.0)
            self.assertAlmostEqual(y, 108 + (index // 5 - 2) * 108, delta=1.0)
            width = boxes[0][2] - boxes[0][0]
            self.assertAlmostEqual(width, (79.2 if index == 18 else 72.0) * (1 - 2 / 30),
                                   delta=1.5, msg=game)
        # The highlight frames the selected card: three rings of 16 vertices.
        highlight = frames(log)[-1].split("B 48\n")
        self.assertEqual(len(highlight), 2)
        ring = [tuple(map(float, l.split()[1:])) for l in highlight[1].splitlines()
                if l.startswith("P ")][:48]
        xs, ys = [p[0] for p in ring], [p[1] for p in ring]
        self.assertAlmostEqual((min(xs) + max(xs)) / 2, 420, delta=1.0)
        self.assertAlmostEqual((min(ys) + max(ys)) / 2, 216, delta=1.0)
        # It reaches the rows above and below but never covers their cards.
        self.assertLessEqual(max(ys) - min(ys), 2 * (108 - 96 / 2))
        # Every card showing its art carries the own-settings mark (a plate,
        # three bars and three knobs), sized to the card: games 11, 16, 21.
        marks = [l for l in frames(log)[-1].splitlines() if l.startswith("R ")]
        self.assertEqual(len(marks), 3 * 7)
        plates = sorted(float(l.split()[3]) for l in marks[::7])
        self.assertAlmostEqual(plates[0], 96 * 0.13, delta=0.5)
        # The selected title and publisher sit in the strip above the hint.
        text = frames(log)[-1]
        self.assertIn("S 320 388", text)
        self.assertIn("Game number 18", text)
        self.assertIn("H 320 428", text)

    GRID_MOVES = {
            "right": (40, 17, "P 18 1 0 0"),
            "row wrap right": (40, 19, "P 20 1 1 0"),
            "down": (40, 17, "P 22 1 1 0"),
            "up": (40, 17, "P 12 -1 -1 0"),
            "down from the short last row": (42, 41, "P 1 1 1 0"),
            "up to the short last row": (42, 1, "P 41 -1 -1 0"),
            "three rows down": (13, 7, "P 12 1 1 0"),
            "three rows up": (13, 2, "P 12 -1 -1 0"),
            "four rows down": (18, 12, "P 17 1 1 0"),
            "four rows up": (18, 2, "P 17 -1 -1 0"),
    }

    def check_grid_move(self, count, start, move):
        log = self.run_script([f"L 2 {count} {start}", "N 40 0.0167", move,
                               "N 1 0.0", "N 60 0.0167"])
        before, after = covers(frames(log)[39]), covers(frames(log)[40])
        for game, boxes in before.items():
            self.assertIn(game, after, game)
            for box in boxes:
                self.assertTrue(any(max(abs(a - b) for a, b in zip(box, other)) < 1.01
                                    for other in after[game]), (game, box, after[game]))
        rest = covers(frames(log)[-1])
        for game, boxes in rest.items():
            self.assertEqual(len(boxes), 1, game)

    def check_all_grid_moves(self):
        for count, start, move in self.GRID_MOVES.values():
            self.check_grid_move(count, start, move)

    def test_grid_moves_never_jump(self):
        for name, (count, start, move) in self.GRID_MOVES.items():
            with self.subTest(move=name):
                self.check_grid_move(count, start, move)

    def test_tiny_grids_and_columns_draw_each_game_once(self):
        for layout in (1, 2):
            for count in range(1, 11):
                for selected in range(count):
                    with self.subTest(layout=layout, count=count, selected=selected):
                        log = self.run_script([f"L {layout} {count} {selected}",
                                               "N 30 0.0167"])
                        rest = covers(frames(log)[-1])
                        for game, boxes in rest.items():
                            self.assertEqual(len(boxes), 1, game)
                        if layout == 2 and count <= 5:
                            self.assertEqual(len(rest), sum(1 for i in range(count) if i % 7 != 3))

    def test_every_layout_flies_to_detail(self):
        for layout in (0, 1, 2):
            with self.subTest(layout=layout):
                log = self.run_script([f"L {layout} 40 18", "N 30 0.0167", "D 1",
                                       "N 60 0.0167"])
                rest = covers(frames(log)[-1])
                self.assertEqual(set(rest), {"G018E0"})
                box = rest["G018E0"][0]
                self.assertAlmostEqual(box[0], 48 + 6, delta=1.0)
                self.assertAlmostEqual(box[2], 228 - 6, delta=1.0)
                self.assertAlmostEqual(box[1], 96 + 8, delta=1.0)
                self.assertAlmostEqual(box[3], 336 - 8, delta=1.0)
                self.assertNotIn("H 320 428", frames(log)[-1])

    def test_off_and_reduced_snap_like_the_carousel(self):
        for layout in (1, 2):
            with self.subTest(layout=layout):
                log = self.run_script(["M 2", f"L {layout} 40 17", "N 5 0.0167",
                                       "P 22 1 1 0", "N 1 0.0167"])
                self.assertEqual(frames(log)[-1], self.run_script(
                    ["M 2", f"L {layout} 40 22", "N 5 0.0167"]).split("F\n")[-1])

    def test_mutants_fail(self):
        work = Path(self.tmp.name)
        mutants = {
            "grid rows never wrap round the screen": ("int copies = rows >= 3u ? 1 : 0;",
                                                      "int copies = 0;"),
            "rows two away show": ("\tif(distance >= 2.0f) {\n\t\treturn 0.0f;\n\t}\n\treturn distance <= 1.0f ? 1.0f - 0.34f * distance :\n\t\t0.66f * (2.0f - distance);",
                                   "\tif(distance >= 3.0f) {\n\t\treturn 0.0f;\n\t}\n\treturn distance <= 1.0f ? 1.0f - 0.34f * distance :\n\t\t0.33f * (3.0f - distance);"),
            "the grid's far cards lose their covers": ("card->art = fabsf(row) < 1.5f;",
                                                       "card->art = distance < 1.5f;"),
            "the column uses the carousel's poses": ("_GameflowSamplePoseIn(gameflowVerticalPoses, slot) :",
                                                     "_GameflowSamplePose(slot) :"),
            "the highlight stays put": ("_GameflowGridQuad(frame->columnPosition, 0.0f,\n\t\t1.0f);",
                                        "_GameflowGridQuad(0.0f, 0.0f,\n\t\t1.0f);"),
        }
        tests = (self.test_grid_rows_and_highlight, self.check_all_grid_moves,
                 self.test_vertical_column)
        original = self.binary
        try:
            for index, (name, (old, new)) in enumerate(mutants.items()):
                with self.subTest(mutant=name):
                    self.assertEqual(self.frame_c.count(old), 1, name)
                    self.binary = build(work, GUI, self.frame_c.replace(old, new),
                                        self.frame_h, True, f"mutant{index}")
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
