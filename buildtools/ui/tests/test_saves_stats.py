#!/usr/bin/env python3
"""Exercise the real stats collector and RAW adapter with read-only devices."""

import argparse
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
GUI = ROOT / "cube/swiss/source/gui"

HARNESS = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "ui_saves_metadata.h"
#include "ui_saves_raw.h"
typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;
typedef struct file_handle file_handle;
typedef struct device DEVICEHANDLER_INTERFACE;
struct file_handle {
	char name[1024]; u32 size, offset; unsigned fileType;
	const u8 *data; DEVICEHANDLER_INTERFACE *device;
};
struct device {
	file_handle *initial; u32 location;
	s32 (*init)(file_handle *); s32 (*deinit)(file_handle *);
	s32 (*readDir)(file_handle *, file_handle **, u32);
	int64_t (*seekFile)(file_handle *, int64_t, unsigned);
	s32 (*readFile)(file_handle *, void *, u32);
	s32 (*closeFile)(file_handle *);
};
typedef struct {
	u8 gamecode[4], company[2], filename[32];
	u32 filelen; int fileno;
} card_dir;
typedef struct {
	u8 gamecode[4], company[2], filename[32];
	u32 len, time;
} card_stat;
#define IS_FILE 1u
#define IS_DIR 2u
#define DEVICE_HANDLER_SEEK_SET 0u
#define DEVICE_CUR 0
#define DEVICE_CONFIG 1
#define LOC_MEMCARD_SLOT_A 1u
#define LOC_MEMCARD_SLOT_B 2u
#define CARD_ERROR_READY 0
#define CARD_ERROR_BUSY (-1)
#define CARD_ERROR_NOCARD (-3)
#define CARD_ERROR_NOFILE (-4)
#define CARD_ERROR_WRONGDEVICE (-5)
static DEVICEHANDLER_INTERFACE __device_card_a, __device_card_b, sd, occupied;
static DEVICEHANDLER_INTERFACE *devices[2];
static file_handle initial[3];
static u8 *image, *gci;
static size_t imageSize, gciSize;
static bool configOk, directoryError, shortRead, seekError, invalidSave;
static bool statusError, statusMismatch, enumError;
static unsigned listingMode, fileReads, fileCloses, configCloses, probes[2];
static unsigned mounts[2], unmounts[2], statusReads, maximumRead;
static int probeResult[2];
static card_dir physical;
static unsigned enumeration[2];
static u32 updated;
static void *host_memalign(size_t alignment, size_t bytes)
{
	void *p = NULL;
	assert(alignment == 32u);
	return posix_memalign(&p, alignment, bytes) == 0 ? p : NULL;
}
#define memalign host_memalign
static const char *saves_folder(void) { return "swiss/saves"; }
static bool config_set_device(void)
{
	devices[DEVICE_CONFIG] = configOk ? &sd : NULL;
	return configOk;
}
static void config_unset_device(void) { ++configCloses; }
static const char *getRelativeName(const char *name)
{
	const char *p = strrchr(name, '/'); return p ? p + 1 : name;
}
static void concat_path(char *out, const char *root, const char *path)
{
	snprintf(out, 1024u, "%s%s", root, path);
}
static s32 CARD_ProbeEx(s32 slot, void *a, void *b)
{
	assert(slot >= 0 && slot < 2 && !a && !b);
	++probes[slot]; return probeResult[slot];
}
static s32 mount(file_handle *f)
{
	unsigned slot = f == &initial[0] ? 0u : 1u;
	++mounts[slot]; return 0;
}
static s32 unmount(file_handle *f)
{
	unsigned slot = f == &initial[0] ? 0u : 1u;
	++unmounts[slot]; return 0;
}
static s32 CARD_FindFirst(s32 slot, card_dir *dir, bool all)
{
	assert(all); enumeration[slot] = 0u;
	if(enumError) return CARD_ERROR_BUSY;
	*dir = physical; return CARD_ERROR_READY;
}
static s32 CARD_FindNext(card_dir *dir)
{
	(void)dir; return CARD_ERROR_NOFILE;
}
static s32 CARD_GetStatus(s32 slot, int file, card_stat *status)
{
	assert(slot == 0 && file == physical.fileno); ++statusReads;
	if(statusError) return CARD_ERROR_BUSY;
	memcpy(status->gamecode, physical.gamecode, 4u);
	memcpy(status->company, physical.company, 2u);
	memcpy(status->filename, physical.filename, 32u);
	status->len = physical.filelen; status->time = updated + 500u;
	if(statusMismatch) status->company[1] ^= 1u;
	return CARD_ERROR_READY;
}
static int64_t seek(file_handle *f, int64_t at, unsigned origin)
{
	assert(origin == 0u && at >= 0 && (uint64_t)at <= f->size);
	if(seekError) return -1;
	f->offset = (u32)at; return at;
}
static s32 readFile(file_handle *f, void *out, u32 bytes)
{
	assert(((uintptr_t)out & 31u) == 0u && bytes <= f->size - f->offset);
	assert(bytes <= UI_SAVES_RAW_METADATA_SIZE);
	++fileReads; if(bytes > maximumRead) maximumRead = bytes;
	if(shortRead) return (s32)bytes - 1;
	memcpy(out, f->data + f->offset, bytes); f->offset += bytes;
	return (s32)bytes;
}
static s32 closeFile(file_handle *f) { (void)f; ++fileCloses; return 0; }
static s32 readDir(file_handle *f, file_handle **files, u32 kind)
{
	unsigned count = listingMode == 1u ? 17u : listingMode == 2u ? 300u : 5u;
	unsigned i;
	assert(kind == UINT32_MAX && !strcmp(f->name, "sd:/swiss/saves"));
	if(directoryError) return -1;
	*files = calloc(count, sizeof(**files)); assert(*files);
	for(i = 0u; i < count; i++) {
		file_handle *entry = &(*files)[i];
		entry->fileType = IS_FILE; entry->device = &sd;
		entry->data = gci; entry->size = (u32)gciSize;
		snprintf(entry->name, sizeof(entry->name), "sd:/swiss/saves/save%u.gci", i);
		if(listingMode == 1u || (listingMode == 0u && i == 1u)) {
			snprintf(entry->name, sizeof(entry->name), "sd:/swiss/saves/card%u.raw", i);
			entry->data = image; entry->size = (u32)imageSize;
		}
		if(listingMode == 0u && i == 2u) {
			strcpy(entry->name, "sd:/swiss/saves/._save.gci");
		}
		if(listingMode == 0u && i == 3u) entry->fileType = IS_DIR;
		if(listingMode == 0u && i == 4u) {
			strcpy(entry->name, "sd:/swiss/saves/readme.txt");
		}
		if(invalidSave && i == 0u) entry->size = 17u;
	}
	return (s32)count;
}
static void reset(void)
{
	configOk = true; directoryError = shortRead = seekError = invalidSave = false;
	statusError = statusMismatch = enumError = false; listingMode = 0u;
	fileReads = fileCloses = configCloses = maximumRead = statusReads = 0u;
	memset(probes, 0, sizeof(probes)); memset(mounts, 0, sizeof(mounts));
	memset(unmounts, 0, sizeof(unmounts));
	probeResult[0] = CARD_ERROR_READY; probeResult[1] = CARD_ERROR_NOCARD;
	devices[0] = NULL; devices[1] = NULL;
}
static u8 *load(const char *path, size_t *size)
{
	FILE *f = fopen(path, "rb"); long length; u8 *data;
	assert(f && fseek(f, 0, SEEK_END) == 0); length = ftell(f); assert(length > 0);
	assert(fseek(f, 0, SEEK_SET) == 0); *size = (size_t)length;
	data = malloc(*size); assert(data && fread(data, 1u, *size, f) == *size);
	assert(fclose(f) == 0); return data;
}
'''

MAIN = r'''
int main(int argc, char **argv)
{
	uiSavesGameStats_t stats; char id[6], wrong[6]; unsigned i;
	assert(argc == 3); image = load(argv[1], &imageSize); gci = load(argv[2], &gciSize);
	memcpy(id, gci, 6u); memcpy(wrong, id, 6u); wrong[5] = wrong[5] == '0' ? '1' : '0';
	updated = (u32)gci[0x28] << 24 | (u32)gci[0x29] << 16 |
		(u32)gci[0x2a] << 8 | gci[0x2b];
	memset(&physical, 0, sizeof(physical));
	memcpy(physical.gamecode, id, 4u); memcpy(physical.company, id + 4, 2u);
	memcpy(physical.filename, gci + 8, 32u); physical.filelen = 2u * 8192u;
	strcpy(initial[2].name, "sd:/");
	__device_card_a.initial = &initial[0]; __device_card_b.initial = &initial[1];
	__device_card_a.init = __device_card_b.init = mount;
	__device_card_a.deinit = __device_card_b.deinit = unmount;
	sd.initial = &initial[2]; sd.readDir = readDir; sd.seekFile = seek;
	sd.readFile = readFile; sd.closeFile = closeFile;
	reset(); Saves_CollectGameStats(id, &stats);
	assert(stats.saves == 3u && stats.blocks == 6u);
	assert(stats.sourceSaves[0] == 1u && stats.sourceSaves[1] == 0u && stats.sourceSaves[2] == 2u);
	assert(!stats.partial && stats.checkedSources == 5u);
	assert(stats.updatedKnown && stats.latestUpdated == updated + 500u);
	assert(fileReads == 2u && fileCloses == 2u && maximumRead == 40960u);
	assert(configCloses == 1u && unmounts[0] == 1u && mounts[1] == 0u);
	reset(); Saves_CollectGameStats(wrong, &stats);
	assert(stats.saves == 0u && stats.blocks == 0u && !stats.updatedKnown);
	reset(); statusError = true; Saves_CollectGameStats(id, &stats);
	assert(stats.saves == 3u && stats.partial && stats.latestUpdated == updated);
	reset(); statusMismatch = true; Saves_CollectGameStats(id, &stats);
	assert(stats.saves == 3u && stats.partial && stats.latestUpdated == updated);
	reset(); invalidSave = true; Saves_CollectGameStats(id, &stats);
	assert(stats.saves == 2u && stats.blocks == 4u && stats.partial);
	reset(); shortRead = true; Saves_CollectGameStats(id, &stats);
	assert(stats.saves == 1u && stats.partial && fileCloses == 2u);
	reset(); seekError = true; Saves_CollectGameStats(id, &stats);
	assert(stats.saves == 1u && stats.partial && fileReads == 0u && fileCloses == 2u);
	reset(); configOk = false; Saves_CollectGameStats(id, &stats);
	assert(stats.saves == 1u && stats.partial && fileReads == 0u && configCloses == 0u);
	reset(); directoryError = true; Saves_CollectGameStats(id, &stats);
	assert(stats.saves == 1u && stats.partial && stats.checkedSources == 1u);
	reset(); enumError = true; Saves_CollectGameStats(id, &stats);
	assert(stats.saves == 2u && stats.partial && unmounts[0] == 1u);
	reset(); devices[0] = &occupied; occupied.location = LOC_MEMCARD_SLOT_A;
	Saves_CollectGameStats(id, &stats);
	assert(stats.saves == 2u && probes[0] == 0u && mounts[0] == 0u);
	reset(); devices[0] = &__device_card_a; Saves_CollectGameStats(id, &stats);
	assert(stats.saves == 3u && unmounts[0] == 0u);
	reset(); probeResult[0] = CARD_ERROR_BUSY; Saves_CollectGameStats(id, &stats);
	assert(stats.saves == 2u && stats.partial && probes[0] == 1u && mounts[0] == 0u);
	reset(); listingMode = 1u; probeResult[0] = CARD_ERROR_NOCARD;
	Saves_CollectGameStats(id, &stats);
	assert(stats.saves == 16u && stats.blocks == 32u && stats.partial && fileReads == 16u);
	reset(); listingMode = 2u; probeResult[0] = CARD_ERROR_NOCARD;
	Saves_CollectGameStats(id, &stats);
	assert(stats.saves == 256u && stats.partial && fileReads == 256u);
	reset(); for(i = 0u; i < 6u; i++) wrong[i] = '\0';
	Saves_CollectGameStats(wrong, &stats);
	assert(stats.saves == 0u && stats.checkedSources == 0u && fileReads == 0u && probes[0] == 0u);
	Saves_CollectGameStats(NULL, &stats); assert(stats.saves == 0u);
	Saves_CollectGameStats(id, NULL);
	free(image); free(gci);
	puts("save stats: exact IDs, real RAW metadata, bounded reads, partial failures and retained mounts PASS");
	return 0;
}
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location(
        "stats_fixture", ROOT / "buildtools/ui/qa/make_test_saves.py")
    fixture = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = fixture
    spec.loader.exec_module(fixture)
    image, entries = fixture.virtual_card()
    sources = []
    for name in ("saves_raw.c", "saves_stats.c"):
        sources.append("\n".join(line for line in (GUI / name).read_text().splitlines()
                                  if not line.startswith("#include")))
    with tempfile.TemporaryDirectory(prefix="save-stats-") as tmp:
        path = Path(tmp)
        (path / "fixture.raw").write_bytes(image)
        (path / "fixture.gci").write_bytes(entries[0])
        (path / "test.c").write_text(HARNESS + "\n".join(sources) + MAIN)
        command = [os.environ.get("CC", "cc"), "-std=c11", "-D_POSIX_C_SOURCE=200112L",
                   "-Wall", "-Wextra", "-Werror", "-g", "-I", str(GUI),
                   str(path / "test.c"), str(GUI / "ui_saves.c"),
                   str(GUI / "ui_saves_metadata.c"), str(GUI / "ui_saves_raw.c"),
                   "-o", str(path / "test")]
        if args.sanitize:
            command += ["-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                        "-fno-omit-frame-pointer"]
            if sys.platform.startswith("linux"):
                command += ["-fno-pie", "-no-pie"]
        subprocess.run(command, check=True)
        subprocess.run([str(path / "test"), str(path / "fixture.raw"),
                        str(path / "fixture.gci")], check=True)


if __name__ == "__main__":
    main()
