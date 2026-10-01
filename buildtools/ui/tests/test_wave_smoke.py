#!/usr/bin/env python3
"""The waves' smoke against a checked GX stream: the cloud noise it bakes, the
band it lays round the crest, and how it drifts."""
import os
from pathlib import Path
import re
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
typedef unsigned short u16;
typedef unsigned int u32;
typedef struct { u8 r,g,b,a; } GXColor;
typedef struct { void *image; } GXTexObj;
typedef struct { float x,y; } indigoPoint_t;
#define ATTRIBUTE_ALIGN(n)
#define PRIMARY_WAVE_SEGMENTS 16
#define CHECK(c,m) do { if(!(c)) { fprintf(stderr,"%s\n",m); exit(73); } } while(0)
enum { GX_QUADS=1,GX_TRIANGLESTRIP=2,GX_VTXFMT0=0,GX_DIRECT=1,GX_VA_POS=9,GX_VA_CLR0=11,
    GX_VA_TEX0=13,GX_VA_TEX1=14,GX_TEX_ST=1,GX_F32=4,GX_TF_I8=1,GX_REPEAT=1,GX_FALSE=0,
    GX_LINEAR=1,GX_ANISO_1=0,GX_TEXMAP2=2,GX_TEXCOORD0=0,GX_TEXCOORD1=1,GX_TG_MTX2x4=1,
    GX_TG_TEX0=4,GX_TG_TEX1=5,GX_IDENTITY=60,GX_TEVSTAGE0=0,GX_TEVSTAGE1=1,GX_COLOR0A0=4,
    GX_CC_ZERO=15,GX_CC_RASC=10,GX_CC_CPREV=0,GX_CA_ZERO=7,GX_CA_RASA=5,GX_CA_TEXA=4,
    GX_CA_APREV=0,GX_TEV_ADD=0,GX_TB_ZERO=0,GX_CS_SCALE_1=0,GX_CS_SCALE_2=1,GX_ENABLE=1,
    GX_TEVPREV=0,GX_BM_BLEND=1,GX_BL_SRCALPHA=4,GX_BL_INVSRCALPHA=5,GX_BL_ONE=1,GX_LO_CLEAR=0 };
