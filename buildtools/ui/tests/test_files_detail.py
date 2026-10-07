#!/usr/bin/env python3
"""A game's Detail from the File Browser: the real window it is built from.

swiss.c's filesDetailSnapshot makes a one-game Library window of the File
Browser's focused entry, which gameflowLoadImageWithContext then opens as the
Library's Detail. Compiled here with the record filler and the devices
mocked, it must give a window FrameBufferMagic.c's own check takes
(_GameflowSnapshotValid, copied in), whose selected record
(gameflowSelectedRecord, copied in) is that entry with the ID its disc header
gave, at the first, a middle and the last row. filesOpensDetail opens Detail
for disc images on a Source that starts them, and nothing else. Every rule
has a mutant that must fail it.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
GUI = ROOT / "cube/swiss/source/gui"


def function(source: str, marker: str) -> str:
    start = source.index(marker)
    opening = source.index("{", start)
    depth = 0
    for end in range(opening, len(source)):
        depth += (source[end] == "{") - (source[end] == "}")
        if depth == 0:
            return source[start:end + 1]
    raise AssertionError(marker)


STUBS = r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui_gameflow.h"
#include "ui_gameflow_library.h"
#include "ui_gameflow_resolver.h"
typedef uint8_t u8;
typedef int8_t s8;
typedef uint32_t u32;
typedef uint64_t u64;
#define BNR_PIXELDATA_LEN (96*32*2)
#define IS_FILE 0
#define IS_DIR 1
#define IS_SPECIAL 2
#define FEAT_BOOT_GCM 0x4
#define DEVICE_CUR 0
typedef struct { const char *name; u32 features; } DEVICEHANDLER_INTERFACE;
typedef struct { char name[1024]; int fileType; char metaId[8]; } file_handle;
static DEVICEHANDLER_INTERFACE sd = {"SD Card", FEAT_BOOT_GCM}, rom = {"Qoob", 0};
static DEVICEHANDLER_INTERFACE *devices[1] = {&sd};
static int curSelection, entryCount;
static u32 gameflowSnapshotGeneration;
static int getSortedDirEntryCount(void) { return entryCount; }
static const char *DeviceDisplayName(DEVICEHANDLER_INTERFACE *device) { return device->name; }
'''

FILLER = r'''
/* The record as gameflowSnapshotRecord leaves it: valid, titled, and with
 * whatever ID the entry's meta had (perhaps none, perhaps stale). */
static void gameflowSnapshotRecord(uiGameflowCardSnapshot_t *record,
	file_handle *file, uiGameflowLibraryMode_t mode)
{
	assert(mode == UI_GAMEFLOW_LIBRARY_IMAGE_FILES);
	record->flags = UI_GAMEFLOW_CARD_VALID;
	snprintf(record->title, sizeof(record->title), "%.95s", file->name);
	memcpy(record->gameId, file->metaId, sizeof(record->gameId));
}
'''

TESTS = r'''
static file_handle entry(const char *name, int type, const char *meta)
{
	file_handle file;
	memset(&file, 0, sizeof(file));
	snprintf(file.name, sizeof(file.name), "%s", name);
	file.fileType = type;
	snprintf(file.metaId, sizeof(file.metaId), "%s", meta);
	return file;
}

static uiGameflowRenderSnapshot_t snapshot;

static void window(int count, int selected, const char *meta)
{
	uiGameflowResolverEntry_t header;
	file_handle game = entry("sd:/games/Copper Orchard [GCOE01].iso", IS_FILE, meta);
	const uiGameflowCardSnapshot_t *record;
	u32 generation = gameflowSnapshotGeneration;

	memset(&header, 0, sizeof(header));
	memcpy(header.gameId, "GCOE01", 7);
	header.headerValid = true;
	memset(&snapshot, 0xA5, sizeof(snapshot));
	entryCount = count;
	curSelection = selected;
	filesDetailSnapshot(&snapshot, &game, &header);
	assert(_GameflowSnapshotValid(&snapshot));
	record = gameflowSelectedRecord(&snapshot);
	assert(record == &snapshot.records[0]);
	assert(snapshot.recordCount == 1u);
	assert(snapshot.selection.itemCount == (u32)count);
	assert(snapshot.selection.selectedIndex == (u32)selected);
	assert(snapshot.selection.generation == generation + 1u);
	assert(snapshot.layout == UI_GAMEFLOW_LAYOUT_HORIZONTAL && snapshot.columns == 0u);
	assert(record->libraryIndex == (u32)selected && record->relativeSlot == 0);
	/* The ID the Library's identity check and the posters go by is the
	 * header's, whatever the meta said. */
	assert(strcmp(record->gameId, "GCOE01") == 0);
	assert(strcmp(record->title, game.name) == 0);
	assert(strcmp(snapshot.deviceName, "SD Card") == 0);
	assert(snapshot.folder[0] == '\0' && snapshot.description[0] == '\0');
	assert(record->column == 0u && record->subfolder == 0u && record->size == 0u);
	assert(snapshot.selection.directionHint == 0 && !snapshot.selection.snapTransition);
}

int main(void)
{
	static const char *const games[] = {"x.iso", "x.gcm", "x.tgc", "x.fdi", "X.ISO"};
	static const char *const others[] = {"x.dol", "x.gcz", "x.rvz", "x.elf", "x.mp3", "x.iso.txt"};
	size_t i;

	window(1, 0, "");
	window(18, 0, "GOLD01");
	window(18, 9, "");
	window(18, 17, "GCOE01");
	window(2000, 1999, "");
	for(i = 0; i < sizeof(games) / sizeof(games[0]); i++) {
		file_handle file = entry(games[i], IS_FILE, "");
		devices[DEVICE_CUR] = &sd;
		assert(filesOpensDetail(&file));
		devices[DEVICE_CUR] = &rom;
		assert(!filesOpensDetail(&file));
		file.fileType = IS_DIR;
		devices[DEVICE_CUR] = &sd;
		assert(!filesOpensDetail(&file));
	}
	for(i = 0; i < sizeof(others) / sizeof(others[0]); i++) {
		file_handle file = entry(others[i], IS_FILE, "");
		assert(!filesOpensDetail(&file));
	}
	puts("File Browser Detail window tests passed");
	return 0;
}
'''


def build_source(swiss: str, frame: str, model: str) -> str:
    sizes = "\n".join(re.findall(r"^#define UI_GAMEFLOW_(?:RENDER_SLOTS|TITLE_LENGTH|COMPANY_LENGTH|"
                                 r"FACTS_LENGTH|DESCRIPTION_LENGTH|CARD_\w+) .*", model, re.MULTILINE))
    record_end = model.index("} uiGameflowCardSnapshot_t;") + len("} uiGameflowCardSnapshot_t;")
    record_start = model.rfind("typedef struct {", 0, record_end)
    window_end = model.index("} uiGameflowRenderSnapshot_t;") + len("} uiGameflowRenderSnapshot_t;")
    return "\n".join([
        STUBS, sizes, model[record_start:window_end], FILLER,
        function(swiss, "static void gameflowCopyText("),
        function(swiss, "static const uiGameflowCardSnapshot_t *gameflowSelectedRecord("),
        function(frame, "static bool _GameflowSnapshotValid("),
        function(swiss, WINDOW),
        function(swiss, OPENS),
        TESTS])


def run(source: str, work: Path, sanitize: bool, name: str) -> bool:
    path = work / f"{name}.c"
    binary = work / name
    path.write_text(source)
    flags = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function", "-g"]
    if sanitize:
        flags += ["-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer"]
    build = subprocess.run([os.environ.get("CC", "cc"), *flags, f"-I{GUI}", "-o", str(binary), str(path),
                            str(GUI / "ui_gameflow_library.c")], capture_output=True, text=True)
    if build.returncode:
        return False
    return subprocess.run([str(binary)], capture_output=True, text=True).returncode == 0


WINDOW = "static void filesDetailSnapshot("
OPENS = "static bool filesOpensDetail(const file_handle *entry)\n{"

MUTANTS = (
    ("the window has no record", "\tsnapshot->recordCount = 1u;\n", ""),
    ("the record is another row", "\trecord->libraryIndex = (u32)curSelection;\n",
     "\trecord->libraryIndex = 0u;\n"),
    ("the selection is another row", "\tsnapshot->selection.selectedIndex = (u32)curSelection;\n",
     "\tsnapshot->selection.selectedIndex = 0u;\n"),
    ("the meta's ID, not the header's", "\tgameflowCopyText(record->gameId, sizeof(record->gameId), header->gameId,\n",
     "\tif(!record->gameId[0]) gameflowCopyText(record->gameId, sizeof(record->gameId), header->gameId,\n"),
    ("the old generation", "\tsnapshot->selection.generation = ++gameflowSnapshotGeneration;\n",
     "\tsnapshot->selection.generation = gameflowSnapshotGeneration;\n"),
    ("a grid layout", "\tsnapshot->layout = UI_GAMEFLOW_LAYOUT_HORIZONTAL;\n",
     "\tsnapshot->layout = UI_GAMEFLOW_LAYOUT_GRID;\n"),
    ("the record keeps stale bytes", "\t\tsizeof(snapshot->records[0]));\n", "\t\t0);\n"),
    ("the window keeps stale bytes", "\tmemset(snapshot, 0, offsetof(uiGameflowRenderSnapshot_t, records) +\n",
     "\tmemset(snapshot, 0, offsetof(uiGameflowRenderSnapshot_t, folder) - 64 +\n"),
    ("Detail on a Source that can't start a disc", "\t\t(devices[DEVICE_CUR]->features & FEAT_BOOT_GCM);",
     "\t\ttrue;"),
    ("Detail for a folder", "\treturn entry->fileType == IS_FILE &&\n", "\treturn true &&\n"),
)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    swiss = (ROOT / "cube/swiss/source/swiss.c").read_text()
    frame = (GUI / "FrameBufferMagic.c").read_text()
    model = (GUI / "FrameBufferMagic.h").read_text()
    with tempfile.TemporaryDirectory() as directory:
        work = Path(directory)
        if not run(build_source(swiss, frame, model), work, args.sanitize, "detail"):
            source = build_source(swiss, frame, model)
            (work / "detail.c").write_text(source)
            build = subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                                    "-Wno-unused-function", f"-I{GUI}", "-o", str(work / "detail"),
                                    str(work / "detail.c"), str(GUI / "ui_gameflow_library.c")],
                                   capture_output=True, text=True)
            print(build.stderr[-3000:], file=sys.stderr)
            if not build.returncode:
                result = subprocess.run([str(work / "detail")], capture_output=True, text=True)
                print(result.stdout + result.stderr, file=sys.stderr)
            return 1
        # Mutants run plain: a mutant only has to fail.
        for n, (label, old, new) in enumerate(MUTANTS):
            body = function(swiss, OPENS if "fileType" in old or "FEAT" in old else WINDOW)
            assert body.count(old) == 1, f"mutation anchor missing: {label}"
            mutated = swiss.replace(body, body.replace(old, new, 1), 1)
            if run(build_source(mutated, frame, model), work, False, f"mutant{n}"):
                raise AssertionError(f"mutant escaped the File Browser Detail window test: {label}")
    print(f"File Browser Detail window tests passed ({len(MUTANTS)} mutants rejected)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
