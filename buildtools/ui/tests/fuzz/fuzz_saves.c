/* Fuzz save files as Memory Cards reads them (.gci, Action Replay .sav,
 * GameShark .gcs): an entry it accepts accounts for the file exactly, the
 * names made from it are whole FAT names however hostile the entry, and the
 * art an entry describes (banner, icon frames, palettes, comment) lies inside
 * the save's data, decodes without reading past it, and animates only frames
 * that have pixels. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ui_saves.h"

static void fat_name(const char *name, size_t capacity)
{
	size_t length = strnlen(name, capacity);

	if (length == capacity || length < 5 || strcmp(name + length - 4, ".gci") != 0)
		abort();
	for (size_t i = 0; i < length; i++) {
		unsigned char c = (unsigned char)name[i];
		if (c < 0x20 || c > 0x7E || strchr("\\/:*?\"<>|", c) != NULL)
			abort();
	}
}

/* The bytes a picture takes in the save, palette included. */
static uint64_t art_bytes(unsigned format, uint64_t pixels)
{
	return format == UI_SAVES_ART_RGB5A3 ? pixels * 2 :
		format == UI_SAVES_ART_CI8_OWN ? pixels + 512 :
		format == UI_SAVES_ART_CI8_SHARED ? pixels : 0;
}

static void inside(uint64_t at, uint64_t size, uint32_t end)
{
	if (at + size > end)
		abort();
}

/* The art of a save whose data, after its entry, is length bytes. */
static void art(const uint8_t entry[UI_SAVES_ENTRY_SIZE], const uint8_t *data, size_t length)
{
	static uint8_t texels[UI_SAVES_BANNER_BYTES];
	uiSavesArt_t a;
	unsigned period = 0;
	bool shared = false;

	if (!UISaves_ArtLayout(entry, length, &a)) {
		if (a.end != 0 || a.frames != 0 || a.comment || a.bannerFormat != UI_SAVES_ART_NONE)
			abort();
		return;
	}
	if (a.end > length || a.end > UI_SAVES_ART_MAX_END || a.frames > UI_SAVES_ICON_FRAMES ||
		a.steps > UI_SAVES_ART_STEPS)
		abort();
	if (a.bannerFormat != UI_SAVES_ART_NONE)
		inside(a.bannerAt, art_bytes(a.bannerFormat, 96 * 32), a.end);
	for (unsigned i = 0; i < a.frames; i++) {
		inside(a.frameAt[i], art_bytes(a.frameFormat[i], 32 * 32), a.end);
		shared = shared || a.frameFormat[i] == UI_SAVES_ART_CI8_SHARED;
	}
	if (shared)
		inside(a.paletteAt, 512, a.end);
	if (a.comment)
		inside(a.commentAt, UI_SAVES_ENTRY_SIZE, a.end);
	if ((a.steps == 0) != (a.frames == 0))
		abort();
	for (unsigned i = 0; i < a.steps; i++) {
		if (a.stepHold[i] < 1 || a.stepHold[i] > 3)
			abort();
		if (a.stepFrame[i] != UI_SAVES_ART_BLANK && (a.stepFrame[i] >= a.frames ||
			a.frameFormat[a.stepFrame[i]] == UI_SAVES_ART_NONE))
			abort();
		period += a.stepHold[i];
	}
	if (period != a.period)
		abort();
	/* Every picture decodes from the bytes read for the art, and only those. */
	if (UISaves_ToRgb5a3(data, a.end, &a, UI_SAVES_ART_BANNER, texels) !=
		(a.bannerFormat != UI_SAVES_ART_NONE))
		abort();
	for (int i = 0; i < (int)a.frames; i++) {
		if (UISaves_ToRgb5a3(data, a.end, &a, i, texels) != (a.frameFormat[i] != UI_SAVES_ART_NONE))
			abort();
	}
	for (uint32_t tick = 0; tick <= period + 1; tick++) {
		int frame = UISaves_ArtStep(&a, tick);
		if (frame < -1 || frame >= (int)a.frames ||
			(frame >= 0 && a.frameFormat[frame] == UI_SAVES_ART_NONE))
			abort();
	}
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	uint8_t entry[UI_SAVES_ENTRY_SIZE], head[UI_SAVES_ENTRY_SIZE];
	char name[96], numbered[96];
	size_t start = UISaves_FindEntry(data, size, entry);
	size_t prefix = size < UI_SAVES_HEAD_SIZE ? size : UI_SAVES_HEAD_SIZE;

	if (start != 0) {
		unsigned blocks = UISaves_Blocks(entry);
		if (start > size || blocks == 0 || size - start != (size_t)blocks * UI_SAVES_BLOCK_SIZE)
			abort();
		art(entry, data + start, size - start);
	}
	/* A file's first UI_SAVES_HEAD_SIZE bytes find what the whole file does. */
	if (UISaves_FindEntryPrefix(data, prefix, size, head) != start ||
		(start != 0 && memcmp(head, entry, sizeof(entry)) != 0))
		abort();
	/* Any 64 bytes are an entry as far as naming goes, and as a card's save
	 * read as a .gci: the entry, then the data. */
	if (size >= UI_SAVES_ENTRY_SIZE) {
		memcpy(entry, data, UI_SAVES_ENTRY_SIZE);
		art(entry, data + UI_SAVES_ENTRY_SIZE, size - UI_SAVES_ENTRY_SIZE);
		UISaves_FileName(name, sizeof(name), entry);
		fat_name(name, sizeof(name));
		UISaves_NumberedName(numbered, sizeof(numbered), name, 1 + data[0] % 64);
		if (strnlen(numbered, sizeof(numbered)) == sizeof(numbered))
			abort();
		UISaves_FileName(name, 8, entry); /* a short buffer truncates, never overruns */
	}
	return 0;
}
