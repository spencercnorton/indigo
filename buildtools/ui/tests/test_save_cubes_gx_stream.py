#!/usr/bin/env python3
"""Run Memory Cards' cube screen against a checked GX stream.

_DrawSaveCubes and its helpers are compiled out of FrameBufferMagic.c with
the real cubes (ui_save_cubes.c), springs (ui_motion.c), icon clock
(ui_saves.c) and screen shapes (ui_stage.c). GX, the font, the backdrop and
the pictures are stand-ins that check every primitive is declared and sent
in full under the pipeline it needs, and log what is drawn. The tests then
check:

- every resting cube's faces go in one batch, then their icons, then the
  focused cube's faces and its icon, last;
- the texture cache is cleared once a frame when an icon is drawn, never
  when none is, and every icon is a 32x32 RGB5A3 frame of a save's slot;
- icons keep their own colors under any Menu Color while the cubes turn;
- a row outside the window, a clear cube and a stack with no grid draw
  nothing; every face is wound clockwise;
- Motion Off is still; the busiest frame stays inside its budget.
"""

import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest


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
#include "ui_save_cubes.h"
#include "ui_stage.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;
typedef struct { u8 r, g, b, a; } GXColor;
typedef struct { u32 val[8]; } GXTexObj;
typedef struct uiDrawObj { int type; void *data; struct uiDrawObj *child; bool disposed; } uiDrawObj_t;
#define ALIGN_LEFT 0
#define ALIGN_CENTER 1
#define ALIGN_RIGHT 2
enum { GX_QUADS = 0x80, GX_TRIANGLES = 0x90, GX_VTXFMT0 = 0, GX_TF_RGB5A3 = 5,
	GX_CLAMP = 0, GX_FALSE = 0, GX_TEXMAP0 = 0, GX_BM_NONE = 0, GX_BM_BLEND = 1,
	GX_BL_SRCALPHA = 4, GX_BL_INVSRCALPHA = 5, GX_LO_CLEAR = 0, GX_NEAR = 0,
	GX_LINEAR = 1 };
static const GXColor settingsInk = {241, 244, 255, 255};
static const GXColor settingsQuiet = {173, 187, 216, 255};
static const GXColor settingsSwatch = {122, 104, 224, 255};

#define CHECK(c) do { if(!(c)) { fprintf(stderr, "failed %d: %s\n", __LINE__, #c); exit(73); } } while(0)
/* The icons' texels: a save's eight frames, as the pool holds them. */
static u8 texels[8 * 2048] __attribute__((aligned(32)));
static u8 banner[6144] __attribute__((aligned(32)));
/* drawInit's textured pipeline, the raster one over it, or the icons'
 * (one TEV stage, blended by alpha); DIRTY after text or a picture. */
