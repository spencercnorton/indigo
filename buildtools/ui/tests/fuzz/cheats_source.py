#!/usr/bin/env python3
"""Write fuzz_cheats.c: parseCheats from cheats/cheats.c, the parser for the
cheats file a game's Cheats page reads from the card, compiled for the host
with a libFuzzer entry point, so the fuzzer runs exactly the code the console
does. The input's first byte picks one of its allocations to fail, as the
console's heap may, so the fuzzer reaches the out-of-memory paths too.

usage: cheats_source.py OUT.c
"""

import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
from test_cheats_gx_stream import extract_function  # noqa: E402

CHEATS = HERE.parents[3] / "cube/swiss/source/cheats"
CHEATS_C = (CHEATS / "cheats.c").read_text()
CHEATS_H = (CHEATS / "cheats.h").read_text()

PRELUDE = r"""
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint32_t u32;
"""

ALLOCATIONS = r"""
static unsigned failAt, allocations;

static void *failingReallocarray(void *pointer, size_t count, size_t size)
{
	return ++allocations == failAt ? NULL : reallocarray(pointer, count, size);
}

static char *failingStrdup(const char *text)
{
	return ++allocations == failAt ? NULL : strdup(text);
}

#define reallocarray failingReallocarray
#define strdup failingStrdup
"""

ENTRY = r"""
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	char *text;
	int i;

	if(size < 1 || size > CHEATS_FILE_MAX + 1 || !(text = malloc(size)))
		return 0;
	failAt = data[0];
	allocations = 0;
	memcpy(text, data + 1, size - 1);
	text[size - 1] = '\0';
	parseCheats(text);
	/* Every cheat kept has its codes. */
	for(i = 0; i < _cheats.num_cheats; i++) {
		if(_cheats.cheat[i].num_codes < 1 || _cheats.cheat[i].codes == NULL)
			abort();
	}
	disposeCheatEntries();
	free(text);
	return 0;
}
"""

types = [re.search(r"typedef struct \{[^}]*\} %s;" % name, CHEATS_H).group(0)
         for name in ("CheatEntry", "CheatEntries")]
parts = [PRELUDE, *types, ALLOCATIONS,
         re.search(r"^#define CHEATS_FILE_MAX .*$", CHEATS_C, re.M).group(0),
         "static CheatEntries _cheats;"]
parts += [extract_function(CHEATS_C, signature) for signature in (
    "static void disposeCheatEntries(", "int isValidCode(", "int isCheatCode(",
    "static bool addCheatCode(", "void parseCheats(")]
Path(sys.argv[1]).write_text("\n\n".join(parts) + "\n" + ENTRY)
