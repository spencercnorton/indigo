#!/usr/bin/env python3
"""Run the real Settings page renderer against a logged panel stream, for
what marks the current tab.

_DrawSettingsPage is compiled out of FrameBufferMagic.c with
the real springs (ui_motion.c), Settings' focus card (ui_settings_focus.c)
and screen shapes (ui_stage.c). Every flat panel they draw is logged with its
place, size and color; the rows, text and banners are stand-ins. The tests
check that the current tab's cell slides from tab to tab on a spring, each edge moving one way only and
never by much in a frame, rest exactly where they always were, and move at
once under Motion Off; and that Settings' focus card, which springs its
middle and its size, rounds its edges, so each edge moves one way too.
"""

import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

from test_cheats_gx_stream import extract_function


ROOT = Path(__file__).resolve().parents[3]
GUI = ROOT / "cube/swiss/source/gui"


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
#include "ui_settings_focus.h"
#include "ui_stage.h"

typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int32_t s32;
typedef uint32_t u32;
typedef struct { u8 r, g, b, a; } GXColor;
typedef struct { void *data; } GXTexObj;
typedef struct { void *data; } GXTlutObj;
typedef struct uiDrawObj { int type; void *data; } uiDrawObj_t;
#define ALIGN_LEFT 0
#define ALIGN_CENTER 1
#define ALIGN_RIGHT 2
#define CARD_BANNER_W 96
#define CARD_BANNER_H 32
#define CARD_BANNER_NONE 0
#define CARD_BANNER_CI 1
#define CARD_BANNER_RGB 2
static void drawInit(void) {}
static void _CheatsPanel(int x, int y, int width, int height, GXColor color)
{
	printf("Q %d %d %d %d %u %u %u\n", x, y, width, height, color.r, color.g, color.b);
}
static void drawStringMedium(int x, int y, const char *text, float scale,
	int align, GXColor color)
{ (void)x; (void)y; (void)text; (void)scale; (void)align; (void)color; }
static void _DrawHintText(int x, int y, const char *text, float scale,
	int align, GXColor color)
{ (void)x; (void)y; (void)text; (void)scale; (void)align; (void)color; }
static int GetTextSizeInPixels(const char *text) { return 20 * (int)strlen(text); }
static void _DrawTexObjNow(GXTexObj *texture, int x, int y, int width, int height,
	int depth, float s1, float s2, float t1, float t2, int centered)
{ (void)texture; (void)x; (void)y; (void)width; (void)height; (void)depth;
  (void)s1; (void)s2; (void)t1; (void)t2; (void)centered; }
static void _SettingsRow(const uiSetLayout_t *layout, int slot,
	const uiSetPageRow_t *row, bool focused)
{ (void)layout; (void)slot; (void)row; (void)focused; }
static float animDelta;
static float UIAnim_Delta(void) { return animDelta; }
static uiMotionMode_t motionMode;
static uiMotionMode_t _CurrentMotionMode(void) { return motionMode; }
"""

DRIVER = r"""
static drawSettingsEvent_t settings;
static uiDrawObj_t settingsEvent = {0, &settings};

/* Three tabs of different widths, and a focus card on the first row. */
static void settingsTab(int tab)
{
	uiSetLayout_t *l = &settings.snapshot.layout;
	l->tabCount = 3;
	l->currentTab = tab;
	l->tabTrack = (uiSetLayoutRect_t){56, 84, 528, 32};
	for(int i = 0; i < 3; ++i) {
		l->tabCell[i] = (uiSetLayoutRect_t){(short)(60 + 170 * i), 86,
			(short)(110 + 20 * i), 28};
	}
	l->selectedRow = 0;
	l->firstVisibleRow = 0;
	l->focusRect = (uiSetLayoutRect_t){48, 150, 544, 40};
}