enum { DIRTY, TEXTURED, RASTER, ICONS };
static int pipeline = DIRTY, stages = 2, primitive;
static bool active, loaded, shift;
static int declared, emitted, phase;
static void drawInit(void) { CHECK(!active); pipeline = TEXTURED; stages = 2; loaded = false; }
static void _SetupRasterColor(void) { CHECK(!active && pipeline == TEXTURED); pipeline = RASTER; }
/* Menu Color: a non-Indigo one turns every color it is given. */
static void UIColor_Apply(u8 *r, u8 *g, u8 *b)
{
	if(shift) { u8 red = *r; *r = *b; *b = *g; *g = (u8)(red / 2); }
}
static void GX_SetNumTevStages(int n) { CHECK(!active); stages = n; }
static void GX_SetBlendMode(int mode, int source, int destination, int op)
{
	(void)op;
	CHECK(!active && mode != GX_BM_NONE);
	if(pipeline == TEXTURED && stages == 1 && source == GX_BL_SRCALPHA &&
		destination == GX_BL_INVSRCALPHA) pipeline = ICONS;
}
static void GX_InvalidateTexAll(void) { CHECK(!active); printf("I\n"); }
static void GX_InitTexObj(GXTexObj *object, void *pointer, int width, int height,
	int format, int s, int t, int mip)
{
	(void)object;
	CHECK(!active && ((uintptr_t)pointer & 31u) == 0 && mip == GX_FALSE);
	printf("T %ld %d %d %d %d %d\n", (long)((u8 *)pointer - texels), width, height,
		format, s, t);
}
static void GX_InitTexObjFilterMode(GXTexObj *object, int minimum, int magnify)
{
	(void)object; (void)minimum; (void)magnify;
}
static void GX_LoadTexObj(GXTexObj *object, int map)
{
	(void)object;
	CHECK(!active && pipeline == ICONS && map == GX_TEXMAP0);
	loaded = true;
	printf("L\n");
}
static float lastX, lastY;
static u8 lastColor[4];
static void GX_Begin(int type, int format, int count)
{
	CHECK(!active && format == GX_VTXFMT0 && count > 0);
	CHECK(count % (type == GX_QUADS ? 4 : 3) == 0);
	CHECK(pipeline == RASTER || (pipeline == ICONS && loaded && type == GX_QUADS && count == 4));
	active = true; declared = count; emitted = 0; phase = 0; primitive = type;
	printf("B %d %d %d\n", type, count, pipeline == ICONS);
}
static void GX_Position3f32(float x, float y, float z)
{
	CHECK(active && phase == 0 && z == 0.0f && isfinite(x) && isfinite(y));
	lastX = x; lastY = y; phase = 1;
}
static void GX_Color4u8(u8 r, u8 g, u8 b, u8 a)
{
	CHECK(active && phase == 1);
	lastColor[0] = r; lastColor[1] = g; lastColor[2] = b; lastColor[3] = a;
	phase = 2;
}
static void GX_TexCoord2f32(float s, float t)
{
	CHECK(active && phase == 2 && emitted < declared);
	CHECK(pipeline == ICONS || (s == 0.0f && t == 0.0f));
	printf("V %.3f %.3f %u %u %u %u %.1f %.1f\n", lastX, lastY, lastColor[0], lastColor[1],
		lastColor[2], lastColor[3], s, t);
	phase = 0; emitted++;
}
static void GX_End(void) { CHECK(active && phase == 0 && emitted == declared); active = false; }
static void _putFlatVertex(float x, float y, GXColor color)
{
	UIColor_Apply(&color.r, &color.g, &color.b);
	GX_Position3f32(x, y, 0.0f);
	GX_Color4u8(color.r, color.g, color.b, color.a);
	GX_TexCoord2f32(0.0f, 0.0f);
}
static void _putFlatRect(float x, float y, float width, float height, GXColor color)
{
	_putFlatVertex(x, y, color);
	_putFlatVertex(x + width, y, color);
	_putFlatVertex(x + width, y + height, color);
	_putFlatVertex(x, y + height, color);
}
static void drawStringMedium(int x, int y, const char *text, float scale, int align, GXColor color)
{
	(void)align; (void)color;
	CHECK(!active && text != NULL && scale > 0.0f);
	printf("S %d %d %s\n", x, y, text);
	pipeline = DIRTY;
}
static void _DrawHintText(int x, int y, const char *text, float scale, int align, GXColor color)
{
	(void)align; (void)color;
	CHECK(!active && text != NULL && scale > 0.0f);
	printf("H %d %d %s\n", x, y, text);
	pipeline = DIRTY;
}
static int GetTextSizeInPixels(const char *text) { return 10 * (int)strlen(text); }
static void _DrawTexObjNow(GXTexObj *texture, int x, int y, int width, int height, int depth,
	float s1, float s2, float t1, float t2, int centered)
{
	(void)texture; (void)s1; (void)s2; (void)t1; (void)t2;
	CHECK(!active && pipeline == TEXTURED && depth == 0 && centered == 0);
	printf("P %d %d %d %d\n", x, y, width, height);
	pipeline = DIRTY;
}
static void _SavesFolder(int x, int y, GXColor color)
{
	(void)color;
	printf("G %d %d\n", x, y);
	pipeline = DIRTY;
}
static void IndigoBackground_DrawSavesBackdrop(void) { CHECK(!active); printf("K\n"); pipeline = DIRTY; }
static float animDelta;
static float UIAnim_Delta(void) { return animDelta; }
static uiMotionMode_t motionMode;
static uiMotionMode_t _CurrentMotionMode(void) { return motionMode; }
"""

DRIVER = r"""
static drawSaveCubesEvent_t page;
static uiDrawObj_t event = {0, &page, NULL, false};
static uiSavesArt_t art;

/* Stack s: cells of saves (each with an icon, but cell 6 not read yet), a
 * folder at cell 2 of the right stack, and free cells after saves. */
