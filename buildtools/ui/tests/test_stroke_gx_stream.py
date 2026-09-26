#!/usr/bin/env python3
"""Compile the actual native stroke emitters against a checked GX FIFO.

Exercises constant screen-space width through perspective, closed joins, alpha
coverage, attribute completeness, matrix restoration, and near-edge-on guards.
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
typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef struct { u8 r,g,b,a; } GXColor;
/* Menu Color is Indigo here: the emitters' recolor passes colors through. */
static void UIColor_Apply(u8 *r,u8 *g,u8 *b) { (void)r; (void)g; (void)b; }
typedef float Mtx[3][4];
typedef struct { float x,y,z; } guVector;
typedef struct { float x,y; } indigoPoint_t;
typedef struct { Mtx model,semanticFaces[4]; float motifAlpha,scaleX,scaleY; } cubeRasterTransform_t;
typedef struct { guVector point[4]; GXColor color[4]; } cubeSurfaceQuad_t;
typedef struct { indigoPoint_t point[24]; int count; } cubeOutline_t;
typedef struct { guVector eye[2]; indigoPoint_t outward[2]; GXColor color[2]; } cubeCoverageEdge_t;
enum { GX_QUADS=1, GX_TRIANGLESTRIP=2, GX_TRIANGLES=3, GX_TRIANGLEFAN=4, GX_VTXFMT0=0, GX_PNMTX0=0 };
/* HOME ENUMS */
enum { GX_ENABLE=1, GX_LEQUAL=2, GX_FALSE=0, GX_TRUE=1,
    GX_CULL_BACK=1, GX_CULL_NONE=0, GX_CULL_FRONT=2,
    GX_BM_BLEND=3, GX_BL_SRCALPHA=4, GX_BL_ONE=5, GX_LO_CLEAR=6, GX_BL_INVSRCALPHA=7,
    GX_BM_NONE=8, GX_BL_ZERO=9 };
typedef struct { bool available; float hourX,hourY,minuteX,minuteY,secondX,secondY; } uiClockFrame_t;
typedef struct { bool available; s8 stickX,stickY,substickX,substickY; u32 buttons; } indigoPadFrame_t;
typedef struct { float stickX,stickY,substickX,substickY; u32 pressed; } controllerPose_t;
typedef struct { float lastLiveInput; bool liveSeen; } controllerIdle_t;
enum { PAD_BUTTON_LEFT=0x0001, PAD_BUTTON_RIGHT=0x0002, PAD_BUTTON_DOWN=0x0004, PAD_BUTTON_UP=0x0008,
    PAD_TRIGGER_Z=0x0010, PAD_TRIGGER_R=0x0020, PAD_TRIGGER_L=0x0040, PAD_BUTTON_A=0x0100,
    PAD_BUTTON_B=0x0200, PAD_BUTTON_X=0x0400, PAD_BUTTON_Y=0x0800, PAD_BUTTON_START=0x1000 };
/* CONTROLLER DEFINES */
#define INDIGO_TAU 6.28318530718f
#define CUBE_CAMERA_Z -5.4f
#define CHECK(c,m) do { if(!(c)) { fprintf(stderr,"%s\n",m); exit(73); } } while(0)
static bool active, uv, surfaceTest;
static int phase, remaining, count, begins, matrixLoads, firstPrimitive;
static guVector positions[4096];
static u8 alphas[4096];
static GXColor colors[4096];
static Mtx loaded;
static int culling, depthWrites;
static void GX_SetCullMode(int mode) { culling=mode; }
static void GX_SetZMode(int enable,int comparison,int write) {
    CHECK(enable==GX_ENABLE && comparison==GX_LEQUAL &&
        (surfaceTest || write==GX_FALSE),"motif depth policy");
    depthWrites=write;
}
static void GX_SetBlendMode(int mode,int source,int destination,int operation) {
    CHECK(operation==GX_LO_CLEAR && ((mode==GX_BM_BLEND && source==GX_BL_SRCALPHA &&
        (destination==GX_BL_ONE || destination==GX_BL_INVSRCALPHA)) ||
        (surfaceTest && mode==GX_BM_NONE && source==GX_BL_ONE && destination==GX_BL_ZERO)),
        "motif blend policy");
}
static void reset(bool textured) {
    CHECK(!active,"previous primitive unfinished");
    uv=textured; surfaceTest=false; phase=remaining=count=begins=matrixLoads=firstPrimitive=0;
}
static void GX_Begin(int primitive,int format,int vertices) {
    CHECK(!active && phase==0,"nested begin or partial vertex");
    CHECK((primitive==GX_QUADS || primitive==GX_TRIANGLESTRIP || primitive==GX_TRIANGLES ||
        primitive==GX_TRIANGLEFAN) && format==0,
        "wrong primitive/format");
    CHECK(vertices>0,"empty primitive");
    if(surfaceTest && begins>0) {
        CHECK(culling==GX_CULL_NONE && depthWrites==GX_FALSE,
            "silhouette fringe inherited culling or writes depth");
    }
    if(!uv && matrixLoads>0 && (!surfaceTest || begins>0)) {
        for(int row=0;row<3;row++) for(int column=0;column<4;column++) {
            CHECK(loaded[row][column]==(row==column?1.0f:0.0f),
                "camera-space geometry inherited a model transform");
        }
    }
    if(begins==0) firstPrimitive=primitive;
    active=true; remaining=vertices; ++begins;
}
static void GX_Position3f32(float x,float y,float z) {
    CHECK(active && phase==0 && remaining>0,"missing attribute or excess vertex");
    CHECK(isfinite(x)&&isfinite(y)&&isfinite(z),"nonfinite vertex");
    CHECK(count<4096,"unbounded vertex stream");
    positions[count]=(guVector){x,y,z}; phase=1;
}
static void GX_Color4u8(u8 r,u8 g,u8 b,u8 a) {
    CHECK(active && phase==1,"color order"); alphas[count]=a;
    colors[count]=(GXColor){r,g,b,a};
    if(uv) phase=2; else { phase=0; --remaining; ++count; }
}
static void GX_TexCoord2f32(float s,float t) {
    CHECK(uv && active && phase==2,"missing or unexpected texture attribute");
    CHECK(s==0 && t==0,"raster UV"); phase=0; --remaining; ++count;
}
static void GX_End(void) {
    CHECK(active && phase==0 && remaining==0,"incomplete GX primitive"); active=false;
}
static void guMtxIdentity(Mtx m) {
    memset(m,0,sizeof(Mtx)); m[0][0]=m[1][1]=m[2][2]=1;
}
static void GX_LoadPosMtxImm(const Mtx m,int slot) {
    CHECK(!active && slot==0,"matrix changed inside primitive");
    memcpy(loaded,m,sizeof(Mtx)); ++matrixLoads;
}
/* EMITTERS */
static bool closef(float a,float b) { return fabsf(a-b)<0.002f; }
static void test_dial(void) {
    for(int segments=1;segments<=24;segments++) {
        reset(true);
        _DrawSystemRing(598,42,19,0.8f,18,segments,(GXColor){122,112,201,62});
        CHECK(count==3*(segments+1)*2 && begins==3,"dial vertex budget");
        for(int band=0;band<3;band++) for(int i=0;i<=segments;i++) {
            int a=(band*(segments+1)+i)*2,b=a+1;
            float wanted[4]={17.7f,18.7f,19.3f,20.3f};
            CHECK(closef(hypotf(positions[a].x-598,positions[a].y-42),wanted[band]) &&
                closef(hypotf(positions[b].x-598,positions[b].y-42),wanted[band+1]),
                "dial coverage/radius changed");
            CHECK((band==0 ? alphas[a]==0 : alphas[a]==62) &&
                (band==2 ? alphas[b]==0 : alphas[b]==62),"dial fringe alpha");
        }
    }
}
static void test_rail_joins(void) {
    indigoPoint_t join;
    CHECK(!railJoin((indigoPoint_t){0,0},(indigoPoint_t){0,0},(indigoPoint_t){1,1},&join),
        "zero-length join accepted");
    CHECK(!railJoin((indigoPoint_t){0,0},(indigoPoint_t){1,0},(indigoPoint_t){0,0},&join),
        "opposed join accepted");
}
/* The default Source, Settings and System icons, with None on Library. */
static const int quadIcons[4]={-1,0,0,0};
static void test_motifs(void) {
    cubeRasterTransform_t r;
    const Mtx semanticFaces[4]={
        {{1,0,0,0},{0,1,0,0},{0,0,1,0}},
        {{0,0,1,0},{0,1,0,0},{-1,0,0,0}},
        {{-1,0,0,0},{0,1,0,0},{0,0,-1,0}},
        {{0,0,-1,0},{0,1,0,0},{1,0,0,0}}
    };
    memcpy(r.semanticFaces,semanticFaces,sizeof(semanticFaces)); r.motifAlpha=1.0f;
    uiClockFrame_t clock={true,0,1,1,0,0.70710678f,0.70710678f};
    r.scaleX=r.scaleY=625.221f;
    for(int angle=0;angle<360;angle+=5) {
        float yaw=angle*INDIGO_TAU/360,c=cosf(yaw),s=sinf(yaw);
        guMtxIdentity(r.model);
        r.model[0][0]=c; r.model[0][2]=s; r.model[2][0]=-s; r.model[2][2]=c;
        r.model[2][3]=-5.4f;
        reset(false);
        drawFaceIcons(1.0f,true,&clock,NULL,quadIcons,&r);
        CHECK(count==520 && begins==3 && matrixLoads==2,"motif fixed stream/matrix budget");
        CHECK(culling==GX_CULL_BACK && memcmp(loaded,r.model,sizeof(Mtx))==0,
            "motif culling/model restoration");
        int lit=0;
        for(int i=0;i<count;i+=20) {
            if(!alphas[i]) continue;
            ++lit;
            CHECK(alphas[i+1] && alphas[i+2] && alphas[i+3],"solid motif core missing");
            for(int j=4;j<20;j+=4) {
                CHECK(alphas[i+j] && !alphas[i+j+1] && !alphas[i+j+2] && alphas[i+j+3],
                    "semantic polygon fringe must reach zero");
            }
        }
        /* Within ~10.5 degrees of Library-front, perspective hides both side
         * faces, and the Library face shows None here. */
        CHECK(lit>0 || angle<=10 || angle>=350,"all face motifs vanished");
        if(angle==0) CHECK(lit==0,"None on the Library face drew something");
        if(angle==90) CHECK(lit==11,"front System must show its clock and all three hands");
    }
    /* Subpixel-width geometry stays ordered and fades instead of inverting. */
    guMtxIdentity(r.model); r.model[2][3]=-5.4f;
    reset(false); GX_Begin(GX_QUADS,0,20);
    putSemanticMotifRect(&r,UI_HOME_FACE_LIBRARY,-0.0003f,-0.3f,0.0003f,0.3f,
        1.012f,(GXColor){255,255,255,200});
    GX_End();
    CHECK(alphas[0]>0 && alphas[0]<200,"subpixel motif did not attenuate");
    float area=0;
    for(int i=0;i<4;i++) area+=positions[i].x*positions[(i+1)%4].y-
        positions[(i+1)%4].x*positions[i].y;
    CHECK(area<0,"subpixel core inverted");
    clock.available=false;
    float yaw=INDIGO_TAU*0.25f;
    r.model[0][0]=cosf(yaw); r.model[0][2]=sinf(yaw);
    r.model[2][0]=-sinf(yaw); r.model[2][2]=cosf(yaw);
    reset(false); drawFaceIcons(1.0f,false,&clock,NULL,quadIcons,&r);
    CHECK(count==520,"invalid clock changed GX count");
    /* Hands occupy the first three of the eleven System shapes. */
    for(int i=15*20;i<18*20;i++) CHECK(alphas[i]==0,"invalid clock invented hands");
}
static int moved(const guVector *before,int n) {
    int changed=0;
    for(int i=0;i<n;i++) if(!closef(before[i].x,positions[i].x) || !closef(before[i].y,positions[i].y)) ++changed;
    return changed;
}
static int brighter(const u8 *before,int n) {
    int changed=0;
    for(int i=0;i<n;i++) if(alphas[i]>before[i]) ++changed;
    return changed;
}
static void drawController(const cubeRasterTransform_t *r,float seconds,bool animated,
    const indigoPadFrame_t *pad) {
    static const int icons[4]={0,-1,-1,-1};
    drawFaceIcons(seconds,animated,NULL,pad,icons,r);
}
static void test_controller(void) {
    static guVector neutral[4096];
    static u8 neutralAlphas[4096];
    cubeRasterTransform_t r;
    const Mtx semanticFaces[4]={
        {{1,0,0,0},{0,1,0,0},{0,0,1,0}},
        {{0,0,1,0},{0,1,0,0},{-1,0,0,0}},
        {{-1,0,0,0},{0,1,0,0},{0,0,-1,0}},
        {{0,0,-1,0},{0,1,0,0},{1,0,0,0}}
    };
    memcpy(r.semanticFaces,semanticFaces,sizeof(semanticFaces)); r.motifAlpha=1.0f;
    r.scaleX=r.scaleY=625.221f;
    guMtxIdentity(r.model); r.model[2][3]=-5.4f;
    /* The traced silhouette is a clockwise loop that fits a band. */
    int points=(int)(sizeof(controllerOutline)/sizeof(controllerOutline[0]));
    float area=0;
    for(int i=0;i<points;i++) area+=controllerOutline[i][0]*controllerOutline[(i+1)%points][1]-
        controllerOutline[(i+1)%points][0]*controllerOutline[i][1];
    CHECK(points<=FACE_BAND_MAX && area<0,"controller outline is not a clockwise loop within a band");
    /* Bands: 12 vertices per point, one primitive. Fills: a fan plus a fringe
     * quad per corner, two primitives. Beans (X, Y, L, R): a 10-quad strip, two
     * 7-vertex cap fans and a 30-quad fringe, four primitives. */
    int bands=12*(points+8+8), fills=5*(20+14+24+16+12+4*4), beans=4*(4*10+2*7+4*30);
    indigoPadFrame_t rest={true,0,0,0,0,0u};
    reset(false); drawController(&r,0.0f,false,&rest);
    CHECK(count==bands+fills+beans && begins==3+2*9+4*4 && matrixLoads==2,
        "controller budget: a part is hidden, overlapping or wound backwards");
    CHECK(culling==GX_CULL_BACK && memcmp(loaded,r.model,sizeof(Mtx))==0,"controller state restore");
    int front=count;
    memcpy(neutral,positions,sizeof(guVector)*front);
    memcpy(neutralAlphas,alphas,front);
    /* The main stick moves its cap and nothing else, in the stick's direction. */
    indigoPadFrame_t left={true,-80,0,0,0,0u};
    reset(false); drawController(&r,0.0f,false,&left);
    CHECK(count==front && moved(neutral,front)==20*5,"main stick must move exactly its cap");
    float shift=0;
    for(int i=0;i<front;i++) shift+=positions[i].x-neutral[i].x;
    CHECK(shift<0,"stick left moved the cap the wrong way");
    indigoPadFrame_t cUp={true,0,0,0,72,0u};
    reset(false); drawController(&r,0.0f,false,&cUp);
    CHECK(count==front && moved(neutral,front)==14*5,"C-stick must move exactly its cap");
    /* A press shrinks and brightens A alone. */
    indigoPadFrame_t a={true,0,0,0,0,PAD_BUTTON_A};
    reset(false); drawController(&r,0.0f,false,&a);
    CHECK(count==front && moved(neutral,front)==24*5,"A press must change exactly the A button");
    /* Its fan and the inner edge of its fringe brighten; the outer fringe stays clear. */
    CHECK(brighter(neutralAlphas,front)==24*3,"pressed A does not brighten");
    /* A held D-pad arm brightens in place. */
    indigoPadFrame_t right={true,0,0,0,0,PAD_BUTTON_RIGHT};
    reset(false); drawController(&r,0.0f,false,&right);
    CHECK(count==front && moved(neutral,front)==0 && brighter(neutralAlphas,front)==4*3,
        "held D-pad arm must brighten without moving");
    /* Idle play waits out the hold after live input, then moves on its own. */
    reset(false); drawController(&r,1.0f,true,&rest);
    CHECK(count==front && moved(neutral,front)==0,"idle play started inside the live-input hold");
    reset(false); drawController(&r,3.9f,true,&rest);
    CHECK(count==front && moved(neutral,front)>0,"idle play missing after the hold");
    reset(false); drawController(&r,3.9f,false,&rest);
    CHECK(count==front && moved(neutral,front)==0,"reduced motion kept idle play");
    reset(false); drawController(&r,9.9f,true,NULL);
    CHECK(count==front && moved(neutral,front)>0,"boot overlay (no pad) lost idle play");
    /* Facing away, the emblem draws nothing at all. */
    float yaw=INDIGO_TAU*0.5f;
    r.model[0][0]=cosf(yaw); r.model[0][2]=sinf(yaw); r.model[2][0]=-sinf(yaw); r.model[2][2]=cosf(yaw);
    reset(false); drawController(&r,0.0f,false,&rest);
    CHECK(count==0 && begins==0,"back-facing controller drew");
}
/* The sharpest corner, in degrees on screen, of the band that opens the
 * stream: its centre line runs between the two sides of each solid quad. */
static float sharpestBandTurn(int points) {
    float worst=0;
    for(int i=0;i<points;i++) {
        float x[3],y[3];
        for(int k=0;k<3;k++) {
            int at=12*((i+k+points-1)%points);
            float depth=-(positions[at].z+positions[at+1].z)*0.5f;
            x[k]=(positions[at].x+positions[at+1].x)*0.5f/depth;
            y[k]=(positions[at].y+positions[at+1].y)*0.5f/depth;
        }
        float ax=x[1]-x[0],ay=y[1]-y[0],bx=x[2]-x[1],by=y[2]-y[1];
        float turn=acosf(fmaxf(-1.0f,fminf(1.0f,(ax*bx+ay*by)/sqrtf((ax*ax+ay*ay)*(bx*bx+by*by)))));
        if(turn>worst) worst=turn;
    }
    return worst*360.0f/INDIGO_TAU;
}
/* roundedOutline keeps every turn under drawFaceBand's 45 degrees, also
 * where a pill's two rounded ends meet, and never repeats a point. */
static void test_rounded_outlines(void) {
    const indigoPoint_t square[4]={{-0.27f,0.27f},{0.27f,0.27f},{0.27f,-0.27f},{-0.27f,-0.27f}};
    const indigoPoint_t pill[4]={{-0.36f,0.36f},{0.36f,0.36f},{0.36f,0.08f},{-0.36f,0.08f}};
    const indigoPoint_t card[5]={{-0.28f,0.36f},{0.12f,0.36f},{0.28f,0.20f},{0.28f,-0.36f},{-0.28f,-0.36f}};
    const indigoPoint_t folder[6]={{-0.42f,0.30f},{-0.12f,0.30f},{-0.04f,0.20f},{0.42f,0.20f},
        {0.42f,-0.30f},{-0.42f,-0.30f}};
    const struct { const indigoPoint_t *corners; int count; float radius; } shapes[]={
        {square,4,0.045f},{pill,4,0.14f},{card,5,0.04f},{folder,6,0.04f}};
    for(unsigned k=0;k<sizeof(shapes)/sizeof(shapes[0]);k++) {
        indigoPoint_t out[FACE_BAND_MAX];
        int n=roundedOutline(shapes[k].corners,shapes[k].count,shapes[k].radius,out,FACE_BAND_MAX);
        CHECK(n>=3,"a rounded outline did not fit");
        for(int i=0;i<n;i++) {
            indigoPoint_t a=out[(i+n-1)%n],b=out[i],c=out[(i+1)%n];
            float ax=b.x-a.x,ay=b.y-a.y,bx=c.x-b.x,by=c.y-b.y;
            float al=sqrtf(ax*ax+ay*ay),bl=sqrtf(bx*bx+by*by);
            CHECK(al>1e-4f && bl>1e-4f,"a rounded outline repeats a point");
            float turn=acosf(fmaxf(-1.0f,fminf(1.0f,(ax*bx+ay*by)/(al*bl))))*360.0f/INDIGO_TAU;
            CHECK(turn<45.0f,"a rounded outline turns too sharply for a band");
        }
    }
    indigoPoint_t tiny[2];
    CHECK(roundedOutline(card,5,0.04f,tiny,2)==0,"an outline overran its buffer");
}
/* Each face shows its own four icons (choice 0-3) on that face and nowhere
 * else. Turned to the camera the whole icon draws, every band and polygon
 * of it; turned away nothing does; quad icons keep their budget at any
 * angle; every icon glows in the same shade. */
static void test_icons_on_their_faces(void) {
    /* The GX primitives each icon draws when its face looks at the camera. */
    static const int primitives[UI_HOME_ICON_COUNT]={
        [UI_HOME_ICON_CONTROLLER]=3+2*9+4*4,[UI_HOME_ICON_BOOKS]=1,[UI_HOME_ICON_COVERS]=3+2,
        [UI_HOME_ICON_PLAY]=1+2,[UI_HOME_ICON_HUB]=1,[UI_HOME_ICON_DISC]=2+2*4,
        [UI_HOME_ICON_SD_CARD]=1+4*2,[UI_HOME_ICON_FOLDER]=1+2,[UI_HOME_ICON_SLIDERS]=1,
        [UI_HOME_ICON_GEAR]=2,[UI_HOME_ICON_TOGGLES]=2+2*2,[UI_HOME_ICON_DIAL]=1+2+7*2,
        [UI_HOME_ICON_CLOCK]=1,[UI_HOME_ICON_INFO]=1+2+2,[UI_HOME_ICON_POWER]=4+2,
        [UI_HOME_ICON_CHIP]=1+2+2+12*2};
    static const int quadBudget[UI_HOME_ICON_COUNT]={
        [UI_HOME_ICON_BOOKS]=260,[UI_HOME_ICON_HUB]=180,[UI_HOME_ICON_SLIDERS]=120,[UI_HOME_ICON_CLOCK]=220};
    const Mtx semanticFaces[4]={
        {{1,0,0,0},{0,1,0,0},{0,0,1,0}},
        {{0,0,1,0},{0,1,0,0},{-1,0,0,0}},
        {{-1,0,0,0},{0,1,0,0},{0,0,-1,0}},
        {{0,0,-1,0},{0,1,0,0},{1,0,0,0}}
    };
    uiClockFrame_t clock={true,0,1,1,0,0.70710678f,0.70710678f};
    indigoPadFrame_t pad={true,0,0,0,0,0u};
    cubeRasterTransform_t r;
    GXColor shade={0,0,0,0};
    bool shaded=false;
    memcpy(r.semanticFaces,semanticFaces,sizeof(semanticFaces)); r.motifAlpha=1.0f;
    r.scaleX=r.scaleY=625.221f;
    for(int face=0;face<4;face++) for(int choice=0;choice<UI_HOME_ICON_CHOICES;choice++) {
        int icon=face*UI_HOME_ICON_CHOICES+choice;
        int choices[4]={-1,-1,-1,-1};
        float nx=semanticFaces[face][0][2],nz=semanticFaces[face][2][2];
        bool shown=false;
        choices[face]=choice;
        for(int angle=0;angle<360;angle+=15) {
            float yaw=angle*INDIGO_TAU/360,c=cosf(yaw),s=sinf(yaw),toward=-s*nx+c*nz;
            guMtxIdentity(r.model);
            r.model[0][0]=c; r.model[0][2]=s; r.model[2][0]=-s; r.model[2][2]=c;
            r.model[2][3]=-5.4f;
            reset(false); drawFaceIcons(3.9f,true,&clock,&pad,choices,&r);
            CHECK(matrixLoads==2 && culling==GX_CULL_BACK && memcmp(loaded,r.model,sizeof(Mtx))==0,
                "icon pass state restore");
            if(quadBudget[icon]) CHECK(count==quadBudget[icon] && begins==1,"quad icon budget moved");
            if(toward>0.99f) CHECK(begins==primitives[icon],"an icon lost a part facing the camera");
            int lit=0;
            for(int i=0;i<count;i++) {
                if(!alphas[i]) continue;
                /* Back to body space: the rotation is orthonormal. */
                float ex=positions[i].x,ey=positions[i].y,ez=positions[i].z+5.4f;
                float bx=c*ex-s*ez,by=ey,bz=s*ex+c*ez;
                float plane=bx*semanticFaces[face][0][2]+by*semanticFaces[face][1][2]+bz*nz;
                CHECK(fabsf(plane-1.012f)<0.1f,"an icon drew off its own face");
                if(!shaded) { shade=colors[i]; shaded=true; }
                CHECK(colors[i].r==shade.r && colors[i].g==shade.g && colors[i].b==shade.b,
                    "icons glow in different shades");
                ++lit;
            }
            if(toward>0.5f) { CHECK(lit>0,"an icon vanished from a face turned to the camera"); shown=true; }
            if(toward<-0.2f) CHECK(lit==0,"an icon drew on a face turned away");
        }
        CHECK(shown,"no angle showed the icon");
    }
    /* A choice outside a face's four draws nothing. */
    const int none[4]={UI_HOME_ICON_CHOICES,-1,1000,4};
    guMtxIdentity(r.model); r.model[2][3]=-5.4f;
    reset(false); drawFaceIcons(1.0f,true,&clock,&pad,none,&r);
    CHECK(count==0 && begins==0 && matrixLoads==2,"an out-of-range choice drew");
    /* The turning Gear and spinning Disc keep every band joint at any turn:
     * a failed join would drop the whole outline. */
    GXColor glow={196,177,255,180};
    for(int step=0;step<48;step++) for(int view=-1;view<=1;view++) {
        float yaw=view*50.0f*INDIGO_TAU/360,c=cosf(yaw),s=sinf(yaw);
        guMtxIdentity(r.model);
        r.model[0][0]=c; r.model[0][2]=s; r.model[2][0]=-s; r.model[2][2]=c;
        r.model[2][3]=-5.4f;
        reset(false); drawGearIcon(&r,UI_HOME_FACE_LIBRARY,glow,step*INDIGO_TAU/48);
        CHECK(count==12*(78+16) && begins==2,"gear outline lost a joint");
        /* drawFaceBand folds a short-sided band at corners past 45 degrees. */
        if(view==0) CHECK(sharpestBandTurn(78)<45.0f,"gear teeth turn too sharply for a band");
        reset(false); drawDiscIcon(&r,UI_HOME_FACE_LIBRARY,glow,step*INDIGO_TAU/48);
        CHECK(count==12*(32+16)+2*(4*10+2*7+4*30) && begins==2+2*4,"disc lost a ring or glint");
    }
}
static void surface_pose(cubeRasterTransform_t *r,float yaw,float pitch,float roll,float scale) {
    float cy=cosf(yaw),sy=sinf(yaw),cx=cosf(pitch),sx=sinf(pitch),cz=cosf(roll),sz=sinf(roll);
    r->scaleX=r->scaleY=625.221f;
    guMtxIdentity(r->model);
    r->model[0][0]=cz*cy*scale; r->model[0][1]=(cz*sy*sx-sz*cx)*scale;
    r->model[0][2]=(cz*sy*cx+sz*sx)*scale;
    r->model[1][0]=sz*cy*scale; r->model[1][1]=(sz*sy*sx+cz*cx)*scale;
    r->model[1][2]=(sz*sy*cx-cz*sx)*scale;
    r->model[2][0]=-sy*scale; r->model[2][1]=cy*sx*scale; r->model[2][2]=cy*cx*scale;
    r->model[0][3]=0.25f; r->model[1][3]=0.1f; r->model[2][3]=-5.4f;
}
static indigoPoint_t projected(const cubeRasterTransform_t *r,guVector p) {
    return (indigoPoint_t){r->scaleX*p.x/-p.z,r->scaleY*p.y/-p.z};
}
static void check_outline(const cubeRasterTransform_t *r,const cubeSurfaceQuad_t *faces,
    const cubeOutline_t *outline) {
    CHECK(outline->count>=3 && outline->count<=24,"invalid projected outline budget");
    for(int i=0;i<outline->count;i++) {
        indigoPoint_t a=outline->point[i],b=outline->point[(i+1)%outline->count];
        CHECK(outlineCross(a,b,outline->point[(i+2)%outline->count])>0,"outline is not convex CCW");
        float length=hypotf(b.x-a.x,b.y-a.y);
        for(int face=0;face<6;face++) for(int vertex=0;vertex<4;vertex++) {
            guVector eye,p=faces[face].point[vertex]; indigoPoint_t screen;
            CHECK(projectRailPoint(r,p.x,p.y,p.z,&eye,&screen),"valid outline source failed projection");
            CHECK(outlineCross(a,b,screen)/length > -0.006f,"outline cut into a solid fill");
        }
    }
    indigoPoint_t center={0,0},outward[2];
    for(int i=0;i<outline->count;i++) {
        center.x+=outline->point[i].x/outline->count;
        center.y+=outline->point[i].y/outline->count;
    }
    CHECK(!cubeOutlineEdge(outline,center,(indigoPoint_t){center.x+0.1f,center.y},outward),
        "interior edge was classified as silhouette");
}
static void check_surface_vertices(const cubeRasterTransform_t *r,const cubeOutline_t *outline,
    const cubeSurfaceQuad_t *quads,int quadsCount,int vertexCount,int cull,bool opaque,bool *covered) {
    reset(false); surfaceTest=true; depthWrites=opaque?GX_TRUE:GX_FALSE;
    if(vertexCount==4) drawCubeSurfacePass(r,outline,quads,quadsCount,cull,opaque);
    else drawCubeSurfacePassVertices(r,outline,quads,quadsCount,vertexCount,cull,opaque);
    CHECK(firstPrimitive==(vertexCount==3?GX_TRIANGLES:GX_QUADS),"surface primitive type");
    CHECK(count>=quadsCount*vertexCount && count<=quadsCount*vertexCount*5 && begins>=1 && begins<=2,
        "surface vertex/draw budget");
    CHECK(culling==cull && depthWrites==(opaque?GX_TRUE:GX_FALSE) &&
        memcmp(loaded,r->model,sizeof(Mtx))==0,"surface state not restored");
    for(int i=0;i<quadsCount*vertexCount;i++) {
        guVector p=quads[i/vertexCount].point[i%vertexCount]; GXColor c=quads[i/vertexCount].color[i%vertexCount];
        CHECK(positions[i].x==p.x && positions[i].y==p.y && positions[i].z==p.z,
            "original fill vertex changed");
        CHECK(memcmp(&colors[i],&c,sizeof(c))==0,"original face gradient changed");
    }
    for(int i=quadsCount*vertexCount;i<count;i+=4) {
        indigoPoint_t a=projected(r,positions[i]),b=projected(r,positions[i+1]);
        indigoPoint_t outsideA=projected(r,positions[i+3]),outsideB=projected(r,positions[i+2]);
        CHECK(alphas[i]>0 && alphas[i+1]>0 && alphas[i+2]==0 && alphas[i+3]==0,
            "solid silhouette did not fade to transparent");
        CHECK(positions[i].z==positions[i+3].z && positions[i+1].z==positions[i+2].z,
            "silhouette coverage changed endpoint depth");
        float dx=b.x-a.x,dy=b.y-a.y,length=hypotf(dx,dy);
        CHECK(closef(fabsf(dx*(outsideA.y-a.y)-dy*(outsideA.x-a.x))/length,1.0f) &&
            closef(fabsf(dx*(outsideB.y-b.y)-dy*(outsideB.x-b.x))/length,1.0f),
            "solid edge coverage is not one source pixel");
        CHECK(hypotf(outsideA.x-a.x,outsideA.y-a.y)<=4.01f &&
            hypotf(outsideB.x-b.x,outsideB.y-b.y)<=4.01f,"outline miter spike");
        bool boundary=false;
        for(int edge=0;edge<outline->count;edge++) {
            indigoPoint_t p=outline->point[edge],q=outline->point[(edge+1)%outline->count];
            float extent=hypotf(q.x-p.x,q.y-p.y);
            if(fabsf(outlineCross(p,q,a))/extent>0.006f ||
               fabsf(outlineCross(p,q,b))/extent>0.006f) continue;
            CHECK(outlineCross(p,q,outsideA)<0 && outlineCross(p,q,outsideB)<0,
                "coverage expanded into the solid instead of outward");
            covered[edge]=true; boundary=true;
        }
        CHECK(boundary,"shared internal mesh edge received a fringe");
        /* Independent eye-space facing check: a fringe must come from a
         * face rendered in this pass, with both original endpoint colors. */
        bool source=false;
        for(int q=0;q<quadsCount;q++) {
            guVector eye[4]; indigoPoint_t screen;
            for(int v=0;v<vertexCount;v++) {
                guVector p=quads[q].point[v];
                CHECK(projectRailPoint(r,p.x,p.y,p.z,&eye[v],&screen),"source projection");
            }
            guVector u={eye[1].x-eye[0].x,eye[1].y-eye[0].y,eye[1].z-eye[0].z};
            guVector v={eye[2].x-eye[0].x,eye[2].y-eye[0].y,eye[2].z-eye[0].z};
            guVector normal={u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
            float facing=normal.x*eye[0].x+normal.y*eye[0].y+normal.z*eye[0].z;
            if((cull==GX_CULL_BACK && facing<=0) ||
               (cull==GX_CULL_FRONT && facing>=0)) continue;
            for(int edge=0;edge<vertexCount;edge++) {
                int next=(edge+1)%vertexCount;
                if(memcmp(&positions[i],&eye[edge],sizeof(guVector)) ||
                   memcmp(&positions[i+1],&eye[next],sizeof(guVector))) continue;
                CHECK(!memcmp(&colors[i],&quads[q].color[edge],sizeof(GXColor)) &&
                    !memcmp(&colors[i+1],&quads[q].color[next],sizeof(GXColor)),
                    "silhouette lost its source face gradient");
                source=true;
            }
        }
        CHECK(source,"silhouette came from a culled face");
    }
}
static void check_surface_pass(const cubeRasterTransform_t *r,const cubeOutline_t *outline,
    const cubeSurfaceQuad_t *quads,int quadsCount,int cull,bool opaque,bool *covered) {
    check_surface_vertices(r,outline,quads,quadsCount,4,cull,opaque,covered);
}
static int compare_point(guVector a,guVector b) {
    if(a.x!=b.x) return a.x<b.x?-1:1;
    if(a.y!=b.y) return a.y<b.y?-1:1;
    if(a.z!=b.z) return a.z<b.z?-1:1;
    return 0;
}
static void test_closed_cube(void) {
    cubeSurfaceQuad_t mesh[26];
    const GXColor color[6]={{19,15,54,255},{28,20,76,255},{50,36,111,255},
        {24,18,64,255},{76,59,139,255},{44,31,102,255}};
    struct {guVector a,b;int uses,balance;} edges[96];
    guVector vertices[96]; int edgeCount=0,vertexCount=0;
    memset(mesh,0,sizeof(mesh)); memset(edges,0,sizeof(edges));
    buildCubeFaces(mesh,1,.78f,color);
    buildChamferStrips(mesh+6,1,.78f,color); buildCubeCorners(mesh+18,1,.78f);
    for(int f=0;f<26;f++) {
        int sides=f<18?4:3;
        guVector p=mesh[f].point[0],a=mesh[f].point[1],b=mesh[f].point[2];
        a=(guVector){a.x-p.x,a.y-p.y,a.z-p.z};
        b=(guVector){b.x-p.x,b.y-p.y,b.z-p.z};
        guVector normal={a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
        CHECK(normal.x*p.x+normal.y*p.y+normal.z*p.z<0,"closed mesh winding/degenerate face");
        for(int v=0;v<sides;v++) {
            guVector from=mesh[f].point[v],to=mesh[f].point[(v+1)%sides];
            int point=0;
            for(;point<vertexCount;point++) if(!compare_point(vertices[point],from)) break;
            if(point==vertexCount) vertices[vertexCount++]=from;
            int direction=compare_point(from,to);
            CHECK(direction!=0,"closed mesh zero-length edge");
            if(direction>0) {guVector swap=from;from=to;to=swap;}
            int edge=0;
            for(;edge<edgeCount;edge++) if(!compare_point(edges[edge].a,from)&&
                !compare_point(edges[edge].b,to)) break;
            if(edge==edgeCount) {edges[edgeCount].a=from;edges[edgeCount++].b=to;}
            edges[edge].uses++;edges[edge].balance+=direction;
            if(f>=18) CHECK(mesh[f].color[v].a>0&&mesh[f].color[v].a<=90&&
                mesh[f].color[v].r<=160,"corner cap is missing or excessively bright");
        }
    }
    CHECK(vertexCount==24&&edgeCount==48&&vertexCount-edgeCount+26==2,
        "sealed cube changed geometry or topology");
    for(int edge=0;edge<edgeCount;edge++) CHECK(edges[edge].uses==2&&edges[edge].balance==0,
        "cube has an open boundary, overlapping face, or inconsistent winding");
}
static void test_chamfer_color_pairs(void) {
    const guVector points[4]={{-1,.78f,-.78f},{-1,.78f,.78f},
        {-.78f,1,.78f},{-.78f,1,-.78f}};
    const GXColor outer={132,108,218,84},inner={196,180,255,112};
    for(int reverse=0;reverse<2;reverse++) {
        guVector p[4]; cubeSurfaceQuad_t quad;
        for(int i=0;i<4;i++) p[i]=points[reverse?3-i:i];
        buildChamferStrip(&quad,p[0].x,p[0].y,p[0].z,p[1].x,p[1].y,p[1].z,
            p[2].x,p[2].y,p[2].z,p[3].x,p[3].y,p[3].z,outer,inner);
        for(int i=0;i<4;i++) {
            int original=0;
            for(;original<4;original++) {
                if(!memcmp(&quad.point[i],&p[original],sizeof(guVector))) break;
            }
            CHECK(original<4,"bevel normalization moved a corner");
            GXColor expected=original<2?outer:inner;
            CHECK(!memcmp(&quad.color[i],&expected,sizeof(GXColor)),
                "bevel winding normalization moved an endpoint color");
        }
    }
}
static void test_surfaces(void) {
    const GXColor colors[6]={{19,15,54,255},{28,20,76,255},{50,36,111,255},
        {24,18,64,255},{76,59,139,255},{44,31,102,255}};
    cubeSurfaceQuad_t core[6],shell[6],strips[12],corners[8];
    cubeOutline_t coreOutline,shellOutline;
    cubeRasterTransform_t r;
    buildCubeFaces(core,.46f,.46f,colors);
    buildCubeFaces(shell,1,.78f,colors);
    buildChamferStrips(strips,1,.78f,colors); buildCubeCorners(corners,1,.78f);
    for(int q=0;q<12;q++) {
        guVector p=strips[q].point[0],a=strips[q].point[1],b=strips[q].point[2];
        a=(guVector){a.x-p.x,a.y-p.y,a.z-p.z};
        b=(guVector){b.x-p.x,b.y-p.y,b.z-p.z};
        guVector normal={a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
        CHECK(normal.x*p.x+normal.y*p.y+normal.z*p.z<0,
            "bevel winding disagrees with clockwise exterior faces");
    }
    int poses=0;
    for(int yaw=0;yaw<360;yaw+=30) for(int pitch=0;pitch<360;pitch+=30)
    for(int roll=0;roll<180;roll+=45) for(int size=0;size<2;size++) {
        surface_pose(&r,yaw*INDIGO_TAU/360,pitch*INDIGO_TAU/360,
            roll*INDIGO_TAU/360,size?1.3f:.65f);
        buildCubeOutline(&r,core,&coreOutline); buildCubeOutline(&r,shell,&shellOutline);
        check_outline(&r,core,&coreOutline); check_outline(&r,shell,&shellOutline);
        bool coreCovered[24]={false},shellCovered[24]={false};
        check_surface_pass(&r,&coreOutline,core,6,GX_CULL_BACK,true,coreCovered);
        check_surface_pass(&r,&shellOutline,strips,12,GX_CULL_FRONT,false,shellCovered);
        check_surface_vertices(&r,&shellOutline,corners,8,3,GX_CULL_FRONT,false,shellCovered);
        check_surface_pass(&r,&shellOutline,shell,6,GX_CULL_FRONT,false,shellCovered);
        check_surface_pass(&r,&shellOutline,shell,6,GX_CULL_BACK,false,shellCovered);
        check_surface_pass(&r,&shellOutline,strips,12,GX_CULL_BACK,false,shellCovered);
        check_surface_vertices(&r,&shellOutline,corners,8,3,GX_CULL_BACK,false,shellCovered);
        for(int i=0;i<coreOutline.count;i++) CHECK(coreCovered[i],"core silhouette has a coverage gap");
        for(int i=0;i<shellOutline.count;i++) CHECK(shellCovered[i],"shell silhouette has a coverage gap");
        ++poses;
    }
    CHECK(poses==1152,"full yaw/pitch/roll/scale sweep missing");
    guMtxIdentity(r.model); buildCubeOutline(&r,shell,&shellOutline);
    CHECK(shellOutline.count==0,"near-plane-invalid outline accepted");
}
static void check_glass_stream(const cubeRasterTransform_t *r,const cubeSurfaceQuad_t *quads,
    int quadsCount,int vertexCount,int *total) {
    reset(false);
    drawGlassReflection(r,quads,quadsCount,vertexCount,1.0f);
    CHECK(!active && matrixLoads==0,"reflection left a primitive open or moved the matrix");
    int sides=vertexCount==3?3:4;
    for(int i=0;i<count;i+=sides) {
        guVector eye; indigoPoint_t p[4]; float area=0; int lit=0;
        for(int v=0;v<sides;v++) {
            CHECK(projectRailPoint(r,positions[i+v].x,positions[i+v].y,positions[i+v].z,&eye,&p[v]),
                "reflection vertex left the view");
            lit|=alphas[i+v];
        }
        for(int v=0;v<sides;v++) area+=p[v].x*p[(v+1)%sides].y-p[(v+1)%sides].x*p[v].y;
        /* GX culls clockwise-negative back faces: every reflection cell must face the camera. */
        CHECK(area<0.0f,"reflection cell faces away and would be culled");
        CHECK(lit,"unlit reflection cell was drawn");
        for(int v=0;v<sides;v++) {
            guVector q=positions[i+v];
            float m=fmaxf(fabsf(q.x),fmaxf(fabsf(q.y),fabsf(q.z)));
            CHECK(m<=1.0001f && m>=.78f-.0001f,"reflection left the glass surface");
        }
    }
    *total+=count;
}
static void test_glass(void) {
    const GXColor tint[6]={{10,1,2,3},{20,1,2,3},{30,1,2,3},{40,1,2,3},{50,1,2,3},{60,1,2,3}};
    GXColor white[6],lit[6];
    cubeRasterTransform_t r;
    for(int f=0;f<6;f++) white[f]=(GXColor){255,255,255,255};
    /* Light belongs to the camera: at rest each face keeps its own tint, after a
     * quarter turn the face now toward the viewer takes the front tint, and a
     * face midway between two directions blends them rather than snapping. */
    surface_pose(&r,0,0,0,1); litCubeTints(&r,tint,lit);
    for(int f=0;f<6;f++) CHECK(lit[f].r==tint[f].r && lit[f].a==3,"cardinal tint changed");
    surface_pose(&r,INDIGO_TAU/4,0,0,1); litCubeTints(&r,tint,lit);
    CHECK(lit[1].r==60 && lit[5].r==30 && lit[2].r==10 && lit[0].r==20 &&
        lit[4].r==50 && lit[3].r==40,"tint turned with the cube instead of the camera");
    surface_pose(&r,INDIGO_TAU/8,0,0,1); litCubeTints(&r,tint,lit);
    CHECK(abs(lit[5].r-45)<=1,"a turning face snapped between tints");
    for(int i=0;i<500;i++) {
        surface_pose(&r,i*0.37f,i*0.23f,i*0.11f,0.5f+i*0.002f); litCubeTints(&r,white,lit);
        for(int f=0;f<6;f++) CHECK(lit[f].r==255 && lit[f].a==255,"tint weights must sum to one");
    }
    /* Only camera-facing glass reflects, and grazing glass fades out before
     * the silhouette rather than drawing a hard aliased rim. */
    guVector eye={0,0,-5},grazing={0.99995f,0,0.01f};
    CHECK(glassReflection(eye,(guVector){0,0,-1}).a==0,"back-facing glass reflected");
    CHECK(glassReflection(eye,grazing).a<=8,"grazing reflection is not faded");
    /* At each Home face's rest pitch the front pane carries a gentle reflection
     * that brightens toward the key light on the right. */
    static const float restPitch[4]={0.09f,0.16f,0.0f,0.135f};
    for(int face=0;face<4;face++) {
        surface_pose(&r,0.28f,restPitch[face],0,0.92f);
        guVector left,right,center,n=cubeViewNormal(&r,(guVector){0,0,1});
        indigoPoint_t screen;
        CHECK(projectRailPoint(&r,-.7f,0,1,&left,&screen) && projectRailPoint(&r,.7f,0,1,&right,&screen) &&
            projectRailPoint(&r,0,0,1,&center,&screen),"front pane left the view");
        int a=glassReflection(left,n).a,b=glassReflection(center,n).a,c=glassReflection(right,n).a;
        CHECK(a<b && b<c,"front reflection does not brighten toward the key");
        CHECK(b>=15 && c<=70,"front reflection is not gentle at rest");
    }
    /* Every reflection cell is complete, camera-facing, lit and on the glass,
     * within a bounded vertex budget, across the full pose sweep. */
    cubeSurfaceQuad_t shell[6],strips[12],corners[8];
    buildCubeFaces(shell,1,.78f,tint); buildChamferStrips(strips,1,.78f,tint);
    buildCubeCorners(corners,1,.78f);
    int most=0;
    for(int yaw=0;yaw<360;yaw+=30) for(int pitch=0;pitch<360;pitch+=30)
    for(int roll=0;roll<180;roll+=45) for(int size=0;size<2;size++) {
        int total=0;
        surface_pose(&r,yaw*INDIGO_TAU/360,pitch*INDIGO_TAU/360,roll*INDIGO_TAU/360,size?1.3f:.65f);
        check_glass_stream(&r,shell,6,4,&total);
        check_glass_stream(&r,strips,12,4,&total);
        check_glass_stream(&r,corners,8,3,&total);
        if(total>most) most=total;
    }
    /* Measured 1,381 at the worst sweep pose; the video thread pays for each. */
    CHECK(most>400 && most<=1600,"reflection vertex budget");
}
int main(void) {
    test_dial(); test_rail_joins(); test_motifs(); test_controller(); test_rounded_outlines();
    test_icons_on_their_faces();
    test_closed_cube(); test_chamfer_color_pairs(); test_surfaces(); test_glass();
    puts("native strokes: bounded complete GX streams, perspective coverage and closed seams");
    return 0;
}
"""

class StrokeGXStreamTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        indigo=(GUI / "indigo_background.c").read_text()
        frame=(GUI / "FrameBufferMagic.c").read_text()
        blocks=[extract_function(indigo, "static void " + name + "(")
                for name in ("putCubeVertex",)]
        blocks += [extract_function(indigo[indigo.rindex("static bool " + name + "("):], "static bool " + name + "(")
                   for name in ("projectRailPoint", "railJoin")]
        blocks += [extract_function(indigo, "static void " + name + "(")
                   for name in ("putProjectedRailVertex",)]
        blocks += [extract_function(indigo, "static guVector semanticFacePoint(")]
        blocks += [extract_function(indigo, "static void " + name + "(") for name in (
            "putSemanticMotifQuad", "putSemanticMotifRect", "putSemanticFaceDiamond",
            "putSemanticFaceHand", "putSemanticFaceBook", "drawFacePolygon", "drawFaceBand",
            "drawFaceCircle", "drawFaceRing", "drawControllerRect")]
        blocks += [re.search(r"static const float controllerOutline\[\]\[2\] = \{.*?\n\};",
            indigo, re.S).group(0)]
        blocks += [extract_function(indigo, "static void " + name + "(")
                   for name in ("drawFaceArc", "drawFaceBean")]
        blocks += [extract_function(indigo, "static float controllerAxis(")]
        blocks += [extract_function(indigo, "static void controllerPose(")]
        blocks += [extract_function(indigo, "static GXColor controllerGlow(")]
        blocks += [extract_function(indigo, "static int roundedOutline(")]
        blocks += [extract_function(indigo, "static void " + name + "(") for name in (
            "drawRoundedBand", "drawRoundedRect", "drawFaceBar", "drawFaceSpoke",
            "drawControllerIcon", "drawHubIcon", "drawSlidersIcon", "drawClockIcon",
            "drawBooksIcon", "drawDiscIcon", "drawGearIcon", "drawCoversIcon",
            "drawPlayIcon", "drawSdCardIcon", "drawFolderIcon", "drawTogglesIcon",
            "drawDialIcon", "drawInfoIcon", "drawPowerIcon", "drawChipIcon")]
        blocks += [extract_function(indigo, "static void drawFaceIcons(")]
        blocks += [extract_function(indigo, "static float outlineCross(")]
        blocks += [extract_function(indigo, "static void buildCubeOutline(")]
        blocks += [extract_function(indigo, "static indigoPoint_t cubeOutlineNormal(")]
        blocks += [extract_function(indigo, "static bool cubeOutlineEdge(")]
        blocks += [extract_function(indigo, "static void " + name + "(") for name in (
            "buildCubeFaces", "buildChamferStrip", "buildChamferStrips",
            "buildCubeCorners", "drawCubeSurfacePassVertices", "drawCubeSurfacePass")]
        blocks += [re.search(r"static const guVector cubeFaceAxes\[6\] = \{.*?\n\};",
            indigo, re.S).group(0)]
        blocks += [extract_function(indigo, signature) for signature in (
            "static guVector cubeViewNormal(", "static void litCubeTints(",
            "static float glassSmoothstep(", "static GXColor glassReflection(",
            "static guVector glassVertexNormal(", "static bool glassSameNormal(",
            "static bool glassCellLit(", "static guVector glassBilinear(",
            "static void drawGlassReflection(")]
        blocks += [frame[frame.index("typedef struct systemDialPoint"):frame.index("static void _SetupRasterColor(")]]
        blocks += [extract_function(frame, "static void " + name + "(")
                   for name in ("_PutSystemDialVertex", "_DrawSystemRing")]
        cls.emitters="\n".join(blocks)
        # The controller's sizing constants come from the source, never a copy.
        cls.defines="\n".join(re.findall(
            r"^#define (?:FACE_POLYGON_MAX|FACE_BAND_MAX|FACE_ARC_MAX|CONTROLLER_IDLE_HOLD) .*$",
            indigo, re.MULTILINE))
        # The face and icon lists come from the source, never a copy.
        home=(GUI / "ui_home.h").read_text()
        cls.enums="\n".join([re.search(r"^#define UI_HOME_ICON_CHOICES \d+$", home, re.M).group(0)] +
            [re.search(r"typedef enum \{[^}]*\} " + name + ";", home).group(0)
             for name in ("uiHomeFace_t", "uiHomeIcon_t")])
        cls.indigo = indigo
        cls.frame = frame

    def test_stream_contract_matches_native_pipelines(self):
        cube = extract_function(self.indigo, "static void setupCubePipeline(")
        raster = extract_function(self.indigo, "static void setupRasterPipeline(")
        frame = extract_function(self.frame, "static void drawInit()\n")
        def attributes(source):
            self.assertIn("GX_ClearVtxDesc();", source)
            return set(re.findall(r"GX_SetVtxDesc\((GX_VA_\w+),\s*GX_DIRECT\)", source))
        self.assertEqual(attributes(cube), {"GX_VA_POS", "GX_VA_CLR0"})
        self.assertEqual(attributes(raster), {"GX_VA_POS", "GX_VA_CLR0", "GX_VA_TEX0"})
        self.assertEqual(attributes(frame), {"GX_VA_POS", "GX_VA_CLR0", "GX_VA_TEX0"})

    def test_bevel_passes_follow_camera(self):
        draw = extract_function(self.indigo, "static void drawCube(")
        def check(source):
            calls = re.findall(r"drawCubeSurfacePass(?:Vertices)?\(&raster, &shellOutline, "
                r"(\w+), (\d+), (?:(3|4), )?(GX_CULL_\w+), false\);", source)
            self.assertEqual(calls, [
                ("strips", "12", "", "GX_CULL_FRONT"),
                ("corners", "8", "3", "GX_CULL_FRONT"),
                ("shell", "6", "", "GX_CULL_FRONT"),
                ("shell", "6", "", "GX_CULL_BACK"),
                ("strips", "12", "", "GX_CULL_BACK"),
                ("corners", "8", "3", "GX_CULL_BACK")])
            self.assertEqual(source.count("buildChamferStrips(strips, outer, inset, lit);"), 1)
        check(draw)
        # Both complete mesh passes must use current facing, not an object-axis split.
        for old,new in [("strips, 12, GX_CULL_FRONT", "strips, 8, GX_CULL_NONE"),
                        ("strips, 12, GX_CULL_BACK", "strips, 4, GX_CULL_NONE"),
                        ("strips, 12, GX_CULL_FRONT", "strips, 12, GX_CULL_BACK"),
                        ("corners, 8, 3, GX_CULL_FRONT", "corners, 8, 3, GX_CULL_BACK")]:
            with self.subTest(mutant=new), self.assertRaises(AssertionError):
                check(draw.replace(old,new,1))

    def run_emitters(self, emitters):
        with tempfile.TemporaryDirectory(prefix="swiss-stroke-gx-") as directory:
            root=Path(directory); source=root / "strokes.c"; binary=root / "strokes"
            source.write_text(HARNESS.replace("/* HOME ENUMS */", self.enums)
                .replace("/* CONTROLLER DEFINES */", self.defines)
                .replace("/* EMITTERS */", emitters))
            result=subprocess.run(shlex.split(os.environ.get("CC", "cc")) +
                ["-std=c99", "-Wall", "-Wextra", "-Werror", str(source), "-o", str(binary), "-lm"],
                capture_output=True,text=True,timeout=30)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            return subprocess.run([str(binary)],capture_output=True,text=True,timeout=5)

    def test_controller_defines_come_from_the_source(self):
        self.assertEqual(self.defines.count("#define"), 4)

    def test_native_emitters(self):
        result=self.run_emitters(self.emitters)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)

    def test_regressions_are_rejected(self):
        mutants={
            "missing UV": ("GX_TexCoord2f32(0.0f, 0.0f);",
                "if(color.a == 254) GX_TexCoord2f32(0.0f, 0.0f);"),
            "opaque fringe": ("if(band == 0) a.a = 0;", "if(band == 0) a.a = color.a;"),
            "depth-dependent width": ("join.x * offset * -eye.z", "join.x * offset * 5.4f"),
            "changed depth": ("/ raster->scaleY, eye.z, color)", "/ raster->scaleY, eye.z - 0.1f, color)"),
            "matrix restore": ("GX_LoadPosMtxImm(raster->model, GX_PNMTX0);", "GX_LoadPosMtxImm(identity, GX_PNMTX0);"),
            "opaque semantic fringe": ("transparent.a = 0;", "transparent.a = color.a;"),
            "wrong face winding": ("area >= -0.001f", "area <= -0.001f"),
            "subpixel brightness": ("fminf(1.0f, clearance * 2.0f)", "1.0f"),
            "degenerate vertex count": ("i < 20;", "i < 19;"),
            "outward silhouette normal": ("{-inward.x, -inward.y}", "{inward.x, inward.y}"),
            "opaque silhouette fringe": ("transparentA.a = transparentB.a = 0;",
                "transparentA.a = transparentB.a = 255;"),
            "silhouette width": ("edge->outward[1], 1.0f, transparentB",
                "edge->outward[1], 2.0f, transparentB"),
            "silhouette face cull": ("cullMode == GX_CULL_BACK && area >= 0.0f",
                "cullMode == GX_CULL_BACK && area <= 0.0f"),
            "silhouette fill changed": ("putCubeVertex(p.x, p.y, p.z, quads[quad].color[vertex]);",
                "putCubeVertex(p.x + 0.01f, p.y, p.z, quads[quad].color[vertex]);"),
            "silhouette source gradient": ("edge->color[1] = quads[quad].color[next];",
                "edge->color[1] = quads[quad].color[vertex];"),
            "silhouette opaque depth restoration": (
                "GX_SetZMode(GX_ENABLE, GX_LEQUAL, GX_TRUE);",
                "GX_SetZMode(GX_ENABLE, GX_LEQUAL, GX_FALSE);"),
            "interior silhouette fringe": (
                "fabsf(outlineCross(p, q, a)) / length > 0.005f ||\n\t\t\t"
                "fabsf(outlineCross(p, q, b)) / length > 0.005f ||", "false ||"),
            "silhouette fringe depth write": (
                "GX_SetZMode(GX_ENABLE, GX_LEQUAL, GX_FALSE);\n"
                "\tGX_Begin(GX_QUADS, GX_VTXFMT0, edgeCount * 4);",
                "GX_SetZMode(GX_ENABLE, GX_LEQUAL, GX_TRUE);\n"
                "\tGX_Begin(GX_QUADS, GX_VTXFMT0, edgeCount * 4);"),
            "silhouette fringe culling": (
                "GX_SetCullMode(GX_CULL_NONE);\n"
                "\tGX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);",
                "GX_SetCullMode(GX_CULL_BACK);\n"
                "\tGX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);"),
            "open corner": ("corner < 8", "corner < 7"),
            "corner reversed winding": ("x * y * z > 0.0f", "x * y * z < 0.0f"),
            "detached cap": ("x * outer, y * inset, z * inset", "x * outer, y * inset, z * inset + 0.01f"),
            "triangle primitive": ("vertexCount == 3 ? GX_TRIANGLES : GX_QUADS", "GX_QUADS"),
            "triangle count": ("count * vertexCount);", "count * 4);"),
            "bevel reversed winding": (
                "normal.x * ax + normal.y * ay + normal.z * az > 0.0f",
                "normal.x * ax + normal.y * ay + normal.z * az < 0.0f"),
            "bevel endpoint gradient": (
                "quad->color[1] = quad->color[3];",
                "quad->color[1] = color;"),
            "tint turns with the cube": (
                "guVector n = cubeViewNormal(raster, cubeFaceAxes[face]);",
                "guVector n = raster ? cubeFaceAxes[face] : cubeFaceAxes[face];"),
            "tint snaps between directions": (
                "float wx = n.x * n.x, wy = n.y * n.y, wz = n.z * n.z;",
                "float wx = n.x * n.x > .5f, wy = n.y * n.y > .5f, wz = n.z * n.z > .5f;"),
            "no grazing fade": ("fminf(1.0f, facing * 6.0f)", "1.0f"),
            "key light on the left": ("{{0.861f, 0.148f, 0.487f}", "{{-0.861f, 0.148f, 0.487f}"),
            "back-facing reflection": ("if(area >= -0.001f) continue;", "if(area >= 1e30f) continue;"),
            "reversed reflection cells": ("{{0, 0}, {1, 0}, {1, 1}, {0, 1}}",
                "{{0, 0}, {0, 1}, {1, 1}, {1, 0}}"),
            "unlit reflection cells": ("if(!glassCellLit(color, first, STEPS + 1)) continue;", ""),
            "unlit corner reflection": ("if((color[0].a | color[1].a | color[2].a) == 0) continue;", ""),
        }
        for name,(old,new) in mutants.items():
            with self.subTest(name=name):
                self.assertIn(old,self.emitters)
                result=self.run_emitters(self.emitters.replace(old,new,1))
                self.assertNotEqual(result.returncode,0,"mutant survived: "+name)

if __name__ == "__main__":
    unittest.main()
