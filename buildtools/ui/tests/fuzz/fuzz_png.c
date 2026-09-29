/* Fuzz an app's picture (the PNG beside a program in /apps): whatever the
 * file holds, UIPng_Info and UIPng_Poster must not overrun or crash, a size
 * Info accepts is within the limits, and Poster never takes a file Info
 * refuses. A mutation almost never keeps its chunk's CRC, so each input is
 * read twice: as it is, then with every CRC made right, so the mutations
 * reach the header, the rows and the decoder too. An input starting with
 * 'N' is also an app's name, for UIPng_NamePoster in a font whose glyphs
 * are 1 to UI_PNG_GLYPH_MAX wide (and without 0x7F). */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "ui_png.h"

static void check(uint8_t *png, size_t size)
{
	static uint8_t poster[UI_PNG_POSTER_BYTES];
	uint32_t width = 0, height = 0;
	bool info, ok;

	info = UIPng_Info(png, size, &width, &height);
	if (info && (width == 0 || height == 0 || width > UI_PNG_MAX_SIDE || height > UI_PNG_MAX_SIDE))
		abort();
	ok = UIPng_Poster(png, size, poster);
	if (ok && !info)
		abort();
}

static bool fuzzGlyph(void *context, unsigned char c,
	uint8_t coverage[UI_PNG_GLYPH_MAX * UI_PNG_GLYPH_MAX], int *width)
{
	(void)context;
	if (c == 0x7F)
		return false;
	*width = 1 + c % UI_PNG_GLYPH_MAX;
	memset(coverage, c, (size_t)*width);
	coverage[(c % 12) * UI_PNG_GLYPH_MAX + *width - 1] = 255;
	return true;
}

static void checkName(const uint8_t *data, size_t size)
{
	static uint8_t poster[UI_PNG_POSTER_BYTES];
	uiPngFont_t font = {12, fuzzGlyph, NULL};
	char name[96];
	size_t n = size < sizeof(name) - 1 ? size : sizeof(name) - 1;

	memcpy(name, data, n);
	name[n] = '\0';
	(void)UIPng_NamePoster(name, &font, poster);
}

/* Rewrites the CRC of each whole chunk after the signature. */
static void repairCrcs(uint8_t *png, size_t size)
{
	size_t at = 8;

	while (at <= size && size - at >= 12) {
		uint32_t length = (uint32_t)png[at] << 24 | (uint32_t)png[at + 1] << 16 |
			(uint32_t)png[at + 2] << 8 | png[at + 3];
		uLong crc;

		if (length > size - at - 12)
			return;
		crc = crc32(crc32(0L, Z_NULL, 0), png + at + 4, (uInt)(length + 4));
		png[at + 8 + length] = (uint8_t)(crc >> 24);
		png[at + 9 + length] = (uint8_t)(crc >> 16);
		png[at + 10 + length] = (uint8_t)(crc >> 8);
		png[at + 11 + length] = (uint8_t)crc;
		at += 12 + (size_t)length;
	}
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	/* A copy of exactly size bytes, so a read past the end is caught. */
	uint8_t *copy = malloc(size ? size : 1);

	if (!copy)
		return 0;
	memcpy(copy, data, size);
	check(copy, size);
	repairCrcs(copy, size);
	check(copy, size);
	if (size > 0 && data[0] == 'N')
		checkName(data + 1, size - 1);
	free(copy);
	return 0;
}
