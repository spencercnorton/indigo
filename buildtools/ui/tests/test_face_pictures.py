#!/usr/bin/env python3
"""Turned faces' icon pictures against a checked GX stream: the doubled
projection, which faces get a picture, the copies that make one, and the quad
that lays it on the glass."""
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
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef signed char s8;
typedef float f32;
typedef float Mtx[3][4];
typedef float Mtx44[4][4];
typedef struct { float x,y,z; } guVector;
typedef struct { float x,y; } indigoPoint_t;
typedef struct { u8 r,g,b,a; } GXColor;
typedef struct { void *image; u16 width,height; } GXTexObj;
typedef struct { int unused; } uiSceneFrame_t;
typedef struct { bool available; } uiClockFrame_t;
typedef struct { bool available; } indigoPadFrame_t;
enum { UI_HOME_FACE_COUNT=5, UI_HOME_ICON_CHOICES=4 };
typedef struct { Mtx model,semanticFaces[UI_HOME_FACE_COUNT];
    float motifAlpha[UI_HOME_FACE_COUNT],scaleX,scaleY; } cubeRasterTransform_t;
#define ATTRIBUTE_ALIGN(n)
#define CUBE_CAMERA_Z -5.4f
#define CHECK(c,m) do { if(!(c)) { fprintf(stderr,"%s\n",m); exit(73); } } while(0)
enum { GX_PERSPECTIVE=0,GX_ORTHOGRAPHIC=1,GX_TF_RGB565=4,GX_CLAMP=0,GX_FALSE=0,GX_TRUE=1,
    GX_LINEAR=1,GX_ANISO_1=0,GX_PNMTX0=0,GX_ENABLE=1,GX_LEQUAL=3,GX_CULL_NONE=0,GX_CULL_BACK=2,
    GX_BM_BLEND=1,GX_BL_SRCALPHA=4,GX_BL_INVSRCALPHA=5,GX_BL_ONE=1,GX_LO_CLEAR=0,GX_DIRECT=1,
    GX_VA_POS=9,GX_VA_CLR0=11,GX_VA_TEX0=13,GX_VTXFMT0=0,GX_TEX_ST=1,GX_F32=4,GX_TEXCOORD0=0,
    GX_TG_MTX2x4=1,GX_TG_TEX0=4,GX_IDENTITY=60,GX_TEVSTAGE0=0,GX_TEXMAP3=3,GX_COLOR0A0=4,
    GX_CC_ZERO=15,GX_CC_TEXC=8,GX_CA_ZERO=7,GX_CA_RASA=5,GX_QUADS=1 };
