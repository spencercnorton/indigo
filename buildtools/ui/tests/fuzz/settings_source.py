#!/usr/bin/env python3
"""Write fuzz_settings.c: the settings test's harness (Swiss's own settings-file
parsers from config.c, compiled for the host) with a libFuzzer entry point in
place of its main, so the settings fuzzer runs exactly the code the console does.

usage: settings_source.py OUT.c
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from test_settings_file import HARNESS  # noqa: E402

# The console's C library is newlib. Its strtok_r leaves the save pointer NULL
# when a string is all delimiters, and config.c's parsers rely on that ("="
# alone on a line); glibc leaves it at the end of the string. Fuzz what the
# console runs: newlib's behaviour, ahead of the harness.
NEWLIB_STRTOK_R = r"""
#include <string.h>
static char *newlib_strtok_r(char *s, const char *delim, char **lasts)
{
	char *token;
	if(s == NULL && (s = *lasts) == NULL)
		return NULL;
	s += strspn(s, delim);
	if(*s == '\0') {
		*lasts = NULL;
		return NULL;
	}
	token = s;
	s += strcspn(s, delim);
	if(*s == '\0') {
		*lasts = NULL;
	} else {
		*s = '\0';
		*lasts = s + 1;
	}
	return token;
}
#define strtok_r newlib_strtok_r
"""

ENTRY = r"""
static char *copy(const uint8_t *data, size_t size)
{
	char *text = malloc(size + 1);
	if(text) {
		memcpy(text, data, size);
		text[size] = '\0';
	}
	return text;
}

/* A global.ini, a game's settings file and a legacy swiss.ini are the same
 * text to the fuzzer. A save merges what Swiss writes over the file on the
 * card: here the input's second half over its first. */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	char *global = copy(data, size), *game = copy(data, size), *legacy = copy(data, size);
	char *card = copy(data, size / 2), *written = copy(data + size / 2, size - size / 2);
	ConfigEntry entry;

	if(global && game && legacy && card && written) {
		set_defaults();
		config_parse_global(global);
		memset(&entry, 0, sizeof(entry));
		config_parse_game(game, &entry);
		free(config_merge_file(card, written, globalOldKeys));
		free(config_merge_file(card, written, gameFileKeys));
		config_parse_legacy(legacy, harness_progress);
	}
	free(global);
	free(game);
	free(legacy);
	free(card);
	free(written);
	return 0;
}
"""

MAIN = "int main(int argc, char **argv)"
if HARNESS.count(MAIN) != 1:
    raise SystemExit("test_settings_file.HARNESS no longer has one main(); update settings_source.py")
Path(sys.argv[1]).write_text(NEWLIB_STRTOK_R + HARNESS.replace(MAIN, "static int harness_main(int argc, char **argv)") + ENTRY)
