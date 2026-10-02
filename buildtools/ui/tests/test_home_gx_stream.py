#!/usr/bin/env python3
"""Run the real Home foreground renderer against a logged GX stream.

_DrawHome, its panels and text, and DrawUpdateHome are compiled out of
FrameBufferMagic.c with the real Home reducer, layout, text fitting, springs
and screen shapes. The scene, the font and GX are stand-ins: the scene frame
is set by the script, and every string and flat quad drawn is logged with its
opacity. The tests then check that the face's name and the hint fade in as
the cube grows back to its Home size, rather than appearing at full strength
over a cube still on its way back.
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
#include "ui_home.h"
#include "ui_home_layout.h"
#include "ui_home_text.h"
#include "ui_motion.h"
#include "ui_scene.h"
#include "ui_stage.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef struct { u8 r, g, b, a; } GXColor;
typedef struct uiDrawObj { int type; void *data; } uiDrawObj_t;
#define ALIGN_LEFT 0
#define ALIGN_CENTER 1
#define ALIGN_RIGHT 2
#define EV_HOME 1
enum { GX_QUADS = 0x80, GX_VTXFMT0 = 0 };
#define CHECK(c) do { if(!(c)) { fprintf(stderr, "failed %d: %s\n", __LINE__, #c); exit(73); } } while(0)
/* The panels' TEV and blend set-up is not what is checked here. */
#define GX_SetNumTexGens(...) ((void)0)
#define GX_SetNumIndStages(...) ((void)0)
#define GX_SetNumTevStages(...) ((void)0)
#define GX_SetTevOrder(...) ((void)0)
#define GX_SetTevColorIn(...) ((void)0)
#define GX_SetTevColorOp(...) ((void)0)
#define GX_SetTevAlphaIn(...) ((void)0)
#define GX_SetTevAlphaOp(...) ((void)0)
#define GX_SetTevDirect(...) ((void)0)
#define GX_SetBlendMode(...) ((void)0)
#define GX_SetZMode(...) ((void)0)
static bool active;
static int declared, emitted, phase;
static void drawInit(void) { CHECK(!active); }
static void UIColor_Apply(u8 *r, u8 *g, u8 *b) { (void)r; (void)g; (void)b; }
static void GX_Begin(int primitive, int format, int count)
{
	CHECK(!active && primitive == GX_QUADS && format == GX_VTXFMT0 &&
		count > 0 && count % 4 == 0);
	active = true; declared = count; emitted = 0; phase = 0;
	printf("B %d\n", count);
}
static void GX_Position3f32(float x, float y, float z)
{
	CHECK(active && phase == 0 && z == 0.0f && isfinite(x) && isfinite(y));
	phase = 1; printf("V %.3f %.3f", x, y);
}
static void GX_Color4u8(u8 r, u8 g, u8 b, u8 a)
{
	(void)r; (void)g; (void)b;
	CHECK(active && phase == 1); phase = 2; printf(" %u\n", a);
}
static void GX_TexCoord2f32(float s, float t)
{
	CHECK(active && phase == 2 && emitted < declared && s == 0.0f && t == 0.0f);
	phase = 0; emitted++;
}
static void GX_End(void) { CHECK(active && phase == 0 && emitted == declared); active = false; }
static void drawStringMedium(int x, int y, const char *text, float scale,
	int align, GXColor color)
{
	CHECK(!active && text != NULL && scale > 0.0f);
	printf("S %d %d %d %.4f %u %u %u %u %s\n", x, y, align, scale,
		color.r, color.g, color.b, color.a, text);
}
static void _DrawHintText(int x, int y, const char *text, float scale,
	int align, GXColor color)
{
	CHECK(!active && text != NULL && scale > 0.0f);
	printf("H %d %d %d %.4f %u %s\n", x, y, align, scale, color.a, text);
}
static int GetTextSizeInPixels(const char *text) { return 14 * (int)strlen(text); }
static int GetHintSizeInPixels(const char *text) { return 9 * (int)strlen(text); }
static uiSceneFrame_t sceneFrame;
const uiSceneFrame_t *UIScene_Frame(void) { return &sceneFrame; }
void UIScene_RequestHome(const uiHomeState_t *home)
{
	sceneFrame.homeFace = home->face;
	sceneFrame.homeSurface = home->surface;
	sceneFrame.homeSelection = home->selection;
}
static float animDelta;
static float UIAnim_Delta(void) { return animDelta; }
static uiMotionMode_t motionMode;
static uiMotionMode_t _CurrentMotionMode(void) { return motionMode; }
static int _videomutex;
static void LWP_MutexLock(int mutex) { (void)mutex; }
static void LWP_MutexUnlock(int mutex) { (void)mutex; }
static uiDrawObj_t *buttonPanel;
"""

DRIVER = r"""
int main(void)
{
	char line[128];
	uiHomeState_t home;
	uiHomeCapabilities_t caps = {true, true, false};

	UIHome_Init(&home, caps);
	sceneFrame.scene = UI_SCENE_HOME;
	sceneFrame.chromeProgress = 1.0f;
	sceneFrame.cubeScale = 0.92f;
	sceneFrame.homeFocusProgress = 1.0f;
	sceneFrame.visible = true;
	buttonPanel = DrawHome();
	DrawUpdateHome(&home, caps, "SD Card");
	while(fgets(line, sizeof(line), stdin)) {
		int a, b;
		float value;
		if(sscanf(line, "I %d", &a) == 1) {
			printf("E %d\n", (int)UIHome_Apply(&home, (uiHomeInput_t)a, caps));
			printf("A %d %d %d\n", (int)home.face, (int)home.surface, home.selection);
			DrawUpdateHome(&home, caps, "SD Card");
		}
		else if(line[0] == 'X') DrawUpdateHome(NULL, caps, NULL);
		else if(line[0] == 'U') DrawUpdateHome(&home, caps, "SD Card");
		else if(sscanf(line, "M %d", &a) == 1) motionMode = (uiMotionMode_t)a;
		else if(sscanf(line, "K %f", &value) == 1) sceneFrame.cubeScale = value;
		else if(sscanf(line, "G %f", &value) == 1) sceneFrame.chromeProgress = value;
		else if(sscanf(line, "N %d %f", &a, &value) == 2) {
			for(b = 0; b < a; ++b) {
				animDelta = value;
				printf("F\n");
				_DrawHome(buttonPanel);
				CHECK(!active);
			}
		}
		else { fprintf(stderr, "bad command: %s", line); return 64; }
	}
	return 0;
}
"""


