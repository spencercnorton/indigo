/* RAW card metadata, followed by bounded virtual GCI reads. The first four
 * bytes hold the image size; only its five metadata blocks are retained. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ui_saves_raw.h"

static bool readAt(void *opaque, uint32_t offset, void *destination,
	uint32_t length)
{
	uint32_t imageSize = *(const uint32_t *)opaque;
	if(offset < UI_SAVES_RAW_METADATA_SIZE || offset > imageSize ||
		length > imageSize - offset || length > UI_SAVES_BLOCK_SIZE) abort();
	memset(destination, 0xa5, length);
	return true;
}

/* A plan either refuses with nothing to write, or leaves metadata that reads
 * back as a card with exactly one save more or fewer. */
static void planned(const uint8_t *metadata, uint32_t imageSize,
	const uiSavesRawCard_t *card, unsigned ordinal)
{
	static uint8_t work[UI_SAVES_RAW_METADATA_SIZE];
	static uiSavesRawCard_t after;
	static uiSavesRawCard_t check;
	uiSavesRawChange_t change;
	uint8_t entry[UI_SAVES_ENTRY_SIZE];
	uiSavesRawStatus_t status;

	memcpy(work, metadata, sizeof(work));
	memcpy(entry, card->entry[ordinal], sizeof(entry));
	status = UISavesRaw_PlanErase(work, imageSize, entry, &after, &change);
	if(status != UI_SAVES_RAW_OK) abort();
	if(after.count + 1u != card->count || change.directory[0] == 0u ||
		change.map[0] == 0u || UISavesRaw_Find(&after, entry) != after.count ||
		UISavesRaw_Parse(work, sizeof(work), imageSize, &check) != UI_SAVES_RAW_OK ||
		check.count != after.count) abort();
	/* The erased save goes back into the space its erasing made. */
	status = UISavesRaw_PlanAdd(work, imageSize, entry, &after, &change);
	if(status != UI_SAVES_RAW_OK || after.count != card->count ||
		after.freeBlocks != card->freeBlocks || change.first < UI_SAVES_RAW_SYSTEM_BLOCKS ||
		change.first >= after.totalBlocks) abort();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t length)
{
	static uint8_t metadata[UI_SAVES_RAW_METADATA_SIZE];
	uiSavesRawCard_t card;
	uiSavesRawCard_t after;
	uiSavesRawChange_t change;
	uint8_t output[512];
	uint8_t entry[UI_SAVES_ENTRY_SIZE];
	uint32_t imageSize;
	uiSavesRawStatus_t status;

	if(length < 4u) return 0;
	imageSize = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
		((uint32_t)data[2] << 8) | data[3];
	status = UISavesRaw_Parse(data + 4u, length - 4u, imageSize, &card);
	if(status != UI_SAVES_RAW_OK) {
		if(card.valid != 0u || card.count != 0u ||
			UISavesRaw_Entry(&card, 0u) != NULL) abort();
		return 0;
	}
	if(card.count > UI_SAVES_CARD_FILES || card.totalBlocks > UI_SAVES_RAW_MAX_BLOCKS ||
		(uint32_t)card.totalBlocks * UI_SAVES_BLOCK_SIZE != imageSize) abort();
	for(unsigned ordinal = 0u; ordinal < card.count; ++ordinal) {
		uint32_t size = UISavesRaw_GciSize(&card, ordinal);
		uint32_t take = size < sizeof(output) ? size : sizeof(output);
		const uint8_t *entry = UISavesRaw_Entry(&card, ordinal);
		if(entry == NULL || size <= UI_SAVES_ENTRY_SIZE) abort();
		if(!UISavesRaw_ReadGci(&card, ordinal, 0u, output, take, readAt, &imageSize) ||
			memcmp(output, entry, UI_SAVES_ENTRY_SIZE) != 0) abort();
		if(!UISavesRaw_ReadGci(&card, ordinal, size - take, output, take,
			readAt, &imageSize)) abort();
		if(UISavesRaw_ReadGci(&card, ordinal, size, output, 1u,
			readAt, &imageSize)) abort();
	}
	if(length - 4u < sizeof(metadata)) return 0;
	memcpy(metadata, data + 4u, sizeof(metadata));
	for(unsigned ordinal = 0u; ordinal < card.count && ordinal < 4u; ++ordinal) {
		planned(metadata, imageSize, &card, ordinal);
	}
	/* A new save of the first save's size, under a name no save has. */
	memset(entry, 0, sizeof(entry));
	memcpy(entry, "FUZZ01\xff", 7u);
	memcpy(entry + 8u, "\x01\x02\x03", 3u);
	entry[0x39] = card.count > 0u ? card.entry[0][0x39] : 1u;
	entry[0x38] = card.count > 0u ? card.entry[0][0x38] : 0u;
	status = UISavesRaw_PlanAdd(metadata, imageSize, entry, &after, &change);
	if(status == UI_SAVES_RAW_OK) {
		if(after.count != card.count + 1u || change.directory[0] == 0u ||
			UISavesRaw_Find(&after, entry) == after.count) abort();
	}
	else if(change.directory[0] != 0u || change.map[0] != 0u ||
		memcmp(metadata, data + 4u, sizeof(metadata)) != 0) abort();
	return 0;
}
