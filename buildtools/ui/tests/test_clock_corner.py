#!/usr/bin/env python3
"""Clock places the time and Temperature the temperature dial, each Right, Left
or Off: in one corner the time sits on the dial's inner side, alone each sits
40 in from the edge. The loading spinner and the Source picker's label take
the corner the two leave free, or inside the dial when they hold a corner
each. Runs the real _Corner, _FreeCorner and _DrawTitleBar."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

from test_cheats_gx_stream import extract_function

ROOT = Path(__file__).resolve().parents[3]
FRAME_C = (ROOT / "cube/swiss/source/gui/FrameBufferMagic.c").read_text()

HARNESS = r"""
#include <stdbool.h>
#include <stdio.h>
typedef unsigned char u8;
typedef signed char s8;
typedef struct { u8 r, g, b, a; } GXColor;
typedef struct uiDrawObj uiDrawObj_t;
enum { ALIGN_LEFT, ALIGN_RIGHT, ALIGN_CENTER };
enum { CLOCK_RIGHT, CLOCK_LEFT, CLOCK_OFF };
static struct { int clockPosition, temperaturePosition; } swissSettings;
static struct { float chromeProgress; } frame = {1.0f};
static const struct { float chromeProgress; } *UIScene_Frame(void) { return (void *)&frame; }
static float stageLeft, stageRight;
static float UIStage_Left(void) { return stageLeft; }
static float UIStage_Right(void) { return stageRight; }
static struct {
	s8 coreTemperature;
	char temperatureText[8];
	char timeText[9];
	struct { bool available; } clock;
} systemInstrument = {40, "40C", "12:34:56", {true}};
static void _DrawSystemDial(float x, float y, s8 temperature, u8 opacity)
{ (void)y; (void)temperature; (void)opacity; printf(" dial %.0f", x); }
static void drawStringMedium(int x, int y, const char *text, float scale, int align, GXColor color)
{
	(void)y; (void)scale; (void)color;
	printf(" %s %d %s", text[0] == '4' ? "temp" : "time", x,
		align == ALIGN_LEFT ? "left" : align == ALIGN_RIGHT ? "right" : "center");
}
"""

MAIN = r"""
int main(void)
{
	static const float stages[2][2] = {{0.0f, 640.0f}, {-80.0f, 720.0f}};
	for(int s = 0; s < 2; s++)
	for(int c = CLOCK_RIGHT; c <= CLOCK_OFF; c++)
	for(int t = CLOCK_RIGHT; t <= CLOCK_OFF; t++) {
		float inset;
		stageLeft = stages[s][0]; stageRight = stages[s][1];
		swissSettings.clockPosition = c;
		swissSettings.temperaturePosition = t;
		printf("%d %c%c:", s, "RLO"[c], "RLO"[t]);
		_DrawTitleBar(NULL);
		int corner = _FreeCorner(&inset);
		printf(" | free %d inset %.0f\n", corner, inset);
	}
	return 0;
}
"""


class ClockCornerTest(unittest.TestCase):
    def test_the_time_and_the_dial_take_their_own_corners(self):
        source = "\n".join([HARNESS, extract_function(FRAME_C, "static int _Corner("),
                            extract_function(FRAME_C, "static int _FreeCorner("),
                            extract_function(FRAME_C, "static void _DrawTitleBar("), MAIN])
        with tempfile.TemporaryDirectory(prefix="indigo-clock-") as tmp:
            work = Path(tmp)
            (work / "clock.c").write_text(source)
            built = subprocess.run(shlex.split(os.environ.get("CC", "cc")) +
                ["-std=c99", "-Wall", "-Wextra", "-Werror", str(work / "clock.c"),
                 "-o", str(work / "clock")], capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            lines = subprocess.run([str(work / "clock")], capture_output=True, text=True,
                                   timeout=30).stdout.splitlines()
        self.assertEqual(lines, [
            # 4:3, Clock then Temperature: both Right as before, both Left
            # mirrored; apart, each alone 40 in, and the other items go
            # inside the dial.
            "0 RR: dial 600 temp 600 center time 568 right | free -1 inset 0",
            "0 RL: dial 40 temp 40 center time 600 right | free -1 inset 48",
            "0 RO: time 600 right | free -1 inset 0",
            "0 LR: dial 600 temp 600 center time 40 left | free 1 inset 48",
            "0 LL: dial 40 temp 40 center time 72 left | free 1 inset 0",
            "0 LO: time 40 left | free 1 inset 0",
            "0 OR: dial 600 temp 600 center | free -1 inset 0",
            "0 OL: dial 40 temp 40 center | free 1 inset 0",
            "0 OO: | free -1 inset 0",
            # 16:9: the same, 40 in from the edges the wider frame shows.
            "1 RR: dial 680 temp 680 center time 648 right | free -1 inset 0",
            "1 RL: dial -40 temp -40 center time 680 right | free -1 inset 48",
            "1 RO: time 680 right | free -1 inset 0",
            "1 LR: dial 680 temp 680 center time -40 left | free 1 inset 48",
            "1 LL: dial -40 temp -40 center time -8 left | free 1 inset 0",
            "1 LO: time -40 left | free 1 inset 0",
            "1 OR: dial 680 temp 680 center | free -1 inset 0",
            "1 OL: dial -40 temp -40 center | free 1 inset 0",
            "1 OO: | free -1 inset 0",
        ])

    def test_the_other_corner_items_move_out_of_its_way(self):
        progress = extract_function(FRAME_C, "static void _DrawProgressBar(")
        self.assertIn("corner = _FreeCorner(&inset);", progress)
        self.assertIn("x = (int)(corner > 0 ? UIStage_Right() - 44.0f - inset : "
                      "UIStage_Left() + 44.0f + inset);", progress)
        # The word stays on the wheel's inner side, in the frame, in either corner.
        self.assertIn('drawString(x - 8 * corner, y, "Loading\\205", 0.55f,\n'
                      "\t\t\tcorner > 0 ? ALIGN_RIGHT : ALIGN_LEFT, loadingColor);", progress)
        selector = extract_function(FRAME_C, "static void _DrawDeviceSelector(")
        self.assertIn("corner = _FreeCorner(&inset);", selector)
        self.assertIn("drawStringMedium((int)(corner > 0 ? 600.0f - inset : 40.0f + inset), 44,",
                      selector)
        self.assertIn("corner > 0 ? ALIGN_RIGHT : ALIGN_LEFT,", selector)


if __name__ == "__main__":
    unittest.main()
