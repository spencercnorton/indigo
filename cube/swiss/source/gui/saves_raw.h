#ifndef SAVES_RAW_H
#define SAVES_RAW_H

#include "deviceHandler.h"
#include "ui_saves_raw.h"

bool SavesRaw_IsImageName(const char *name);
uiSavesRawStatus_t SavesRaw_Load(file_handle *image, uiSavesRawCard_t *card);

/* A read-only source: no write, delete, move or formatting operation exists
 * here. A caller may export the returned .gci through its normal copy path. */
bool SavesRaw_ReadGci(file_handle *image, const uiSavesRawCard_t *card,
	unsigned ordinal, uint32_t offset, void *destination, uint32_t length);

#endif
