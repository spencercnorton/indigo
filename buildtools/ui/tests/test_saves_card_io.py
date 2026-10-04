#!/usr/bin/env python3
"""Run the real card driver and Memory Cards copy engine against two cards.

The CARD API model deliberately implements libogc2's fixed-code/string
boundary and its leading-0xff wildcard lookup. The production read, write,
erase, lookup and copy functions are extracted unchanged. Saves sharing a
filename must still keep their game and maker identity, including when a
failed write or read-back removes only the newly created destination.
"""

import argparse
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

from test_cheats_gx_stream import extract_function

ROOT = Path(__file__).resolve().parents[3]
DRIVER = ROOT / "cube/swiss/source/devices/memcard/deviceHandler-CARD.c"
SAVES = ROOT / "cube/swiss/source/gui/saves.c"

PRELUDE = r"""
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "ui_saves.h"
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;
typedef struct {
    s32 chn, fileno;
    u32 filelen;
    u8 permissions;
    char filename[32];
    u8 gamecode[4], company[2];
    bool showall;
} card_dir;
typedef struct { s32 chn, filenum; u32 offset, len; } card_file;
typedef struct {
    u8 gamecode[4], company[2];
    char filename[32];
    u32 len, time, icon_addr, comment_addr;
    u16 icon_fmt, icon_speed;
    u8 banner_fmt;
} card_stat;
typedef struct { u32 totalSpace; } device_info;
typedef struct file_handle file_handle;
typedef struct {
    file_handle *initial;
    s32 (*init)(file_handle *);
    device_info *(*info)(file_handle *);
    s32 (*readDir)(file_handle *, file_handle **, u32);
    s32 (*readFile)(file_handle *, void *, u32);
    s32 (*writeFile)(file_handle *, const void *, u32);
    s32 (*deleteFile)(file_handle *);
    s32 (*closeFile)(file_handle *);
    long long (*seekFile)(file_handle *, long long, u32);
} DEVICEHANDLER_INTERFACE;
struct file_handle {
    char name[1024];
    u32 size, offset;
    u8 other[128];
    DEVICEHANDLER_INTERFACE *device;
};
#define CARD_FILENAMELEN 32
#define CARD_MAXFILES 127
#define CARD_ERROR_READY 0
#define CARD_ERROR_NOFILE -4
#define CARD_ERROR_BUSY -1
#define CARD_ERROR_FATAL_ERROR -128
#define CARD_ERROR_INSSPACE -9
#define CARD_ERROR_NOENT -8
#define CARD_ERROR_EXIST -7
#define CARD_ERROR_NOCARD -3
#define DEVICE_HANDLER_SEEK_SET 0
#define SAVES_MAX_BYTES (16u * 1024u * 1024u)
#define SAVES_SYSTEM_BLOCKS 5
#define CARD_SPEED_FAST 1
#define CARD_BANNER_NONE 0
#define CARD_ICON_RGB 2
#define D_FAIL 0
#define print_debug(...) ((void)0)
#define DCFlushRange(p, n) ((void)0)
#define CARD_SetGameAndCompany() CARD_SetGamecode("SWIS"); CARD_SetCompany("S0")
typedef int uiDrawObj_t;
static uiDrawObj_t *DrawMessageBox(int style, const char *text) {
    (void)style; (void)text; return NULL;
}
static void DrawPublish(uiDrawObj_t *box) { (void)box; }
static void DrawDispose(uiDrawObj_t *box) { (void)box; }
static void wait_press_A(void) {}
static const char *cardError(int error) { (void)error; return "error"; }
static void *memalign(size_t alignment, size_t bytes) {
    (void)alignment; return malloc(bytes);
}
static char *getRelativeName(char *path) { return strrchr(path, '/') + 1; }
static file_handle initial_CARDA = { .name = "carda:/" };
static file_handle initial_CARDB = { .name = "cardb:/" };
static u32 card_sectorsize[2] = {8192, 8192};
static const u8 gamecube_rgb[1] = {0};
static bool isCopyGCIMode;
static struct { bool mounted; } places[2];

/* Fixed-length codes are libogc2 API filters; a rejected string length
 * leaves the filter as ff, which means any game or maker. */
static u8 gameFilter[4], makerFilter[2];
static void CARD_SetGamecode(const char *code) {
    memset(gameFilter, 0xff, sizeof(gameFilter));
    if(code != NULL && strlen(code) <= 4) memcpy(gameFilter, code, 4);
}
static void CARD_SetCompany(const char *code) {
    memset(makerFilter, 0xff, sizeof(makerFilter));
    if(code != NULL && strlen(code) <= 2) memcpy(makerFilter, code, 2);
}
typedef struct {
    bool live;
    card_dir dir;
    u8 bytes[8192];
    card_stat stat;
} record;
static record cards[2][8];
static bool failWrite, failReadBack, wrongReadIdentity;
static int writes, deletes, mounts;
static int lookup(int slot, const char *name) {
    for(int i = 0; i < 8; i++) {
        record *r = &cards[slot][i];
        if(r->live && !strncmp(r->dir.filename, name, 32) &&
           (gameFilter[0] == 0xff || !memcmp(r->dir.gamecode, gameFilter, 4)) &&
           (makerFilter[0] == 0xff || !memcmp(r->dir.company, makerFilter, 2)))
            return i;
    }
    return -1;
}
static s32 CARD_Open(int slot, const char *name, card_file *file) {
    int i = lookup(slot, name);
    if(i < 0) return CARD_ERROR_NOFILE;
    *file = (card_file){ .chn = slot, .filenum = i, .len = 8192 };
    return CARD_ERROR_READY;
}
static s32 CARD_Create(int slot, const char *name, u32 length, card_file *file) {
    assert(length == 8192);
    if(lookup(slot, name) >= 0) return CARD_ERROR_EXIST;
    for(int i = 0; i < 8; i++) {
        record *r = &cards[slot][i];
        if(r->live) continue;
        memset(r, 0, sizeof(*r)); r->live = true;
        r->dir.chn = slot; r->dir.fileno = i; r->dir.filelen = length;
        r->dir.showall = true;
        memcpy(r->dir.gamecode, gameFilter, 4);
        memcpy(r->dir.company, makerFilter, 2);
        memcpy(r->dir.filename, name, strnlen(name, 32));
        memcpy(r->stat.gamecode, gameFilter, 4);
        memcpy(r->stat.company, makerFilter, 2);
        memcpy(r->stat.filename, r->dir.filename, 32);
        r->stat.len = length;
        return CARD_Open(slot, name, file);
    }
    return CARD_ERROR_NOENT;
}
static s32 CARD_Read(card_file *file, void *buffer, u32 length, u32 at) {
    assert(at == 0 && length == 8192);
    memcpy(buffer, cards[file->chn][file->filenum].bytes, length);
    if(failReadBack) ((u8 *)buffer)[0] ^= 1;
    return CARD_ERROR_READY;
}
static s32 CARD_ReadUnaligned(card_file *file, void *buffer, u32 length, u32 at, int slot) {
    (void)slot; return CARD_Read(file, buffer, length, at);
}
static s32 CARD_Write(card_file *file, const void *buffer, u32 length, u32 at) {
    writes++; assert(at == 0 && length == 8192);
    memcpy(cards[file->chn][file->filenum].bytes, buffer, length);
    return failWrite ? CARD_ERROR_FATAL_ERROR : CARD_ERROR_READY;
}
static s32 CARD_GetStatus(int slot, int i, card_stat *stat) {
    *stat = cards[slot][i].stat;
    if(wrongReadIdentity) stat->gamecode[3] ^= 1;
    return CARD_ERROR_READY;
}
static s32 CARD_SetStatus(int slot, int i, const card_stat *stat) {
    record *r = &cards[slot][i];
    r->stat.banner_fmt = stat->banner_fmt; r->stat.time = stat->time;
    r->stat.icon_addr = stat->icon_addr; r->stat.icon_fmt = stat->icon_fmt;
    r->stat.icon_speed = stat->icon_speed; r->stat.comment_addr = stat->comment_addr;
    return CARD_ERROR_READY;
}
static s32 CARD_Close(card_file *file) { (void)file; return 0; }
static s32 CARD_Delete(int slot, const char *name) {
    int i = lookup(slot, name);
    if(i < 0) return CARD_ERROR_NOFILE;
    cards[slot][i].live = false; deletes++; return 0;
}
static bool findFile(file_handle *file) { (void)file; assert(false); return false; }
static s32 mounted(file_handle *file) { (void)file; mounts++; return 0; }
static device_info *info(file_handle *file) {
    static device_info room = { 64u * 8192u }; (void)file; return &room;
}
static s32 closeFile(file_handle *file) { (void)file; return 0; }
static long long seekFile(file_handle *file, long long at, u32 type) {
    assert(type == DEVICE_HANDLER_SEEK_SET); file->offset = (u32)at; return at;
}
static void concatf_path(char *out, const char *root, const char *fmt,
                         int length, const char *name) {
    (void)fmt; snprintf(out, 1024, "%s%.*s", root, length, name);
}
static DEVICEHANDLER_INTERFACE devices[2];
static DEVICEHANDLER_INTERFACE *slotDevice(int slot) { return &devices[slot]; }
static const char *slotName(int slot) { return slot ? "Slot B" : "Slot A"; }
static bool isCard(DEVICEHANDLER_INTERFACE *device) {
    return device == &devices[0] || device == &devices[1];
}
static void handle(file_handle *file, int slot, int i) {
    memset(file, 0, sizeof(*file));
    snprintf(file->name, sizeof(file->name), "card%c:/%.*s", 'a' + slot,
             32, cards[slot][i].dir.filename);
    file->device = &devices[slot]; file->size = 8192;
    memcpy(file->other, &cards[slot][i].dir, sizeof(card_dir));
}
static s32 readDir(file_handle *root, file_handle **entries, u32 type) {
    (void)type; int slot = root == &initial_CARDB, n = 1;
    *entries = calloc(9, sizeof(file_handle)); assert(*entries != NULL);
    for(int i = 0; i < 8; i++) if(cards[slot][i].live)
        handle(&(*entries)[n++], slot, i);
    return n;
}
static void setGCIInfo(const void *buffer);
static void setCopyGCIMode(bool mode);
"""