def renderer(frame_c: str) -> str:
    return "\n".join([
        between(frame_c, "typedef struct drawHomeEvent {", "} drawHomeEvent_t;", True),
        between(frame_c, "static const char homeContextCommand[] =",
                "/* One device the Source picker lists"),
        extract_function(frame_c, "static void _putFlatVertex("),
        extract_function(frame_c, "static void _putFlatRect("),
        between(frame_c, "static void _DrawHomeText(", "// External\nuiDrawObj_t* DrawHome(void)"),
        extract_function(frame_c, "uiDrawObj_t* DrawHome(void)"),
        extract_function(frame_c, "static void _PrepareHomeText("),
        extract_function(frame_c, "void DrawUpdateHome("),
    ])


def parse(log: str) -> list[dict]:
    frames = []
    for line in log.splitlines():
        tag, _, rest = line.partition(" ")
        if tag == "F":
            frames.append({"strings": [], "hints": [], "quads": []})
        elif not frames:
            continue
        elif tag == "S":
            x, y, align, scale, r, g, b, alpha, text = rest.split(" ", 8)
            frames[-1]["strings"].append({"x": int(x), "y": int(y), "scale": float(scale),
                                          "color": (int(r), int(g), int(b)),
                                          "alpha": int(alpha), "text": text})
        elif tag == "H":
            x, y, align, scale, alpha, text = rest.split(" ", 5)
            frames[-1]["hints"].append({"alpha": int(alpha), "text": text})
        elif tag == "V":
            x, y, alpha = rest.split()
            frames[-1]["quads"].append((float(x), float(y), int(alpha)))
    return frames


def rects(frame: dict) -> list[dict]:
    """Every four vertices are one flat rectangle."""
    found = []
    quads = frame["quads"]
    for i in range(0, len(quads), 4):
        xs = [q[0] for q in quads[i:i + 4]]
        ys = [q[1] for q in quads[i:i + 4]]
        found.append({"x": min(xs), "y": min(ys), "w": max(xs) - min(xs),
                      "h": max(ys) - min(ys), "alpha": quads[i][2]})
    return found


class HomeGXStreamTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.frame_c = (GUI / "FrameBufferMagic.c").read_text()
        cls.directory = tempfile.TemporaryDirectory(prefix="swiss-home-gx-")
        work = Path(cls.directory.name)
        harness = work / "home.c"
        cls.program = work / "home"
        harness.write_text("\n".join([PRELUDE, renderer(cls.frame_c), DRIVER]))
        command = shlex.split(os.environ.get("CC", "cc")) + [
            "-std=c99", "-Wall", "-Wextra", "-pedantic", f"-I{GUI}", str(harness),
            *(str(GUI / name) for name in ("ui_home.c", "ui_home_layout.c",
                                           "ui_home_text.c", "ui_motion.c", "ui_stage.c")),
            "-o", str(cls.program), "-lm",
        ]
        compiled = subprocess.run(command, capture_output=True, text=True, timeout=60)
        assert compiled.returncode == 0, compiled.stdout + compiled.stderr

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def run_script(self, script: str) -> list[dict]:
        result = subprocess.run([str(self.program)], input=script,
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stderr)
        return parse(result.stdout)

    def test_the_name_and_hint_fade_in_as_the_cube_returns(self):
        rest = self.run_script("N 2 0.0166667\n")[-1]
        self.assertEqual([s["alpha"] for s in rest["strings"]], [255])
        self.assertEqual([h["alpha"] for h in rest["hints"]], [184])
        # Coming back from the Library, Settings, Source or System, the cube
        # is at 0.70 of its size or less: the chrome is not there yet.
        for scale in (0.44, 0.56, 0.70):
            frame = self.run_script(f"K {scale}\nN 1 0.0166667\n")[-1]
            self.assertEqual(frame["strings"] + frame["hints"], [], scale)
        names, hints = [], []
        script = "".join(f"K {0.70 + step * 0.01:.2f}\nN 1 0.0166667\n" for step in range(23))
        for frame in self.run_script(script)[1:]:
            names.append(frame["strings"][0]["alpha"])
            hints.append(frame["hints"][0]["alpha"])
        for alphas, full in ((names, 255), (hints, 184)):
            self.assertEqual(alphas, sorted(alphas), "the chrome flickered as it came in")
            self.assertLess(alphas[0], full * 0.1)
            self.assertTrue(all(b - a < full * 0.2 for a, b in zip(alphas, alphas[1:])),
                            "the chrome stepped in")
            # Full past 0.88.
            self.assertEqual(alphas[18:], [full] * 4)
        # And at the rows' larger 0.95.
        frame = self.run_script("K 0.95\nN 1 0.0166667\n")[-1]
        self.assertEqual([s["alpha"] for s in frame["strings"] + frame["hints"]], [255, 184])


if __name__ == "__main__":
    unittest.main(verbosity=2)
