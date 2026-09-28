#!/usr/bin/env python3
"""Write fuzz_fst.c: get_fst_details from gcm.c, the file-table lookup the
Library runs for every disc image it lists (to find the game's banner),
compiled for the host with a libFuzzer entry point, so the fuzzer runs
exactly the code the console does.

usage: fst_source.py OUT.c
Build it with -funsigned-char: plain char is unsigned on the console's
PowerPC, and gcm.c reads the table's bytes through char.
"""

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
from test_cheats_gx_stream import extract_function  # noqa: E402

GCM_C = (HERE.parents[3] / "cube/swiss/source/gcm.c").read_text()

PRELUDE = r"""
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
typedef uint32_t u32;
"""

ENTRY = r"""
/* The input is the whole table, in a buffer exactly its size as get_fst reads
 * it, so AddressSanitizer sees any read past its end. The Library asks for
 * opening.bnr; launching a game asks for the files it patches. */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	char *fst;
	u32 offset, length;

	if(size > 0xFFFFFFFFu || !(fst = malloc(size ? size : 1)))
		return 0;
	memcpy(fst, data, size);
	get_fst_details(fst, (u32)size, "opening.bnr", &offset, &length);
	get_fst_details(fst, (u32)size, "claire.rel", &offset, &length);
	free(fst);
	return 0;
}
"""

Path(sys.argv[1]).write_text(PRELUDE + extract_function(GCM_C, "void get_fst_details(") + "\n" + ENTRY)
