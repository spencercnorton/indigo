#ifndef INDIGO_ICONV_CHECK_H
#define INDIGO_ICONV_CHECK_H

#include <iconv.h>
#include <stdbool.h>
#include <string.h>

/* Banner decoding accepts these encodings. Exercise the exact linked PPC
 * library, including Japanese transliteration and Unicode byte orders. */
static bool iconv_check(void)
{
	static const struct {
		const char *encoding, *input, *output;
		size_t bytes, expected;
	} cases[] = {
		{"ASCII", "Indigo", "Indigo", 6, 6},
		{"SHIFT_JIS", "\x82\x60", "A", 2, 1},
		{"WINDOWS-1252", "\x80\xe9", "\x80\xe9", 2, 2},
		{"UTF-8", "caf\xc3\xa9", "caf\xe9", 5, 4},
		{"UTF-16", "\xfe\xff\x00\xe9", "\xe9", 4, 1},
		{"UTF-32", "\x00\x00\xfe\xff\x00\x00\x00\xe9", "\xe9", 8, 1}
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
		char in[32], out[32] = {0}, *read = in, *write = out;
		size_t left = cases[i].bytes, room = sizeof(out);
		memcpy(in, cases[i].input, left);
		iconv_t cd = iconv_open("WINDOWS-1252/TRANSLIT/IGNORE", cases[i].encoding);
		if (cd == (iconv_t)-1) return false;
		size_t result = iconv(cd, &read, &left, &write, &room);
		iconv_close(cd);
		if (result == (size_t)-1 || left != 0 ||
		    sizeof(out) - room != cases[i].expected ||
		    memcmp(out, cases[i].output, cases[i].expected) != 0) return false;
	}
	return true;
}

#endif