static void stack(int s, int cells, int first, int saves, unsigned listing)
{
	uiSaveCubesStack_t *stack = &page.snapshot.grid.stack[s];

	stack->cells = (s16)cells;
	stack->first = (s16)first;
	stack->listing = listing;
	for(int k = 0; k < UI_SAVE_CUBES_DRAWN; k++) {
		int cell = (first - 1) * 4 + k;
		uiSaveCubesCell_t *c = &stack->cell[k];

		memset(c, 0, sizeof(*c));
		c->kind = cell < saves ? (s == 1 && cell == 2 ? UI_SAVE_CUBES_KIND_FOLDER :
			UI_SAVE_CUBES_KIND_SAVE) : UI_SAVE_CUBES_KIND_EMPTY;
		if(c->kind == UI_SAVE_CUBES_KIND_SAVE && cell != 6) {
			c->texels = texels;
			c->art = &art;
		}
	}
	snprintf(page.snapshot.stack[s].name, sizeof(page.snapshot.stack[s].name), "%c", 'A' + s);
	snprintf(page.snapshot.stack[s].free, sizeof(page.snapshot.stack[s].free), "%d", 59);
	snprintf(page.snapshot.stack[s].note[0], 96, "Nothing is inserted in Slot %c.", 'A' + s);
	snprintf(page.snapshot.stack[s].note[1], 96, "Put one in, then press L or R.");
	page.snapshot.stack[s].noteScale[0] = 0.6f;
	page.snapshot.stack[s].noteScale[1] = 0.48f;
}

