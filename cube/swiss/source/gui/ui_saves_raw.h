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
	UI_SAVES_RAW_READ_ERROR
} uiSavesRawStatus_t;

/* A bounded, read-only snapshot. Never allocate a whole card image. */
typedef struct {
	uint32_t valid;
	uint16_t totalBlocks;
	uint16_t freeBlocks;
	uint16_t count;
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

const char *UISavesRaw_StatusText(uiSavesRawStatus_t status);

#endif
