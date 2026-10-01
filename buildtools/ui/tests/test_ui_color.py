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

    def test_the_color_tables_agree(self):
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
        # Backdrop and Wave Color: following Menu Color first, then the colors.
        layers = re.findall(r'"([^"]+)"', re.search(r"char \*uiLayerColorStr\[\] = \{(.*?)\};",
                                                     SETTINGS_C).group(1))
        self.assertEqual(layers, ["Menu Color"] + names)

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

    def test_the_lists_preview_each_color(self):
        settings = (GUI / "settings.c").read_text()
        pick = extract_function(settings, "static bool settingsPick(")
        update = pick.index("DrawUpdateSettingsList(box, &shown,")
        # Menu, Backdrop and Wave Color's lists preview the value they have
        # focused, in their own layer, with the list's redraw; any other list
        # has no layer (-1) and previews nothing.
        self.assertRegex(pick[update:], r"^DrawUpdateSettingsList\(box, &shown,\s*"
                         r"settingsColorLayer\(pick->page, pick->option\), "
                         r"\(int\)list\.value\[focus\]\);")
        self.assertLess(pick.index("settingsDrawPicker(row.label, &list, focus, &shown);"),
                        update)
        self.assertEqual(settings.count("DrawUpdateSettingsList("), 1)
        layer = extract_function(settings, "static int settingsColorLayer(")
        self.assertIn("if(page == PAGE_INTERFACE) {", layer)
        for option, name in (("SET_UI_COLOR", "MENU"), ("SET_UI_BACKDROP_COLOR", "BACKDROP"),
                             ("SET_UI_WAVE_COLOR", "WAVES")):
            self.assertIn(f"case {option}: return UI_COLOR_LAYER_{name};", layer)
        # Each color row's swatch shows its own layer.
        self.assertIn("settingsColorLayer(ref->page, ref->option) + 1);", settings)
        swatch = extract_function(FRAME_C, "static void _SettingsSwatch(")
        order = [swatch.index(token) for token in (
            "UIColor_Select(frameColors[layer]);", "UIColor_Apply(&color.r",
            "UIColor_Select(frameColors[UI_COLOR_LAYER_MENU]);")]
        self.assertEqual(order, sorted(order))
        self.assertIn("(float)y, row->swatch - 1);", FRAME_C)
        # The list's new focus and its color change in one step...
        listed = extract_function(FRAME_C, "void DrawUpdateSettingsList(")
        locked = listed[listed.index("LWP_MutexLock(_videomutex);"):
                        listed.index("LWP_MutexUnlock(_videomutex);")]
        self.assertIn("*(uiSetListSnapshot_t*)list->data = *snapshot;", locked)
        self.assertIn("if(previewLayer >= 0 && previewLayer < UI_COLOR_LAYERS) {\n"
                      "\t\t\tmenuColorPreview[previewLayer] = previewColor;", locked)
        # ... and so do a page's text and the colors it pins, which ends any
        # preview. Disposing the page lets the colors go.
        paged = extract_function(FRAME_C, "void DrawUpdateSettingsPage(")
        locked = paged[paged.index("LWP_MutexLock(_videomutex);"):
                       paged.index("LWP_MutexUnlock(_videomutex);")]
        for token in ("->snapshot = *snapshot;", "menuColorPage = page;",
                      "menuColorPinned[i] = colors[i];", "menuColorPreview[i] = -1;"):
            self.assertIn(token, locked)
        dispose = extract_function(FRAME_C, "void DrawDispose(")
        self.assertIn("menuColorPreview[i] = -1;", dispose)
        self.assertIn("menuColorPinned[i] = -1;", dispose)
        # The frame takes its colors with the page they go with, before it draws.
        loop = extract_function(FRAME_C, "static void *videoUpdate(")
        order = [loop.index(token) for token in (
            "LWP_MutexLock(_videomutex);", "_SelectFrameColors();", "UIScene_Update(")]
        self.assertEqual(order, sorted(order))
        # settings.c pins the colors each page was built with, once published.
        page = extract_function(settings, "uiDrawObj_t* settings_draw_page(")
        self.assertLess(page.index("DrawPublish(settingsPageEvent);"),
                        page.index("DrawUpdateSettingsPage(settingsPageEvent, &page, "
                                   "(const int[UI_COLOR_LAYERS]) {\n\t\tswissSettings.uiColor, "
                                   "swissSettings.uiBackdropColor, swissSettings.uiWaveColor});"))

    def test_the_backdrop_and_the_waves_take_their_own_colors(self):
        """The real _SelectFrameColors and UIColor_Layer: a list's focus beats
        its page's color, which beats the setting, and a backdrop or wave
        setting of 0 follows whatever the menus show."""
        harness = "\n".join([
            "#include <stdio.h>",
            '#include "ui_color.h"',
            "static struct { int uiColor, uiBackdropColor, uiWaveColor; } swissSettings;",
            "static int menuColorPinned[UI_COLOR_LAYERS], menuColorPreview[UI_COLOR_LAYERS];",
            "static int frameColors[UI_COLOR_LAYERS], selected, drawn[UI_COLOR_LAYERS];",
            "void UIColor_Select(int color) { selected = color; }",
            "void IndigoBackground_SetColors(const int colors[UI_COLOR_LAYERS])",
            "{ for(int i = 0; i < UI_COLOR_LAYERS; i++) drawn[i] = colors[i]; }",
            extract_function(UI_COLOR_C, "int UIColor_Layer("),
            extract_function(FRAME_C, "static void _SelectFrameColors("),
            "int main(void) {",
            "\twhile(scanf(\"%d %d %d %d %d %d %d %d %d\", &swissSettings.uiColor,",
            "\t\t&swissSettings.uiBackdropColor, &swissSettings.uiWaveColor,",
            "\t\t&menuColorPinned[0], &menuColorPinned[1], &menuColorPinned[2],",
            "\t\t&menuColorPreview[0], &menuColorPreview[1], &menuColorPreview[2]) == 9) {",
            "\t\t_SelectFrameColors();",
            "\t\tprintf(\"%d %d %d %d %d %d %d\\n\", selected, frameColors[0], frameColors[1],",
            "\t\t\tframeColors[2], drawn[0], drawn[1], drawn[2]);",
            "\t}",
            "\treturn 0;",
            "}", ""])
        work = Path(self.tmp.name)
        (work / "frame.c").write_text(harness)
        result = subprocess.run(shlex.split(os.environ.get("CC", "cc")) +
            ["-std=c99", "-Wall", "-Wextra", "-Werror", "-I" + str(GUI),
             str(work / "frame.c"), "-o", str(work / "frame")],
            capture_output=True, text=True, timeout=60)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

        def expect(settings, pinned, preview):
            chosen = [p if p >= 0 else (q if q >= 0 else s)
                      for s, q, p in zip(settings, pinned, preview)]
            menu = chosen[0]
            colors = [menu] + [c - 1 if c > 0 else menu for c in chosen[1:]]
            return (colors[0], *colors, *colors)

        none = (-1, -1, -1)
        cases = []
        for settings in [(m, b, w) for m in (0, 1, 7) for b in (0, 1, 4, 8) for w in (0, 3, 8)]:
            for pinned in (none, settings, (2, 0, 5)):
                for preview in (none, (5, -1, -1), (-1, 7, -1), (-1, -1, 1)):
                    cases.append((settings, pinned, preview))
        text = "".join(" ".join(map(str, s + q + p)) + "\n" for s, q, p in cases)
        run = subprocess.run([str(work / "frame")], input=text, capture_output=True,
                             text=True, timeout=30)
        self.assertEqual(run.returncode, 0, run.stderr)
        got = [tuple(int(v) for v in line.split()) for line in run.stdout.splitlines()]
        self.assertEqual(got, [expect(*case) for case in cases])
        # Read plainly: by default the backdrop and waves are the menus' color;
        # Backdrop Color Gold (1 + 3) keeps the backdrop Gold under Azure menus;
        # previewing Menu Color takes a following backdrop along, and
        # previewing Backdrop Color leaves the menus alone.
        self.assertEqual(expect((1, 0, 0), none, none), (1, 1, 1, 1, 1, 1, 1))
        self.assertEqual(expect((1, 4, 0), none, none)[:4], (1, 1, 3, 1))
        self.assertEqual(expect((1, 0, 4), (1, 0, 4), (6, -1, -1))[:4], (6, 6, 6, 3))
        self.assertEqual(expect((1, 0, 0), (1, 0, 0), (-1, 8, -1))[:4], (1, 1, 7, 1))

    def test_each_layer_is_drawn_in_its_own_color(self):
        background = (GUI / "indigo_background.c").read_text()
        draw = extract_function(background, "void IndigoBackground_Draw(")
        order = [draw.index(token) for token in (
            "UIColor_Select(layerColors[UI_COLOR_LAYER_BACKDROP]);",
            "drawIndigoWash(255, UIColor_BackdropShade(layerColors[UI_COLOR_LAYER_BACKDROP]));",
            "drawGlobeGrid(", "UIColor_Select(layerColors[UI_COLOR_LAYER_WAVES]);",
            "drawSilkWaves(", "UIColor_Select(layerColors[UI_COLOR_LAYER_MENU]);",
            "if(!scene->visible) {", "drawRadialDisc(", "drawCubeLight(", "drawCube(scene,")]
        self.assertEqual(order, sorted(order))
        self.assertEqual(draw.count("UIColor_Select("), 3)
        # The boot veil is the backdrop too; the cube under it is the menus'.
        boot = extract_function(background, "void IndigoBackground_DrawBootOverlay(")
        order = [boot.index(token) for token in (
            "drawCube(scene,", "UIColor_Select(layerColors[UI_COLOR_LAYER_BACKDROP]);",
            "drawIndigoWash(veilAlpha, UIColor_BackdropShade(layerColors[UI_COLOR_LAYER_BACKDROP]));",
            "UIColor_Select(layerColors[UI_COLOR_LAYER_MENU]);")]
        self.assertEqual(order, sorted(order))
        self.assertEqual(background.count("drawIndigoWash("), 3)   # its definition and these two
        # The wash's four corners all go through the shade.
        wash = extract_function(background, "static void drawIndigoWash(")
        self.assertEqual(wash.count("WASH("), 5)   # the macro and its four corners

    def test_a_jet_black_backdrop_is_darker(self):
        """Jet Black, the color with no saturation, shades the backdrop to about a
        third; every other color and anything out of range draws it as designed."""
        work = Path(self.tmp.name)
        (work / "shade.c").write_text("\n".join([
            "#include <stdio.h>", '#include "ui_color.h"',
            "int main(void) { for(int c = -1; c <= 9; c++) printf(\"%d %.3f\\n\", c, "
            "UIColor_BackdropShade(c)); return 0; }", ""]))
        result = subprocess.run(shlex.split(os.environ.get("CC", "cc")) +
            ["-std=c99", "-Wall", "-Wextra", "-Werror", "-I" + str(GUI), str(work / "shade.c"),
             str(GUI / "ui_color.c"), "-o", str(work / "shade"), "-lm"],
            capture_output=True, text=True, timeout=60)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        shades = dict(line.split() for line in subprocess.run(
            [str(work / "shade")], capture_output=True, text=True, timeout=30).stdout.splitlines())
        jet_black = str(self.count - 1)
        self.assertLess(float(shades[jet_black]), 0.5)
        self.assertGreater(float(shades[jet_black]), 0.0)
        for color, shade in shades.items():
            if color != jet_black:
                self.assertEqual(float(shade), 1.0, color)

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
