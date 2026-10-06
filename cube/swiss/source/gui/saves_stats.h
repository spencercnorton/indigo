#ifndef SAVES_STATS_H
#define SAVES_STATS_H

#include "ui_saves_metadata.h"

/* Where a copy of a game's save is: on the card in a slot, a .gci, .gcs or
 * .sav file in the Save Folder, or one save inside a .raw card image there. */
typedef enum {
	SAVES_COPY_SLOT_A = 0,
	SAVES_COPY_SLOT_B,
	SAVES_COPY_FILE,
	SAVES_COPY_IMAGE
} savesCopySource_t;

#define SAVES_COPIES_MAX 8u
#define SAVES_COPY_PATH 1024u	/* PATHNAME_MAX */

typedef struct {
	savesCopySource_t source;
	char path[SAVES_COPY_PATH];	/* FILE and IMAGE: the file on the settings device */
	uint32_t size;			/* FILE and IMAGE: its size */
	unsigned ordinal;		/* IMAGE: the save's place in the image */
	uint8_t entry[UI_SAVES_ENTRY_SIZE];	/* game, maker, name, blocks and date */
} savesCopy_t;

/* The first SAVES_COPIES_MAX copies the scan counted, and the slots that hold
 * a memory card it read. */
typedef struct {
	unsigned count;
	savesCopy_t copy[SAVES_COPIES_MAX];
	bool cards[2];
} savesCopies_t;

/* A bounded, read-only scan while opening Game Detail, never while drawing.
 * Reads the configured Save Folder's direct save files and RAW images plus
 * available physical cards. A partial result is explicitly marked. With
 * copies, it also lists where the copies are, for a choice of the one to
 * start the game with. */
void Saves_CollectGameStats(const char gameId[6], uiSavesGameStats_t *stats,
	savesCopies_t *copies);

#endif