MAIN = r"""
static void add(int slot, int i, const char *game, const char *maker, u8 byte) {
    CARD_SetGamecode(game); CARD_SetCompany(maker);
    card_file file; assert(CARD_Create(slot, "same-name", 8192, &file) == 0);
    assert(file.filenum == i);
    memset(cards[slot][i].bytes, byte, 8192);
}
static void reset(void) {
    memset(cards, 0, sizeof(cards)); writes = deletes = mounts = 0;
    failWrite = failReadBack = wrongReadIdentity = false;
    devices[0] = (DEVICEHANDLER_INTERFACE){ &initial_CARDA, mounted, info,
        readDir, deviceHandler_CARD_readFile, deviceHandler_CARD_writeFile,
        deviceHandler_CARD_deleteFile, closeFile, seekFile };
    devices[1] = devices[0]; devices[1].initial = &initial_CARDB;
}
static void verifySurvivor(int slot, u8 byte) {
    assert(cards[slot][0].live);
    for(int i = 0; i < 8192; i++) assert(cards[slot][0].bytes[i] == byte);
}
static void readErase(const char *game, const char *maker) {
    reset(); add(0, 0, "GALE", "01", 'E'); add(0, 1, game, maker, 'P');
    file_handle file; handle(&file, 0, 1);
    card_dir *dir = (card_dir *)file.other;
    if(game[0] != '\0' && maker[0] != '\0') {
        assert(strlen((char *)dir->gamecode) == 7);
        assert(strlen((char *)dir->company) == 3);
    }
    u32 length = 0; u8 *data = saveRead(&file, &length);
    assert(data != NULL && length == 8256);
    assert(!memcmp(data, game, 4) && !memcmp(data + 4, maker, 2));
    for(int i = 64; i < 8256; i++) assert(data[i] == 'P');
    free(data);
    assert(deviceHandler_CARD_deleteFile(&file) == 0);
    assert(!cards[0][1].live && deletes == 1); verifySurvivor(0, 'E');
}
static void copyCases(const char *game, const char *maker) {
    for(int failure = 0; failure < 4; failure++) {
        reset(); add(0, 0, "GALE", "01", 'E'); add(0, 1, game, maker, 'P');
        add(1, 0, "GALE", "01", 'E');
        file_handle source; handle(&source, 0, 1);
        u32 length = 0; u8 *data = saveRead(&source, &length);
        assert(data != NULL);
        /* readFile builds its GCI's integer fields in the target's native
         * big-endian layout. Normalise only the block count on this host
         * before ui_saves.c's deliberately byte-based parser uses it. */
        data[0x38] = 0; data[0x39] = 1;
        failWrite = failure == 1; failReadBack = failure == 2;
        wrongReadIdentity = failure == 3;
        char why[160];
        bool ok = cardWrite(1, data, data + 64, length - 64, why, sizeof(why));
        assert(ok == (failure == 0));
        verifySurvivor(1, 'E');
        assert(cards[1][1].live == ok);
        assert(deletes == (ok ? 0 : 1));
        if(ok) {
            /* Move uses this same source handle only after cardWrite succeeds. */
            assert(deviceHandler_CARD_deleteFile(&source) == 0);
            assert(!cards[0][1].live); verifySurvivor(0, 'E');
            assert(!memcmp(cards[1][1].dir.gamecode, game, 4));
            assert(!memcmp(cards[1][1].dir.company, maker, 2));
            for(int i = 0; i < 8192; i++) assert(cards[1][1].bytes[i] == 'P');
            /* Copying the exact identity again never writes or erases it. */
            int beforeWrites = writes, beforeDeletes = deletes;
            assert(!cardWrite(1, data, data + 64, length - 64, why, sizeof(why)));
            assert(writes == beforeWrites && deletes == beforeDeletes);
        }
        free(data);
    }
}
static void wildcardCases(void) {
    for(int position = 0; position <= 4; position += 4) {
        reset(); add(1, 0, "GALE", "01", 'E');
        u8 entry[64] = {0}, blocks[8192]; char why[160];
        memcpy(entry, "GALE01", 6); memcpy(entry + 8, "same-name", 9);
        entry[0x39] = 1; entry[position] = 0xff; memset(blocks, 'P', sizeof(blocks));
        assert(!cardWrite(1, entry, blocks, sizeof(blocks), why, sizeof(why)));
        assert(writes == 0 && deletes == 0 && mounts == 0); verifySurvivor(1, 'E');
        /* The driver's public copy entry point has the same safeguard. */
        file_handle dest = { .name = "cardb:/same-name" };
        setGCIInfo(entry);
        assert(deviceHandler_CARD_writeFile(&dest, blocks, sizeof(blocks)) < 0);
        setGCIInfo(NULL);
        assert(writes == 0 && deletes == 0); verifySurvivor(1, 'E');
        handle(&dest, 1, 0);
        ((card_dir *)dest.other)->gamecode[position == 0 ? 0 : 3] =
            position == 0 ? 0xff : 'E';
        if(position == 4) ((card_dir *)dest.other)->company[0] = 0xff;
        assert(deviceHandler_CARD_deleteFile(&dest) < 0);
        u32 length = 0; assert(saveRead(&dest, &length) == NULL);
        assert(writes == 0 && deletes == 0); verifySurvivor(1, 'E');
    }
}
int main(void) {
    readErase("GALP", "01"); readErase("GALE", "02");
    readErase("\0ABC", "\0D");
    copyCases("GALP", "01"); copyCases("GALE", "02");
    copyCases("\0ABC", "\0D"); wildcardCases();
    puts("card identity: reads, Erase, Copy, Move, both rollback paths and wildcards PASS");
    return 0;
}
"""


