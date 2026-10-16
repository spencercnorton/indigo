#ifndef UI_SAVES_RAW_H
#define UI_SAVES_RAW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ui_saves.h"

#define UI_SAVES_RAW_SYSTEM_BLOCKS 5u
#define UI_SAVES_RAW_METADATA_SIZE (UI_SAVES_RAW_SYSTEM_BLOCKS * UI_SAVES_BLOCK_SIZE)
#define UI_SAVES_RAW_MAX_BLOCKS 2048u

typedef enum {
	UI_SAVES_RAW_OK = 0,
	UI_SAVES_RAW_UNFORMATTED,
	UI_SAVES_RAW_INVALID_ARGUMENT,
	UI_SAVES_RAW_INVALID_SIZE,
	UI_SAVES_RAW_INVALID_HEADER,
	UI_SAVES_RAW_INVALID_METADATA,
	UI_SAVES_RAW_INVALID_CHAIN,
	UI_SAVES_RAW_READ_ERROR,
	/* Writing a save in or out. */
	UI_SAVES_RAW_EXISTS,		/* a save of this game, maker and name is there */
	UI_SAVES_RAW_FULL,		/* the directory holds its 127 saves */
	UI_SAVES_RAW_NO_ROOM,		/* too few free blocks */
	UI_SAVES_RAW_NOT_FOUND,		/* the save isn't there as it was listed */
	UI_SAVES_RAW_READ_ONLY,		/* the image's device can't change it in place */
	UI_SAVES_RAW_WRITE_ERROR,
	UI_SAVES_RAW_MISMATCH		/* what was written didn't read back the same */
} uiSavesRawStatus_t;

/* A bounded snapshot. Never allocate a whole card image. */
typedef struct {
	uint32_t valid;
	uint16_t totalBlocks;
	uint16_t freeBlocks;
	uint16_t count;
	uint8_t directoryCopy;		/* the copy read, 0 or 1, of each */
	uint8_t mapCopy;
	uint8_t directoryIndex[UI_SAVES_CARD_FILES];
	uint8_t entry[UI_SAVES_CARD_FILES][UI_SAVES_ENTRY_SIZE];
	uint16_t next[UI_SAVES_RAW_MAX_BLOCKS];
} uiSavesRawCard_t;

typedef bool (*uiSavesRawReadAt_t)(void *opaque, uint32_t offset,
	void *destination, uint32_t length);

/* Checks the header, redundant directories and block maps, then every
 * selected save's complete chain, before exposing any save. The counters
 * use the GameCube's signed comparison, with the first copy winning ties.
 * A bad metadata copy is tolerated; two bad copies make the card invalid.
 * Blocks the map holds that no save's chain reaches are left alone: the
 * GameCube's own write order leaves them when a write is cut short.
 * No image bytes are changed, repaired or formatted. */
uiSavesRawStatus_t UISavesRaw_Parse(const uint8_t *metadata,
	size_t metadataLength, uint32_t imageSize, uiSavesRawCard_t *card);

const uint8_t *UISavesRaw_Entry(const uiSavesRawCard_t *card, unsigned ordinal);
uint32_t UISavesRaw_GciSize(const uiSavesRawCard_t *card, unsigned ordinal);

/* Reads a virtual .gci: its 64-byte directory entry, then its save's blocks
 * in chain order. The reader sees only bounded ranges in the source image.
 * A failed read returns false; callers must discard any partial output. */
bool UISavesRaw_ReadGci(const uiSavesRawCard_t *card, unsigned ordinal,
	uint32_t offset, void *destination, uint32_t length,
	uiSavesRawReadAt_t readAt, void *opaque);

/* The ordinal of card's save of entry's game, maker and name, or card->count
 * when it has none. */
unsigned UISavesRaw_Find(const uiSavesRawCard_t *card,
	const uint8_t entry[UI_SAVES_ENTRY_SIZE]);

/* A change to an image, planned in its five system blocks (metadata) before
 * anything is written. The new directory and block map each go into the
 * older of their two copies with the next update count, as the GameCube's
 * card library writes them, so the newer copy stands until the new one is
 * whole; when the count can't go higher (the console compares it signed),
 * into both, the older first. The system blocks to write, 0 ending each. */
typedef struct {
	uint8_t directory[2];
	uint8_t map[2];
	uint16_t first;		/* an added save's first block */
} uiSavesRawChange_t;

/* Plans adding the save whose directory entry is entry (a .gci's first 64
 * bytes; its blocks follow it there): its blocks are the free ones after the
 * last allocated, as the GameCube allocates them, chained in that order, and
 * entry goes into the first free directory place with that first block.
 * Nothing is planned for a save of the same game, maker and name, a full
 * directory, too few blocks or an image that doesn't parse. On success the
 * planned metadata has parsed into card with the save added, and every
 * other byte of the image is as it was. */
uiSavesRawStatus_t UISavesRaw_PlanAdd(uint8_t *metadata, uint32_t imageSize,
	const uint8_t entry[UI_SAVES_ENTRY_SIZE], uiSavesRawCard_t *card,
	uiSavesRawChange_t *change);

/* Plans removing the save whose directory entry is exactly entry (as it was
 * listed) and freeing its blocks; their bytes stay, as the GameCube leaves
 * them. On success the planned metadata has parsed into card without it. */
uiSavesRawStatus_t UISavesRaw_PlanErase(uint8_t *metadata, uint32_t imageSize,
	const uint8_t entry[UI_SAVES_ENTRY_SIZE], uiSavesRawCard_t *card,
	uiSavesRawChange_t *change);

const char *UISavesRaw_StatusText(uiSavesRawStatus_t status);

#endif
