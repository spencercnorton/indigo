#!/usr/bin/env python3
"""Menu Color recolors Indigo's own colors and nothing else.

ui_color.c turns the hue of Indigo's blue-violet family and leaves every
other color alone. This test compiles it and runs every color written in
the GUI sources through it: Indigo must leave all of them exactly as they
are, neutrals (white, which also draws posters and banners) and the colors
that mean something must never change, and every other color must follow
the chosen color with its luma kept.
"""

import math
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
SWISS_H = (ROOT / "cube/swiss/include/swiss.h").read_text()
SETTINGS_C = (GUI / "settings.c").read_text()
UI_COLOR_C = (GUI / "ui_color.c").read_text()
FRAME_C = (GUI / "FrameBufferMagic.c").read_text()
FONT_C = (GUI / "IPLFontWrite.c").read_text()
# The controller-button icons keep the controller's own colors: their shapes
# go through _HintVertex and their letters through drawStringMediumUntinted.
UNTINTED = "static void _DrawHintGlyph("

DRIVER = r"""
#include <stdint.h>
#include <stdio.h>
#include "ui_color.h"

/* color r g b per line in, r g b out */
int main(void)
{
	int color, r, g, b;
	while(scanf("%d %d %d %d", &color, &r, &g, &b) == 4) {
		uint8_t red = (uint8_t)r, green = (uint8_t)g, blue = (uint8_t)b;
		UIColor_Select(color);
		UIColor_Apply(&red, &green, &blue);
		printf("%d %d %d\n", red, green, blue);
	}
	return 0;
}
"""

# Colors that mean the same thing under every Menu Color.
SEMANTIC = {
    (26, 93, 103), (115, 234, 234), (151, 250, 246),   # enabled cheats
    (255, 207, 139),                                   # cheat warning
    (82, 20, 42), (255, 75, 118), (255, 132, 158),     # restart confirmation
    (255, 225, 232), (205, 169, 184),
    (243, 126, 145),                                   # recoverable error
    (0, 128, 0), (255, 255, 0), (255, 128, 0),         # legacy tick, star, progress
    (255, 0, 0), (0, 255, 0),                          # glass: TEV channel masks, not colors
    (255, 247, 236),                                   # glass: the sun's warm white core
    (255, 184, 150), (90, 224, 246),                   # glass: dispersion fringes on the rim
}
# Pure blue can't turn without clipping, so its luma moves. It is the legacy
# backdrop's tint, which the opaque Indigo wash covers.
CLIPS = {(0, 0, 255)}
NEUTRALS = [(0, 0, 0), (87, 87, 87), (128, 128, 128), (200, 200, 200),
            (216, 216, 216), (255, 255, 255)]
MARGIN = 4.0


def luma(rgb):
    r, g, b = rgb
    return (77 * r + 150 * g + 29 * b + 128) >> 8


def chroma_angle(rgb):
    """(angle from the indigo axis in degrees, chroma) as ui_color.c sees it."""
    y = luma(rgb)
    u, v = rgb[2] - y, rgb[0] - y
    return math.degrees(math.atan2(v, u)), math.hypot(u, v)


def literal_colors():
    """Every chromatic color written out in the GUI sources, with where it is."""
    brace = re.compile(r"\{\s*(\d{1,3})\s*,\s*(\d{1,3})\s*,\s*(\d{1,3})\s*,[^{}]*\}")
    call = re.compile(r"GX_Color4u8\(\s*(\d{1,3})\s*,\s*(\d{1,3})\s*,\s*(\d{1,3})\s*,")
    found = {}
    for path in sorted(GUI.glob("*.c")):
        if path.name in ("blankbanner.c",):
            continue
        text = path.read_text(errors="replace")
        if UNTINTED in text:
            icons = extract_function(text, UNTINTED)
            text = text.replace(icons, "\n" * icons.count("\n"))
        for number, line in enumerate(text.splitlines(), 1):
            for match in list(brace.finditer(line)) + list(call.finditer(line)):
                rgb = tuple(int(part) for part in match.groups())
                if max(rgb) > 255 or max(rgb) - min(rgb) < 12:
                    continue
                found.setdefault(rgb, f"{path.name}:{number}")
    return found


class MenuColorTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(prefix="swiss-ui-color-")
        work = Path(cls.tmp.name)
        (work / "driver.c").write_text(DRIVER)
        cls.binary = work / "driver"
        result = subprocess.run(shlex.split(os.environ.get("CC", "cc")) +
            ["-std=c99", "-Wall", "-Wextra", "-Werror", "-I" + str(GUI),
             str(work / "driver.c"), str(GUI / "ui_color.c"), "-o", str(cls.binary), "-lm"],
            capture_output=True, text=True, timeout=60)
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        members = re.findall(r"(UI_COLOR_\w+)", re.search(r"enum uiColor\s*\{(.*?)\};",
                                                          SWISS_H, re.S).group(1))
        cls.count = members.index("UI_COLOR_MAX")
        cls.literals = literal_colors()

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def apply(self, pairs):
        """[(color, rgb)] -> [rgb] through the real ui_color.c."""
        text = "".join(f"{color} {r} {g} {b}\n" for color, (r, g, b) in pairs)
        result = subprocess.run([str(self.binary)], input=text, capture_output=True,
                                text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        return [tuple(int(part) for part in line.split())
                for line in result.stdout.splitlines()]

    def test_the_three_tables_agree(self):
        names = re.findall(r'"([^"]+)"', re.search(r"char \*uiColorStr\[\] = \{(.*?)\};",
                                                    SETTINGS_C).group(1))
        rows = re.findall(r"^\t\{\s*-?[\d.]+f,\s*[\d.]+f\s*\},\t/\* ([\w ]+?)(?:,.*?)? \*/$",
                          UI_COLOR_C, re.M)
        members = re.findall(r"UI_COLOR_(\w+)", re.search(r"enum uiColor\s*\{(.*?)\};",
                                                          SWISS_H, re.S).group(1))[:-1]
        self.assertEqual(rows, names)
        self.assertEqual([m.replace("_", " ").lower() for m in members],
                         [name.lower() for name in names])
        self.assertEqual(names[0], "Indigo")

    def test_controller_buttons_keep_their_colors(self):
        vertex = extract_function(FRAME_C, "static void _HintVertex(")
        letter = extract_function(FRAME_C, "static void _HintLetter(")
        self.assertIn("GX_Color4u8(color.r, color.g, color.b, color.a);", vertex)
        self.assertNotIn("UIColor_Apply", vertex)
        self.assertIn("drawStringMediumUntinted(", letter)
        self.assertNotIn("drawStringMedium(", letter)
        untinted = extract_function(FONT_C, "void drawStringMediumUntinted(")
        self.assertNotIn("UIColor_Apply", untinted)
        # Every other string follows the Menu Color.
        for name in ("void drawString(", "void drawStringMedium(",
                     "void drawStringWithCaret(", "void drawStringEllipsis("):
            self.assertIn("UIColor_Apply(&fontColor.r", extract_function(FONT_C, name), name)

    def test_the_list_previews_each_color(self):
        settings = (GUI / "settings.c").read_text()
        pick = extract_function(settings, "static bool settingsPick(")
        update = pick.index("DrawUpdateSettingsList(box, &shown,")
        # Only Menu Color's list previews, the value it has focused, with the
        # list's redraw.
        self.assertRegex(pick[update:], r"^DrawUpdateSettingsList\(box, &shown,\s*"
                         r"pick->page == PAGE_INTERFACE && pick->option == SET_UI_COLOR \?"
                         r"\s*\(int\)list\.value\[focus\] : -1\);")
        self.assertLess(pick.index("settingsDrawPicker(row.label, &list, focus, &shown);"),
                        update)
        self.assertEqual(settings.count("DrawUpdateSettingsList("), 1)
        # The list's new focus and its color change in one step...
        listed = extract_function(FRAME_C, "void DrawUpdateSettingsList(")
        locked = listed[listed.index("LWP_MutexLock(_videomutex);"):
                        listed.index("LWP_MutexUnlock(_videomutex);")]
        self.assertIn("*(uiSetListSnapshot_t*)list->data = *snapshot;", locked)
        self.assertIn("if(previewColor >= 0) {\n\t\t\tmenuColorPreview = previewColor;", locked)
        # ... and so do a page's text and the color it pins, which ends any
        # preview. Disposing the page lets the color go.
        paged = extract_function(FRAME_C, "void DrawUpdateSettingsPage(")
        locked = paged[paged.index("LWP_MutexLock(_videomutex);"):
                       paged.index("LWP_MutexUnlock(_videomutex);")]
        for token in ("->snapshot = *snapshot;", "menuColorPage = page;",
                      "menuColorPinned = menuColor;", "menuColorPreview = -1;"):
            self.assertIn(token, locked)
        dispose = extract_function(FRAME_C, "void DrawDispose(")
        self.assertIn("menuColorPreview = -1;", dispose)
        self.assertIn("menuColorPinned = -1;", dispose)
        self.assertIn("int menuColor = menuColorPreview >= 0 ? menuColorPreview : menuColorPinned;",
                      FRAME_C)
        # settings.c pins the color each page was built with, once published.
        page = extract_function(settings, "uiDrawObj_t* settings_draw_page(")
        self.assertLess(page.index("DrawPublish(settingsPageEvent);"),
                        page.index("DrawUpdateSettingsPage(settingsPageEvent, &page, "
                                   "swissSettings.uiColor);"))

    def test_indigo_is_exactly_as_designed(self):
        colors = list(self.literals) + NEUTRALS + sorted(SEMANTIC)
        self.assertEqual(self.apply([(0, rgb) for rgb in colors]), colors)
        # Out of range reads as Indigo, never as a table overrun.
        self.assertEqual(self.apply([(self.count, rgb) for rgb in colors]), colors)
        self.assertEqual(self.apply([(-1, rgb) for rgb in colors]), colors)

    def test_neutrals_and_meanings_never_change(self):
        fixed = NEUTRALS + sorted(SEMANTIC)
        for color in range(self.count):
            self.assertEqual(self.apply([(color, rgb) for rgb in fixed]), fixed, color)

    def test_every_indigo_color_follows_the_menu_color(self):
        family = [rgb for rgb in self.literals if rgb not in SEMANTIC]
        self.assertGreater(len(family), 100)
        jet_black = self.count - 1
        for rgb, out in zip(family, self.apply([(jet_black, rgb) for rgb in family])):
            # Jet Black is the family with no saturation: gray at the same luma.
            y = luma(rgb)
            self.assertEqual(out, (y, y, y), f"{rgb} at {self.literals[rgb]}")
        kept = [rgb for rgb in family if rgb not in CLIPS]
        for color in range(1, self.count):
            for rgb, out in zip(kept, self.apply([(color, rgb) for rgb in kept])):
                self.assertNotEqual(out, rgb, f"color {color}: {rgb} at {self.literals[rgb]}")
                self.assertLessEqual(abs(luma(out) - luma(rgb)), 3,
                                     f"color {color}: {rgb} at {self.literals[rgb]} -> {out}")

    def family_edges(self):
        """The family's edges, measured: Jet Black grays what it recolors."""
        probes = []
        for step in range(-360, 361):
            turn = math.radians(step / 4)
            u, v = 50 * math.cos(turn), 50 * math.sin(turn)
            r, b = round(128 + v), round(128 + u)
            probes.append((r, round(128 - (77 * v + 29 * u) / 150), b))
        outs = self.apply([(self.count - 1, rgb) for rgb in probes])
        turned = [chroma_angle(rgb)[0] for rgb, out in zip(probes, outs) if out != rgb]
        return min(turned), max(turned)

    def test_no_color_sits_on_the_edge_of_the_family(self):
        # A color within a few degrees of an edge could change sides on a
        # rounding tweak; move it or list what it means.
        low, high = self.family_edges()
        self.assertLess(low, -45)
        self.assertGreater(high, 24)
        for rgb, where in self.literals.items():
            angle, chroma = chroma_angle(rgb)
            if rgb in SEMANTIC:
                self.assertTrue(angle < low - MARGIN or angle > high + MARGIN,
                                f"{rgb} at {where} means something but is Indigo's")
            elif chroma >= 3:
                self.assertTrue(low + MARGIN < angle < high - MARGIN,
                                f"{rgb} at {where} ({angle:.1f} degrees) is outside Indigo's "
                                "family: add it to SEMANTIC if it should keep its color")


if __name__ == "__main__":
    unittest.main()
