#!/usr/bin/env python3
"""Execute native background waves/grid against a checked textured GX FIFO."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

from test_cheats_gx_stream import extract_function

ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / "cube/swiss/source/gui/indigo_background.c"

HARNESS = r"""
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned char u8;
typedef struct { u8 r,g,b,a; } GXColor;
/* Menu Color is Indigo here: the emitters' recolor passes colors through. */
static void UIColor_Apply(u8 *r,u8 *g,u8 *b) { (void)r; (void)g; (void)b; }
typedef struct { float x,y; } indigoPoint_t;
typedef struct { float sine,cosine,stepSine,stepCosine; } waveOscillator_t;
enum { GX_QUADS=1,GX_TRIANGLESTRIP=2,GX_LINES=3,GX_VTXFMT0=0,
    GX_DISABLE=0,GX_ALWAYS=1,GX_FALSE=0,GX_CULL_NONE=0,
    GX_BM_BLEND=1,GX_BL_SRCALPHA=2,GX_BL_INVSRCALPHA=3,GX_BL_ONE=4,GX_LO_CLEAR=0 };
#define INDIGO_TAU 6.28318530718f
#define PRIMARY_WAVE_SEGMENTS 16
#define REAR_WAVE_SEGMENTS 12
#define GLOBE_SEGMENTS 24
#define CHECK(c,m) do { if(!(c)) { fprintf(stderr,"%s\n",m); exit(73); } } while(0)
static indigoPoint_t positions[1024];
static GXColor colors[1024];
static int count,begins,remaining,phase,destination,blendAt[1024];
static bool active;
static void GX_SetZMode(int enable,int comparison,int write) {
    CHECK(enable==GX_DISABLE && comparison==GX_ALWAYS && write==GX_FALSE,
        "background depth state changed");
}
static void GX_SetCullMode(int mode) { CHECK(mode==GX_CULL_NONE,"background culling"); }
static void GX_SetBlendMode(int mode,int source,int dest,int operation) {
    CHECK(mode==GX_BM_BLEND && source==GX_BL_SRCALPHA && operation==GX_LO_CLEAR &&
        (dest==GX_BL_INVSRCALPHA || dest==GX_BL_ONE),"background blend state");
    destination=dest;
}
static void reset(void) {
    CHECK(!active,"unfinished preceding primitive");
    count=begins=remaining=phase=0; destination=GX_BL_INVSRCALPHA;
}
static void GX_Begin(int primitive,int format,int vertices) {
    CHECK(!active && phase==0 && vertices>0,"invalid GX begin");
    CHECK((primitive==GX_QUADS || primitive==GX_TRIANGLESTRIP) && format==GX_VTXFMT0,
        "background reverted to hard lines or incorrect vertex format");
    active=true; remaining=vertices; begins++;
}
static void GX_Position3f32(float x,float y,float z) {
    CHECK(active && phase==0 && remaining>0 && count<1024,"bad position stream");
    CHECK(isfinite(x)&&isfinite(y)&&z==0,"invalid raster position");
    positions[count]=(indigoPoint_t){x,y}; blendAt[count]=destination; phase=1;
}
static void GX_Color4u8(u8 r,u8 g,u8 b,u8 a) {
    CHECK(active && phase==1,"missing/unordered vertex color");
    colors[count]=(GXColor){r,g,b,a}; phase=2;
}
static void GX_TexCoord2f32(float s,float t) {
    CHECK(active && phase==2 && s==0 && t==0,"missing/unordered raster UV");
    phase=0; remaining--; count++;
}
static void GX_End(void) {
    CHECK(active && phase==0 && remaining==0,"incomplete GX vertex/primitive"); active=false;
}
/* EMITTERS */
static bool closef(float a,float b) { return fabsf(a-b)<.003f; }
static bool same(indigoPoint_t a,indigoPoint_t b) { return closef(a.x,b.x)&&closef(a.y,b.y); }
static float sideDistance(indigoPoint_t a,indigoPoint_t b,indigoPoint_t p) {
    return ((b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x))/hypotf(b.x-a.x,b.y-a.y);
}
/* Independent trigonometric reference for the unchanged colored ribbons.
 * The native recurrence uses rounded step constants, so allow subpixel error. */
