#ifndef UI_SAVES_H
#define UI_SAVES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Memory Cards (Home > System > Memory Cards): the pure half. saves.c asks
 * the card and SD drivers for the bytes; these decide what the bytes are and
 * where a save may go. */

#define UI_SAVES_ENTRY_SIZE 64u	/* a card's directory entry, a .gci's header */
#define UI_SAVES_BLOCK_SIZE 8192u
#define UI_SAVES_NAME_LENGTH 32u	/* CARD_FILENAMELEN */

/* The places a save can go, in the order the destination list shows them. */
typedef enum {
	UI_SAVES_PLACE_SLOT_A = 0,
	UI_SAVES_PLACE_SLOT_B,
	UI_SAVES_PLACE_FOLDER,	/* Settings > Storage > Save Folder */
	UI_SAVES_PLACE_CHOOSE,	/* another folder, chosen then */
	UI_SAVES_PLACE_COUNT
} uiSavesPlace_t;

/* A .gci is the card's directory entry, then the save's blocks. Action
 * Replay (.sav, "DATELGC_SAVE") and GameShark (.gcs, "GCSAVE") files put
 * their own header first. Copies the entry to entry as a .gci has it and
 * returns where the blocks start, or 0 when the file isn't one whole save:
 * its length must be the entry's block count exactly. */
size_t UISaves_FindEntry(const uint8_t *file, size_t length,
	uint8_t entry[UI_SAVES_ENTRY_SIZE]);

/* The blocks the entry says the save has. */
unsigned UISaves_Blocks(const uint8_t entry[UI_SAVES_ENTRY_SIZE]);

/* "01-GALE-SuperSmashBros0110290334.gci", the maker, game code and card
 * name, as Dolphin names a GCI folder's files, so a folder of them can be a
 * Dolphin memory card. A character a FAT name can't hold becomes '_'. */
void UISaves_FileName(char *out, size_t capacity,
	const uint8_t entry[UI_SAVES_ENTRY_SIZE]);

/* "name.gci" as "name_2.gci": the attempt'th name for a copy when the first
 * is taken (attempt 1 is the name itself). */
void UISaves_NumberedName(char *out, size_t capacity, const char *name,
	int attempt);

/* Where a save in from can go: a slot with a card that isn't from, the Save
 * Folder unless the save is in it, and any folder when there is an SD card
 * to hold one. Returns how many it wrote to out. */
int UISaves_Destinations(uiSavesPlace_t from, bool cardA, bool cardB,
	bool folders, bool inSaveFolder, uiSavesPlace_t out[UI_SAVES_PLACE_COUNT]);

#endif
