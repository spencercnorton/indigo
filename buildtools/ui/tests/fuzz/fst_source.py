#!/usr/bin/env python3
"""Write fuzz_fst.c: from gcm.c, the two checks every file-table walk goes
through (fst_entries, fst_name), get_fst_details (the lookup the Library runs
for every disc image it lists, to find the game's banner), adjust_tgc_fst and
calc_fst_entries_size (a launch from a TGC, a disc opened as a folder),
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
typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
#define FST_ENTRY_SIZE 12
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
	(void)calc_fst_entries_size(fst, (u32)size);
	adjust_tgc_fst(fst, (u32)size, 0x10000u, 0x8000u, 0x2000u);
	free(fst);
	return 0;
}
"""

FUNCTIONS = ("static u32 fst_entries(", "static const char *fst_name(", "void get_fst_details(",
             "u64 calc_fst_entries_size(", "void adjust_tgc_fst(")
Path(sys.argv[1]).write_text(PRELUDE + "\n".join(extract_function(GCM_C, f) for f in FUNCTIONS) + "\n" + ENTRY)