static indigoPoint_t wavePoint(bool rear,int point,float row,float seconds,float strength) {
    int segments=rear?12:16;
    float ca,cb,width;
    if(rear) {
        ca=sinf(2.05f-seconds*.031f+point*atan2f(.328866647f,.944376370f));
        cb=sinf(.20f+seconds*.018f+point*atan2f(.608761429f,.793353340f));
        width=31+4*sinf(1.40f+seconds*.016f+point*atan2f(.377840787f,.925870585f));
    } else {
        ca=sinf(.35f+seconds*.052f+point*atan2f(.301537960f,.953454172f));
        cb=sinf(1.15f-seconds*.029f+point*atan2f(.562083378f,.827080574f));
        width=24+5*sinf(.80f-seconds*.021f+point*atan2f(.353474844f,.935444031f));
    }
    return (indigoPoint_t){-48+(rear?0:(1-strength)*-24)+736.0f*point/segments,
        (rear?220:272)+(.70f+strength*.30f)*((rear?30:43)*ca+(rear?7:10)*cb)+width*row};
}
static GXColor faded(GXColor c,int point,int segments,float strength) {
    float t=(float)point/segments; c.a=(u8)(c.a*4*t*(1-t)*strength); return c;
}
static void checkColor(int i,GXColor expected) {
    CHECK(colors[i].r==expected.r && colors[i].g==expected.g && colors[i].b==expected.b &&
        abs((int)colors[i].a-expected.a)<=1,"original wave color/fade changed");
}
static void checkWave(float seconds,bool animated,float strength) {
    static const float rows[2][4]={{-1,0,1,0},{-1,-.28f,.30f,1}};
    static const GXColor palette[2][4]={
        {{55,47,140,4},{128,105,232,48},{42,31,105,4},{0,0,0,0}},
        {{86,60,168,6},{196,178,255,82},{113,85,210,46},{45,31,103,6}}};
    reset(); drawSilkWaves(seconds,animated,strength);
    if(strength<=0) { CHECK(count==0 && begins==0,"hidden wave emitted geometry"); return; }
    if(strength>1) strength=1;
    if(!animated) seconds=0;
    CHECK(count==510 && begins==8,"wave geometry is missing or unbounded");
    CHECK(destination==GX_BL_INVSRCALPHA,"wave additive blending leaked");
    int cursor=52; /* two rear edge feathers remain behind both ribbon fills */
    for(int layer=0;layer<2;layer++) {
        int segments=layer?16:12;
        for(int segment=0;segment<segments;segment++) for(int row=0;row<(layer?3:2);row++) {
            const int sample[4]={segment,segment+1,segment+1,segment};
            const int level[4]={row,row,row+1,row+1};
            for(int v=0;v<4;v++,cursor++) {
                CHECK(same(positions[cursor],wavePoint(!layer,sample[v],rows[layer][level[v]],
                    seconds,strength)),"original ribbon fill path changed");
                checkColor(cursor,faded(palette[layer][level[v]],sample[v],segments,strength));
                CHECK(blendAt[cursor]==GX_BL_INVSRCALPHA,"ribbon gained additive fill");
            }
        }
    }
    CHECK(cursor==340,"original ribbon fill budget changed");
    for(int layer=0;layer<2;layer++) for(int edge=0;edge<2;edge++) {
        if(edge==0) cursor=layer?340:0;
        int segments=layer?16:12,side=edge?1:-1;
        for(int i=0;i<=segments;i++) {
            int v=cursor+i*2;
            CHECK(same(positions[v],wavePoint(!layer,i,side,seconds,strength)),
                "wave feather disconnected from original fill");
            checkColor(v,faded(palette[layer][edge?(layer?3:2):0],i,segments,strength));
            CHECK(colors[v+1].a==0,"ribbon fringe did not reach transparent");
            CHECK(blendAt[v]==GX_BL_INVSRCALPHA,"wave feather blend changed");
            if(i<segments) {
                CHECK(closef(sideDistance(positions[v],positions[v+2],positions[v+1]),side) &&
                    closef(sideDistance(positions[v],positions[v+2],positions[v+3]),side),
                    "ribbon edge is not one pixel outward");
            }
        }
        cursor+=(segments+1)*2;
    }
    CHECK(cursor==408,"ribbon feather budget");
    for(int band=0;band<3;band++) for(int i=0;i<=16;i++) {
        int v=408+band*34+i*2;
        GXColor c=faded((GXColor){238,232,255,34},i,16,strength);
        CHECK(colors[v].a==(band==0?0:c.a) && colors[v+1].a==(band==2?0:c.a),
            "crest profile lost transparent fringes/original brightness");
        CHECK(colors[v].r==238 && colors[v].g==232 && colors[v].b==255,
            "crest color changed");
        CHECK(blendAt[v]==GX_BL_ONE && blendAt[v+1]==GX_BL_ONE,"crest lost additive blend");
        if(band<2) CHECK(same(positions[v+1],positions[v+34]),"crest coverage seam");
        if(i<16) {
            indigoPoint_t a={(positions[442+i*2].x+positions[443+i*2].x)*.5f,
                (positions[442+i*2].y+positions[443+i*2].y)*.5f};
            indigoPoint_t b={(positions[444+i*2].x+positions[445+i*2].x)*.5f,
                (positions[444+i*2].y+positions[445+i*2].y)*.5f};
            CHECK(same(a,wavePoint(false,i,-.28f,seconds,strength)),"crest moved off ribbon shoulder");
            float width=sideDistance(a,b,positions[v+1])-sideDistance(a,b,positions[v]);
            CHECK(closef(width,band==1?1.0f/3:1),"crest integrated 8/6-pixel width changed");
        }
    }
}
static void testWaves(void) {
    const float strengths[]={-1,0,.2f,.76f,1,2};
    const float times[]={0,17.5f,1035.75f};
    for(int motion=0;motion<2;motion++) for(int t=0;t<3;t++) for(int s=0;s<6;s++)
        checkWave(times[t],motion,strengths[s]);
    indigoPoint_t saved[510]; GXColor savedColors[510];
    reset(); drawSilkWaves(0,false,.76f);
    memcpy(saved,positions,sizeof(saved)); memcpy(savedColors,colors,sizeof(savedColors));
    reset(); drawSilkWaves(817.25f,false,.76f);
    CHECK(!memcmp(saved,positions,sizeof(saved)) && !memcmp(savedColors,colors,sizeof(savedColors)),
        "disabled background animation still moves");
    reset(); drawSilkWaves(817.25f,true,.76f);
    CHECK(memcmp(saved,positions,sizeof(saved)),"enabled wave animation stopped");
}
static void testGrid(void) {
    const float radii[6][2]={{252,176},{252,116},{252,58},{70,184},{140,184},{218,184}};
    for(int drift=0;drift<3;drift++) {
        reset(); drawGlobeGrid(320.25f,212.75f,drift*3.25f);
        CHECK(count==600 && begins==12,"grid coverage budget");
        for(int ring=0;ring<6;ring++) for(int band=0;band<2;band++) {
            int start=ring*100+band*50;
            CHECK(same(positions[start],positions[start+48]) &&
                same(positions[start+1],positions[start+49]),"closed grid join has a seam");
            for(int i=0;i<=24;i++) {
                int v=start+i*2;
                CHECK(colors[v].a==(band?7:0) && colors[v+1].a==(band?0:7),"grid triangular alpha");
                CHECK(colors[v].r==117 && colors[v].g==101 && colors[v].b==209,"grid color changed");
                if(!band) {
                    indigoPoint_t p={320.25f+radii[ring][0]*cosf(i*INDIGO_TAU/24),
                        212.75f+drift*3.25f+radii[ring][1]*sinf(i*INDIGO_TAU/24)};
                    CHECK(same(positions[v+1],p),"grid ellipse/drift changed");
                    CHECK(same(positions[v+1],positions[v+50]),"grid band seam");
                }
                if(i<24) {
                    indigoPoint_t a=positions[ring*100+i*2+1],b=positions[ring*100+i*2+3];
                    CHECK(closef(sideDistance(a,b,positions[v+(band?1:0)]),band?.5f:-.5f),
                        "grid 3/6-pixel integrated width changed");
                }
            }
        }
    }
}
int main(void) {
    testWaves(); testGrid();
    indigoPoint_t p[2]={{0,0},{0,0}},join[2];
    CHECK(!buildRasterJoins(p,join,1,false) && !buildRasterJoins(p,join,2,false),
        "degenerate raster path accepted");
    puts("background GX: original color/motion, complete streams, joined native coverage");
    return 0;
}
"""


class BackgroundGXStreamTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = SOURCE.read_text()
        marker = "static bool railJoin("
        blocks = [extract_function(source[source.rindex(marker):], marker)]
        for result, names in [
            ("void", ("putVertex", "initWaveOscillator", "advanceWaveOscillator", "buildWavePath")),
            ("float", ("waveEdgeFade",)), ("GXColor", ("waveVertexColor",)),
            ("bool", ("buildRasterJoins",)),
            ("void", ("drawRasterStroke", "drawWaveFeather", "drawSilkWaves", "drawGlobeGrid")),
        ]:
            blocks += [extract_function(source, f"static {result} {name}(") for name in names]
        cls.emitters = "\n".join(blocks)

    def run_emitters(self, emitters):
        with tempfile.TemporaryDirectory(prefix="swiss-background-gx-") as directory:
            root = Path(directory)
            source, binary = root / "background.c", root / "background"
            source.write_text(HARNESS.replace("/* EMITTERS */", emitters))
            result = subprocess.run(shlex.split(os.environ.get("CC", "cc")) +
                ["-std=c99", "-Wall", "-Wextra", "-Werror", str(source), "-o", str(binary), "-lm"],
                capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            return subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)

    def test_native_background(self):
        result = self.run_emitters(self.emitters)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_regressions_are_rejected(self):
        mutants = {
            "missing UV": ("GX_TexCoord2f32(0.0f, 0.0f);",
                "if(color.a == 254) GX_TexCoord2f32(0.0f, 0.0f);"),
            "opaque stroke fringe": ("if(side == 0 || side == bands) color.a = 0;", "/* hard edge */"),
            "wrong crest width": ("7.0f/6.0f}", "13.0f/6.0f}"),
            "grid closure": ("joins[GLOBE_SEGMENTS] = joins[0];",
                "joins[GLOBE_SEGMENTS] = (indigoPoint_t){0,0};"),
            "wave clamp": ("strength = 1.0f;", "strength = 0.5f;"),
            "wave color": ("{196, 178, 255, 82}", "{170, 178, 255, 82}"),
            "ribbon fringe": ("edge.a = 0;", "edge.a = 255;"),
            "inward wave fringe": ("points[i].y + joins[i].y * side", "points[i].y - joins[i].y * side"),
            "disabled animation": ("animated ? seconds : 0.0f", "animated ? seconds : 1.0f"),
            "hard crest": ("GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT0, count * 2);",
                "GX_Begin(GX_LINES, GX_VTXFMT0, count * 2);"),
        }
        for name, (old, new) in mutants.items():
            with self.subTest(name=name):
                self.assertIn(old, self.emitters)
                result = self.run_emitters(self.emitters.replace(old, new, 1))
                self.assertNotEqual(result.returncode, 0, "mutant survived: " + name)


if __name__ == "__main__":
    unittest.main()