static void UIColor_Apply(u8 *r,u8 *g,u8 *b) { (void)r; (void)g; (void)b; }
static float UIStage_PixelWidth(void) { return 1.0f; }
static int flushed, invalidated, restored, loadedMap=-1, coords=1, alphaScale=-1, destination;
static void *texture;
static void DCFlushRange(void *p,u32 n) { (void)p; (void)n; flushed++; }
static void GX_InitTexObj(GXTexObj *o,void *image,int w,int h,int format,int ws,int wt,int mip) {
    CHECK(w==64 && h==64 && format==GX_TF_I8 && ws==GX_REPEAT && wt==GX_REPEAT && !mip,
        "smoke noise is not a 64x64 I8 texture that repeats");
    o->image=image; texture=image;
}
static void GX_InitTexObjLOD(GXTexObj *o,int a,int b,float c,float d,float e,int f,int g,int h) {
    (void)o;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h;
}
static void GX_InvalidateTexAll(void) { invalidated++; }
static void GX_LoadTexObj(GXTexObj *o,int map) { CHECK(o->image==texture,"smoke reads another texture"); loadedMap=map; }
static void GX_ClearVtxDesc(void) { coords=0; }
static void GX_SetVtxDesc(int attribute,int type) {
    CHECK(type==GX_DIRECT,"indexed smoke attribute");
    if(attribute==GX_VA_TEX0 || attribute==GX_VA_TEX1) coords++;
}
static void GX_SetVtxAttrFmt(int a,int b,int c,int d,int e) { (void)a;(void)b;(void)c;(void)d;(void)e; }
static void GX_SetNumTexGens(int n) { (void)n; }
static void GX_SetTexCoordGen(int a,int b,int c,int d) { (void)a;(void)b;(void)c;(void)d; }
static void GX_SetNumTevStages(int n) { (void)n; }
static void GX_SetTevOrder(int a,int b,int c,int d) { (void)a;(void)b;(void)c;(void)d; }
static void GX_SetTevColorIn(int s,int a,int b,int c,int d) { (void)s;(void)a;(void)b;(void)c;(void)d; }
static void GX_SetTevAlphaIn(int s,int a,int b,int c,int d) { (void)s;(void)a;(void)b;(void)c;(void)d; }
static void GX_SetTevColorOp(int s,int a,int b,int c,int d,int e) { (void)s;(void)a;(void)b;(void)c;(void)d;(void)e; }
static void GX_SetTevAlphaOp(int s,int a,int b,int scale,int d,int e) {
    (void)a;(void)b;(void)d;(void)e; if(s==GX_TEVSTAGE1) alphaScale=scale;
}
static void GX_SetBlendMode(int mode,int source,int dest,int operation) {
    CHECK(mode==GX_BM_BLEND && source==GX_BL_SRCALPHA && operation==GX_LO_CLEAR,"smoke blend");
    destination=dest;
}
/* The raster pipeline's state comes back: one texture coordinate, alpha blend. */
static void setupRasterPipeline(void) { restored++; coords=1; destination=GX_BL_INVSRCALPHA; }
static float px[2048],py[2048],s0[2048],t0[2048],s1[2048],t1[2048];
static u8 alpha[2048];
static int count,remaining,phase,blendAt[2048],coordsAt[2048];
static bool active;
static void GX_Begin(int primitive,int format,int vertices) {
    CHECK(!active && (primitive==GX_TRIANGLESTRIP || primitive==GX_QUADS) && format==GX_VTXFMT0 &&
        vertices>0,"bad smoke primitive"); active=true; remaining=vertices;
}
static void GX_Position3f32(float x,float y,float z) {
    CHECK(active && phase==0 && remaining>0 && count<2048 && isfinite(x) && isfinite(y) && z==0,
        "bad smoke position"); px[count]=x; py[count]=y; phase=1;
}
static void GX_Color4u8(u8 r,u8 g,u8 b,u8 a) {
    (void)r;(void)g;(void)b; CHECK(active && phase==1,"smoke color order");
    alpha[count]=a; blendAt[count]=destination; coordsAt[count]=coords; phase=2;
}
static void GX_TexCoord2f32(float s,float t) {
    CHECK(active && phase>=2 && phase<2+coords && isfinite(s) && isfinite(t),"smoke coordinate order");
    if(phase==2) { s0[count]=s; t0[count]=t; } else { s1[count]=s; t1[count]=t; }
    if(++phase==2+coords) { phase=0; remaining--; count++; }
}
static void GX_End(void) { CHECK(active && remaining==0 && phase==0,"unfinished smoke primitive"); active=false; }
/* EMITTERS */
static indigoPoint_t crest[PRIMARY_WAVE_SEGMENTS+1];
static int smokeStart(void) {
    for(int i=0;i<count;i++) if(coordsAt[i]==2) return i;
    return -1;
}
static void draw(float seconds,bool animated) {
    count=0; restored=0; drawWaveSmoke(crest,seconds,animated,.76f);
    CHECK(!active && restored==1,"smoke left its pipeline loaded");
}
static void testNoise(void) {
    draw(0,false);
    CHECK(flushed==1 && invalidated==1 && loadedMap==GX_TEXMAP2,"noise not baked once into TEXMAP2");
    static u8 n[64][64];
    for(int y=0;y<64;y++) for(int x=0;x<64;x++)
        n[y][x]=waveSmokeTexels[((y>>2)*8+(x>>3))*32+(y&3)*8+(x&7)];
    int low=255,high=0; double inside=0,seam=0;
    for(int y=0;y<64;y++) for(int x=0;x<64;x++) {
        low=n[y][x]<low?n[y][x]:low; high=n[y][x]>high?n[y][x]:high;
        if(x<63) inside+=abs(n[y][x]-n[y][x+1]); else seam+=abs(n[y][63]-n[y][0]);
        if(y<63) inside+=abs(n[y][x]-n[y+1][x]); else seam+=abs(n[63][x]-n[0][x]);
    }
    CHECK(low==0 && high==255,"noise does not span clear to bright");
    /* It repeats: across the wrap it changes no more than between neighbours. */
    CHECK(seam/128 < 2*inside/(2*64*63),"noise has a seam where it repeats");
    draw(10,false);
    CHECK(flushed==1,"noise baked again");
}
static void testBand(void) {
    draw(0,false);
    int first=smokeStart();
    CHECK(first>0,"no crest glow before the smoke");
    for(int i=0;i<first;i++) CHECK(blendAt[i]==GX_BL_ONE,"crest glow is not added light");
    CHECK(count-first==8*34,"smoke band budget");
    CHECK(alphaScale==GX_CS_SCALE_2,"smoke noise product not doubled");
    for(int strip=0;strip<8;strip++) {
        int side=strip/4,row=strip%4;
        for(int i=0;i<=16;i++) for(int k=0;k<2;k++) {
            int v=first+strip*34+i*2+k,level=row+k;
            CHECK(blendAt[v]==GX_BL_ONE,"smoke is not added light");
            CHECK(fabsf(px[v]-crest[i].x)<.001f,"smoke left the crest's columns");
            float off=py[v]-crest[i].y;
            if(level==0) CHECK(fabsf(off)<.001f,"smoke does not start on the crest");
            else CHECK(side==0?off<0:off>0,"smoke row on the wrong side of the crest");
            if(level==4) CHECK(alpha[v]==0,"smoke edge is not clear");
            if(k==1) CHECK(alpha[v]<=alpha[v-1],"smoke brightens away from the crest");
        }
    }
    CHECK(alpha[first+16]>alpha[first],"smoke not brightest mid-screen");
}
static void testDrift(void) {
    static float still[2048],moved[2048];
    draw(0,false); int first=smokeStart();
    for(int i=first;i<count;i++) still[i]=s0[i]+t0[i]+s1[i]+t1[i];
    draw(3600.0f*30,false);
    for(int i=first;i<count;i++) CHECK(still[i]==s0[i]+t0[i]+s1[i]+t1[i],"stopped smoke still drifts");
    draw(37,true);
    for(int i=first;i<count;i++) moved[i]=s0[i]+t0[i]+s1[i]+t1[i];
    draw(38,true);
    int changed=0; for(int i=first;i<count;i++) changed+=moved[i]!=s0[i]+t0[i]+s1[i]+t1[i];
    CHECK(changed==count-first,"smoke does not drift");
    /* Thirty hours on, the coordinates stay small enough to keep their precision. */
    draw(3600.0f*30,true);
    for(int i=first;i<count;i++)
        CHECK(fabsf(s0[i])<8 && fabsf(t0[i])<8 && fabsf(s1[i])<8 && fabsf(t1[i])<8,
            "smoke coordinates grow with time");
}
int main(void) {
    for(int i=0;i<=16;i++) crest[i]=(indigoPoint_t){-48+46.0f*i,240+30*sinf(i*.4f)};
    testNoise(); testBand(); testDrift();
    puts("wave smoke: full-range repeating noise, a band from the crest, bounded drift");
    return 0;
}
"""


class WaveSmokeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = SOURCE.read_text()
        marker = "static bool railJoin("
        blocks = [extract_function(source[source.rindex(marker):], marker)]
        for signature in ("static void putVertex(", "static float waveEdgeFade(",
                "static GXColor waveVertexColor(", "static bool buildRasterJoins(",
                "static void drawRasterStroke("):
            blocks.append(extract_function(source, signature))
        blocks.append(re.search(r"^#define WAVE_SMOKE_SIZE .*$", source, re.M).group(0))
        blocks += re.findall(r"^static (?:u8 waveSmokeTexels|GXTexObj waveSmokeTexObj|"
                             r"bool waveSmokeBaked)\b.*;$", source, re.M)
        for signature in ("static float waveSmokeNoise(", "static void bakeWaveSmoke(",
                "static void putSmokeVertex(", "static void drawWaveSmoke("):
            blocks.append(extract_function(source, signature))
        cls.emitters = "\n".join(blocks)

    def run_harness(self, emitters):
        with tempfile.TemporaryDirectory(prefix="swiss-wave-smoke-") as directory:
            root = Path(directory)
            source, binary = root / "smoke.c", root / "smoke"
            source.write_text(HARNESS.replace("/* EMITTERS */", emitters))
            result = subprocess.run(shlex.split(os.environ.get("CC", "cc")) +
                ["-std=c99", "-Wall", "-Wextra", "-Werror", str(source), "-o", str(binary), "-lm"],
                capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            return subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)

    def test_smoke(self):
        result = self.run_harness(self.emitters)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_regressions_are_rejected(self):
        mutants = {
            "noise not stretched": ("(waveSmokeNoise(x, y) - low) / (high - low) * 255.0f",
                "waveSmokeNoise(x, y) * 255.0f"),
            "noise lattice does not wrap": ("(u32)((x0 + (i & 1)) % cells)",
                "(u32)((x0 + (i & 1)))"),
            "smoke on one side": ("static const float reach[2] = {-60.0f, 76.0f};",
                "static const float reach[2] = {60.0f, 76.0f};"),
            "hard smoke edge": ("1.0f, 0.77f, 0.50f, 0.23f, 0.0f", "1.0f, 0.77f, 0.50f, 0.23f, 0.2f"),
            "unbounded drift": ("fmodf(t * 0.020f, 1.0f)", "t * 0.020f"),
            "smoke keeps moving when stopped": ("float t = animated ? seconds : 0.0f;",
                "float t = animated ? seconds : seconds;"),
            "pipeline left loaded": ("\tsetupRasterPipeline();\n}", "\tif(0) setupRasterPipeline();\n}"),
        }
        for name, (old, new) in mutants.items():
            with self.subTest(name=name):
                self.assertIn(old, self.emitters)
                result = self.run_harness(self.emitters.replace(old, new, 1))
                self.assertNotEqual(result.returncode, 0, "mutant survived: " + name)


if __name__ == "__main__":
    unittest.main()
