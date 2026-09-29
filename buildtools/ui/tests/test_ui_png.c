/* The host half of test_ui_png.py: runs ui_png on files it wrote.
 *
 *   test_ui_png poster IN.png OUT.bin   a poster (UI_PNG_POSTER_BYTES), or
 *                                       exit 3 when ui_png refuses the PNG
 *   test_ui_png info IN.png             "WIDTH HEIGHT", or exit 3
 *   test_ui_png cmpr IN.rgb SIZE OUT    SIZE x SIZE RGB as GX CMPR
 *   test_ui_png name NAME OUT.bin        NAME as a poster in a block font:
 *                                       each letter a 10-pixel-tall block,
 *                                       8 pixels wide (name-wide: 20), a
 *                                       space 5; name-empty's font has no
 *                                       letters. Exit 3 when refused.
 *   test_ui_png poster-stopped IN OUT    / name-stopped NAME OUT: the same
 *                                       with the stop already set: exit 3.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_png.h"

static unsigned char *slurp(const char *path, size_t *size)
{
	FILE *file = fopen(path, "rb");
	unsigned char *data = NULL;
	long length;

	if(file == NULL) {
		perror(path);
		exit(2);
	}
	if(fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 ||
		fseek(file, 0, SEEK_SET) != 0) {
		perror(path);
		exit(2);
	}
	/* One byte more, so an empty file still has a buffer. */
	data = malloc((size_t)length + 1u);
	if(data == NULL || fread(data, 1, (size_t)length, file) != (size_t)length) {
		perror(path);
		exit(2);
	}
	fclose(file);
	*size = (size_t)length;
	return data;
}

/* The block font: every letter a block in a 12-pixel cell. */
static int blockWidth = 8;
static bool blockEmpty;

static bool blockGlyph(void *context, unsigned char c,
	uint8_t coverage[UI_PNG_GLYPH_MAX * UI_PNG_GLYPH_MAX], int *width)
{
	int w = c == ' ' ? 5 : blockWidth;
	int x, y;

	(void)context;
	if(blockEmpty) return false;
	*width = w;
	if(c != ' ') {
		for(y = 1; y <= 10; ++y) {
			for(x = 1; x < w - 1; ++x) {
				coverage[y * UI_PNG_GLYPH_MAX + x] = 255u;
			}
		}
	}
	return true;
}

static void spill(const char *path, const unsigned char *data, size_t size)
{
	FILE *file = fopen(path, "wb");

	if(file == NULL || fwrite(data, 1, size, file) != size || fclose(file) != 0) {
		perror(path);
		exit(2);
	}
}

int main(int argc, char **argv)
{
	size_t size;
	unsigned char *data;

	if(argc == 4 && strcmp(argv[1], "poster-stopped") == 0) {
		static const volatile bool stop = true;
		unsigned char *poster = malloc(UI_PNG_POSTER_BYTES);
		bool ok;

		data = slurp(argv[2], &size);
		ok = poster != NULL && UIPng_PosterUntil(data, size, poster, &stop);
		free(data);
		free(poster);
		return ok ? 0 : 3;
	}
	if(argc == 4 && strcmp(argv[1], "name-stopped") == 0) {
		static const volatile bool stop = true;
		uiPngFont_t font = {12, blockGlyph, NULL};
		unsigned char *poster = malloc(UI_PNG_POSTER_BYTES);
		bool ok = poster != NULL &&
			UIPng_NamePosterUntil(argv[2], &font, poster, &stop);

		free(poster);
		return ok ? 0 : 3;
	}
	if(argc == 4 && strcmp(argv[1], "poster") == 0) {
		unsigned char *poster = malloc(UI_PNG_POSTER_BYTES);
		bool ok;

		data = slurp(argv[2], &size);
		ok = poster != NULL && UIPng_Poster(data, size, poster);
		if(ok) spill(argv[3], poster, UI_PNG_POSTER_BYTES);
		free(data);
		free(poster);
		return ok ? 0 : 3;
	}
	if(argc == 3 && strcmp(argv[1], "info") == 0) {
		uint32_t width, height;
		bool ok;

		data = slurp(argv[2], &size);
		ok = UIPng_Info(data, size, &width, &height);
		if(ok) printf("%u %u\n", (unsigned)width, (unsigned)height);
		free(data);
		return ok ? 0 : 3;
	}
	if(argc == 5 && strcmp(argv[1], "cmpr") == 0) {
		unsigned long side = strtoul(argv[3], NULL, 10);
		unsigned char *out;

		data = slurp(argv[2], &size);
		if(side == 0u || side % 8u != 0u || size != side * side * 3u) {
			fprintf(stderr, "cmpr: %s is not %lux%lu RGB\n", argv[2], side, side);
			return 2;
		}
		out = malloc(side * side / 2u);
		if(out == NULL) return 2;
		UIPng_EncodeCmpr(data, (uint32_t)side, out);
		spill(argv[4], out, side * side / 2u);
		free(data);
		free(out);
		return 0;
	}
	if(argc == 4 && strncmp(argv[1], "name", 4) == 0) {
		uiPngFont_t font = {12, blockGlyph, NULL};
		unsigned char *poster = malloc(UI_PNG_POSTER_BYTES);
		bool ok;

		blockWidth = strcmp(argv[1], "name-wide") == 0 ? 20 : 8;
		blockEmpty = strcmp(argv[1], "name-empty") == 0;
		ok = poster != NULL && UIPng_NamePoster(argv[2], &font, poster);
		if(ok) spill(argv[3], poster, UI_PNG_POSTER_BYTES);
		free(poster);
		return ok ? 0 : 3;
	}
	fprintf(stderr, "usage: %s poster IN OUT | info IN | cmpr IN SIZE OUT"
		" | name[-wide|-empty] NAME OUT\n", argv[0]);
	return 2;
}
