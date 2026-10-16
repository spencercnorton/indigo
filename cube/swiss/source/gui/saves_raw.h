#ifndef SAVES_RAW_H
#define SAVES_RAW_H

#include "deviceHandler.h"
#include "ui_saves_raw.h"

bool SavesRaw_IsImageName(const char *name);
uiSavesRawStatus_t SavesRaw_Load(file_handle *image, uiSavesRawCard_t *card);

/* Reads a virtual .gci: its entry, then its blocks in chain order. */
bool SavesRaw_ReadGci(file_handle *image, const uiSavesRawCard_t *card,
	unsigned ordinal, uint32_t offset, void *destination, uint32_t length);

/* Whether the image's saves can change: images change in place through
 * FatFs, on a device that writes. Nothing here creates, formats, grows or
 * shrinks an image. */
bool SavesRaw_Writable(const file_handle *image);

/* Adds the save gci holds (a .gci: its entry, then its blocks, length bytes)
 * to the image: its blocks, then the block map, then the directory, each
 * made to last before the next, then every block written read back. An
 * added save that doesn't read back the same is taken out again. */
uiSavesRawStatus_t SavesRaw_Add(file_handle *image, const uint8_t *gci,
	uint32_t length);

/* Erases the save whose directory entry is exactly entry, as it was listed:
 * the directory, then the block map, then both read back. */
uiSavesRawStatus_t SavesRaw_Erase(file_handle *image,
	const uint8_t entry[UI_SAVES_ENTRY_SIZE]);

#endif
