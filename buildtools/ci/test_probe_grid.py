"""Compile the probe's real layout and draw every cell into guarded XFBs."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ProbeGrid(unittest.TestCase):
    def test_the_report_fits_low_and_full_height_framebuffers(self):
        source = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include "probe_grid.h"
int main(void) {
    const int widths[] = {320, 512, 640};
    const int heights[] = {240, 264, 480, 528, 576};
    for(unsigned wi=0; wi<sizeof(widths)/sizeof(*widths); wi++)
        for(unsigned hi=0; hi<sizeof(heights)/sizeof(*heights); hi++) {
            int width=widths[wi], height=heights[hi], stride=width/2;
            size_t count=(size_t)stride*height;
            uint32_t *xfb=calloc(count+16,sizeof(*xfb));
            assert(xfb);
            for(size_t i=count;i<count+16;i++) xfb[i]=0x5a5a5a5a;
            probeGrid_t grid;
            assert(ProbeGrid_Layout(width,height,23,&grid));
            assert(grid.cell_width%2==0);
            for(int word=0;word<23;word++) for(int bit=0;bit<32;bit++) {
                int x=grid.x+bit*grid.cell_width, y=grid.y+word*grid.cell_height;
                for(int row=y;row<y+grid.cell_height;row++)
                    for(int col=x/2;col<(x+grid.cell_width)/2;col++) {
                        size_t index=(size_t)row*stride+col;
                        assert(index<count);
                        xfb[index]=0xffffffff;
                    }
            }
            for(size_t i=count;i<count+16;i++) assert(xfb[i]==0x5a5a5a5a);
            if(width==640 && height==480) {
                assert(grid.cell_width==16 && grid.cell_height==16);
            }
            free(xfb);
        }
    probeGrid_t grid;
    assert(!ProbeGrid_Layout(640,112,23,&grid));
    assert(!ProbeGrid_Layout(100,480,23,&grid));
    assert(!ProbeGrid_Layout(640,480,0,&grid));
    assert(!ProbeGrid_Layout(640,480,23,0));
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            code = Path(directory) / "grid.c"
            exe = Path(directory) / "grid"
            code.write_text(source)
            subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra",
                            "-Werror", "-I", str(ROOT / "buildtools/ui/emulator/probe"),
                            str(code), "-o", str(exe)], check=True, capture_output=True)
            subprocess.run([str(exe)], check=True, capture_output=True)


if __name__ == "__main__":
    unittest.main()
