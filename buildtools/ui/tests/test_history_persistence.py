#!/usr/bin/env python3
"""Compile actual history I/O adapter with fault-injected in-memory devices."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest
from test_cheats_gx_stream import extract_function

ROOT = Path(__file__).resolve().parents[3]
GUI = ROOT / "cube/swiss/source/gui"
CONFIG = ROOT / "cube/swiss/source/config/config.c"
HARNESS = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ui_game_history.h"
typedef int32_t s32;
typedef uint32_t u32;
typedef struct DEVICEHANDLER_INTERFACE DEVICEHANDLER_INTERFACE;
typedef struct { char name[1024]; u32 size; int fileType, status; DEVICEHANDLER_INTERFACE *device; } file_handle;
struct DEVICEHANDLER_INTERFACE {
    file_handle *initial; void *context; u32 features, quirks;
    s32 (*statFile)(file_handle *);
    s32 (*readFile)(file_handle *, void *, u32);
    s32 (*writeFile)(file_handle *, const void *, u32);
    s32 (*closeFile)(file_handle *);
};
enum { DEVICE_CONFIG, DEVICE_CUR, IS_FILE=1, FR_NO_FILE=4, FR_NO_PATH=5,
    FEAT_WRITE=2, QUIRK_GCLOADER_WRITE_CONFLICT=64 };
static DEVICEHANDLER_INTERFACE *devices[2];
static const char *playHistoryFilenames[2] = {
    "swiss/settings/play-history-0.ini", "swiss/settings/play-history-1.ini"
};
static uiGameHistory_t playHistory;
static DEVICEHANDLER_INTERFACE *playHistoryDevice;
static int playHistorySlot=-1;
static char disk[2][4096];
static u32 sizes[2];
static bool exists[2];
static int failStat=-1, failRead=-1, shortWrite, failClose, writes, wrapperCloses;
static int allocFail=-1, allocations;
static time_t now=978307260;
static int slotof(file_handle *f) {
    assert(f->status==0); /* history never owns/remaps a mapped game handle */
    assert(f->device==devices[DEVICE_CONFIG]);
    return strstr(f->name,"history-1")!=NULL ? 1:0;
}
static s32 deviceHandler_FAT_statFile(file_handle *f) {
    int s=slotof(f);
    if(failStat==s) return -9;
    if(!exists[s]) return FR_NO_FILE;
    f->size=sizes[s]; f->fileType=IS_FILE; return 0;
}
static s32 fakeRead(file_handle *f, void *buf, u32 size) {
    int s=slotof(f);
    if(failRead==s) return -1;
    assert(size<=sizes[s]); memcpy(buf,disk[s],size); return (s32)size;
}
static s32 deviceHandler_FAT_writeFile(file_handle *f,const void *buf,u32 size) {
    int s=slotof(f); ++writes;
    assert(size<4096);
    /* CREATE_ALWAYS truncates only this inactive slot. */
    sizes[s]=shortWrite ? size/2 : size; exists[s]=true;
    memcpy(disk[s],buf,sizes[s]); return (s32)sizes[s];
}
static s32 deviceHandler_FAT_closeFile(file_handle *f) { (void)slotof(f); return failClose ? -1:0; }
static s32 wrapperClose(file_handle *f) { ++wrapperCloses; return deviceHandler_FAT_closeFile(f); }
static void concat_path(char *out,const char *a,const char *b) { (void)snprintf(out,1024,"%s%s",a,b); }
static void *checkedMalloc(size_t size) { if(allocations++==allocFail) return NULL; return malloc(size); }
static void *checkedCalloc(size_t n,size_t size) { if(allocations++==allocFail) return NULL; return calloc(n,size); }
static time_t fakeTime(time_t *out) { if(out) *out=now; return now; }
#define malloc checkedMalloc
#define calloc checkedCalloc
#define time fakeTime
#define print_debug(...) ((void)0)
/* ADAPTER */
#undef malloc
#undef calloc
#undef time

static file_handle initial={.name="gcldr:/"};
static DEVICEHANDLER_INTERFACE device={
    .initial=&initial,.context=&initial,.features=FEAT_WRITE,
    .statFile=deviceHandler_FAT_statFile,.readFile=fakeRead,
    .writeFile=deviceHandler_FAT_writeFile,.closeFile=wrapperClose
};
static void reset(void) {
    memset(disk,0,sizeof(disk)); memset(sizes,0,sizeof(sizes)); memset(exists,0,sizeof(exists));
    devices[DEVICE_CONFIG]=devices[DEVICE_CUR]=&device; device.context=&initial;
    device.features=FEAT_WRITE; device.quirks=0;
    failStat=failRead=allocFail=-1; shortWrite=failClose=writes=allocations=wrapperCloses=0;
    now=978307260; config_load_play_history();
}
static void seed(void) {
    reset(); assert(playHistory.available && playHistory.count==0);
    config_record_game_handoff("GMSE01",6);
    assert(writes==1 && playHistorySlot==0 && exists[0] && !exists[1]);
    assert(UIGameHistory_Find(&playHistory,"GMSE01",6)==(uint64_t)now);
    assert(wrapperCloses==2); /* startup only; handoff uses pure FAT close */
}
static void preserved(const char *old, u32 size) {
    assert(sizes[0]==size && memcmp(disk[0],old,size)==0);
}
int main(void) {
    char old[4096]; u32 oldSize; bool available;
    seed();
    oldSize=sizes[0]; memcpy(old,disk[0],oldSize);
    now+=60; shortWrite=1;
    config_record_game_handoff("GALE01",6);
    preserved(old,oldSize); assert(!playHistory.available);
    shortWrite=0; config_load_play_history();
    assert(playHistorySlot==0 && playHistory.count==1);
    assert(config_last_played("GMSE01",6,&available)==978307260 && available);
    assert(config_last_played("GALE01",6,&available)==0 && available);
    now+=60; config_record_game_handoff("GALE01",6);
    assert(playHistorySlot==1 && playHistory.generation==2 && playHistory.count==2);
    preserved(old,oldSize);
    config_load_play_history(); assert(playHistorySlot==1 && playHistory.count==2);

    seed(); oldSize=sizes[0]; memcpy(old,disk[0],oldSize);
    now+=60; failClose=1; config_record_game_handoff("GALE01",6);
    preserved(old,oldSize); assert(!playHistory.available); failClose=0;
    /* A fully durable second slot can still be recovered despite close error. */
    config_load_play_history(); assert(playHistory.available);
    for(int i=0;i<2;i++) {
        seed(); oldSize=sizes[0]; memcpy(old,disk[0],oldSize);
        allocations=0; allocFail=i; config_record_game_handoff("GALE01",6);
        preserved(old,oldSize); assert(writes==1 && !exists[1]);
    }
    reset(); failRead=0; exists[0]=true; sizes[0]=30;
    config_load_play_history(); assert(!playHistory.available);
    reset(); failStat=1; config_load_play_history(); assert(!playHistory.available);
    seed(); allocations=0; allocFail=0; config_load_play_history(); assert(!playHistory.available);
    seed(); exists[1]=true; sizes[1]=4096; config_load_play_history();
    assert(playHistory.available && playHistorySlot==0);
    seed(); devices[DEVICE_CUR]=NULL; config_record_game_handoff("GALE01",6); assert(writes==1);
    devices[DEVICE_CUR]=&device; device.context=NULL;
    config_record_game_handoff("GALE01",6); assert(writes==1);
    device.context=&initial; device.quirks=QUIRK_GCLOADER_WRITE_CONFLICT;
    config_record_game_handoff("GALE01",6); assert(writes==1);
    device.quirks=0; now=0; config_record_game_handoff("GALE01",6); assert(writes==1);
    now=978307260; device.features=0; config_record_game_handoff("GALE01",6); assert(writes==1);
    devices[DEVICE_CONFIG]=NULL;
    assert(config_last_played("GMSE01",6,&available)==0 && !available);
    puts("History adapter fault coverage: PASS");
    return 0;
}
'''

class PersistenceTests(unittest.TestCase):
    def test_actual_fat_inner_allocation_failure(self):
        # Link the complete vendored FatFs implementation, not a mock f_open:
        # its early NULL guard makes the driver's failed FFFIL allocation safe.
        fat = ROOT / "cube/swiss/source/fatfs"
        driver = (ROOT / "cube/swiss/source/devices/fat/deviceHandler-FAT.c").read_text()
        wrappers = "\n".join(extract_function(driver, marker) for marker in (
            "s32 deviceHandler_FAT_readFile(",
            "s32 deviceHandler_FAT_writeFile(",
            "s32 deviceHandler_FAT_closeFile(",
        ))
        harness = r'''
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include "ff.h"
#include "diskio.h"
typedef int32_t s32;
typedef uint32_t u32;
typedef struct {
    char name[1024]; FFFIL *ffsFp; uint64_t fileBase;
    u32 size, offset; int fileType; unsigned int blockSize;
} file_handle;
enum { IS_FILE = 1 };
static unsigned allocations;
static void *fail_alloc(size_t size) {
    assert(size == sizeof(FFFIL)); ++allocations; return NULL;
}
/* Any low-level operation would mean f_open failed to reject NULL early. */
DSTATUS disk_initialize(BYTE p) { (void)p; abort(); }
DSTATUS disk_status(BYTE p) { (void)p; abort(); }
DRESULT disk_shutdown(BYTE p) { (void)p; abort(); }
DRESULT disk_read(BYTE p, BYTE *b, LBA_t s, UINT n, BYTE o) {
    (void)p; (void)b; (void)s; (void)n; (void)o; abort();
}
DRESULT disk_write(BYTE p, const BYTE *b, LBA_t s, UINT n, BYTE o) {
    (void)p; (void)b; (void)s; (void)n; (void)o; abort();
}
DRESULT disk_ioctl(BYTE p, BYTE c, void *b) { (void)p; (void)c; (void)b; abort(); }
DWORD get_fattime(void) { abort(); }
int ff_mutex_create(int v) { (void)v; abort(); }
void ff_mutex_delete(int v) { (void)v; abort(); }
int ff_mutex_take(int v) { (void)v; abort(); }
void ff_mutex_give(int v) { (void)v; abort(); }
WCHAR ff_oem2uni(WCHAR c, WORD p) { (void)c; (void)p; abort(); }
WCHAR ff_uni2oem(DWORD c, WORD p) { (void)c; (void)p; abort(); }
DWORD ff_wtoupper(DWORD c) { (void)c; abort(); }
#define malloc fail_alloc
/* WRAPPERS */
#undef malloc
int main(void) {
    file_handle file = {.name="gcldr:/swiss/settings/play-history-1.ini"};
    char byte = 0;
    assert(f_open(NULL, file.name, FA_CREATE_ALWAYS | FA_WRITE) == FR_INVALID_OBJECT);
    assert(deviceHandler_FAT_writeFile(&file, &byte, 1u) == -1);
    assert(file.ffsFp == NULL && allocations == 1u);
    assert(deviceHandler_FAT_closeFile(&file) == 0);
    assert(deviceHandler_FAT_readFile(&file, &byte, 1u) == -1);
    assert(file.ffsFp == NULL && allocations == 2u);
    assert(deviceHandler_FAT_closeFile(&file) == 0);
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as temp:
            c, exe = Path(temp) / "fat-oom.c", Path(temp) / "fat-oom"
            c.write_text(harness.replace("/* WRAPPERS */", wrappers))
            flags = ["-std=c11", "-fsanitize=address,undefined",
                     "-fno-sanitize-recover=all", "-fno-omit-frame-pointer"]
            if os.uname().sysname == "Linux":
                flags += ["-fno-pie", "-no-pie"]
            subprocess.run(shlex.split(os.environ.get("CC", "cc")) + flags +
                           ["-I", str(fat), str(c), str(fat / "ff.c"),
                            "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

    def test_compiled_adapter_faults(self):
        source = CONFIG.read_text()
        markers = ["static int config_read_play_history_slot(",
                   "static void config_load_play_history(", "uint64_t config_last_played(",
                   "void config_record_game_handoff("]
        adapter = "\n".join(extract_function(source, marker) for marker in markers)
        with tempfile.TemporaryDirectory() as temp:
            c = Path(temp) / "history.c"
            exe = Path(temp) / "history"
            c.write_text(HARNESS.replace("/* ADAPTER */", adapter))
            flags = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-Wconversion",
                     "-Wsign-conversion", "-pedantic", "-fsanitize=address,undefined",
                     "-fno-sanitize-recover=all", "-fno-omit-frame-pointer"]
            if os.uname().sysname == "Linux":
                flags += ["-fno-pie", "-no-pie"]
            subprocess.run(shlex.split(os.environ.get("CC", "cc")) + flags +
                           ["-I", str(GUI), str(c), str(GUI / "ui_game_history.c"),
                            "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

    def test_handoff_contract(self):
        source = CONFIG.read_text()
        writer = extract_function(source, "void config_record_game_handoff(")
        for forbidden in ["config_set_device(", "config_unset_device(", "config_file_write(",
                          "config_load_play_history(", "->deleteFile(", "->init(",
                          "->deinit(", "->test(", "->setupFile("]:
            self.assertNotIn(forbidden, writer)
        swiss = (ROOT / "cube/swiss/source/swiss.c").read_text()
        # One write, where Swiss saves its recent list: before the fragment
        # table, FST and patches are prepared. Nothing may write the
        # card once setupFile has mapped the game's sectors.
        self.assertEqual(swiss.count("config_record_game_handoff("), 1)
        self.assertNotIn("config_record_game_handoff(", extract_function(swiss, "void load_app("))
        launch = extract_function(
            swiss, "static void load_game_with_context(gameflowLaunchContext_t *context) {")
        write = launch.index("config_record_game_handoff(")
        self.assertLess(launch.index("config_update_recent(true)"), write)
        for later in ["config_load_current(config)", "setupFile(&curFile, disc2File, NULL, -2)",
                      "check_game(&curFile", "->setupFile(&curFile, disc2File, filesToPatch"]:
            self.assertLess(write, launch.index(later), later)
        publish = extract_function(swiss, "static bool gameflowPublishDetail(")
        self.assertIn("config_last_played(", publish)
        self.assertIn("UI_GAME_SAVE_NOT_CHECKED", publish)
        self.assertNotIn("config_record_game_handoff(", publish)

if __name__ == "__main__":
    unittest.main()
