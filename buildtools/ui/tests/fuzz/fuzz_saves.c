/* Fuzz save files as Memory Cards reads them (.gci, Action Replay .sav,
 * GameShark .gcs): an entry it accepts accounts for the file exactly, and the
 * names made from it are whole FAT names however hostile the entry. */
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

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	uint8_t entry[UI_SAVES_ENTRY_SIZE];
	char name[96], numbered[96];
	size_t start = UISaves_FindEntry(data, size, entry);

	if (start != 0) {
		unsigned blocks = UISaves_Blocks(entry);
		if (start > size || blocks == 0 || size - start != (size_t)blocks * UI_SAVES_BLOCK_SIZE)
			abort();
	}
	/* Any 64 bytes are an entry as far as naming goes. */
	if (size >= UI_SAVES_ENTRY_SIZE) {
		memcpy(entry, data, UI_SAVES_ENTRY_SIZE);
		UISaves_FileName(name, sizeof(name), entry);
		fat_name(name, sizeof(name));
		UISaves_NumberedName(numbered, sizeof(numbered), name, 1 + data[0] % 64);
		if (strnlen(numbered, sizeof(numbered)) == sizeof(numbered))
			abort();
		UISaves_FileName(name, 8, entry); /* a short buffer truncates, never overruns */
	}
	return 0;
}
