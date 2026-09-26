#!/usr/bin/env python3
"""Exercise actual cube pipeline and symbol transform, including vertical poses."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest
from test_cheats_gx_stream import extract_function

GUI = Path(__file__).resolve().parents[3] / 'cube/swiss/source/gui'
PRELUDE = r'''
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui_scene.h"
#include "ui_cube_motif.h"
typedef float Mtx[3][4];
typedef float Mtx44[4][4];
typedef struct { float x,y,z; } guVector;
static Mtx loaded;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"pose assertion line %d\n",__LINE__); exit(71); } } while(0)
static void guMtxIdentity(Mtx m) { memset(m,0,sizeof(Mtx)); for(int i=0;i<3;i++) m[i][i]=1; }
static void guMtxCopy(Mtx a,Mtx b) { memcpy(b,a,sizeof(Mtx)); }
static void guMtxRotAxisRad(Mtx m,const guVector *a,float r) {
 float c=cosf(r),s=sinf(r); guMtxIdentity(m);
 if(a->x==1) { m[1][1]=m[2][2]=c; m[1][2]=-s; m[2][1]=s; }
 else { m[0][0]=m[2][2]=c; m[0][2]=s; m[2][0]=-s; }
}
static void guMtxConcat(Mtx a,Mtx b,Mtx out) {
 Mtx n={0}; for(int i=0;i<3;i++) { for(int j=0;j<3;j++) for(int k=0;k<3;k++) n[i][j]+=a[i][k]*b[k][j];
 n[i][3]=a[i][3]; for(int k=0;k<3;k++) n[i][3]+=a[i][k]*b[k][3]; } guMtxCopy(n,out);
}
static void guMtxScaleApply(Mtx a,Mtx out,float x,float y,float z) {
 float scale[3]={x,y,z}; for(int i=0;i<3;i++) for(int j=0;j<4;j++) out[i][j]=a[i][j]*scale[i];
}
static void guMtxTransApply(Mtx a,Mtx out,float x,float y,float z) {
 guMtxCopy(a,out); out[0][3]+=x; out[1][3]+=y; out[2][3]+=z;
}
static void guPerspective(Mtx44 p,float fov,float aspect,float near,float far) {
 (void)near;(void)far; memset(p,0,sizeof(Mtx44));p[1][1]=1/tanf(fov*3.14159265359f/360);p[0][0]=p[1][1]/aspect;
}
#define GX_LoadPosMtxImm(m,i) guMtxCopy(m,loaded)
'''
MAIN = r'''
static void frameFor(uiSceneFrame_t *frame,const uiHomeState_t *home) {
 memset(frame,0,sizeof(*frame)); frame->cubeScale=1; frame->cubePitch=-0.09f;
 UIHome_OrientationMatrix(&home->orientation,frame->homeOrientation);
 uiCubeMotifBasis_t basis; UICubeMotif_Build(home,&basis);
 memcpy(frame->homeMotifBasis,basis.face,sizeof(basis.face)); frame->homeMotifAlpha=0.7f;
}
static guVector transformed(guVector p) {
 guVector q={loaded[0][0]*p.x+loaded[0][1]*p.y+loaded[0][2]*p.z,
 loaded[1][0]*p.x+loaded[1][1]*p.y+loaded[1][2]*p.z,
 loaded[2][0]*p.x+loaded[2][1]*p.y+loaded[2][2]*p.z}; return q;
}
int main(void) {
 uiHomeOrientation_t orientations[24]; UIHome_OrientationInit(&orientations[0]);int count=1;
 for(int i=0;i<count;i++) for(int axis=UI_HOME_TURN_HORIZONTAL;axis<=UI_HOME_TURN_VERTICAL;axis++) {
  uiHomeOrientation_t next=orientations[i]; UIHome_OrientationTurn(&next,(uiHomeTurnAxis_t)axis,1);
  int found=0;for(int j=0;j<count;j++) if(memcmp(&next,&orientations[j],sizeof(next))==0) found=1;
  if(!found) { CHECK(count<24);orientations[count++]=next; }
 }
 CHECK(count==24);
 for(int i=0;i<count;i++) for(int face=0;face<4;face++) for(int axis=1;axis<=2;axis++) {
  uiHomeState_t home;UIHome_Init(&home,(uiHomeCapabilities_t){true,false});home.orientation=orientations[i];home.face=(uiHomeFace_t)face;home.turnAxis=(uiHomeTurnAxis_t)axis;
  uiSceneFrame_t frame;frameFor(&frame,&home);cubeRasterTransform_t raster;memset(&raster,0,sizeof(raster));setupCubePipeline(&frame,0,false,&raster);
  CHECK(fabsf(raster.motifAlpha-.7f)<.0001f);
  for(int r=0;r<3;r++) for(int c=0;c<3;c++) CHECK(fabsf(loaded[r][c]-(float)home.orientation.m[r][c])<.0001f);
  guVector right=transformed(semanticFacePoint(&raster,face,1,0,0));
  guVector up=transformed(semanticFacePoint(&raster,face,0,1,0));
  guVector normal=transformed(semanticFacePoint(&raster,face,0,0,1));
  CHECK(fabsf(right.x-1)<.0001f && fabsf(right.y)<.0001f && fabsf(right.z)<.0001f);
  CHECK(fabsf(up.x)<.0001f && fabsf(up.y-1)<.0001f && fabsf(up.z)<.0001f);
  CHECK(fabsf(normal.x)<.0001f && fabsf(normal.y)<.0001f && fabsf(normal.z-1)<.0001f);
 }
 /* An actual half-completed vertical turn moves the front normal vertically,
    while an equivalent horizontal turn moves it sideways. */
 for(int vertical=0;vertical<2;vertical++) for(int sign=-1;sign<=1;sign+=2) {
  uiHomeState_t home; UIHome_Init(&home,(uiHomeCapabilities_t){true,false});uiSceneFrame_t frame;frameFor(&frame,&home);
  Mtx turn;guVector axis=vertical?(guVector){1,0,0}:(guVector){0,1,0};guMtxRotAxisRad(turn,&axis,(float)sign*0.7853981634f);
  for(int r=0;r<3;r++) for(int c=0;c<3;c++) frame.homeOrientation[r][c]=turn[r][c];
  cubeRasterTransform_t raster;setupCubePipeline(&frame,0,false,&raster);guVector normal=transformed((guVector){0,0,1});
  CHECK(fabsf(normal.z-.70710678f)<.0001f);
  CHECK(vertical?(fabsf(normal.x)<.0001f && fabsf(normal.y+(float)sign*.70710678f)<.0001f):(fabsf(normal.y)<.0001f && fabsf(normal.x-(float)sign*.70710678f)<.0001f));
 }
 /* The boot fly-in: the cube starts farther off along the camera axis,
    spun about its vertical axis and then tumbled about its horizontal one. */
 {
  uiHomeState_t home;UIHome_Init(&home,(uiHomeCapabilities_t){true,false});uiSceneFrame_t frame;frameFor(&frame,&home);
  frame.introDistance=12.0f;frame.introSpin=0.7f;
  cubeRasterTransform_t raster;setupCubePipeline(&frame,0,false,&raster);
  CHECK(fabsf(loaded[0][3])<.0001f && fabsf(loaded[1][3])<.0001f && fabsf(loaded[2][3]-(CUBE_CAMERA_Z-12.0f))<.0001f);
  float cy=cosf(0.7f),sy=sinf(0.7f),cx=cosf(0.7f*CUBE_INTRO_TUMBLE),sx=sinf(0.7f*CUBE_INTRO_TUMBLE);
  float expect[3][3]={{cy,sy*sx,sy*cx},{0,cx,-sx},{-sy,cy*sx,cy*cx}};
  for(int r=0;r<3;r++) for(int c=0;c<3;c++) CHECK(fabsf(loaded[r][c]-expect[r][c])<.0001f);
 }
 puts("cube render pose: 192 settled bindings, four directional midpoints and the boot fly-in passed"); return 0;
}
'''

class CubeRenderPose(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source=(GUI/'indigo_background.c').read_text()
        # The pipeline loads its projection through loadCubeProjection().
        cls.setup='static Mtx44 cubeProjection;\n'+extract_function(source,'static void loadCubeProjection(')+'\n'+extract_function(source,'static void setupCubePipeline(')
        cls.point=extract_function(source,'static guVector semanticFacePoint(')
        raster=re.search(r'typedef struct cubeRasterTransform \{.*?\} cubeRasterTransform_t;',source,re.S).group()
        constants='\n'.join(line for line in source.splitlines() if line.startswith('#define CUBE_'))
        names=set(re.findall(r'\bGX_[A-Za-z0-9_]+',cls.setup))
        calls=set(re.findall(r'\b(GX_[A-Za-z0-9_]+)\(',cls.setup))
        macros='\n'.join(f'#define {n}(...) ((void)0)' if n in calls else f'#define {n} 0' for n in sorted(names) if n!='GX_LoadPosMtxImm')
        cls.prefix=PRELUDE+'\n'+raster+'\n'+constants+'\n'+macros+'\n'
    def execute(self,setup,point):
        with tempfile.TemporaryDirectory(prefix='swiss-cube-pose-') as folder:
            source=Path(folder)/'test.c'; binary=Path(folder)/'test'
            source.write_text(self.prefix+setup+'\n'+point+'\n'+MAIN)
            subprocess.run(shlex.split(os.environ.get('CC','cc'))+['-std=c11','-Wall','-Wextra','-Werror','-I'+str(GUI),str(source),str(GUI/'ui_home.c'),str(GUI/'ui_cube_motif.c'),'-lm','-o',str(binary)],check=True,capture_output=True,text=True)
            return subprocess.run([str(binary)],capture_output=True,text=True,timeout=5)
    def test_actual_pipeline_and_face_binding(self):
        result=self.execute(self.setup,self.point)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
    def test_ignoring_navigation_is_rejected(self):
        mutant=self.setup.replace('guMtxConcat(rotation, navigation, rotation);','(void)navigation;')
        self.assertNotEqual(mutant,self.setup)
        self.assertNotEqual(self.execute(mutant,self.point).returncode,0)
    def test_flying_in_from_nowhere_is_rejected(self):
        mutant=self.setup.replace('CUBE_CAMERA_Z - scene->introDistance','CUBE_CAMERA_Z + 0.0f * scene->introDistance')
        self.assertNotEqual(mutant,self.setup)
        self.assertNotEqual(self.execute(mutant,self.point).returncode,0)
    def test_spinning_without_the_tumble_is_rejected(self):
        mutant=self.setup.replace('scene->introSpin * CUBE_INTRO_TUMBLE','0.0f * scene->introSpin')
        self.assertNotEqual(mutant,self.setup)
        self.assertNotEqual(self.execute(mutant,self.point).returncode,0)
    def test_wrong_face_basis_is_rejected(self):
        mutant=self.point.replace('semanticFaces[face]','semanticFaces[(face + 1) % UI_HOME_FACE_COUNT]')
        self.assertNotEqual(mutant,self.point)
        self.assertNotEqual(self.execute(self.setup,mutant).returncode,0)
if __name__=='__main__': unittest.main()
