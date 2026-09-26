#!/usr/bin/env python3
"""The cube's light: refraction, dispersion, bloom, rim, halo and sun flare.

Compiles the real emitters from indigo_background.c against a checked GX
stub and proves what the glass does with light: the frame behind the front
glass is bent toward the cube's middle and parted into red, green and blue in
that order, the bent layer fades out at the silhouette, the sun glint is found
only where the glass mirrors the key light toward the camera, the soft glows
and rim draw closed bounded streams, and the frame copies keep their contract
(half size, RGBA8, box filter, never clearing the EFB, cache written back
once before the GPU first writes the buffer). A source check pins where the
passes sit in drawCube, and mutants prove each property is really tested.
"""
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

HARNESS = r"""
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui_scene.h"
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef struct { u8 r,g,b,a; } GXColor;
typedef float Mtx[3][4];
typedef struct { float x,y,z; } guVector;
typedef struct { float x,y; } indigoPoint_t;
typedef struct { Mtx model,semanticFaces[4]; float motifAlpha,scaleX,scaleY; } cubeRasterTransform_t;
typedef struct { guVector point[4]; GXColor color[4]; } cubeSurfaceQuad_t;
typedef struct { indigoPoint_t point[24]; int count; } cubeOutline_t;
typedef struct { void *data; u16 w, h; u8 fmt; } GXTexObj;
#define CHECK(c,m) do { if(!(c)) { fprintf(stderr,"%s\n",m); exit(73); } } while(0)
#define INDIGO_TAU 6.28318530718f
#define RADIAL_SEGMENTS 24
#define CUBE_CAMERA_Z -5.4f
#define BOOT_CUBE_HANDOFF 0.625f
#define ATTRIBUTE_ALIGN(v) __attribute__((aligned(v)))
enum { GX_QUADS=1, GX_TRIANGLESTRIP=2, GX_TRIANGLES=3, GX_VTXFMT0=0,
    GX_BM_BLEND=1, GX_BL_SRCALPHA=2, GX_BL_ONE=3, GX_BL_INVSRCALPHA=4, GX_LO_CLEAR=0,
    GX_TF_RGBA8=6, GX_CLAMP=0, GX_LINEAR=1, GX_ANISO_1=0, GX_TRUE=1, GX_FALSE=0,
    GX_TEXMAP0=0 };
/* Menu Color is Indigo here: the emitters' recolor passes colors through. */
static void UIColor_Apply(u8 *r,u8 *g,u8 *b) { (void)r; (void)g; (void)b; }
static bool active; static int phase,remaining,count,begins,uvs,uvCalls,blendDst;
static guVector positions[8192];
static GXColor colors[8192];
static float uv[8192][3][2];
static int primitives[1024], primitiveSizes[1024];
static void reset(int texcoords) {
    CHECK(!active,"previous primitive unfinished");
    phase=remaining=count=begins=0; uvs=texcoords; blendDst=GX_BL_INVSRCALPHA;
}
static void GX_SetBlendMode(int mode,int source,int dest,int op) {
    CHECK(mode==GX_BM_BLEND && source==GX_BL_SRCALPHA && op==GX_LO_CLEAR &&
        (dest==GX_BL_ONE || dest==GX_BL_INVSRCALPHA),"glass blend policy");
    blendDst=dest;
}
static void GX_Begin(int primitive,int format,int vertices) {
    CHECK(!active && phase==0 && vertices>0 && format==GX_VTXFMT0,"bad begin");
    CHECK(primitive==GX_QUADS || primitive==GX_TRIANGLES || primitive==GX_TRIANGLESTRIP,
        "unexpected primitive");
    CHECK(begins<1024,"unbounded primitives");
    primitives[begins]=primitive; primitiveSizes[begins]=vertices;
    active=true; remaining=vertices; begins++;
}
static void GX_Position3f32(float x,float y,float z) {
    CHECK(active && phase==0 && remaining>0 && count<8192,"position order/budget");
    CHECK(isfinite(x)&&isfinite(y)&&isfinite(z),"nonfinite position");
    positions[count]=(guVector){x,y,z}; phase=1;
}
static void GX_Color4u8(u8 r,u8 g,u8 b,u8 a) {
    CHECK(active && phase==1,"color order"); colors[count]=(GXColor){r,g,b,a};
    uvCalls=0;
    if(uvs==0) { phase=0; remaining--; count++; } else phase=2;
}
static void GX_TexCoord2f32(float s,float t) {
    CHECK(active && phase==2 && uvCalls<uvs,"texture coordinate order");
    CHECK(isfinite(s)&&isfinite(t),"nonfinite texture coordinate");
    uv[count][uvCalls][0]=s; uv[count][uvCalls][1]=t;
    if(++uvCalls==uvs) { phase=0; remaining--; count++; }
}
static void GX_End(void) { CHECK(active && phase==0 && remaining==0,"incomplete primitive"); active=false; }
/* Frame copies. */
static int copies, flushes, copyL, copyT, copyW, copyH, dstW, dstH, dstFmt, dstMip, clearFlag;
static void DCFlushRange(void *p,u32 n) { (void)p; CHECK(n>0,"empty flush"); flushes++; }
static void GX_SetTexCopySrc(u16 l,u16 t,u16 w,u16 h) { copyL=l; copyT=t; copyW=w; copyH=h; }
static void GX_SetTexCopyDst(u16 w,u16 h,u32 f,u8 m) { dstW=w; dstH=h; dstFmt=(int)f; dstMip=m; }
static void GX_CopyTex(void *d,u8 clear) { CHECK(d!=NULL,"copy target"); CHECK(flushes==1,"copy before its cache flush"); clearFlag=clear; copies++; }
static void GX_PixModeSync(void) {}
static void GX_InitTexObj(GXTexObj *o,void *d,u16 w,u16 h,u8 f,u8 a,u8 b,u8 m) { (void)a;(void)b;(void)m; o->data=d; o->w=w; o->h=h; o->fmt=f; }
static void GX_InitTexObjLOD(GXTexObj *o,u8 a,u8 b,float c,float d,float e,u8 f,u8 g,u8 h) { (void)o;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h; }
static void GX_InvalidateTexAll(void) {}
static void GX_LoadTexObj(const GXTexObj *o,u8 map) { CHECK(o->data!=NULL && map==GX_TEXMAP0,"texture load"); }
static void putCubeVertex(float x,float y,float z,GXColor c) {
    UIColor_Apply(&c.r,&c.g,&c.b); GX_Position3f32(x,y,z); GX_Color4u8(c.r,c.g,c.b,c.a);
}
static void putVertex(indigoPoint_t p, GXColor c) {
    UIColor_Apply(&c.r,&c.g,&c.b); GX_Position3f32(p.x,p.y,0); GX_Color4u8(c.r,c.g,c.b,c.a);
    GX_TexCoord2f32(0,0);
}
static void guMtxIdentity(Mtx m) { memset(m,0,sizeof(Mtx)); m[0][0]=m[1][1]=m[2][2]=1; }
/* EMITTERS */
static void pose(cubeRasterTransform_t *r,float yaw,float pitch,float roll,float scale) {
    float cy=cosf(yaw),sy=sinf(yaw),cp=cosf(pitch),sp=sinf(pitch),cr=cosf(roll),sr=sinf(roll);
    float R[3][3]={{cy*cr+sy*sp*sr,-cy*sr+sy*sp*cr,sy*cp},{cp*sr,cp*cr,-sp},
        {-sy*cr+cy*sp*sr,sy*sr+cy*sp*cr,cy*cp}};
    memset(r,0,sizeof(*r));
    for(int i=0;i<3;i++) { for(int j=0;j<3;j++) r->model[i][j]=R[i][j]*scale; }
    r->model[2][3]=CUBE_CAMERA_Z;
    r->scaleX=r->scaleY=625.2f;
}
static const glassRefraction_t glass0={320,240,36,0.045f,0.17f,1.0f/640,1.0f/480,{128,125,134,255}};
static bool near(float a,float b,float e) { return fabsf(a-b)<=e; }
static void test_vertex_optics(void) {
    cubeRasterTransform_t r; glassSun_t sun={0,0,0,0}; glassRefractedVertex_t out;
    guVector eye={0,0,-4.4f};
    pose(&r,0,0,0,1);
    /* Face-on at the centre: no lateral bend, all channels together, fully
     * opaque, and the frame sampled exactly where it was. */
    refractGlassVertex(&r,&glass0,(guVector){0,0,1},eye,(guVector){0,0,1},&sun,&out);
    CHECK(out.color.a==255,"face-on glass is not the refracting layer");
    /* The tint's alpha is the layer's opacity: the boot fades it in. */
    glassRefraction_t half=glass0; half.tint.a=128;
    refractGlassVertex(&r,&half,(guVector){0,0,1},eye,(guVector){0,0,1},&sun,&out);
    CHECK(out.color.a==128,"the refracting layer ignores its opacity");
    for(int c=0;c<3;c++) CHECK(near(out.s[c]*640,320,.01f) && near(out.t[c]*480,240,.01f),
        "centre of the face moved");
    /* Off-centre on the face: magnified toward the centre by the lens. */
    guVector off={1.1f,-0.6f,-4.4f};
    refractGlassVertex(&r,&glass0,(guVector){0,0,1},off,(guVector){0,0,1},&sun,&out);
    float sx=320+625.2f*1.1f/4.4f, sy=240+625.2f*0.6f/4.4f;
    CHECK(near(out.s[1]*640,320+(sx-320)*(1-0.045f),.05f) &&
        near(out.t[1]*480,240+(sy-240)*(1-0.045f),.05f),"face is not magnified about the centre");
    CHECK(near(out.s[0],out.s[2],1e-6f),"flat face parted colours without a bend");
    /* A bevel turned right bends the frame toward the middle (left), and
     * blue bends furthest, red least: a prism's order. */
    guVector bevel={0.7071f,0,0.7071f};
    refractGlassVertex(&r,&glass0,(guVector){1,0,0.9f},(guVector){0.8f,0,-4.6f},bevel,&sun,&out);
    float base=320+625.2f*0.8f/4.6f; base=320+(base-320)*(1-0.045f);
    CHECK(out.s[0]*640<base && out.s[1]*640<out.s[0]*640 && out.s[2]*640<out.s[1]*640,
        "dispersion lost its red-green-blue order or its direction");
    CHECK(near((base-out.s[1]*640),36*0.7071f,.05f),"bend does not follow the normal");
    /* A bevel turned up bends the frame down (screen y grows). */
    refractGlassVertex(&r,&glass0,(guVector){0,1,0.9f},(guVector){0,0.8f,-4.6f},
        (guVector){0,0.7071f,0.7071f},&sun,&out);
    CHECK(out.t[1]*480>240-(625.2f*0.8f/4.6f)*(1-0.045f),"upward bevel bent the wrong way");
    /* Grazing glass fades: at the silhouette the bent image meets the straight one. */
    refractGlassVertex(&r,&glass0,(guVector){1,0,0},(guVector){0,0,-5},(guVector){1,0,0},&sun,&out);
    CHECK(out.color.a==0,"silhouette glass not faded");
    refractGlassVertex(&r,&glass0,(guVector){1,0,0},(guVector){0,0,-5},(guVector){0.9982f,0,0.06f},&sun,&out);
    int nearEdge=out.color.a;
    refractGlassVertex(&r,&glass0,(guVector){1,0,0},(guVector){0,0,-5},(guVector){0.985f,0,0.1736f},&sun,&out);
    CHECK(nearEdge>0 && nearEdge<60 && out.color.a>nearEdge && out.color.a<255,
        "glancing fade is not gradual");
}
static void test_sun(void) {
    cubeRasterTransform_t r; glassRefractedVertex_t out;
    pose(&r,0,0,0,1);
    /* A normal halfway between the view and the key light mirrors the sun. */
    guVector v={0,0,1}, h={glassSunDirection.x+v.x,glassSunDirection.y+v.y,glassSunDirection.z+v.z};
    float l=sqrtf(h.x*h.x+h.y*h.y+h.z*h.z); h=(guVector){h.x/l,h.y/l,h.z/l};
    glassSun_t sun={0,0,0,0};
    refractGlassVertex(&r,&glass0,(guVector){0,0,1},(guVector){0,0,-4.4f},h,&sun,&out);
    CHECK(sun.peak>0.99f && sun.weight>0.9f,"the mirrored sun was not found");
    CHECK(near(sun.x/sun.weight,320,.01f) && near(sun.y/sun.weight,240,.01f),"glint placed wrong");
    /* Face-on glass and glass turned away never glint. */
    glassSun_t none={0,0,0,0};
    refractGlassVertex(&r,&glass0,(guVector){0,0,1},(guVector){0,0,-4.4f},(guVector){0,0,1},&none,&out);
    refractGlassVertex(&r,&glass0,(guVector){0,0,1},(guVector){0,0,-4.4f},(guVector){0,0,-1},&none,&out);
    CHECK(none.peak==0 && none.weight==0,"glint without the sun's reflection");
}
static void test_refraction_stream(void) {
    const GXColor tint[6]={{1,2,3,4},{1,2,3,4},{1,2,3,4},{1,2,3,4},{1,2,3,4},{1,2,3,4}};
    cubeSurfaceQuad_t shell[6],strips[12],corners[8];
    buildCubeFaces(shell,1,.78f,tint); buildChamferStrips(strips,1,.78f,tint); buildCubeCorners(corners,1,.78f);
    int most=0,poses=0;
    for(int yaw=0;yaw<360;yaw+=30) for(int pitch=0;pitch<360;pitch+=30)
    for(int roll=0;roll<180;roll+=45) for(int size=0;size<2;size++) {
        cubeRasterTransform_t r; glassSun_t sun={0,0,0,0},quiet={0,0,0,0};
        pose(&r,yaw*INDIGO_TAU/360,pitch*INDIGO_TAU/360,roll*INDIGO_TAU/360,size?1.3f:.65f);
        reset(3);
        drawGlassRefraction(&r,&glass0,shell,6,4,1,&sun,true);
        drawGlassRefraction(&r,&glass0,strips,12,4,1,&sun,true);
        drawGlassRefraction(&r,&glass0,corners,8,3,1,&sun,true);
        CHECK(!active,"refraction left a primitive open");
        int total=count;
        for(int i=0;i<count;i++) {
            guVector q=positions[i];
            float m=fmaxf(fabsf(q.x),fmaxf(fabsf(q.y),fabsf(q.z)));
            CHECK(m<=1.0001f && m>=.78f-.0001f,"refraction left the outer glass");
        }
        /* Every cell faces the camera, as GX's culling requires. */
        int at=0;
        for(int p=0;p<begins;p++) {
            int sides=primitives[p]==GX_TRIANGLES?3:4;
            for(int first=at;first<at+primitiveSizes[p];first+=sides) {
                float area=0; indigoPoint_t s[4]; guVector e;
                for(int v=0;v<sides;v++) CHECK(projectRailPoint(&r,positions[first+v].x,
                    positions[first+v].y,positions[first+v].z,&e,&s[v]),"cell left the view");
                for(int v=0;v<sides;v++) area+=s[v].x*s[(v+1)%sides].y-s[(v+1)%sides].x*s[v].y;
                CHECK(area<0.0f,"refraction cell faces away and would be culled");
            }
            at+=primitiveSizes[p];
        }
        /* Measuring alone emits nothing and finds the same sun. */
        reset(3);
        drawGlassRefraction(&r,&glass0,shell,6,4,1,&quiet,false);
        drawGlassRefraction(&r,&glass0,strips,12,4,1,&quiet,false);
        drawGlassRefraction(&r,&glass0,corners,8,3,1,&quiet,false);
        CHECK(count==0 && begins==0,"a measuring walk drew");
        CHECK(near(quiet.peak,sun.peak,1e-6f) && near(quiet.weight,sun.weight,1e-3f),
            "the measuring walk found another sun");
        if(total>most) most=total;
        poses++;
    }
    CHECK(poses==1152,"pose sweep incomplete");
    /* Measured 700 at the worst pose; the video thread pays for each. */
    CHECK(most>300 && most<=900,"refraction vertex budget");
}
static void test_soft_glow(void) {
    reset(1);
    drawSoftGlow(100,80,40,20,(GXColor){10,20,30,255},0.5f);
    CHECK(begins==3 && count==3*(RADIAL_SEGMENTS+1)*2,"glow is not three closed rings");
    static const float ring[4]={0,.28f,.58f,1}, fall[4]={1,.74f,.30f,0};
    for(int band=0;band<3;band++) {
        int first=band*(RADIAL_SEGMENTS+1)*2;
        CHECK(near(positions[first].x,positions[first+RADIAL_SEGMENTS*2].x,.01f) &&
            near(positions[first].y,positions[first+RADIAL_SEGMENTS*2].y,.01f),"ring not closed");
        for(int i=0;i<=RADIAL_SEGMENTS;i++) {
            guVector a=positions[first+i*2],b=positions[first+i*2+1];
            float ra=hypotf((a.x-100)/40,(a.y-80)/20), rb=hypotf((b.x-100)/40,(b.y-80)/20);
            CHECK(near(ra,ring[band],.002f) && near(rb,ring[band+1],.002f),"ring radius");
            CHECK(abs(colors[first+i*2].a-(int)(127.5f*fall[band]+.5f))<=1 &&
                abs(colors[first+i*2+1].a-(int)(127.5f*fall[band+1]+.5f))<=1,"gaussian profile");
        }
    }
    CHECK(colors[3*(RADIAL_SEGMENTS+1)*2-1].a==0,"glow has a hard edge");
    reset(1); drawSoftGlow(1,1,10,10,(GXColor){1,2,3,4},0.0f);
    CHECK(count==0,"an invisible glow drew");
}
static void test_flare(void) {
    glassSun_t none={0.005f,100,100,1};
    reset(1); drawSunFlare(&none,1,0,1); CHECK(count==0 && begins==0,"flare without a glint");
    glassSun_t sun={1,480,120,2};
    reset(1); drawSunFlare(&sun,1,0.3f,1);
    CHECK(begins>0 && count<=1400,"flare budget");
    CHECK(blendDst==GX_BL_INVSRCALPHA,"flare left additive blending on");
    for(int i=0;i<count;i++) CHECK(positions[i].x>-200 && positions[i].x<840 &&
        positions[i].y>-200 && positions[i].y<680,"flare escaped the screen");
    /* The core sits on the glint. */
    int hits=0; for(int i=0;i<count;i++) if(near(positions[i].x,240,.01f) && near(positions[i].y,60,.01f)) hits++;
    CHECK(hits>0,"flare is not centred on the glint");
    reset(1); drawSunFlare(&sun,0,0,1); CHECK(count==0,"a scene without light flared");
}
static void test_rim(void) {
    cubeOutline_t o={{{-100,-100},{100,-100},{100,100},{-100,100}},4};
    reset(1); drawGlassRim(&o,1);
    CHECK(begins==9 && count==3*3*5*2,"rim is not three feathered closed strokes");
    CHECK(blendDst==GX_BL_INVSRCALPHA,"rim left additive blending on");
    /* Outline corners in screen space: (420,340) (420,140) (220,140) (220,340).
     * The upper right faces the key light, the lower left does not. */
    int lit=0, dark=0;
    for(int i=0;i<count;i++) {
        if(colors[i].a==0) continue;
        if(positions[i].x>=410 && positions[i].y<=150) lit++;
        if(positions[i].x<=230 && positions[i].y>=330) dark++;
    }
    CHECK(lit>0 && dark==0,"rim does not follow the key light");
    reset(1); drawGlassRim(&o,0); CHECK(count==0,"a scene without light drew a rim");
}
static void test_sheen(void) {
    const GXColor tint[6]={{1,2,3,4},{1,2,3,4},{1,2,3,4},{1,2,3,4},{1,2,3,4},{1,2,3,4}};
    cubeSurfaceQuad_t shell[6],strips[12];
    cubeRasterTransform_t r;
    buildCubeFaces(shell,1,.78f,tint); buildChamferStrips(strips,1,.78f,tint);
    pose(&r,0.28f,0.09f,0,0.92f);
    reset(0); drawGlassSheen(&r,shell,6,4,1,0,0.4f,1); drawGlassSheen(&r,strips,12,4,1,0,0.4f,1);
    CHECK(!active && count>0 && count<=1200,"sheen missing or unbounded at the centre");
    for(int i=0;i<count;i++) {
        guVector q=positions[i]; float m=fmaxf(fabsf(q.x),fmaxf(fabsf(q.y),fabsf(q.z)));
        CHECK(m<=1.0001f && m>=.78f-.0001f,"sheen left the glass");
    }
    for(int i=0;i<count;i+=4) CHECK((colors[i].a|colors[i+1].a|colors[i+2].a|colors[i+3].a)!=0,
        "a dark sheen cell was drawn");
    int centre=count;
    reset(0); drawGlassSheen(&r,shell,6,4,1,3.0f,0.4f,1); drawGlassSheen(&r,strips,12,4,1,3.0f,0.4f,1);
    CHECK(count<centre/4,"the band did not leave the cube at the end of its sweep");
    reset(0); drawGlassSheen(&r,shell,6,4,1,0,0.4f,0); CHECK(count==0,"a resting sheen drew");
}
static unsigned char texels[16];
static void test_copy(void) {
    glassEfbWidth=640; glassEfbHeight=480; flushes=copies=0;
    CHECK(glassCopyFrame() && copies==1 && flushes==1,"first copy");
    CHECK(copyL==0 && copyT==0 && copyW==640 && copyH==480 && dstW==320 && dstH==240,"copy is not the frame at half size");
    CHECK(dstFmt==GX_TF_RGBA8 && dstMip==GX_TRUE && clearFlag==GX_FALSE,"copy format, filter or clear");
    CHECK(glassCopyFrame() && copies==2 && flushes==1,"the buffer was flushed again");
    CHECK(near(glassCopyS()*640,1,1e-6f) && near(glassCopyT()*480,1,1e-6f),"copy coordinates");
    glassEfbHeight=574;
    CHECK(glassCopyFrame() && dstH==284 && copyH==568,"odd heights must stay tile-aligned");
    CHECK(glassCopyT()*480>1.0f,"copy coordinates ignore the cropped rows");
    glassEfbHeight=6; CHECK(!glassCopyFrame(),"a tiny frame was copied");
    (void)texels;
}
static void test_scene_strength(void) {
    uiSceneFrame_t s; memset(&s,0,sizeof(s));
    s.introProgress=1;
    s.scene=UI_SCENE_HOME; s.orbitStrength=1.4f; CHECK(glassSceneStrength(&s)==1,"strength clamp");
    s.orbitStrength=-1; CHECK(glassSceneStrength(&s)==0,"strength clamp low");
    s.scene=UI_SCENE_SETTINGS; s.orbitStrength=0.58f; CHECK(glassSceneStrength(&s)==0,"Settings kept the screen glass");
    s.scene=UI_SCENE_LIBRARY; s.orbitStrength=0.72f; CHECK(near(glassSceneStrength(&s),.72f,1e-6f),"Library strength");
    /* The boot overlay's cube is plain glass, and the light fades in after
     * the background cube takes over at BOOT_CUBE_HANDOFF. */
    s.scene=UI_SCENE_HOME; s.orbitStrength=1;
    s.introProgress=0.5f; CHECK(glassSceneStrength(&s)==0,"the boot overlay's cube was lit");
    s.introProgress=BOOT_CUBE_HANDOFF; CHECK(glassSceneStrength(&s)==0,"the light popped in at the handoff");
    s.introProgress=(BOOT_CUBE_HANDOFF+1)/2; float half=glassSceneStrength(&s);
    CHECK(half>0.3f && half<0.7f,"the light does not fade in after the handoff");
}
int main(void) {
    test_vertex_optics(); test_sun(); test_refraction_stream(); test_soft_glow();
    test_flare(); test_rim(); test_copy(); test_scene_strength(); test_sheen();
    puts("glass light: refraction, dispersion, glint, glows, rim and copies hold");
    return 0;
}
"""

