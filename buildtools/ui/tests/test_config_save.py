#!/usr/bin/env python3
"""A settings save survives losing power at any point.

config.c's config_file_write, config_file_read and config_file_delete run
against an in-memory card that loses power before any one of its writes,
deletes and renames, or halfway through a write, across two saves in a row.
After each, what reads back must be whole: the settings from before that
save or the ones it wrote. The card renames the way FatFs does, refusing to
rename onto a file that exists.
"""

import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

from test_cheats_gx_stream import extract_function

ROOT = Path(__file__).resolve().parents[3]
CONFIG_C = (ROOT / "cube/swiss/source/config/config.c").read_text()

PRELUDE = r"""
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned int u32;
typedef int s32;
#define PATHNAME_MAX 1024
typedef struct { char name[PATHNAME_MAX]; u32 size; } file_handle;
typedef struct {
	file_handle *initial;
	s32 (*statFile)(file_handle *);
	s32 (*readFile)(file_handle *, void *, u32);
	s32 (*writeFile)(file_handle *, const void *, u32);
	s32 (*closeFile)(file_handle *);
	s32 (*deleteFile)(file_handle *);
	s32 (*renameFile)(file_handle *, char *);
} DEVICEHANDLER_INTERFACE;
enum { DEVICE_CONFIG };
static DEVICEHANDLER_INTERFACE *devices[1];
#define print_debug(...) ((void)0)
static void concat_path(char *out, const char *base, const char *path)
{
	snprintf(out, PATHNAME_MAX, "%s%s", base, path);
}

/* The card: a few named files. Power fails before step crashAt. */
typedef struct { char name[PATHNAME_MAX]; char data[64]; u32 size; int used; } cardFile;
static cardFile card[4];
static int steps, crashAt;
static jmp_buf powerLost;

static cardFile *find(const char *name)
{
	for(int i = 0; i < 4; i++)
		if(card[i].used && !strcmp(card[i].name, name)) return &card[i];
	return NULL;
}

static void step(void)
{
	if(++steps == crashAt) longjmp(powerLost, 1);
}

static s32 cardStat(file_handle *f)
{
	cardFile *c = find(f->name);
	if(!c) return 4;	/* FR_NO_FILE */
	f->size = c->size;
	return 0;
}

static s32 cardRead(file_handle *f, void *buffer, u32 length)
{
	cardFile *c = find(f->name);
	if(!c || length > c->size) return -1;
	memcpy(buffer, c->data, length);
	return (s32)length;
}

/* Creates or truncates, like FA_CREATE_ALWAYS. Power can fail halfway. */
static s32 cardWrite(file_handle *f, const void *data, u32 length)
{
	cardFile *c = find(f->name);
	step();
	for(int i = 0; !c && i < 4; i++)
		if(!card[i].used) { c = &card[i]; c->used = 1; strcpy(c->name, f->name); }
	if(!c || length > sizeof(c->data)) return -1;
	c->size = length / 2;
	memcpy(c->data, data, c->size);
	step();
	c->size = length;
	memcpy(c->data, data, length);
	return (s32)length;
}

static s32 cardClose(file_handle *f) { (void)f; return 0; }

static s32 cardDelete(file_handle *f)
{
	cardFile *c = find(f->name);
	step();
	if(!c) return 4;
	c->used = 0;
	return 0;
}

static s32 cardRename(file_handle *f, char *name)
{
	cardFile *c = find(f->name);
	step();
	if(!c) return 4;
	if(find(name)) return 8;	/* FR_EXIST */
	strcpy(c->name, name);
	strcpy(f->name, name);
	return 0;
}

static file_handle root = { "card:/" };
static DEVICEHANDLER_INTERFACE device = {
	&root, cardStat, cardRead, cardWrite, cardClose, cardDelete, cardRename
};
"""

