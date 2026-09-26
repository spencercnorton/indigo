#!/usr/bin/env python3
"""Execute the display-copy boundary with stale depth and disabled widget masks."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest
from test_cheats_gx_stream import extract_function

SOURCE = Path(__file__).resolve().parents[3] / 'cube/swiss/source/gui/FrameBufferMagic.c'
HARNESS = r'''
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
enum { GX_ENABLE=1, GX_TRUE=1, GX_ALWAYS=7 };
static bool depthWrite, colorWrite;
static float depth[8];
static int copied, screen;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"copy assertion line%d\n",__LINE__);exit(71); } } while(0)
static void GX_SetZMode(int enabled,int function,int write) { (void)enabled;(void)function;depthWrite=write; }
static void GX_SetColorUpdate(int write) { colorWrite=write; }
/* GX_CopyDisp overrides the depth compare but preserves updateenable, just
 * like libogc2 gx.c and the EFB copy-clear path in Dolphin's renderer. */
static void GX_CopyDisp(void *framebuffer,int clear) {
 CHECK(framebuffer==&screen); CHECK(clear); CHECK(colorWrite);copied++;
 if(depthWrite) for(int i=0;i<8;i++) depth[i]=1.0f;
}
'''
MAIN = r'''
int main(void) {
 for(int frame=0;frame<400;frame++) {
  for(int i=0;i<8;i++) depth[i]=(float)(i+1)/16.0f;
  depthWrite=false; colorWrite=false;
  copyDisplayFrame(&screen);
  for(int i=0;i<8;i++) CHECK(depth[i]==1.0f);
  /* A receding cube fragment must pass where last frame had a nearer core. */
  CHECK(.9f<=depth[frame%8]);
 }
 CHECK(copied==400);return 0;
}
'''
class CopyClear(unittest.TestCase):
    def execute(self,body):
        with tempfile.TemporaryDirectory(prefix='swiss-copy-') as root:
            source=Path(root)/'test.c';binary=Path(root)/'test'
            source.write_text(HARNESS+body+MAIN)
            subprocess.run(shlex.split(os.environ.get('CC','cc'))+['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function',str(source),'-o',str(binary)],check=True,capture_output=True)
            return subprocess.run([str(binary)],capture_output=True,text=True)
    def test_copy_clears_widget_masks_and_old_depth(self):
        body=extract_function(SOURCE.read_text(),'static void copyDisplayFrame(')
        result=self.execute(body)
        self.assertEqual(result.returncode,0,result.stderr)
        for before,after in [('GX_SetZMode(GX_ENABLE, GX_ALWAYS, GX_TRUE);',''),('GX_SetColorUpdate(GX_ENABLE);',''),('GX_CopyDisp(framebuffer, GX_TRUE);','GX_CopyDisp(framebuffer, 0);')]:
            with self.subTest(mutant=before):
                self.assertIn(before,body)
                self.assertNotEqual(self.execute(body.replace(before,after)).returncode,0)
    def test_frame_boundary_and_poster_retreat_are_connected(self):
        source=SOURCE.read_text();video=extract_function(source,'static void *videoUpdate(')
        self.assertEqual(video.count('copyDisplayFrame(xfb[whichfb]);'),1)
        self.assertNotIn('GX_CopyDisp(',video)
        self.assertLess(video.index('copyDisplayFrame('),video.index('GX_DrawDone();'))
        flow=extract_function(source,'static void _DrawGameflow(')
        self.assertIn('scene->chromeProgress * scene->libraryReveal',flow)
        self.assertLess(flow.index('scene->libraryReveal'),flow.index('_GameflowDrawPoster('))
if __name__=='__main__': unittest.main()