def harness(driver=None, saves=None):
    driver = DRIVER.read_text() if driver is None else driver
    saves = SAVES.read_text() if saves is None else saves
    header = (DRIVER.parent / "deviceHandler-CARD.h").read_text()
    gci = header[header.index("typedef struct {"):header.index(" GCI;") + 5]
    prelude = PRELUDE
    raw_reader = "static bool readSaveAt(" in saves
    if raw_reader:
        prelude = prelude.replace("static struct { bool mounted; } places[2];", r"""
#include "ui_saves_raw.h"
#define SAVES_TABS 4
#define SAVES_TAB_FOLDER 2
typedef struct {
    bool mounted, rawOpen;
    file_handle rawImage, *entries;
    uiSavesRawCard_t *rawCard;
    int entryCount;
} savesPlace_t;
static savesPlace_t places[SAVES_TABS];
static bool SavesRaw_ReadGci(file_handle *image, const uiSavesRawCard_t *card,
    unsigned ordinal, uint32_t offset, void *destination, uint32_t length) {
    (void)image; (void)card; (void)ordinal; (void)offset;
    (void)destination; (void)length;
    assert(!"physical-card lane must not read a RAW image"); return false;
}
""")
    pieces = [prelude, gci, "static GCI *gciInfo;"]
    if "static bool CARD_SetEntryIdentity(" in driver:
        pieces.append(extract_function(driver, "static bool CARD_SetEntryIdentity("))
    for marker in ("void setGCIInfo(", "void setCopyGCIMode(",
                   "s32 deviceHandler_CARD_readFile(", "s32 deviceHandler_CARD_writeFile(",
                   "s32 deviceHandler_CARD_deleteFile("):
        pieces.append(extract_function(driver, marker))
    if raw_reader:
        for marker in ("static savesPlace_t *rawSource(", "static bool readSaveAt("):
            pieces.append(extract_function(saves, marker))
    for marker in ("static u8 *saveRead(", "static file_handle *cardFind(",
                   "static const char *cardWhy(", "static bool cardWrite("):
        pieces.append(extract_function(saves, marker))
    return "\n".join(pieces + [MAIN])


def main():
    global ROOT, DRIVER, SAVES
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--source-root", type=Path, default=ROOT,
                        help="Review a separate source worktree without editing it")
    args = parser.parse_args()
    ROOT = args.source_root.resolve()
    DRIVER = ROOT / "cube/swiss/source/devices/memcard/deviceHandler-CARD.c"
    SAVES = ROOT / "cube/swiss/source/gui/saves.c"
    with tempfile.TemporaryDirectory(prefix="indigo-card-io-") as temp:
        source = Path(temp) / "card_io.c"
        binary = Path(temp) / "card_io"
        source.write_text(harness())
        flags = ["-std=gnu11", "-Wall", "-Wextra", "-Werror", "-Wno-sign-compare"]
        if args.sanitize:
            flags += ["-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                      "-fno-omit-frame-pointer"]
            if sys.platform.startswith("linux"):
                flags += ["-fno-pie", "-no-pie"]
        subprocess.run(shlex.split(os.environ.get("CC", "cc")) + flags +
                       ["-I", str(ROOT / "cube/swiss/source/gui"), str(source),
                        str(ROOT / "cube/swiss/source/gui/ui_saves.c"), "-o", str(binary)],
                       check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
