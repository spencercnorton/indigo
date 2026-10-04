#include "saves_raw.h"
#include "main.h"

#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

bool SavesRaw_IsImageName(const char *name)
{
	const char *leaf;
	size_t length;

	if(name == NULL) return false;
	leaf = strrchr(name, '/');
	leaf = leaf != NULL ? leaf + 1 : name;
	length = strlen(leaf);
	return leaf[0] != '.' && length > 4u &&
		!strcasecmp(leaf + length - 4u, ".raw");
}

static bool imageReader(void *opaque, uint32_t offset, void *destination,
	uint32_t length)
{
	file_handle *image = opaque;
	uint8_t *aligned;
	uint8_t *out = destination;
	bool result = true;

	if(offset > image->size || length > image->size - offset) return false;
	if(((uintptr_t)destination & 31u) == 0u) {
		return image->device->seekFile(image, offset, DEVICE_HANDLER_SEEK_SET) == offset &&
			image->device->readFile(image, destination, length) == (s32)length;
	}
	/* Partial GCI reads can leave the caller's buffer unaligned. */
	aligned = memalign(32, UI_SAVES_BLOCK_SIZE);
	if(aligned == NULL) return false;
	while(length != 0u) {
		uint32_t take = length < UI_SAVES_BLOCK_SIZE ? length : UI_SAVES_BLOCK_SIZE;
		if(image->device->seekFile(image, offset, DEVICE_HANDLER_SEEK_SET) != offset ||
			image->device->readFile(image, aligned, take) != (s32)take) {
			result = false;
			break;
		}
		memcpy(out, aligned, take);
		out += take;
		offset += take;
		length -= take;
	}
	free(aligned);
	return result;
}

static bool canRead(const file_handle *image)
{
	return image != NULL && image->fileType == IS_FILE && image->device != NULL &&
		image->device->seekFile != NULL && image->device->readFile != NULL &&
		image->device->closeFile != NULL;
}

uiSavesRawStatus_t SavesRaw_Load(file_handle *image, uiSavesRawCard_t *card)
{
	uint8_t *metadata;
	uiSavesRawStatus_t status;

	if(card == NULL) return UI_SAVES_RAW_INVALID_ARGUMENT;
	memset(card, 0, sizeof(*card));
	if(!canRead(image)) return UI_SAVES_RAW_INVALID_ARGUMENT;
	if(image->size < 4u * 131072u ||
		image->size > UI_SAVES_RAW_MAX_BLOCKS * UI_SAVES_BLOCK_SIZE ||
		(image->size & (image->size - 1u)) != 0u) return UI_SAVES_RAW_INVALID_SIZE;
	metadata = memalign(32, UI_SAVES_RAW_METADATA_SIZE);
	if(metadata == NULL) return UI_SAVES_RAW_READ_ERROR;
	status = imageReader(image, 0u, metadata, UI_SAVES_RAW_METADATA_SIZE) ?
		UISavesRaw_Parse(metadata, UI_SAVES_RAW_METADATA_SIZE, image->size, card) :
		UI_SAVES_RAW_READ_ERROR;
	image->device->closeFile(image);
	free(metadata);
	return status;
}

bool SavesRaw_ReadGci(file_handle *image, const uiSavesRawCard_t *card,
	unsigned ordinal, uint32_t offset, void *destination, uint32_t length)
{
	bool result;

	if(!canRead(image) || card == NULL ||
		image->size != (uint32_t)card->totalBlocks * UI_SAVES_BLOCK_SIZE) return false;
	result = UISavesRaw_ReadGci(card, ordinal, offset, destination, length,
		imageReader, image);
	image->device->closeFile(image);
	return result;
}
