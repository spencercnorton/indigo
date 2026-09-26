#!/usr/bin/env python3
"""Execute the real cheat-panel emitter against a checked GX FIFO contract.

Missing texture coordinates desynchronize GX_VTXFMT0 even when TEV ignores the
texture. A native preview exposed this exact defect; text/source checks alone
cannot establish that every declared vertex emits every enabled attribute.
"""

import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / "cube/swiss/source/gui/FrameBufferMagic.c"


def extract_function(source: str, marker: str) -> str:
    start = source.index(marker)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise ValueError(f"Unterminated function: {marker}")


HARNESS = r"""
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct { unsigned char r, g, b, a; } GXColor;
/* Menu Color is Indigo here: the panel's recolor passes colors through. */
static void UIColor_Apply(unsigned char *r, unsigned char *g, unsigned char *b)
{
    (void)r; (void)g; (void)b;
}
enum { GX_TRIANGLEFAN = 6, GX_VTXFMT0 = 0 };
static int phase, declared, emitted, begins, ends, initializations;
static bool active, pipelineReady, rasterReady;
static int left, top, panelWidth, panelHeight;
static float positions[10][2];
static GXColor wanted;

#define CHECK(condition, message) do { \
    if(!(condition)) { fprintf(stderr, "%s\n", message); exit(73); } \
} while(0)

static void drawInit(void)
{
    CHECK(!active, "pipeline reset inside active primitive");
    pipelineReady = true;
    rasterReady = false;
    ++initializations;
}

static void _SetupRasterColor(void)
{
    CHECK(pipelineReady, "panel did not restore GX_VTXFMT0 before emission");
    rasterReady = true;
}

static void GX_Begin(int primitive, int format, int vertices)
{
    CHECK(!active && rasterReady, "begin without configured pipeline");
    CHECK(primitive == GX_TRIANGLEFAN && format == GX_VTXFMT0,
        "unexpected primitive or vertex format");
    CHECK(vertices == 10, "fan must declare center plus nine perimeter vertices");
    declared = vertices;
    active = true;
    phase = 0;
    ++begins;
}

static void GX_Position3f32(float x, float y, float z)
{
    CHECK(active && phase == 0, "missing/reordered color or UV before next vertex");
    CHECK(emitted < 10, "too many vertex positions");
    CHECK(isfinite(x) && isfinite(y) && z == 0.0f, "invalid panel position");
    CHECK(x >= (float)left && x <= (float)(left + panelWidth),
        "panel vertex outside horizontal bounds");
    CHECK(y >= (float)top && y <= (float)(top + panelHeight),
        "panel vertex outside vertical bounds");
    positions[emitted][0] = x;
    positions[emitted][1] = y;
    phase = 1;
}

static void GX_Color4u8(unsigned char r, unsigned char g, unsigned char b,
    unsigned char a)
{
    CHECK(active && phase == 1, "color missing its preceding position");
    CHECK(r == wanted.r && g == wanted.g && b == wanted.b && a == wanted.a,
        "vertex color differs from panel color");
    phase = 2;
}

static void GX_TexCoord2f32(float s, float t)
{
    CHECK(active && phase == 2, "UV missing its preceding position/color");
    CHECK(s == 0.0f && t == 0.0f, "unexpected raster-only panel UV");
    phase = 0;
    ++emitted;
}

static void GX_End(void)
{
    CHECK(active && phase == 0, "last vertex has incomplete attributes");
    CHECK(emitted == declared, "declared/emitted vertex count differs");
    active = false;
    ++ends;
}

/* PANEL_SOURCE */

int main(void)
{
    static const int cases[][4] = {
        {40, 148, 560, 34}, {-6, -6, 652, 492}, {526, 153, 62, 24},
        {608, 148, 2, 234}, {40, 152, 3, 26}, {40, 413, 560, 1},
        {52, 376, 1, 4}, {0, 0, 1, 1}, {40, 264, 560, 44}
    };
    size_t index;
    for(index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        left = cases[index][0]; top = cases[index][1];
        panelWidth = cases[index][2]; panelHeight = cases[index][3];
        wanted = (GXColor){115, 234, 234, 254};
        phase = declared = emitted = begins = ends = initializations = 0;
        active = pipelineReady = rasterReady = false;
        _CheatsPanel(left, top, panelWidth, panelHeight, wanted);
        CHECK(begins == 1 && ends == 1 && emitted == 10,
            "panel did not emit exactly one complete fan");
        CHECK(initializations == 2, "panel must establish and restore drawing state");
        CHECK(positions[0][0] == (float)left + (float)panelWidth * 0.5f &&
            positions[0][1] == (float)top + (float)panelHeight * 0.5f,
            "fan center is not the panel midpoint");
        CHECK(positions[1][0] == positions[9][0] &&
            positions[1][1] == positions[9][1], "fan perimeter is not closed");
    }
    puts("cheat GX stream: nine panel geometries, complete position/color/UV vertices");
    return 0;
}
"""