int main(void)
{
	char line[160];

	art.steps = 2;
	art.stepFrame[0] = 0;
	art.stepFrame[1] = 1;
	art.stepHold[0] = art.stepHold[1] = 1;
	art.period = 2;
	snprintf(page.snapshot.hint[0], 96, "STICK / D-PAD  Select   B  Finish   A  Confirm");
	snprintf(page.snapshot.hint[1], 96, "L/R  Card");
	while(fgets(line, sizeof(line), stdin)) {
		int a, b, c, d, e, f, g, h;
		float dt;
		if(sscanf(line, "S %d %d %d %d %d", &a, &b, &c, &d, &e) == 5) stack(a, b, c, d, (unsigned)e);
		else if(sscanf(line, "C %d %d", &a, &b) == 2) {
			page.snapshot.grid.focusStack = (s8)a;
			page.snapshot.grid.focusCell = (s16)b;
		}
		else if(sscanf(line, "I %d %d %d", &a, &b, &c) == 3) {
			page.snapshot.info = (u8)a;
			page.snapshot.banner = b ? banner : NULL;
			page.snapshot.folder = (u8)c;
			snprintf(page.snapshot.line[0], 48, "Sky Harbor Racing");
			snprintf(page.snapshot.line[1], 48, "Cup 3 cleared");
			snprintf(page.snapshot.blocks, 16, "11");
		}
		else if(sscanf(line, "W %d", &a) == 1) UIStage_SetWide(a != 0);
		else if(sscanf(line, "M %d", &a) == 1) motionMode = (uiMotionMode_t)a;
		else if(sscanf(line, "U %d", &a) == 1) shift = a != 0;
		else if(line[0] == 'R') memset(&page.motion, 0, sizeof(page.motion));
		else if(sscanf(line, "N %d %f", &a, &dt) == 2) {
			for(b = 0; b < a; ++b) {
				animDelta = dt;
				printf("F\n");
				drawInit();	/* as videoDrawEvent does before each event */
				_DrawSaveCubes(&event);
				CHECK(!active && pipeline == TEXTURED);
			}
		}
		else { fprintf(stderr, "bad command: %s", line); (void)f; (void)g; (void)h; return 64; }
	}
	return 0;
}
"""


def screen(frame_c: str, frame_h: str) -> str:
    return "\n".join([
        between(frame_h, "typedef struct {\n\tchar name[4];", "} uiSaveCubesPageSnapshot_t;", True),
        between(frame_c, "/* What one frame's cubes need. */", "uiDrawObj_t* DrawSaveCubesPage("),
    ])


def parse(log: str) -> list[dict]:
    frames = []
    for line in log.splitlines():
        tag, _, rest = line.partition(" ")
        if tag == "F":
            frames.append({"events": [], "batches": [], "textures": [], "invalidations": 0})
            continue
        frame = frames[-1]
        if tag == "B":
            kind, count, icon = map(int, rest.split())
            frame["batches"].append({"kind": kind, "count": count, "icon": bool(icon),
                                     "vertices": [], "at": len(frame["events"])})
            frame["events"].append(("B", len(frame["batches"]) - 1))
        elif tag == "V":
            x, y, r, g, b, a, s, t = rest.split()
            frame["batches"][-1]["vertices"].append(
                (float(x), float(y), int(r), int(g), int(b), int(a), float(s), float(t)))
        elif tag == "T":
            offset, width, height, fmt, s, t = map(int, rest.split())
            frame["textures"].append({"offset": offset, "size": (width, height), "format": fmt,
                                      "wrap": (s, t), "loaded": False})
            frame["events"].append(("T", None))
        elif tag == "L":
            frame["textures"][-1]["loaded"] = True
            frame["events"].append(("L", None))
        elif tag == "I":
            frame["invalidations"] += 1
            frame["events"].append(("I", None))
        else:
            frame["events"].append((tag, rest))
    return frames


def quads(batch: dict) -> list[list[tuple]]:
    per = 4 if batch["kind"] == 0x80 else 3
    return [batch["vertices"][i:i + per] for i in range(0, len(batch["vertices"]), per)]


def area(quad: list[tuple]) -> float:
    return 0.5 * sum(quad[i][0] * quad[(i + 1) % 4][1] - quad[(i + 1) % 4][0] * quad[i][1]
                     for i in range(4))


def cube_batches(frame: dict) -> list[dict]:
    """Batches of cube faces: raster quads that aren't a box's 20 vertices."""
    return [b for b in frame["batches"] if not b["icon"] and b["kind"] == 0x80 and b["count"] != 20]


def icons(frame: dict) -> list[dict]:
    return [b for b in frame["batches"] if b["icon"]]


def width(batch: dict) -> float:
    xs = [v[0] for v in batch["vertices"]]
    return max(xs) - min(xs)


AT_REST = "S 0 40 1 30 1\nS 1 20 0 18 2\nC 0 5\nI 1 1 0\nN 120 0.0166667\n"


class SaveCubesGXStreamTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.frame_c = (GUI / "FrameBufferMagic.c").read_text()
        cls.frame_h = (GUI / "FrameBufferMagic.h").read_text()
        cls.cubes_c = (GUI / "ui_save_cubes.c").read_text()
        cls.source = screen(cls.frame_c, cls.frame_h)
        cls.directory = tempfile.TemporaryDirectory(prefix="swiss-save-cubes-gx-")
        cls.program = cls.build(cls.source, cls.cubes_c, "cubes")

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    @classmethod
    def build(cls, source: str, cubes: str, name: str) -> Path:
        work = Path(cls.directory.name)
        harness = work / f"{name}.c"
        pure = work / f"{name}_cubes.c"
        program = work / name
        harness.write_text("\n".join([PRELUDE, source, DRIVER]))
        pure.write_text(cubes)
        command = shlex.split(os.environ.get("CC", "cc")) + [
            "-std=c99", "-Wall", "-Wextra", "-pedantic", "-Wno-unused-function", f"-I{GUI}",
            str(harness), str(pure), str(GUI / "ui_motion.c"), str(GUI / "ui_saves.c"),
            str(GUI / "ui_stage.c"), "-o", str(program), "-lm",
        ]
        compiled = subprocess.run(command, capture_output=True, text=True, timeout=60)
        assert compiled.returncode == 0, compiled.stdout + compiled.stderr
        return program

    def run_script(self, script: str, program: Path | None = None) -> list[dict]:
        result = subprocess.run([str(program or self.program)], input=script,
                                capture_output=True, text=True, timeout=60)
        self.assertEqual(result.returncode, 0, result.stderr)
        return parse(result.stdout)

    def assert_rest(self, frame: dict) -> None:
        """Slot A's 16 cubes in its window, Slot B's 16, the focus (A's cell
        5) last: one batch of resting faces, their icons, then the focus."""
        bodies = cube_batches(frame)
        self.assertEqual(len(bodies), 2, "resting faces, then the focused cube's")
        resting, focused = bodies
        # 31 resting cubes: 15 of A's saves and B's 16 cells (a folder, saves
        # and free cells). Each shows two to four faces' quads.
        self.assertGreater(len(quads(resting)), 31 * 3)
        self.assertGreater(width(focused), 56.0, "the focus is 1.5x")
        self.assertLess(width(focused), 80.0)
        drawn = icons(frame)
        # The saves in each window, less cell 6 (not read yet) of each, A's
        # focus and B's folder; then the focus's.
        self.assertEqual(len(drawn), (16 - 2) + (16 - 2) + 1)
        self.assertLess(drawn[-2]["at"], focused["at"], "the focus is drawn after the icons at rest")
        self.assertGreater(drawn[-1]["at"], focused["at"], "its icon after its faces")
        self.assertLess(resting["at"], drawn[0]["at"], "every face at rest before an icon")
        for quad in quads(resting) + quads(focused):
            self.assertGreater(area(quad), 0.0, "a face wound the wrong way")
        self.assertEqual(frame["events"][0], ("K", ""))

    def test_rest(self):
        for wide in (0, 1):
            with self.subTest(wide=wide):
                frames = self.run_script(f"W {wide}\n" + AT_REST)
                self.assert_rest(frames[-1])

    def test_one_invalidation_a_frame_and_icon_textures(self):
        frames = self.run_script(AT_REST)
        for frame in frames:
            self.assertEqual(frame["invalidations"], 1)
            for texture in frame["textures"]:
                if not texture["loaded"]:
                    continue	# the info bar's banner
                self.assertEqual((texture["size"], texture["format"], texture["wrap"]),
                                 ((32, 32), 5, (0, 0)))
                self.assertEqual(texture["offset"] % 2048, 0)
                self.assertTrue(0 <= texture["offset"] < 8 * 2048)
        # The icons animate: both frames show over two ticks.
        offsets = {t["offset"] for f in frames for t in f["textures"] if t["loaded"]}
        self.assertEqual(offsets, {0, 2048})
        # No icon, no clearing: free cells and folders only.
        frames = self.run_script("S 0 16 0 0 1\nS 1 16 0 0 2\nC 0 0\nN 10 0.0166667\n")
        for frame in frames:
            self.assertEqual(frame["invalidations"], 0)
            self.assertEqual(icons(frame), [])

    def test_icons_keep_their_colors(self):
        plain = self.run_script(AT_REST)[-1]
        turned = self.run_script("U 1\n" + AT_REST)[-1]
        for batch in icons(turned):
            for vertex in batch["vertices"]:
                self.assertEqual(vertex[2:5], (255, 255, 255))
        self.assertNotEqual([v[2:6] for v in cube_batches(plain)[0]["vertices"]],
                            [v[2:6] for v in cube_batches(turned)[0]["vertices"]],
                            "the cubes follow Menu Color")

    def test_what_draws_nothing(self):
        # A stack with no grid draws no cube, only why.
        frames = self.run_script("S 0 0 0 0 1\nS 1 16 0 5 2\nC 1 0\nN 30 0.0166667\n")
        frame = frames[-1]
        notes = [e for e in frame["events"] if e[0] == "S" and "Nothing is inserted" in e[1]]
        self.assertEqual(len(notes), 1)
        for batch in cube_batches(frame) + icons(frame):
            for vertex in batch["vertices"]:
                self.assertGreater(vertex[0], 320.0)
        # Rows outside the window fade while it scrolls, then draw nothing.
        frames = self.run_script(AT_REST + "S 0 40 2 30 1\nC 0 9\nN 3 0.0166667\nN 120 0.0166667\n")
        alphas = [v[5] for b in cube_batches(frames[121]) for v in b["vertices"]]
        self.assertTrue(any(0 < a < 200 for a in alphas), "no row faded")
        self.assertTrue(all(84.0 < v[1] < 370.0 for b in cube_batches(frames[-1])
                            for v in b["vertices"]))
        self.assertEqual(len(icons(frames[-1])), (16 - 1) + (16 - 2) + 1)

    def test_the_info_bar(self):
        base = "S 0 40 1 30 1\nS 1 20 0 18 2\nC 0 5\n"
        def shown(info):
            frame = self.run_script(base + info + "N 2 0.0166667\n")[-1]
            return [e for e in frame["events"] if e[0] in ("P", "G")], \
                any(e[0] == "S" and "Sky Harbor" in e[1] for e in frame["events"])
        # The banner; a save without one shows its icon in its place, as the
        # IPL; a folder its picture; a free cell nothing at all.
        self.assertEqual(shown("I 1 1 0\n"), ([("P", "56 381 96 32")], True))
        self.assertEqual(shown("I 1 0 0\n"), ([("P", "88 381 32 32")], True))
        self.assertEqual(shown("I 1 0 1\n"), ([("G", "56 381")], True))
        self.assertEqual(shown("I 0 0 0\n"), ([], False))

    def test_motion_off_is_still(self):
        frames = self.run_script("M 2\n" + AT_REST)
        self.assertEqual(frames[-1]["batches"], frames[-2]["batches"])
        self.assertEqual({t["offset"] for f in frames for t in f["textures"] if t["loaded"]}, {0})
        self.assert_rest(frames[-1])

    def test_the_busiest_frame_holds_its_budget(self):
        # Both stacks full and scrolling, Menu Widescreen, the focus grown
        # and an info bar: the cubes, boxes and arrows a frame sends.
        frames = self.run_script("W 1\nS 0 128 4 127 1\nS 1 128 4 127 2\nC 0 17\nI 1 1 0\n"
                                 "N 60 0.0166667\nS 0 128 5 127 1\nS 1 128 5 127 2\nC 0 21\n"
                                 "N 6 0.0166667\n")
        frame = frames[-1]
        vertices = sum(len(b["vertices"]) for b in frame["batches"])
        self.assertGreater(len(icons(frame)), 32)
        self.assertLessEqual(vertices, 3500)
        self.assertLessEqual(len(frame["batches"]), 140)

    def test_no_depth_and_no_opaque_blend(self):
        self.assertNotIn("GX_SetZMode", self.source)
        self.assertNotIn("GX_BM_NONE", self.source)
        self.assertNotIn("sqrtf", self.cubes_c)

    def test_mutations_are_rejected(self):
        fbm, pure = "fbm", "pure"
        mutations = {
            "a face's area read backwards": (pure, "if(cubesArea(x, y, cubesFace[i]) > 0.5f)",
                                             "if(cubesArea(x, y, cubesFace[i]) < -0.5f)"),
            "no texture cache clearing": (fbm, "\t\t\t\tGX_InvalidateTexAll();\n", ""),
            "the cache cleared for every icon": (
                fbm, "\t\tGX_LoadTexObj(&draw->icon, GX_TEXMAP0);",
                "\t\tGX_InvalidateTexAll();\n\t\tGX_LoadTexObj(&draw->icon, GX_TEXMAP0);"),
            "the focus drawn at rest": (pure, "floats[n++] = focused || g > CUBES_FLOATING;",
                                        "floats[n++] = false;"),
            "icons through Menu Color": (
                fbm, "_SaveCubesVertex(quad->x[v], quad->y[v], white, s[v], t[v]);",
                "{ GXColor c = white; UIColor_Apply(&c.r, &c.g, &c.b);"
                " _SaveCubesVertex(quad->x[v], quad->y[v], c, s[v], t[v]); }"),
            "no window fade": (pure, "fade = outside <= 0.0f ? 1.0f : 1.0f - outside / CUBES_FADE_SPAN;",
                               "fade = 1.0f;"),
        }
        script = "U 1\n" + AT_REST
        for name, (where, old, new) in mutations.items():
            with self.subTest(mutation=name):
                source, cubes = self.source, self.cubes_c
                if where == fbm:
                    self.assertIn(old, source)
                    source = source.replace(old, new, 1)
                else:
                    self.assertIn(old, cubes)
                    cubes = cubes.replace(old, new, 1)
                program = self.build(source, cubes, "mutant")
                result = subprocess.run([str(program)], input=script, capture_output=True,
                                        text=True, timeout=60)
                if result.returncode != 0:
                    continue	# a stand-in caught it
                frames = parse(result.stdout)
                with self.assertRaises(AssertionError):
                    self.assert_rest(frames[-1])
                    for frame in frames:
                        self.assertEqual(frame["invalidations"], 1)
                        for batch in icons(frame):
                            for vertex in batch["vertices"]:
                                self.assertEqual(vertex[2:5], (255, 255, 255))


if __name__ == "__main__":
    unittest.main(verbosity=2)
