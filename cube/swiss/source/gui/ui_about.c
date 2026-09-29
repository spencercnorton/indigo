/*
 * ui_about.c -- game descriptions for the Spotlight layout, indexed from
 * /swiss/ui/descriptions.txt. Format and contract: ui_about.h.
 */

#include <stdlib.h>
#include <string.h>

#include "ui_about.h"

#define UI_ABOUT_ID_LENGTH 6u

static bool idChar(char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

static bool validId(const char *id)
{
	size_t i;

	for(i = 0u; i < UI_ABOUT_ID_LENGTH; ++i) {
		if(!idChar(id[i])) {
			return false;
		}
	}
	return true;
}

/* By ID, then by place in the file, so a game's first line sorts first. */
static int compareEntries(const void *a, const void *b)
{
	const uiAboutEntry_t *left = (const uiAboutEntry_t *)a;
	const uiAboutEntry_t *right = (const uiAboutEntry_t *)b;
	int byId = memcmp(left->id, right->id, UI_ABOUT_ID_LENGTH);

	if(byId != 0) {
		return byId;
	}
	return left->offset < right->offset ? -1 : left->offset > right->offset;
}

size_t UIAbout_Index(uiAbout_t *about, const char *data, size_t size,
	uiAboutEntry_t *entries, size_t capacity)
{
	size_t line = 0u;
	size_t count = 0u;
	size_t kept = 0u;
	size_t i;

	if(about == NULL) {
		return 0u;
	}
	memset(about, 0, sizeof(*about));
	if(data == NULL || entries == NULL || size > UI_ABOUT_MAX_BYTES) {
		return 0u;
	}
	while(line < size) {
		size_t end = line;
		size_t stop;

		while(end < size && data[end] != '\n') {
			++end;
		}
		stop = end > line && data[end - 1u] == '\r' ? end - 1u : end;
		/* "GMSE01 text": an ID, a space or a tab, and some text. A comment
		 * or a blank line has no ID, so it is passed over like a bad one. */
		if(count < capacity && stop - line > UI_ABOUT_ID_LENGTH + 1u &&
			(data[line + UI_ABOUT_ID_LENGTH] == ' ' ||
			data[line + UI_ABOUT_ID_LENGTH] == '\t') && validId(data + line)) {
			size_t length = stop - line - UI_ABOUT_ID_LENGTH - 1u;
			memcpy(entries[count].id, data + line, UI_ABOUT_ID_LENGTH);
			entries[count].offset = (uint32_t)(line + UI_ABOUT_ID_LENGTH + 1u);
			entries[count].length = (uint16_t)(length > 0xFFFFu ? 0xFFFFu : length);
			++count;
		}
		line = end + 1u;
	}
	qsort(entries, count, sizeof(entries[0]), compareEntries);
	for(i = 0u; i < count; ++i) {
		if(kept > 0u && memcmp(entries[kept - 1u].id, entries[i].id,
			UI_ABOUT_ID_LENGTH) == 0) {
			continue;
		}
		entries[kept++] = entries[i];
	}
	about->data = data;
	about->entries = entries;
	about->count = kept;
	return kept;
}

/* The first entry whose ID's first `length` characters are not below gameId's. */
static size_t lowerBound(const uiAbout_t *about, const char *gameId,
	size_t length)
{
	size_t lo = 0u;
	size_t hi = about->count;

	while(lo < hi) {
		size_t mid = lo + (hi - lo) / 2u;
		if(memcmp(about->entries[mid].id, gameId, length) < 0) {
			lo = mid + 1u;
		}
		else {
			hi = mid;
		}
	}
	return lo;
}

bool UIAbout_Find(const uiAbout_t *about, const char *gameId, char *out,
	size_t capacity)
{
	const uiAboutEntry_t *found = NULL;
	size_t at;
	size_t length;
	size_t i;

	if(out == NULL || capacity == 0u) {
		return false;
	}
	out[0] = '\0';
	/* validId stops at a short ID's NUL: it never reads past it. */
	if(about == NULL || about->entries == NULL || gameId == NULL ||
		!validId(gameId)) {
		return false;
	}
	at = lowerBound(about, gameId, UI_ABOUT_ID_LENGTH);
	if(at < about->count && memcmp(about->entries[at].id, gameId,
		UI_ABOUT_ID_LENGTH) == 0) {
		found = &about->entries[at];
	}
	else {
		at = lowerBound(about, gameId, 4u);
		if(at < about->count && memcmp(about->entries[at].id, gameId, 4u) == 0) {
			found = &about->entries[at];
		}
	}
	if(found == NULL) {
		return false;
	}
	length = found->length < capacity - 1u ? found->length : capacity - 1u;
	for(i = 0u; i < length; ++i) {
		unsigned char c = (unsigned char)about->data[found->offset + i];
		out[i] = c < 0x20u || c == 0x7Fu ? ' ' : (char)c;
	}
	out[length] = '\0';
	return true;
}