# The Settings page's choice arrows are the one raw GX emitter it adds; the
# rest of the page is drawn with _CheatsPanel and the font.
ARROW_HARNESS = r"""
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct { unsigned char r, g, b, a; } GXColor;
static int applied;
static void UIColor_Apply(unsigned char *r, unsigned char *g, unsigned char *b)
{
    (void)r; (void)g; (void)b;
    ++applied;
}
enum { GX_TRIANGLES = 1, GX_VTXFMT0 = 0 };
static int phase, declared, emitted, begins, ends, initializations;
static bool active, pipelineReady, rasterReady;
static float xs[3], ys[3];

#define CHECK(condition, message) do { \
    if(!(condition)) { fprintf(stderr, "%s\n", message); exit(73); } \
} while(0)

static void drawInit(void)
{
    CHECK(!active, "pipeline reset inside active primitive");
    pipelineReady = true;
    rasterReady = false;
    ++initializations;
}

static void _SetupRasterColor(void)
{
    CHECK(pipelineReady, "arrow did not restore GX_VTXFMT0 before emission");
    rasterReady = true;
}

static void GX_Begin(int primitive, int format, int vertices)
{
    CHECK(!active && rasterReady, "begin without configured pipeline");
    CHECK(primitive == GX_TRIANGLES && format == GX_VTXFMT0 && vertices == 3,
        "an arrow is one triangle");
    declared = vertices;
    active = true;
    phase = 0;
    ++begins;
}

static void GX_Position3f32(float x, float y, float z)
{
    CHECK(active && phase == 0, "missing/reordered color or UV before next vertex");
    CHECK(emitted < 3, "too many vertex positions");
    CHECK(isfinite(x) && isfinite(y) && z == 0.0f, "invalid arrow position");
    xs[emitted] = x;
    ys[emitted] = y;
    phase = 1;
}

static void GX_Color4u8(unsigned char r, unsigned char g, unsigned char b,
    unsigned char a)
{
    (void)r; (void)g; (void)b; (void)a;
    CHECK(active && phase == 1, "color missing its preceding position");
    phase = 2;
}

static void GX_TexCoord2f32(float s, float t)
{
    CHECK(active && phase == 2, "UV missing its preceding position/color");
    CHECK(s == 0.0f && t == 0.0f, "unexpected raster-only UV");
    phase = 0;
    ++emitted;
}

static void GX_End(void)
{
    CHECK(active && phase == 0, "last vertex has incomplete attributes");
    CHECK(emitted == declared, "declared/emitted vertex count differs");
    active = false;
    ++ends;
}

/* ARROW_SOURCE */

int main(void)
{
    static const float directions[2] = {-1.0f, 1.0f};
    int i;
    for(i = 0; i < 2; ++i) {
        float d = directions[i];
        phase = declared = emitted = begins = ends = initializations = applied = 0;
        active = pipelineReady = rasterReady = false;
        _SettingsArrow(100.0f, 50.0f, d, (GXColor){196, 177, 255, 255});
        CHECK(begins == 1 && ends == 1 && emitted == 3,
            "arrow did not emit exactly one complete triangle");
        CHECK(applied == 3, "an arrow vertex skips the Menu Color");
        CHECK(initializations == 2, "arrow must establish and restore drawing state");
        /* It points the way it steps, centred on its line. */
        CHECK((xs[0] - 100.0f) * d > 0.0f && (xs[1] - 100.0f) * d < 0.0f &&
            (xs[2] - 100.0f) * d < 0.0f, "arrow points the wrong way");
        CHECK(ys[0] == 50.0f && ys[1] < 50.0f && ys[2] > 50.0f,
            "arrow is not centred on its line");
    }
    puts("settings arrow GX stream: both directions, complete vertices");
    return 0;
}
"""


class CheatGXStreamTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SOURCE.read_text()
        cls.panel = extract_function(cls.source, "static void _CheatsPanel(")

    def compile_and_run(self, panel: str) -> subprocess.CompletedProcess:
        with tempfile.TemporaryDirectory(prefix="swiss-cheat-gx-") as directory:
            root = Path(directory)
            source = root / "panel.c"
            executable = root / "panel"
            source.write_text(HARNESS.replace("/* PANEL_SOURCE */", panel))
            command = shlex.split(os.environ.get("CC", "cc")) + [
                "-std=c99", "-Wall", "-Wextra", "-pedantic", str(source),
                "-o", str(executable), "-lm",
            ]
            compiled = subprocess.run(command, capture_output=True, text=True, timeout=30)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            return subprocess.run([str(executable)], capture_output=True,
                                  text=True, timeout=5)

    def test_stream_contract_matches_live_vertex_format(self):
        initialization = extract_function(self.source, "static void drawInit()")
        raster = extract_function(self.source, "static void _SetupRasterColor(void)")
        for attribute in ("POS", "CLR0", "TEX0"):
            self.assertRegex(initialization,
                             rf"GX_SetVtxDesc\(GX_VA_{attribute},\s*GX_DIRECT\)")
        self.assertRegex(initialization,
                         r"GX_SetVtxAttrFmt\(GX_VTXFMT0,\s*GX_VA_TEX0,\s*GX_TEX_ST,\s*GX_F32")
        self.assertNotRegex(raster, r"GX_(?:ClearVtxDesc|SetVtxDesc|SetVtxAttrFmt)\(")

    def compile_and_run_arrow(self, source: str) -> subprocess.CompletedProcess:
        with tempfile.TemporaryDirectory(prefix="swiss-settings-gx-") as directory:
            root = Path(directory)
            harness = root / "arrow.c"
            executable = root / "arrow"
            harness.write_text(ARROW_HARNESS.replace("/* ARROW_SOURCE */", source))
            command = shlex.split(os.environ.get("CC", "cc")) + [
                "-std=c99", "-Wall", "-Wextra", "-pedantic", str(harness),
                "-o", str(executable), "-lm",
            ]
            compiled = subprocess.run(command, capture_output=True, text=True, timeout=30)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            return subprocess.run([str(executable)], capture_output=True,
                                  text=True, timeout=5)

    def test_settings_arrow_emits_complete_vertices(self):
        vertex = extract_function(self.source, "static void _putFlatVertex(")
        arrow = extract_function(self.source, "static void _SettingsArrow(")
        source = vertex + "\n" + arrow
        result = self.compile_and_run_arrow(source)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        uv = "GX_TexCoord2f32(0.0f, 0.0f);"
        for name, mutant in {
            "missing-uv": source.replace(uv, "", 1),
            "declared-count-mismatch": source.replace("GX_VTXFMT0, 3)", "GX_VTXFMT0, 4)"),
            "unrecolored": source.replace("UIColor_Apply(&color.r, &color.g, &color.b);", "", 1),
            "backwards": source.replace("x + 3.0f * direction", "x - 3.0f * direction"),
        }.items():
            with self.subTest(mutation=name):
                self.assertNotEqual(mutant, source)
                result = self.compile_and_run_arrow(mutant)
                self.assertEqual(result.returncode, 73, result.stdout + result.stderr)

    def test_real_panel_emits_complete_bounded_vertices(self):
        result = self.compile_and_run(self.panel)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_historical_and_adjacent_protocol_mutations_are_rejected(self):
        uv = "GX_TexCoord2f32(0.0f, 0.0f);"
        self.assertEqual(self.panel.count(uv), 2)
        last = self.panel.rfind(uv)
        mutations = {
            "missing-center-uv": self.panel.replace(uv, "", 1),
            "missing-perimeter-uv": self.panel[:last] + self.panel[last:].replace(uv, "", 1),
            "historical-missing-all-uv": self.panel.replace(uv, ""),
            "declared-count-mismatch": self.panel.replace("GX_VTXFMT0, 10)", "GX_VTXFMT0, 9)"),
            "missing-pipeline-initialization": self.panel.replace("drawInit();", "", 1),
            "narrow-panel-corner-overflow": self.panel.replace(" && width > 12", ""),
            "incomplete-last-vertex": self.panel.replace("i < 9", "i < 8"),
        }
        for name, mutant in mutations.items():
            with self.subTest(mutation=name):
                self.assertNotEqual(mutant, self.panel)
                result = self.compile_and_run(mutant)
                self.assertEqual(result.returncode, 73, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
