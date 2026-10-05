#!/usr/bin/env python3
"""Execute the Memory Cards folder controller with recorded button presses."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
from test_cheats_gx_stream import extract_function

ROOT = Path(__file__).resolve().parents[3]
GUI = ROOT / 'cube/swiss/source/gui'
SAVES = (GUI / 'saves.c').read_text()

PRELUDE = r'''
#define _GNU_SOURCE
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui_folder.h"
typedef uint32_t u32;
enum {IS_FILE, IS_DIR, IS_SPECIAL};
enum {BUTTON_A=1, BUTTON_B=2, BUTTON_Y=4, BUTTON_LEFT=8, BUTTON_RIGHT=16,
 BUTTON_UP=32, BUTTON_DOWN=64};
#define SAVES_BUTTONS 127u
typedef struct {bool card;} device;
typedef struct {int fileType; device *device; char name[1024];} file_handle;
typedef struct {int unused;} savesInput_t;
typedef struct {int unused;} uiDrawObj_t;
static uiFolderColors_t colors;
static uiDrawObj_t page;
static uiFolderSnapshot_t first, last;
static unsigned commands[32], commandCount, commandAt, writes, dialogs, redraws, reads;
static bool failSave;
static bool isCard(const device *d) {return d && d->card;}
static bool SavesRaw_IsImageName(const char *path)
{size_t n=strlen(path); return n>4 && !strcmp(path+n-4,".raw");}
static int GetTextSizeInPixels(const char *text) {return (int)strlen(text)*10;}
static void UICheats_Fit(char *out,size_t capacity,const char *text,int width,float scale,
 int (*measure)(const char*))
{(void)width;(void)scale;(void)measure;snprintf(out,capacity,"%s",text);}
static uint8_t config_folder_color(const char *path) {return UIFolder_GetColor(&colors,path);}
static bool config_set_folder_color(const char *path,uint8_t color)
{++writes;return !failSave && UIFolder_SetColor(&colors,path,color);}
static void folderContents(uiFolderSnapshot_t *snapshot,file_handle *file)
{(void)file;++reads;strcpy(snapshot->summary,"2 save files, 0 card images, 0 folders");}
static uiDrawObj_t *DrawMemoryCardFolder(const uiFolderSnapshot_t *snapshot)
{++dialogs;first=last=*snapshot;return &page;}
static void DrawPublish(uiDrawObj_t *p) {assert(p==&page);}
static void DrawUpdateMemoryCardFolder(uiDrawObj_t *p,const uiFolderSnapshot_t *snapshot)
{assert(p==&page);++redraws;last=*snapshot;}
static void DrawDispose(uiDrawObj_t *p) {assert(p==&page);}
static void inputInit(savesInput_t *input) {(void)input;}
static u32 inputNext(savesInput_t *input)
{(void)input;assert(commandAt<commandCount);return commands[commandAt++];}
static u32 padsButtonsHeld(void) {return 0u;}
static void VIDEO_WaitVSync(void) {}
'''
DRIVER = r'''
static void press(const unsigned *input,unsigned count)
{memcpy(commands,input,count*sizeof(*input));commandCount=count;commandAt=0;redraws=0;}
int main(void)
{
 device sd={false},physical={true};
 file_handle directory={IS_DIR,&sd,"sda:/swiss/saves/Backups"};
 file_handle image={IS_FILE,&sd,"sda:/swiss/saves/Demo Card.raw"};
 file_handle save={IS_FILE,&sd,"sda:/swiss/saves/profile.gci"};
 file_handle parent={IS_SPECIAL,&sd,"sda:/swiss"};
 assert(folderIdentity(&directory));assert(folderIdentity(&image));
 assert(!folderIdentity(&save));assert(!folderIdentity(&parent));assert(!folderIdentity(NULL));
 directory.device=&physical;assert(!folderIdentity(&directory));directory.device=&sd;
 showFolderIdentity(&save);showFolderIdentity(&parent);assert(dialogs==0 && reads==0 && writes==0);
 unsigned chosen[]={BUTTON_RIGHT,BUTTON_RIGHT,BUTTON_A};press(chosen,3);
 showFolderIdentity(&directory);assert(commandAt==3 && writes==1 && reads==1);
 assert(first.color==0 && config_folder_color(directory.name)==2);
 unsigned cancel[]={BUTTON_RIGHT,BUTTON_B};press(cancel,2);
 showFolderIdentity(&directory);assert(first.color==2 && last.color==3);
 assert(config_folder_color(directory.name)==2 && writes==1);
 unsigned reset[]={BUTTON_Y,BUTTON_A};press(reset,2);
 showFolderIdentity(&directory);assert(config_folder_color(directory.name)==0 && writes==2);
 failSave=true;press(chosen,3);
 commands[3]=BUTTON_B;commandCount=4;showFolderIdentity(&directory);
 assert(commandAt==4 && strstr(last.status,"Could not save") && config_folder_color(directory.name)==0);
 failSave=false;press(chosen,3);showFolderIdentity(&image);
 assert(config_folder_color(image.name)==2 && config_folder_color(directory.name)==0);
 assert(config_folder_color("sdb:/swiss/saves/Demo Card.raw")==0);
 puts("Memory Cards folder controller: contextual eligibility, save/cancel/reset, failure stays open, device identity PASS");
 return 0;
}
'''
source='\n'.join([PRELUDE,extract_function(SAVES,'static bool folderIdentity('),
                  extract_function(SAVES,'static void showFolderIdentity('),DRIVER])
with tempfile.TemporaryDirectory(prefix='memory-folder-controls-') as temp:
    code,binary=Path(temp)/'controls.c',Path(temp)/'controls'
    code.write_text(source)
    flags=shlex.split(os.environ.get('CC','cc'))+['-std=c11','-Wall','-Wextra','-Werror',f'-I{GUI}']
    if '--sanitize' in sys.argv:
        flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
        if os.uname().sysname=='Linux':flags+=['-fno-pie','-no-pie']
    subprocess.run(flags+[str(code),str(GUI/'ui_folder.c'),'-o',str(binary)],check=True,timeout=60)
    subprocess.run([str(binary)],check=True,timeout=15)
