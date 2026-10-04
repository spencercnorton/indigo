#!/usr/bin/env python3
"""Run the folder launcher's real entry-building loop against card listings."""

import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
GUI = ROOT / "cube/swiss/source/gui"


def entry_loop(source: str) -> str:
    start = source.index("static bool gameflowResolveAndLoadFolder(")
    start = source.index("for(i = 0; i < context.childCount; ++i)", start)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


HARNESS = r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ui_gameflow_resolver.h"

typedef uint32_t u32;
#define PATHNAME_MAX 1024
#define ATTRIB_HIDDEN 0x02u
typedef struct {
	char name[PATHNAME_MAX];
	unsigned fileAttrib;
	uiGameflowLibraryEntryType_t fileType;
} file_handle;
static struct { bool showHiddenFiles; } swissSettings;
static unsigned headerReads;

static const char *getRelativeName(const char *name)
{
	const char *slash = strrchr(name, '/');
	return slash != NULL ? slash + 1 : name;
}

static uiGameflowLibraryEntryType_t gameflowEntryType(const file_handle *file)
{
	return file->fileType;
}

static void gameflowReadResolverHeader(file_handle *file,
	uiGameflowResolverEntry_t *entry)
{
	++headerReads;
	if(strcmp(getRelativeName(file->name), "game.iso") == 0) {
		entry->headerValid = true;
		memcpy(entry->gameId, "GMSE01", 7u);
	}
}

static void buildEntries(file_handle *children, int count,
	uiGameflowResolverEntry_t *resolverEntries)
{
	struct { file_handle *children; int childCount; } context = {children, count};
	int i;
	memset(resolverEntries, 0, (size_t)count * sizeof(*resolverEntries));
	/* ENTRY_LOOP */
}

static void checkListing(const char *extra, unsigned attributes,
	bool showHidden, uiGameflowResolveStatus_t expected, unsigned expectedReads)
{
	file_handle children[2] = {0};
	uiGameflowResolverEntry_t entries[2];
	uiGameflowResolverFolder_t folder = {0};
	uiGameflowResolverResult_t result;

	strcpy(children[0].name, "sd:/games/Title [GMSE01]/game.iso");
	strcpy(children[1].name, extra);
	children[0].fileType = children[1].fileType = UI_GAMEFLOW_LIBRARY_ENTRY_FILE;
	children[1].fileAttrib = attributes;
	memcpy(folder.gameId, "GMSE01", 7u);
	swissSettings.showHiddenFiles = showHidden;
	headerReads = 0;
	buildEntries(children, 2, entries);
	assert(headerReads == expectedReads);
	assert(UIGameflowResolver_Resolve(&folder, entries, 2u, &result) == expected);
	if(expected == UI_GAMEFLOW_RESOLVE_OK) {
		assert(result.primarySourceIndex == 0u);
	}
}

int main(void)
{
	char longMetadata[PATHNAME_MAX];

	checkListing("._game.iso", 0u, false, UI_GAMEFLOW_RESOLVE_OK, 1u);
	checkListing("._game.iso", 0u, true, UI_GAMEFLOW_RESOLVE_OK, 1u);
	checkListing(".hidden.iso", 0u, false, UI_GAMEFLOW_RESOLVE_OK, 1u);
	checkListing("hidden.iso", ATTRIB_HIDDEN, false, UI_GAMEFLOW_RESOLVE_OK, 1u);
	checkListing(".hidden.iso", 0u, true, UI_GAMEFLOW_RESOLVE_INVALID_METADATA, 2u);
	checkListing("hidden.iso", ATTRIB_HIDDEN, true,
		UI_GAMEFLOW_RESOLVE_INVALID_METADATA, 2u);
	checkListing("broken.iso", 0u, false, UI_GAMEFLOW_RESOLVE_INVALID_METADATA, 2u);
	memset(longMetadata, 'x', sizeof(longMetadata));
	memcpy(longMetadata, "._", 2u);
	memcpy(longMetadata + sizeof(longMetadata) - 5u, ".iso", 5u);
	checkListing(longMetadata, 0u, true, UI_GAMEFLOW_RESOLVE_OK, 1u);
	puts("game folder visibility tests passed");
	return 0;
}
'''


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    source = (ROOT / "cube/swiss/source/swiss.c").read_text()
    with tempfile.TemporaryDirectory() as folder:
        path = Path(folder)
        harness = path / "visibility.c"
        harness.write_text(HARNESS.replace("/* ENTRY_LOOP */", entry_loop(source)))
        command = [os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra",
                   "-Werror", "-O1", "-I", str(GUI), str(harness),
                   str(GUI / "ui_gameflow_library.c"),
                   str(GUI / "ui_gameflow_resolver.c"), "-o", str(path / "visibility")]
        if args.sanitize:
            command += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
            if sys.platform.startswith("linux"):
                command += ["-fno-pie", "-no-pie"]
        subprocess.run(command, check=True)
        subprocess.run([str(path / "visibility")], check=True)


if __name__ == "__main__":
    main()
