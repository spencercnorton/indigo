#ifndef UI_SAVES_METADATA_H
#define UI_SAVES_METADATA_H

#include "ui_saves.h"

/* Slot A, Slot B and the configured Save Folder. Entries in a RAW image
 * and exported copies are counted separately; this is not a progress score. */
#define UI_SAVES_STATS_SOURCES 3u
typedef struct {
	uint32_t saves;
	uint32_t blocks;
	uint32_t latestUpdated;
	uint32_t sourceSaves[UI_SAVES_STATS_SOURCES];
	uint8_t checkedSources;
	bool updatedKnown;
	bool partial;
} uiSavesGameStats_t;

/* The one per-save date in a GCI/RAW directory entry: seconds since
 * 2000-01-01. There is no separate creation date. The UI treats zero and
 * all-ones dates as unavailable rather than showing an epoch/default date.
 * The outer SD file's dates do not describe this save. */
uint32_t UISaves_UpdatedSeconds(const uint8_t entry[UI_SAVES_ENTRY_SIZE]);

/* Calendar text from the save's recorded clock, without assuming a time
 * zone. Works beyond 2038 independently of the target's time_t width.
 * False leaves an empty string for an unavailable date or short buffer. */
bool UISaves_FormatUpdated(uint32_t seconds, char *out, size_t capacity);

/* A bounded caption for the ANSI IPL font: collapse controls/whitespace,
 * preserve Windows-1252 accents and symbols, and decode their common UTF-8
 * forms. The output is always terminated; false means no readable text. */
bool UISaves_DisplayText(char *out, size_t capacity, const uint8_t *text,
	size_t length);

/* Prefer a readable declared comment. A small identity fallback names games
 * whose saves can omit that comment; it changes display text only. Unknown
 * entries retain their bounded directory name, with separators as spaces.
 * Never scans arbitrary save payloads or changes art/date metadata. */
bool UISaves_DisplayTitle(char *out, size_t capacity,
	const uint8_t entry[UI_SAVES_ENTRY_SIZE], const uint8_t *comment,
	size_t commentLength);

/* Add one valid directory entry only when all six game/maker bytes match.
 * Blocks count the save data; wrappers and RAW system blocks are excluded. */
bool UISaves_StatsAdd(uiSavesGameStats_t *stats,
	const uint8_t entry[UI_SAVES_ENTRY_SIZE], const char gameId[6],
	unsigned source);

#endif
