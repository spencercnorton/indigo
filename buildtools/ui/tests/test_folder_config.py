#!/usr/bin/env python3
"""Execute the production folder-color save against controlled storage faults."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
from test_cheats_gx_stream import extract_function
from test_settings_file import CONFIG_C, SWISS, between

SOURCE = "\n".join([
    r'''
#define _GNU_SOURCE
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int s32;
#define PATHNAME_MAX 1024
#define SWISS_BASE_DIR "swiss"
#define SWISS_SETTINGS_DIR "swiss/settings"
#define SWISS_SETTINGS_FILENAME "global.ini"
#define FR_NO_FILE 4
#define FR_NO_PATH 5
typedef struct { char name[PATHNAME_MAX]; } file_handle;
typedef struct {
 file_handle *initial;
 s32 (*statFile)(file_handle *);
 s32 (*closeFile)(file_handle *);
} DEVICEHANDLER_INTERFACE;
enum { DEVICE_CONFIG };
static DEVICEHANDLER_INTERFACE *devices[1];
static bool mountFailure, readFailure, writeFailure, unknownStat, liveExists, recoveryExists;
static unsigned writes, defaultWrites, mkdirs;
static bool globalFileLoaded;
static void ensure_path(int slot, char *path, char *old, bool hidden)
{ (void)slot; (void)path; (void)old; (void)hidden; ++mkdirs; }
static char stored[2048], written[8192];
static unsigned deviceSets, deviceUnsets;
static int config_set_device(void) { ++deviceSets; return !mountFailure; }
static void config_unset_device(void) { ++deviceUnsets; }
static void concat_path(char *out, const char *base, const char *leaf)
{
 assert(strlen(base) + strlen(leaf) + 2u < PATHNAME_MAX);
 strcpy(out, base); if(out[strlen(out)-1u] != '/') strcat(out, "/"); strcat(out, leaf);
}
static s32 deviceHandler_FAT_statFile(file_handle *file)
{
 if(unknownStat) return 1;
 bool isNew = strstr(file->name, ".new") != NULL;
 return (isNew ? recoveryExists : liveExists) ? 0 : FR_NO_FILE;
}
static s32 closeFile(file_handle *file) { (void)file; return 0; }
static char *config_file_read(char *path)
{
 (void)path;
 if(readFailure || (!liveExists && !recoveryExists)) return NULL;
 return strdup(stored);
}
static int config_file_write(char *path, char *text)
{
 (void)path; ++writes;
 if(writeFailure) return 0;
 assert(strlen(text) < sizeof(written)); strcpy(written, text); return 1;
}
static int config_update_global(bool check)
{
 (void)check; ++defaultWrites; return !writeFailure;
}
''',
    (SWISS / "source/gui/ui_folder.h").read_text(),
    (SWISS / "source/gui/ui_folder.c").read_text().replace('#include "ui_folder.h"', ''),
    "static uiFolderColors_t folderColors;",
    extract_function(CONFIG_C, "uint8_t config_folder_color("),
    extract_function(CONFIG_C, "static void config_name_new("),
    between(CONFIG_C, "/* Keys a global.ini may still carry", "int config_update_global("),
    extract_function(CONFIG_C, "static bool config_folder_settings_absent("),
    extract_function(CONFIG_C, "bool config_set_folder_color("),
    r'''
static void reset(void)
{
 mountFailure = readFailure = writeFailure = unknownStat = false;
 liveExists = true; recoveryExists = false; writes = defaultWrites = mkdirs = 0; globalFileLoaded = false;
 strcpy(stored, "# Keep this note\nMenu Color=Gold\nFuture Setting=on\nMemory Card Folder Colors=\n");
 memset(&folderColors, 0, sizeof(folderColors));
 assert(UIFolder_SetColor(&folderColors, "sda:/swiss/saves/first", 1u));
 assert(UIFolder_SetColor(&folderColors, "sda:/swiss/saves/second", 2u));
 assert(UIFolder_SetColor(&folderColors, "sda:/swiss/saves/first", 0u));
}
int main(void)
{
 file_handle root = { "card:/" };
 DEVICEHANDLER_INTERFACE device = { &root, deviceHandler_FAT_statFile, closeFile };
 devices[DEVICE_CONFIG] = &device;
 (void)globalOldKeys; (void)gameFileKeys;
 uiFolderColors_t before;
 reset();
 assert(config_set_folder_color("sda:/swiss/saves/Spaces #;~%=\n", 4u, true));
 assert(strstr(written, "# Keep this note") != NULL);
 assert(strstr(written, "Menu Color=Gold") != NULL);
 assert(strstr(written, "Future Setting=on") != NULL);
 assert(strstr(written, "sda:/swiss/saves/Spaces%20#%3B%7E%25%3D%0A~4") != NULL);
 assert(config_folder_color("sda:/swiss/saves/Spaces #;~%=\n") == 4u);
 assert(writes == 1u && defaultWrites == 0u);
 for(unsigned fault = 0; fault < 5; ++fault) {
  reset(); before = folderColors;
  if(fault == 0) mountFailure = true;
  if(fault == 1) readFailure = true;
  if(fault == 2) writeFailure = true;
  if(fault == 3) { readFailure = true; unknownStat = true; }
  if(fault == 4) { readFailure = true; liveExists = false; recoveryExists = true; }
  assert(!config_set_folder_color("sda:/swiss/saves/second", 0u, true));
  assert(memcmp(&folderColors, &before, sizeof(before)) == 0);
  assert(defaultWrites == 0u);
  if(fault != 2) assert(writes == 0u);
 }
 reset(); liveExists = recoveryExists = false;
 assert(config_set_folder_color("sda:/swiss/saves/new", 3u, true));
 assert(defaultWrites == 0u && writes == 1u && mkdirs == 2u);
 assert(globalFileLoaded);
 assert(strstr(written, "Menu Color") == NULL);
 assert(strstr(written, "Memory Card Folder Colors=") == written);
 reset(); liveExists = recoveryExists = false; writeFailure = true; before = folderColors;
 assert(!config_set_folder_color("sda:/swiss/saves/new", 3u, true));
 assert(memcmp(&folderColors, &before, sizeof(before)) == 0);
 /* Memory Cards holds the settings device for its whole visit: a save from
  * there neither mounts it again nor lets it go. */
 reset(); deviceSets = deviceUnsets = 0;
 assert(config_set_folder_color("sda:/swiss/saves/held", 2u, false));
 assert(deviceSets == 0u && deviceUnsets == 0u && writes == 1u);
 reset(); deviceSets = deviceUnsets = 0;
 assert(config_set_folder_color("sda:/swiss/saves/own", 2u, true));
 assert(deviceSets == 1u && deviceUnsets == 1u);
 /* Without a settings device it saves nothing, mounted or not. */
 reset(); devices[DEVICE_CONFIG] = NULL; before = folderColors;
 assert(!config_set_folder_color("sda:/swiss/saves/held", 2u, false));
 assert(memcmp(&folderColors, &before, sizeof(before)) == 0 && writes == 0u);
 devices[DEVICE_CONFIG] = &device;
 puts("production folder save, independent settings, fault rollback and a held device: PASS");
 return 0;
}
'''
])
with tempfile.TemporaryDirectory() as temporary:
    source, binary = Path(temporary) / "folder.c", Path(temporary) / "folder"
    source.write_text(SOURCE)
    command = shlex.split(os.environ.get("CC", "cc")) + ["-std=c11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function"]
    if "--sanitize" in __import__("sys").argv:
        command += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        # Match the host suite: high-ASLR Linux can place a PIE binary in
        # ASan's shadow map and recursively fault before main (see Makefile).
        if os.uname().sysname == "Linux":
            command += ["-fno-pie", "-no-pie"]
    subprocess.run(command + [str(source), "-o", str(binary)], check=True, timeout=60)
    subprocess.run([str(binary)], check=True, timeout=15)