static Mtx44 cubeProjection;
static u16 glassEfbWidth=640, glassEfbHeight=480;
static bool wide;
static void UIStage_Project(float p[4][4]) { if(wide) for(int c=0;c<4;c++) p[0][c]*=.75f; }
static float UIStage_FrameX(float x) { return wide ? 320+(x-320)*.75f : x; }
static void guMtxIdentity(Mtx m) { memset(m,0,sizeof(Mtx)); m[0][0]=m[1][1]=m[2][2]=1; }
static void guOrtho(Mtx44 m,float t,float b,float l,float r,float n,float f) {
    (void)n;(void)f; CHECK(t==0 && l==0 && b==glassEfbHeight && r==glassEfbWidth,
        "pictures not laid in frame pixels"); memset(m,0,sizeof(Mtx44));
}
static void DCFlushRange(void *p,u32 n) { (void)p;(void)n; }
/* The pose: what setupCubePipeline would build. */
static cubeRasterTransform_t pose;
static int poseCalls, restored;
static void setupCubePipeline(const uiSceneFrame_t *s,float t,bool a,cubeRasterTransform_t *r) {
    (void)s;(void)t;(void)a; *r=pose; poseCalls++;
}
static void loadCubeProjection(void) { restored|=1; }
static void restoreCubeRaster(void) { restored|=2; }
static Mtx44 loaded; static int loadedType, scissor[4], copies, clears, syncs, invalidated;
static u16 copySrc[2], copyDst[2]; static int copyFilter;
static void GX_LoadProjectionMtx(Mtx44 m,int type) { memcpy(loaded,m,sizeof(Mtx44)); loadedType=type; }
static void GX_SetScissor(u32 x,u32 y,u32 w,u32 h) { scissor[0]=x; scissor[1]=y; scissor[2]=w; scissor[3]=h; }
static void GX_SetTexCopySrc(u16 x,u16 y,u16 w,u16 h) {
    CHECK(x==0 && y==0,"picture copied from off the corner"); copySrc[0]=w; copySrc[1]=h;
}
static void GX_SetTexCopyDst(u16 w,u16 h,int format,int filter) {
    CHECK(format==GX_TF_RGB565,"picture format"); copyDst[0]=w; copyDst[1]=h; copyFilter=filter;
}
static void GX_CopyTex(void *dest,int clear) { (void)dest; copies++; clears+=clear; }
static void GX_PixModeSync(void) { syncs++; }
static void GX_InvalidateTexAll(void) { invalidated++; }
static void GX_InitTexObj(GXTexObj *o,void *image,u16 w,u16 h,int format,int ws,int wt,int mip) {
    CHECK(format==GX_TF_RGB565 && ws==GX_CLAMP && wt==GX_CLAMP && !mip,"picture texture");
    o->image=image; o->width=w; o->height=h;
}
static void GX_InitTexObjLOD(GXTexObj *o,int a,int b,float c,float d,float e,int f,int g,int h) {
    (void)o;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h;
}
static GXTexObj *bound;
static void GX_LoadTexObj(GXTexObj *o,int map) { CHECK(map==GX_TEXMAP3,"picture map"); bound=o; }
static Mtx posMtx;
static void GX_LoadPosMtxImm(const float m[3][4],int slot) { (void)slot; memcpy(posMtx,m,sizeof(Mtx)); }
static void GX_ClearVtxDesc(void) {}
static void GX_SetVtxDesc(int a,int t) { (void)a;(void)t; }
static void GX_SetVtxAttrFmt(int a,int b,int c,int d,int e) { (void)a;(void)b;(void)c;(void)d;(void)e; }
static void GX_SetNumTexGens(int n) { (void)n; }
static void GX_SetTexCoordGen(int a,int b,int c,int d) { (void)a;(void)b;(void)c;(void)d; }
static void GX_SetNumTevStages(int n) { (void)n; }
static void GX_SetTevOrder(int a,int b,int c,int d) { (void)a;(void)b;(void)c;(void)d; }
static void GX_SetTevColorIn(int s,int a,int b,int c,int d) { (void)s;(void)a;(void)b;(void)c;(void)d; }
static void GX_SetTevAlphaIn(int s,int a,int b,int c,int d) { (void)s;(void)a;(void)b;(void)c;(void)d; }
static void GX_SetZMode(int a,int b,int c) { (void)a;(void)b;(void)c; }
static int cull, blendDest;
static void GX_SetCullMode(int m) { cull=m; }
static void GX_SetBlendMode(int m,int s,int d,int o) { (void)m;(void)s;(void)o; blendDest=d; }
static float qx[64],qy[64],qs[64],qt[64]; static u8 qa[64]; static int qn,qphase; static GXTexObj *qtex[64];
static void GX_Begin(int p,int f,int n) { CHECK(p==GX_QUADS && f==GX_VTXFMT0 && n==4,"picture quad"); }
static void GX_Position3f32(float x,float y,float z) { (void)z; qx[qn]=x; qy[qn]=y; qphase=1; }
static void GX_Color4u8(u8 r,u8 g,u8 b,u8 a) { CHECK(qphase==1 && r==255 && g==255 && b==255,"picture tint"); qa[qn]=a; qphase=2; }
static void GX_TexCoord2f32(float s,float t) {
    CHECK(qphase==2 && blendDest==GX_BL_ONE,"picture not added light"); qs[qn]=s; qt[qn]=t; qtex[qn]=bound; qn++; qphase=0;
}
static void GX_End(void) {}
/* What the icon pass is asked to draw, at the size it draws it. */
static int drawn[8], drawnCount; static float drawnScale[8], drawnScaleY[8], drawnAlpha[8];
static void drawOneFaceIcon(const cubeRasterTransform_t *r,int face,int choice,float seconds,
    bool animated,const uiClockFrame_t *clock,const indigoPadFrame_t *pad) {
    (void)choice;(void)seconds;(void)animated;(void)clock;(void)pad;
    CHECK(loadedType==GX_PERSPECTIVE,"picture drawn without the doubled projection");
    drawn[drawnCount]=face; drawnScale[drawnCount]=r->scaleX/pose.scaleX;
    drawnScaleY[drawnCount]=r->scaleY/pose.scaleY;
    drawnAlpha[drawnCount]=r->motifAlpha[face]; drawnCount++;
}
/* EMITTERS */
static void frameOf(Mtx44 p,guVector e,float *x,float *y) {
    float cx=p[0][0]*e.x+p[0][1]*e.y+p[0][2]*e.z+p[0][3];
    float cy=p[1][0]*e.x+p[1][1]*e.y+p[1][2]*e.z+p[1][3];
    float w=p[3][0]*e.x+p[3][1]*e.y+p[3][2]*e.z+p[3][3];
    *x=(1+cx/w)*glassEfbWidth/2; *y=(1-cy/w)*glassEfbHeight/2;
}
static void testProjection(void) {
    for(int shape=0;shape<4;shape++) {
        wide=shape&1; glassEfbHeight=shape&2?528:480;
        Mtx44 base,fine;
        for(int r=0;r<4;r++) for(int c=0;c<4;c++) base[r][c]=cubeProjection[r][c];
        UIStage_Project(base);
        facePictureProjection(fine,118,64);
        for(int i=0;i<50;i++) {
            guVector e={sinf(i*1.7f)*1.4f,cosf(i*2.3f)*1.1f,-4.2f-(i%7)*.2f};
            float fx,fy,bx,by; frameOf(base,e,&fx,&fy); frameOf(fine,e,&bx,&by);
            CHECK(fabsf(bx-4*(fx-118))<.02f && fabsf(by-2*(fy-64))<.01f,
                "4x2 projection does not put the picture at the corner");
        }
    }
    wide=false; glassEfbHeight=480;
}
static void face(int f,float yaw) {
    float c=cosf(yaw),s=sinf(yaw);
    memset(pose.semanticFaces[f],0,sizeof(Mtx));
    pose.semanticFaces[f][0][0]=c; pose.semanticFaces[f][0][2]=s;
    pose.semanticFaces[f][1][1]=1; pose.semanticFaces[f][2][0]=-s; pose.semanticFaces[f][2][2]=c;
}
static void setPose(float yaw) {
    float c=cosf(yaw),s=sinf(yaw);
    guMtxIdentity(pose.model);
    pose.model[0][0]=c*.92f; pose.model[0][2]=s*.92f; pose.model[2][0]=-s*.92f; pose.model[2][2]=c*.92f;
    pose.model[2][3]=CUBE_CAMERA_Z;
}
static void testHome(void) {
    static const int icons[UI_HOME_FACE_COUNT]={0,0,0,0,0};
    uiSceneFrame_t scene={0}; uiClockFrame_t clock={true}; indigoPadFrame_t pad={true};
    /* Library square on, Source to its right, Settings behind, System to its left. */
    for(int f=0;f<4;f++) face(f,f*-1.5707963f);
    face(4,3.14159265f);
    for(int f=0;f<UI_HOME_FACE_COUNT;f++) pose.motifAlpha[f]=f==4?0:1;
    pose.scaleX=pose.scaleY=625.221f;
    for(int step=0;step<=40;step++) {
        float yaw=-0.28f-step*0.04f;
        setPose(yaw);
        copies=clears=syncs=invalidated=drawnCount=qn=restored=0;
        renderFacePictures(&scene,1,true,&clock,&pad,icons);
        CHECK(scissor[0]==0 && scissor[1]==0 && scissor[2]==640 && scissor[3]==480,
            "scissor left on the picture corner");
        int expected=0;
        for(int f=0;f<UI_HOME_FACE_COUNT;f++) {
            float facing=faceFacing(&pose,f);
            float weight=pose.motifAlpha[f]*(1-faceStrokeShare(facing));
            bool wanted=facing>0 && weight>=1.0f/255;
            bool made=false;
            for(int i=0;i<facePictureCount;i++) if(drawn[i]==f) made=true;
            CHECK(made==wanted,"a face turned away got no picture, or a square one did");
            expected+=wanted;
        }
        CHECK(facePictureCount==expected && drawnCount==expected,"picture count");
        CHECK(copies==expected && clears==copies,"each picture is one copy, which clears its corner");
        if(expected) CHECK(copySrc[0]==4*facePictures[expected-1].width &&
            copySrc[1]==2*facePictures[expected-1].height &&
            copyDst[0]==2*facePictures[expected-1].width && copyDst[1]==facePictures[expected-1].height &&
            copyFilter,"picture copy is not a 2x2 box of a 4x2 render");
        CHECK(expected==0 || (syncs==1 && invalidated==1),"pictures read before the copies land");
        for(int i=0;i<facePictureCount;i++) {
            const facePicture_t *p=&facePictures[i];
            CHECK(drawnScale[i]==4 && drawnScaleY[i]==2 && drawnAlpha[i]==1,
                "picture not drawn four times across, twice down, at full light");
            CHECK(p->texture.width==2*p->width && p->texture.height==p->height,
                "picture texture is not two texels to a pixel across");
            CHECK(p->width%4==0 && p->height%4==0 && p->width<=FACE_PICTURE_MAX_W &&
                p->height<=FACE_PICTURE_MAX_H,"picture slot size");
            CHECK(p->x+p->width<=640 && p->y+p->height<=480,"picture off the frame");
            float facing=faceFacing(&pose,drawn[i]);
            CHECK(fabsf(p->weight-(1-faceStrokeShare(facing)))<1e-5f,
                "picture and strokes do not add up to the whole icon");
            /* The icon's square lies inside its picture. */
            for(int corner=0;corner<4;corner++) {
                guVector b=semanticFacePoint(&pose,drawn[i],corner&1?.74f:-.74f,corner&2?.74f:-.74f,1.012f);
                guVector e; indigoPoint_t at;
                CHECK(projectRailPoint(&pose,b.x,b.y,b.z,&e,&at),"icon behind the camera");
                float x=320+at.x,y=240-at.y;
                CHECK(x>p->x && x<p->x+p->width && y>p->y && y<p->y+p->height,"icon spills off its picture");
            }
        }
        drawFacePictures(&pose);
        CHECK(qn==4*expected,"one quad per picture");
        CHECK(expected==0 || (restored==3 && cull==GX_CULL_BACK && blendDest==GX_BL_INVSRCALPHA &&
            !memcmp(posMtx,pose.model,sizeof(Mtx))),"cube pipeline not restored after the pictures");
        for(int v=0;v<qn;v++) {
            const facePicture_t *p=&facePictures[v/4];
            CHECK(qtex[v]==&p->texture,"quad shows another picture");
            CHECK(qx[v]==p->x+((v%4==1||v%4==2)?p->width:0) && qy[v]==p->y+(v%4>=2?p->height:0),
                "quad off its picture");
            CHECK(qs[v]==((v%4==1||v%4==2)?1:0) && qt[v]==(v%4>=2?1:0),"picture coordinates");
            CHECK(abs(qa[v]-(int)(255*p->weight+.5f))<=1,"picture weight");
        }
    }
    /* Home at rest: the side face shows its icon. */
    setPose(-0.28f); renderFacePictures(&scene,1,true,&clock,&pad,icons);
    CHECK(facePictureCount==1,"the side face at rest has no picture");
}
int main(void) {
    float cot=1/tanf(21*3.14159265f/180);
    memset(cubeProjection,0,sizeof(Mtx44));
    cubeProjection[0][0]=cot*.75f; cubeProjection[1][1]=cot;
    cubeProjection[2][2]=-.00125f; cubeProjection[2][3]=-.1f; cubeProjection[3][2]=-1;
    testProjection(); testHome();
    puts("face pictures: doubled at the corner, every turned face, cleared copies, restored state");
    return 0;
}
"""


class FacePictureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = SOURCE.read_text()
        blocks = [extract_function((SOURCE.parent / "ui_motion.c").read_text(),
                                   "float UIMotion_Smoothstep(")]
        blocks += [extract_function(source, s) for s in (
            "static float fastSqrt(",
            "static bool projectRailPoint(", "static guVector semanticFacePoint(",
            "static float faceFacing(", "static float faceStrokeShare(")]
        blocks += re.findall(r"^#define FACE_PICTURE_\w+ .*$", source, re.M)
        blocks.append(re.search(r"typedef struct facePicture \{.*?\} facePicture_t;",
                                source, re.S).group(0))
        blocks += re.findall(r"^static (?:u8 facePictureTexels|facePicture_t facePictures|"
                             r"int facePictureCount|bool facePictureTexelsFlushed)\b.*?;$",
                             source, re.M | re.S)
        blocks += [extract_function(source, s) for s in (
            "static void facePictureProjection(", "static void renderFacePictures(",
            "static void drawFacePictures(")]
        cls.emitters = "\n".join(blocks)

    def run_harness(self, emitters):
        with tempfile.TemporaryDirectory(prefix="swiss-face-pictures-") as directory:
            root = Path(directory)
            source, binary = root / "pictures.c", root / "pictures"
            source.write_text(HARNESS.replace("/* EMITTERS */", emitters))
            result = subprocess.run(shlex.split(os.environ.get("CC", "cc")) +
                ["-std=c99", "-Wall", "-Wextra", "-Werror", str(source), "-o", str(binary), "-lm"],
                capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            return subprocess.run([str(binary)], capture_output=True, text=True, timeout=10)

    def test_pictures(self):
        result = self.run_harness(self.emitters)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_regressions_are_rejected(self):
        mutants = {
            "projection not scaled across": ("projection[0][column] = 4.0f * projection[0][column]",
                "projection[0][column] = 2.0f * projection[0][column]"),
            "picture off by a pixel": ("int x0 = (int)floorf(left) - 1,", "int x0 = (int)floorf(left) + 2,"),
            "corner left for the next picture": ("GX_CopyTex(facePictureTexels[facePictureCount], GX_TRUE);",
                "GX_CopyTex(facePictureTexels[facePictureCount], GX_FALSE);"),
            "scissor left on": ("\tGX_SetScissor(0, 0, glassEfbWidth, glassEfbHeight);\n", "\n"),
            "strokes and picture both full": ("float weight = raster.motifAlpha[face] * (1.0f - "
                "faceStrokeShare(facing));", "float weight = raster.motifAlpha[face];"),
            "drawn at size": ("fine.scaleX *= 4.0f;", "fine.scaleX *= 1.0f;"),
            "texture one texel to a pixel": ("(u16)(width * 2), (u16)height, GX_TF_RGB565",
                "(u16)width, (u16)height, GX_TF_RGB565"),
            "no sync before reading": ("\t\tGX_PixModeSync();\n", "\t\tif(0) GX_PixModeSync();\n"),
            "pipeline not restored": ("\trestoreCubeRaster();\n\tGX_LoadPosMtxImm(raster->model",
                "\tif(0) restoreCubeRaster();\n\tGX_LoadPosMtxImm(raster->model"),
        }
        for name, (old, new) in mutants.items():
            with self.subTest(name=name):
                self.assertIn(old, self.emitters)
                result = self.run_harness(self.emitters.replace(old, new, 1))
                self.assertNotEqual(result.returncode, 0, "mutant survived: " + name)


if __name__ == "__main__":
    unittest.main()
