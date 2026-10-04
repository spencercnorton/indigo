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

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t length)
{
	uiSavesRawCard_t card;
	uint8_t output[512];
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
	return 0;
}
