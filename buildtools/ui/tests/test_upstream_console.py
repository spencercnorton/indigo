#!/usr/bin/env python3
"""Run the actual console model classifier with Gateway and retail identities."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

from test_cheats_gx_stream import extract_function

ROOT = Path(__file__).resolve().parents[3]
SOURCE = (ROOT / "cube/swiss/source/gui/info.c").read_text()
PREFIX = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define SYS_CONSOLE_MASK 0xff
#define SYS_CONSOLE_DEVELOPMENT 1
static int consoleType;
static int SYS_GetConsoleType(void) {return consoleType;}
static unsigned short deviceCode;
static unsigned short *DVDDeviceCode=&deviceCode;
static char IPLInfo[256];
static struct {uint32_t rel_date; char pad[8];} drive;
#define DVDDriveInfo (&drive)
static int getFontEncode(void) {return 0;}
static void infoCopy(char *out,size_t size,const char *text) {snprintf(out,size,"%s",text);}
'''
DRIVER = r'''
int main(void) {
    char out[128];memcpy(IPLInfo,"(C) ",4);deviceCode=0x8000;
    drive.rel_date=0x20010619;drive.pad[1]='M';
    infoGetConsoleModel(out,sizeof(out));
    assert(strcmp(out,"NINTENDO GATEWAY GAMECUBE MSA-500")==0);
    struct {char text[8];char guard;} small={{0},'X'};
    infoGetConsoleModel(small.text,sizeof(small.text));
    assert(small.guard=='X'&&small.text[7]==0);
    drive.rel_date=0x20010618;infoGetConsoleModel(out,sizeof(out));
    assert(strcmp(out,"PANASONIC Q SL-GC10-S")==0);
    drive.pad[1]=0;infoGetConsoleModel(out,sizeof(out));
    assert(strcmp(out,"NINTENDO GAMECUBE DOL-001 (USA)")==0);
    consoleType=SYS_CONSOLE_DEVELOPMENT;deviceCode=0x8201;
    infoGetConsoleModel(out,sizeof(out));assert(strcmp(out,"NPDP-GDEV (GCT-0100)")==0);
    puts("console identity: Gateway precedence, retail/development fallback and bounded text PASS");
    return 0;
}
'''


def main():
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp)
        (path / "console.c").write_text(PREFIX + extract_function(SOURCE, "static void infoGetConsoleModel(") + DRIVER)
        command = shlex.split(os.environ.get("CC", "cc")) + ["-std=c11", "-Wall", "-Wextra", "-Werror"]
        if "--sanitize" in sys.argv:
            command += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
            if sys.platform != "darwin":
                command += ["-no-pie"]
        subprocess.run(command + [str(path / "console.c"), "-o", str(path / "console")], check=True)
        subprocess.run([str(path / "console")], check=True)


if __name__ == "__main__":
    main()
