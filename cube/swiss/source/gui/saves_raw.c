#include "saves_raw.h"
#include "main.h"
#include "deviceHandler-FAT.h"

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

/* ------------------------------------------------------------------------
 * Changing an image in place. The device's writeFile opens with
 * FA_CREATE_ALWAYS, which would empty the image, so an image is opened here
 * as it is, read and written, and never past its end.
 * --------------------------------------------------------------------- */

bool SavesRaw_Writable(const file_handle *image)
{
	return image != NULL && image->fileType == IS_FILE && image->device != NULL &&
		(image->device->features & FEAT_WRITE) &&
		image->device->writeFile == deviceHandler_FAT_writeFile;
}

static bool imageAt(FFFIL *fp, uint32_t offset, uint32_t length)
{
	return offset <= f_size(fp) && length <= f_size(fp) - offset &&
		f_lseek(fp, offset) == FR_OK && f_tell(fp) == offset;
}

static bool imageWrite(FFFIL *fp, uint32_t offset, const void *data, uint32_t length)
{
	UINT done = 0;

	return imageAt(fp, offset, length) &&
		f_write(fp, data, length, &done) == FR_OK && done == length;
}

static bool imageRead(FFFIL *fp, uint32_t offset, void *data, uint32_t length)
{
	UINT done = 0;

	return imageAt(fp, offset, length) &&
		f_read(fp, data, length, &done) == FR_OK && done == length;
}

/* The planned system blocks, in their order, each on the card before the
 * next is written. */
static bool writeSystem(FFFIL *fp, const uint8_t *metadata, const uint8_t blocks[2])
{
	unsigned i;

	for(i = 0u; i < 2u && blocks[i] != 0u; i++) {
		if(!imageWrite(fp, blocks[i] * UI_SAVES_BLOCK_SIZE,
			metadata + blocks[i] * UI_SAVES_BLOCK_SIZE, UI_SAVES_BLOCK_SIZE) ||
			f_sync(fp) != FR_OK) return false;
	}
	return true;
}

/* Opens the image the page listed, as it is, with its system blocks read. */
static uiSavesRawStatus_t imageOpen(file_handle *image, FFFIL *fp, BYTE mode,
	uint8_t *metadata)
{
	/* No reader may hold the file while it is opened for writing. */
	image->device->closeFile(image);
	if(f_open(fp, image->name, mode) != FR_OK) {
		return (mode & FA_WRITE) ? UI_SAVES_RAW_WRITE_ERROR : UI_SAVES_RAW_READ_ERROR;
	}
	if(f_size(fp) != image->size) {
		f_close(fp);
		return UI_SAVES_RAW_INVALID_SIZE;
	}
	if(metadata != NULL && !imageRead(fp, 0u, metadata, UI_SAVES_RAW_METADATA_SIZE)) {
		f_close(fp);
		return UI_SAVES_RAW_READ_ERROR;
	}
	return UI_SAVES_RAW_OK;
}

/* Each written system block, and an added save's blocks, as the card now
 * holds them: the image opened again, read only. */
static bool readBack(file_handle *image, const uint8_t *metadata,
	const uiSavesRawChange_t *change, const uiSavesRawCard_t *card,
	const uint8_t *gci, uint8_t *block)
{
	FFFIL fp;
	unsigned blocks = gci != NULL ? UISaves_Blocks(gci) : 0u;
	unsigned at = change->first;
	unsigned i;
	bool same = imageOpen(image, &fp, FA_READ, NULL) == UI_SAVES_RAW_OK;

	if(!same) return false;
	for(i = 0u; same && i < blocks; i++) {
		same = at >= UI_SAVES_RAW_SYSTEM_BLOCKS && at < card->totalBlocks &&
			imageRead(&fp, at * UI_SAVES_BLOCK_SIZE, block, UI_SAVES_BLOCK_SIZE) &&
			!memcmp(block, gci + UI_SAVES_ENTRY_SIZE + i * UI_SAVES_BLOCK_SIZE,
			UI_SAVES_BLOCK_SIZE);
		if(same) at = card->next[at];
	}
	for(i = 0u; same && i < 4u; i++) {
		unsigned system = i < 2u ? change->map[i] : change->directory[i - 2u];
		if(system == 0u) continue;
		same = imageRead(&fp, system * UI_SAVES_BLOCK_SIZE, block, UI_SAVES_BLOCK_SIZE) &&
			!memcmp(block, metadata + system * UI_SAVES_BLOCK_SIZE, UI_SAVES_BLOCK_SIZE);
	}
	f_close(&fp);
	return same;
}