MAIN = r"""
static char *settings(void)
{
	return config_file_read("global.ini");
}

/* Saves text, with power lost before step crash (0: never); then reads the
 * settings back. A save that lost no power must have written text. */
static char *save(const char *text, int crash)
{
	char copy[64];
	char *read;
	int lost;

	steps = 0;
	crashAt = crash;
	strcpy(copy, text);
	lost = setjmp(powerLost);
	if(!lost && !config_file_write("global.ini", copy)) {
		puts("a save with no power lost failed");
		exit(1);
	}
	crashAt = 0;
	read = settings();
	if(!lost && (read == NULL || strcmp(read, text))) {
		puts("a save with no power lost did not write its settings");
		exit(1);
	}
	return read;
}

static int same(const char *read, const char *expected)
{
	return read != NULL && !strcmp(read, expected);
}

int main(void)
{
	devices[DEVICE_CONFIG] = &device;
	for(int first = 0; first <= 9; first++) {
		for(int second = 0; second <= 9; second++) {
			memset(card, 0, sizeof(card));
			free(save("A=1 settings before", 0));
			char *middle = save("B=2 the first save", first);
			if(!same(middle, "A=1 settings before") && !same(middle, "B=2 the first save")) {
				printf("power lost at step %d of a save: read back %s\n", first, middle ? middle : "nothing");
				return 1;
			}
			char *after = save("C=3 the second save", second);
			if(!same(after, middle) && !same(after, "C=3 the second save")) {
				printf("power lost at steps %d then %d: read back %s\n", first, second, after ? after : "nothing");
				return 1;
			}
			free(middle);
			free(after);
		}
	}
	/* A clean save leaves only the file. */
	memset(card, 0, sizeof(card));
	free(save("A=1", 0));
	free(save("B=2", 0));
	if(card[0].used + card[1].used + card[2].used + card[3].used != 1) return 1;
	/* Deleting takes a lone NAME.new with it: power lost before the swap
	 * of a first save. */
	memset(card, 0, sizeof(card));
	free(save("A=1", 4));
	config_file_delete("global.ini");
	if(config_file_read("global.ini") != NULL) return 1;
	puts("settings saves survive power lost at every step");
	return 0;
}
"""


UPSTREAM_WRITE = r"""int config_file_write(char* filename, char* contents) {
	file_handle *configFile = (file_handle*)calloc(1, sizeof(file_handle));
	concat_path(configFile->name, devices[DEVICE_CONFIG]->initial->name, filename);
	u32 len = strlen(contents);
	devices[DEVICE_CONFIG]->deleteFile(configFile);
	if(devices[DEVICE_CONFIG]->writeFile(configFile, contents, len) == len &&
		!devices[DEVICE_CONFIG]->closeFile(configFile)) {
		free(configFile);
		return 1;
	}
	devices[DEVICE_CONFIG]->closeFile(configFile);
	free(configFile);
	return 0;
}"""


class ConfigSaveTests(unittest.TestCase):
    def run_source(self, config):
        source = PRELUDE + "\n".join(extract_function(config, signature) for signature in (
            "static void config_name_new(", "char* config_file_read(",
            "int config_file_write(", "void config_file_delete(")) + MAIN
        with tempfile.TemporaryDirectory(prefix="config-save-") as directory:
            path = Path(directory)
            (path / "save.c").write_text(source)
            flags = ["-std=gnu11", "-Wall", "-Werror", "-Wno-unused-function",
                     "-fsanitize=address,undefined", "-g"]
            # Sanitizer executables and high-ASLR hosts: see Makefile.
            if os.uname().sysname == "Linux":
                flags += ["-fno-pie", "-no-pie"]
            subprocess.run(shlex.split(os.environ.get("CC", "cc")) + flags + [
                "-o", str(path / "save"), str(path / "save.c")], check=True)
            # Power lost mid-save skips the save's frees: no leak report.
            return subprocess.run([str(path / "save")], capture_output=True, text=True,
                                  timeout=60,
                                  env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"))

    def test_a_save_survives_power_lost_at_every_step(self):
        result = self.run_source(CONFIG_C)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_the_old_way_loses_the_settings(self):
        # Upstream's save: delete the file, then write it.
        old = CONFIG_C.replace(extract_function(CONFIG_C, "int config_file_write("), UPSTREAM_WRITE)
        self.assertNotEqual(old, CONFIG_C)
        self.assertNotEqual(self.run_source(old).returncode, 0)

    def test_a_lone_new_file_is_kept_before_the_next_save(self):
        # Without making NAME.new the file first, the next save deletes it
        # and loses the settings if power fails again.
        old = CONFIG_C.replace(
            "\tif(device->statFile(configFile) && !device->statFile(newFile)) {",
            "\tif(0) {", 1)
        self.assertNotEqual(old, CONFIG_C)
        self.assertNotEqual(self.run_source(old).returncode, 0)


if __name__ == "__main__":
    unittest.main()