FUNCTIONS = [
    "static bool railJoin(", "static bool buildRasterJoins(", "static void drawRasterStroke(",
    "static bool projectRailPoint(", "static guVector cubeViewNormal(",
    "static void buildCubeFaces(", "static void buildChamferStrip(",
    "static void buildChamferStrips(", "static void buildCubeCorners(",
    "static float glassSmoothstep(", "static guVector glassVertexNormal(",
    "static bool glassSameNormal(", "static guVector glassBilinear(",
    "static float glassSceneStrength(", "static bool glassCopyFrame(",
    "static float glassCopyS(", "static float glassCopyT(",
    "static void refractGlassVertex(", "static void putGlassRefractedVertex(",
    "static void drawGlassRefraction(", "static void drawSoftGlow(",
    "static void drawLightRay(", "static void drawGlassRim(", "static void drawSunFlare(",
    "static bool glassCellLit(", "static void drawGlassSheen(",
]


class GlassLightTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = (GUI / "indigo_background.c").read_text()
        blocks = []
        for name in ("glassSun", "glassRefraction", "glassRefractedVertex"):
            blocks.append(re.search(r"typedef struct " + name + r" \{.*?\} \w+;",
                cls.source, re.S).group(0))
        blocks.append(re.search(r"static const guVector glassSunDirection = \{.*?\};",
            cls.source).group(0))
        blocks.append("\n".join(re.findall(r"^#define GLASS_\w+ .*$", cls.source, re.M)))
        blocks.append(re.search(r"static u8 glassTexels\[.*?;\n(?:static .*?;\n)+",
            cls.source).group(0))
        for signature in FUNCTIONS:
            text = cls.source
            if signature == "static bool railJoin(":
                text = text[text.rindex(signature):]
            blocks.append(extract_function(text, signature))
        cls.emitters = "\n".join(blocks)

    def run_emitters(self, emitters):
        with tempfile.TemporaryDirectory(prefix="swiss-glass-") as directory:
            root = Path(directory)
            (root / "glass.c").write_text(HARNESS.replace("/* EMITTERS */", emitters))
            result = subprocess.run(shlex.split(os.environ.get("CC", "cc")) +
                ["-std=c99", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
                 "-I" + str(GUI), str(root / "glass.c"), "-o", str(root / "glass"), "-lm"],
                capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            return subprocess.run([str(root / "glass")], capture_output=True, text=True,
                timeout=30)

    def test_native_glass(self):
        result = self.run_emitters(self.emitters)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_passes_sit_where_the_light_needs_them(self):
        draw = extract_function(self.source, "static void drawCube(")
        order = [draw.index(token) for token in (
            "drawCubeSurfacePass(&raster, &coreOutline, core, 6, GX_CULL_BACK, true);",
            "drawCubeSurfacePass(&raster, &shellOutline, shell, 6, GX_CULL_FRONT, false);",
            "refract = screenGlass && strength > 0.01f && glassCopyFrame();",
            "drawGlassRefraction(&raster, &glass, corners, 8, 3, outer, &sun, refract);",
            "if(refract) restoreCubeRaster();",
            "litCubeTints(&raster, frontGlassColors, lit);",
            "drawGlassReflection(&raster, corners, 8, 3, outer);",
            "drawGlassBloom(",
            "loadCubeProjection();\n\t\trestoreCubeRaster();",
            "drawFaceIcons(seconds, animated, clock, pad, icons, &raster);",
            "drawGlassRim(&shellOutline, strength);",
            "drawSunFlare(&sun, strength,")]
        self.assertEqual(order, sorted(order),
            "the glass passes left their place between the interior and the front glass")
        # The icons come after the bloom so their strokes stay sharp, on the
        # cube's own projection again (the bloom leaves an orthographic one).
        self.assertIn("if(light && screenGlass) {", draw)
        # Spencer turned the corner sun off; the switch keeps it one edit away.
        self.assertIn("if(CUBE_SUN_FLARE) {\n\t\t\tdrawSunFlare(", draw)
        self.assertRegex(self.source, r"\n#define CUBE_SUN_FLARE 0\n")
        boot = extract_function(self.source, "void IndigoBackground_DrawBootOverlay(")
        self.assertIn("drawCube(scene, seconds, animated, clock, NULL, icons, false);", boot,
            "the boot overlay must never copy the frame: widgets sit under it")
        home = extract_function(self.source, "void IndigoBackground_Draw(")
        self.assertIn("drawCube(scene, seconds, cubeMotionActive, clock, pad, icons, true);", home)
        self.assertLess(home.index("drawCubeLight("), home.index("drawCube(scene,"),
            "the halo and caustic belong behind the cube")

    def test_pipelines(self):
        def attributes(source):
            self.assertIn("GX_ClearVtxDesc();", source)
            return set(re.findall(r"GX_SetVtxDesc\((GX_VA_\w+),\s*GX_DIRECT\)", source))
        refract = extract_function(self.source, "static void setupGlassRefractionPipeline(")
        restore = extract_function(self.source, "static void restoreCubeRaster(")
        self.assertEqual(attributes(refract),
            {"GX_VA_POS", "GX_VA_CLR0", "GX_VA_TEX0", "GX_VA_TEX1", "GX_VA_TEX2"})
        self.assertIn("GX_SetNumTexGens(3);", refract)
        self.assertIn("GX_SetNumTevStages(4);", refract)
        self.assertIn("GX_CS_SCALE_2", refract)
        self.assertEqual(attributes(restore), {"GX_VA_POS", "GX_VA_CLR0"})
        self.assertIn("GX_SetNumTexGens(0);", restore)
        self.assertIn("GX_SetNumTevStages(1);", restore)
        bloom = extract_function(self.source, "static void drawGlassBloom(")
        self.assertIn("UIColor_Apply(&threshold.r, &threshold.g, &threshold.b);", bloom,
            "the bloom threshold must follow Menu Color with the glass it cuts")
        self.assertIn("GX_TEV_SUB", bloom)
        self.assertIn("GX_SetBlendMode(GX_BM_BLEND, GX_BL_INVDSTCLR, GX_BL_ONE, GX_LO_CLEAR);", bloom,
            "the bloom must screen, not add: added, a face turning through the light burns white")
        self.assertIn("glow.b = (u8)(glow.b * strength + 0.5f);", bloom,
            "a screen has no source alpha: the strength must ride in the glow colour")
        self.assertLess(bloom.index("glassCopyFrame()"), bloom.index("GX_Begin("))
        self.assertIn("setupRasterPipeline();\n}", bloom, "bloom left its TEV stages behind")
        frame = (GUI / "FrameBufferMagic.c").read_text()
        background = extract_function(frame, "static void _DrawBackground(")
        self.assertLess(background.index("IndigoBackground_SetFramebuffer("),
            background.index("IndigoBackground_Draw("))

    def test_regressions_are_rejected(self):
        mutants = {
            "no silhouette fade": ("(float)glass->tint.a * glassSmoothstep(0.0f, 0.34f, facing) + 0.5f",
                "(float)glass->tint.a + 0.5f * facing"),
            "refraction pops in at boot": ("(float)glass->tint.a * glassSmoothstep(0.0f, 0.34f, facing)",
                "255.0f * glassSmoothstep(0.0f, 0.34f, facing)"),
            "dispersion reversed": ("float k = 1.0f + glass->dispersion * (float)(channel - 1);",
                "float k = 1.0f - glass->dispersion * (float)(channel - 1);"),
            "no dispersion": ("float k = 1.0f + glass->dispersion * (float)(channel - 1);",
                "float k = 1.0f;"),
            "bend away from the middle": ("float ox = -n.x * glass->bend, oy = n.y * glass->bend;",
                "float ox = n.x * glass->bend, oy = n.y * glass->bend;"),
            "bend upside down": ("float ox = -n.x * glass->bend, oy = n.y * glass->bend;",
                "float ox = -n.x * glass->bend, oy = -n.y * glass->bend;"),
            "no magnification": ("(sx - glass->centerX) * (1.0f - glass->magnify)",
                "(sx - glass->centerX)"),
            "broad glint": ("glassSmoothstep(0.955f, 0.9985f, d)", "glassSmoothstep(0.2f, 0.9985f, d)"),
            "back-facing refraction": ("\t\tif(area >= -0.001f) continue;\n\t\tif(vertexCount == 3) {\n\t\t\tfor",
                "\t\tif(area >= 1e30f) continue;\n\t\tif(vertexCount == 3) {\n\t\t\tfor"),
            "measuring walk draws": ("\t\t\tif(!emit) continue;\n\t\t\tGX_Begin(GX_TRIANGLES",
                "\t\t\tGX_Begin(GX_TRIANGLES"),
            "copy clears the frame": ("GX_CopyTex(glassTexels, GX_FALSE);", "GX_CopyTex(glassTexels, GX_TRUE);"),
            "copy without the box filter": ("GX_SetTexCopyDst(width, height, GX_TF_RGBA8, GX_TRUE);",
                "GX_SetTexCopyDst(width, height, GX_TF_RGBA8, GX_FALSE);"),
            "no cache write-back": ("\t\tDCFlushRange(glassTexels, sizeof(glassTexels));\n", ""),
            "flush every frame": ("\t\tglassTexelsFlushed = true;\n", ""),
            "untiled height": ("(u16)((glassEfbHeight / 2u) & ~3u)", "(u16)(glassEfbHeight / 2u)"),
            "hard glow edge": ("{1.0f, 0.74f, 0.30f, 0.0f}", "{1.0f, 0.74f, 0.30f, 0.1f}"),
            "open glow ring": ("for(int i = 0; i <= RADIAL_SEGMENTS; i++) {\n\t\t\tputVertex((indigoPoint_t) {x + radiusX * ring",
                "for(int i = 0; i < RADIAL_SEGMENTS; i++) {\n\t\t\tputVertex((indigoPoint_t) {x + radiusX * ring"),
            "rim on the dark side": ("float lit = nx * lightX + ny * lightY;", "float lit = -(nx * lightX + ny * lightY);"),
            "flare stays additive": ("\t\t\tghosts[ghost].color, ghosts[ghost].alpha * intensity);\n\t}\n\tGX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);",
                "\t\t\tghosts[ghost].color, ghosts[ghost].alpha * intensity);\n\t}"),
            "sheen everywhere": ("bump = bump <= 0.0f ? 0.0f : bump * bump;", "bump = 1.0f;"),
            "sheen draws dark cells": ("\t\tif(cells == 0) continue;\n\t\tGX_Begin(GX_QUADS, GX_VTXFMT0, (u16)(cells * 4));",
                "\t\tcells = rows * columns;\n\t\tGX_Begin(GX_QUADS, GX_VTXFMT0, (u16)(cells * 4));"),
            "light pops in after boot": ("return strength * glassSmoothstep(BOOT_CUBE_HANDOFF, 1.0f, scene->introProgress);",
                "return strength;"),
            "Settings keeps screen glass": ("\t\treturn 0.0f;\n\t}\n\t/* During the boot reveal",
                "\t\treturn strength;\n\t}\n\t/* During the boot reveal"),
        }
        for name, (old, new) in mutants.items():
            with self.subTest(name=name):
                self.assertIn(old, self.emitters)
                result = self.run_emitters(self.emitters.replace(old, new, 1))
                self.assertNotEqual(result.returncode, 0, "mutant survived: " + name)


if __name__ == "__main__":
    unittest.main()
