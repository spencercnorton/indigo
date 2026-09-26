#include "ui_saves.h"

#include <stdio.h>
#include <string.h>

#define DATEL_MAGIC "DATELGC_SAVE"
#define DATEL_ENTRY 0x80u
#define GCS_MAGIC "GCSAVE"
#define GCS_ENTRY 0x110u

unsigned UISaves_Blocks(const uint8_t entry[UI_SAVES_ENTRY_SIZE])
{
	return ((unsigned)entry[0x38] << 8) | (unsigned)entry[0x39];
}

static void swapPairs(uint8_t *bytes, size_t length)
{
	size_t i;

	for(i = 0u; i + 1u < length; i += 2u) {
		uint8_t first = bytes[i];
		bytes[i] = bytes[i + 1u];
		bytes[i + 1u] = first;
	}
}

size_t UISaves_FindEntry(const uint8_t *file, size_t length,
	uint8_t entry[UI_SAVES_ENTRY_SIZE])
{
	size_t at = 0u;
	unsigned blocks;

	if(file == NULL || entry == NULL) {
		return 0u;
	}
	if(length >= sizeof(DATEL_MAGIC) - 1u &&
		!memcmp(file, DATEL_MAGIC, sizeof(DATEL_MAGIC) - 1u)) {
		at = DATEL_ENTRY;
	}
	else if(length >= sizeof(GCS_MAGIC) - 1u &&
		!memcmp(file, GCS_MAGIC, sizeof(GCS_MAGIC) - 1u)) {
		at = GCS_ENTRY;
	}
	if(length < at + UI_SAVES_ENTRY_SIZE) {
		return 0u;
	}
	memcpy(entry, file + at, UI_SAVES_ENTRY_SIZE);
	if(at == DATEL_ENTRY) {
		/* Action Replay keeps two stretches byte-swapped, as the file
		 * browser's Copy reads them: bytes 6-7 and the 20 from the icon
		 * address on. */
		swapPairs(entry + 0x06, 2u);
		swapPairs(entry + 0x2C, 20u);
	}
	blocks = UISaves_Blocks(entry);
	if(blocks == 0u || length - at - UI_SAVES_ENTRY_SIZE !=
		(size_t)blocks * UI_SAVES_BLOCK_SIZE) {
		return 0u;
	}
	return at + UI_SAVES_ENTRY_SIZE;
}

/* Printable ASCII a FAT name can hold; anything else is '_'. */
static char fatSafe(uint8_t c)
{
	if(c < 0x20u || c > 0x7Eu || strchr("\\/:*?\"<>|", c) != NULL) {
		return '_';
	}
	return (char)c;
}

void UISaves_FileName(char *out, size_t capacity,
	const uint8_t entry[UI_SAVES_ENTRY_SIZE])
{
	char name[UI_SAVES_NAME_LENGTH + 1u];
	size_t length = 0u;
	size_t i;

	if(out == NULL || capacity == 0u) {
		return;
	}
	for(i = 0u; i < UI_SAVES_NAME_LENGTH && entry[8u + i] != 0u; i++) {
		name[length++] = fatSafe(entry[8u + i]);
	}
	/* FAT drops a name's trailing spaces and dots; keep the name written. */
	while(length > 0u && (name[length - 1u] == ' ' ||
		name[length - 1u] == '.')) {
		length--;
	}
	name[length] = '\0';
	(void)snprintf(out, capacity, "%c%c-%c%c%c%c-%s.gci",
		fatSafe(entry[4]), fatSafe(entry[5]), fatSafe(entry[0]),
		fatSafe(entry[1]), fatSafe(entry[2]), fatSafe(entry[3]),
		length > 0u ? name : "save");
}

void UISaves_NumberedName(char *out, size_t capacity, const char *name,
	int attempt)
{
	const char *dot;

	if(out == NULL || capacity == 0u) {
		return;
	}
	if(name == NULL) {
		name = "";
	}
	dot = strrchr(name, '.');
	if(attempt <= 1) {
		(void)snprintf(out, capacity, "%s", name);
	}
	else if(dot == NULL || dot == name) {
		(void)snprintf(out, capacity, "%s_%d", name, attempt);
	}
	else {
		(void)snprintf(out, capacity, "%.*s_%d%s", (int)(dot - name), name,
			attempt, dot);
	}
}

int UISaves_Destinations(uiSavesPlace_t from, bool cardA, bool cardB,
	bool folders, bool inSaveFolder, uiSavesPlace_t out[UI_SAVES_PLACE_COUNT])
{
	int count = 0;

	if(out == NULL) {
		return 0;
	}
	if(cardA && from != UI_SAVES_PLACE_SLOT_A) {
		out[count++] = UI_SAVES_PLACE_SLOT_A;
	}
	if(cardB && from != UI_SAVES_PLACE_SLOT_B) {
		out[count++] = UI_SAVES_PLACE_SLOT_B;
	}
	if(folders && !inSaveFolder) {
		out[count++] = UI_SAVES_PLACE_FOLDER;
	}
	if(folders) {
		out[count++] = UI_SAVES_PLACE_CHOOSE;
	}
	return count;
}