/* Plans and writes an erase on an open image, and reads it back. */
static uiSavesRawStatus_t eraseOpen(file_handle *image, FFFIL *fp,
	uint8_t *metadata, const uint8_t entry[UI_SAVES_ENTRY_SIZE],
	uiSavesRawCard_t *card, uint8_t *block)
{
	uiSavesRawChange_t change;
	uiSavesRawStatus_t status = UISavesRaw_PlanErase(metadata, image->size, entry,
		card, &change);

	if(status != UI_SAVES_RAW_OK) {
		f_close(fp);
		return status;
	}
	/* The directory first: stopped before the map, the save is gone and its
	 * blocks are held, as on the console. */
	if(!writeSystem(fp, metadata, change.directory) ||
		!writeSystem(fp, metadata, change.map)) {
		f_close(fp);
		return UI_SAVES_RAW_WRITE_ERROR;
	}
	f_close(fp);
	return readBack(image, metadata, &change, card, NULL, block) ?
		UI_SAVES_RAW_OK : UI_SAVES_RAW_MISMATCH;
}

uiSavesRawStatus_t SavesRaw_Add(file_handle *image, const uint8_t *gci,
	uint32_t length)
{
	FFFIL fp;
	uiSavesRawChange_t change;
	uiSavesRawCard_t *card = NULL;
	uint8_t *metadata = NULL;
	uint8_t *block = NULL;
	uint8_t added[UI_SAVES_ENTRY_SIZE];
	uiSavesRawStatus_t status;
	unsigned blocks;
	unsigned at;
	unsigned i;

	if(gci == NULL || length <= UI_SAVES_ENTRY_SIZE) return UI_SAVES_RAW_INVALID_ARGUMENT;
	blocks = UISaves_Blocks(gci);
	if(blocks == 0u || blocks > UI_SAVES_RAW_MAX_BLOCKS ||
		length != UI_SAVES_ENTRY_SIZE + blocks * UI_SAVES_BLOCK_SIZE) {
		return UI_SAVES_RAW_INVALID_ARGUMENT;
	}
	if(!SavesRaw_Writable(image)) return UI_SAVES_RAW_READ_ONLY;
	metadata = memalign(32, UI_SAVES_RAW_METADATA_SIZE);
	block = memalign(32, UI_SAVES_BLOCK_SIZE);
	card = calloc(1, sizeof(*card));
	if(metadata == NULL || block == NULL || card == NULL) {
		status = UI_SAVES_RAW_READ_ERROR;
		goto done;
	}
	status = imageOpen(image, &fp, FA_READ | FA_WRITE, metadata);
	if(status != UI_SAVES_RAW_OK) goto done;
	status = UISavesRaw_PlanAdd(metadata, image->size, gci, card, &change);
	if(status != UI_SAVES_RAW_OK) {
		f_close(&fp);
		goto done;
	}
	/* The save's blocks go where the map still shows free blocks: stopped
	 * here, the image reads as it did. Then the map, then the directory. */
	for(i = 0u, at = change.first; i < blocks; i++, at = card->next[at]) {
		memcpy(block, gci + UI_SAVES_ENTRY_SIZE + i * UI_SAVES_BLOCK_SIZE,
			UI_SAVES_BLOCK_SIZE);
		if(!imageWrite(&fp, at * UI_SAVES_BLOCK_SIZE, block, UI_SAVES_BLOCK_SIZE)) break;
	}
	if(i != blocks || f_sync(&fp) != FR_OK || !writeSystem(&fp, metadata, change.map) ||
		!writeSystem(&fp, metadata, change.directory)) {
		f_close(&fp);
		status = UI_SAVES_RAW_WRITE_ERROR;
		goto done;
	}
	f_close(&fp);
	if(readBack(image, metadata, &change, card, gci, block)) goto done;
	/* It wasn't there before (the plan checked), so what is there now is
	 * this copy's own: take it out, if the directory holds it as written. */
	status = UI_SAVES_RAW_MISMATCH;
	memcpy(added, gci, sizeof(added));
	added[0x36] = (uint8_t)(change.first >> 8);
	added[0x37] = (uint8_t)change.first;
	if(imageOpen(image, &fp, FA_READ | FA_WRITE, metadata) == UI_SAVES_RAW_OK) {
		(void)eraseOpen(image, &fp, metadata, added, card, block);
	}
done:
	free(card);
	free(block);
	free(metadata);
	return status;
}

uiSavesRawStatus_t SavesRaw_Erase(file_handle *image,
	const uint8_t entry[UI_SAVES_ENTRY_SIZE])
{
	FFFIL fp;
	uiSavesRawCard_t *card = NULL;
	uint8_t *metadata = NULL;
	uint8_t *block = NULL;
	uiSavesRawStatus_t status;

	if(entry == NULL) return UI_SAVES_RAW_INVALID_ARGUMENT;
	if(!SavesRaw_Writable(image)) return UI_SAVES_RAW_READ_ONLY;
	metadata = memalign(32, UI_SAVES_RAW_METADATA_SIZE);
	block = memalign(32, UI_SAVES_BLOCK_SIZE);
	card = calloc(1, sizeof(*card));
	if(metadata == NULL || block == NULL || card == NULL) {
		status = UI_SAVES_RAW_READ_ERROR;
	}
	else if((status = imageOpen(image, &fp, FA_READ | FA_WRITE, metadata)) ==
		UI_SAVES_RAW_OK) {
		status = eraseOpen(image, &fp, metadata, entry, card, block);
	}
	free(card);
	free(block);
	free(metadata);
	return status;
}
