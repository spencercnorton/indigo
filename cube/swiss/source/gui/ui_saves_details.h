#ifndef UI_SAVES_DETAILS_H
#define UI_SAVES_DETAILS_H

#include <stdbool.h>
#include <stdint.h>

/* Menu-thread copy of one save's already-read metadata. The renderer never
 * retains a file handle or a borrowed string, and cannot read storage. */
typedef struct {
	char name[64];
	char source[32];
	char updated[32];
	uint32_t blocks;
	bool estimated;
} uiSaveDetailsSnapshot_t;

bool UISaveDetails_Build(uiSaveDetailsSnapshot_t *snapshot, const char *name,
	uint32_t blocks, bool estimated, const char *source, const char *updated);
bool UISaveDetails_Valid(const uiSaveDetailsSnapshot_t *snapshot);

#endif