int main(void)
{
	char line[128];
	while(fgets(line, sizeof(line), stdin)) {
		int a, b;
		float dt;
		if(sscanf(line, "S %d", &a) == 1) settingsTab(a);
		else if(sscanf(line, "M %d", &a) == 1) motionMode = (uiMotionMode_t)a;
		else if(sscanf(line, "R %d %d", &a, &b) == 2) {
			/* The focus card's rect: x and width, centred the same. */
			settings.snapshot.layout.focusRect.x = (short)a;
			settings.snapshot.layout.focusRect.w = (short)b;
		}
		else if(sscanf(line, "N %c %d %f", (char *)&b, &a, &dt) == 3) {
			for(int i = 0; i < a; ++i) {
				animDelta = dt;
				printf("F\n");
				_DrawSettingsPage(&settingsEvent);
			}
		}
		else { fprintf(stderr, "bad command: %s", line); return 64; }
	}
	return 0;
}
"""


def renderers(frame_c: str, frame_h: str) -> str:
    return "\n".join([
        between(frame_c, "static const GXColor settingsInk",
                "typedef struct {\n\tuiSetPageSnapshot_t snapshot;"),
        between(frame_c, "typedef struct {\n\tuiSetPageSnapshot_t snapshot;",
                "} drawSettingsEvent_t;", True),
        extract_function(frame_c, "static void _SettingsBox("),
        extract_function(frame_c, "static void _SettingsFocusCard("),
        extract_function(frame_c, "static void _PagePanel("),
        extract_function(frame_c, "static void _DrawSettingsPage("),
    ])


def panels(log: str) -> list[list[tuple]]:
    """Each frame's panels: (x, y, width, height, (r, g, b))."""
    frames = []
    for line in log.splitlines():
        if line == "F":
            frames.append([])
        elif line.startswith("Q "):
            x, y, w, h, r, g, b = map(int, line.split()[1:])
            frames[-1].append((x, y, w, h, (r, g, b)))
    return frames


FOCUS = (38, 37, 78)    # settingsFocus


class PageTabsGXStreamTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        frame_c = (GUI / "FrameBufferMagic.c").read_text()
        frame_h = (GUI / "FrameBufferMagic.h").read_text()
        cls.directory = tempfile.TemporaryDirectory(prefix="swiss-page-tabs-")
        work = Path(cls.directory.name)
        harness = work / "tabs.c"
        cls.program = work / "tabs"
        harness.write_text("\n".join([PRELUDE, renderers(frame_c, frame_h), DRIVER]))
        command = shlex.split(os.environ.get("CC", "cc")) + [
            "-std=c99", "-Wall", "-Wextra", "-Wno-unused-function", f"-I{GUI}",
            str(harness), *(str(GUI / name) for name in (
                "ui_motion.c", "ui_settings_focus.c", "ui_stage.c")),
            "-o", str(cls.program), "-lm",
        ]
        compiled = subprocess.run(command, capture_output=True, text=True, timeout=60)
        assert compiled.returncode == 0, compiled.stdout + compiled.stderr

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def run_script(self, script: str) -> list[list[tuple]]:
        result = subprocess.run([str(self.program)], input=script,
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        return panels(result.stdout)

    def check_slide(self, frames, mark, start, end):
        """mark picks a frame's marker; it goes from start to end smoothly."""
        path = [mark(f) for f in frames]
        self.assertEqual(path[0][:2], start[:2], path[:3])
        self.assertEqual(path[-1], end)
        lefts = [p[0] for p in path]
        rights = [p[0] + p[1] for p in path]
        distance = abs(end[0] - start[0])
        for edge in (lefts, rights):
            steps = [b - a for a, b in zip(edge, edge[1:])]
            self.assertTrue(all(0 <= step <= distance * 0.3 for step in steps), edge)
            self.assertGreater(sum(1 for step in steps if step), 5, edge)
        # Its width stays between the two tabs' widths.
        low, high = sorted((start[1], end[1]))
        self.assertTrue(all(low <= p[1] <= high for p in path), path)

    def test_settings_tab_cell_slides(self):
        def cell(frame):
            found = [(x, w) for x, y, w, h, color in frame if color == FOCUS and y == 86]
            self.assertEqual(len(found), 1)
            return found[0]
        frames = self.run_script("S 0\nN s 30 0.0166667\nS 2\nN s 40 0.0166667\n")
        self.assertEqual(cell(frames[29]), (60, 110))
        self.check_slide(frames[29:], cell, (60, 110), (400, 150))
        frames = self.run_script("M 2\nS 0\nN s 3 0.0166667\nS 2\nN s 1 0.0166667\n")
        self.assertEqual(cell(frames[-1]), (400, 150))

    def test_settings_focus_card_edges_move_one_way(self):
        """A card that narrows about its middle: its left edge only moves
        right and its right edge only left, a pixel or a few at a time."""
        def card(frame):
            found = [(x, x + w) for x, y, w, h, color in frame if color == FOCUS and y == 150]
            self.assertEqual(len(found), 1)
            return found[0]
        frames = self.run_script("S 0\nN s 30 0.0166667\nR 68 504\nN s 60 0.0166667\n")[29:]
        self.assertEqual(card(frames[0]), (48, 592))
        self.assertEqual(card(frames[-1]), (68, 572))
        lefts = [card(f)[0] for f in frames]
        rights = [card(f)[1] for f in frames]
        self.assertEqual(lefts, sorted(lefts))
        self.assertEqual(rights, sorted(rights, reverse=True))



if __name__ == "__main__":
    unittest.main(verbosity=2)
