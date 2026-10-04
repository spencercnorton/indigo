#include "ui_saves_metadata.h"

#include <stdio.h>
#include <string.h>

static uint32_t read32(const uint8_t *p)
{
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
		(uint32_t)p[2] << 8 | (uint32_t)p[3];
}

uint32_t UISaves_UpdatedSeconds(const uint8_t entry[UI_SAVES_ENTRY_SIZE])
{
	uint32_t seconds;

	if(entry == NULL) return 0u;
	seconds = read32(entry + 0x28);
	return seconds == UINT32_MAX ? 0u : seconds;
}

static bool leap(unsigned year)
{
	return year % 4u == 0u && (year % 100u != 0u || year % 400u == 0u);
}

bool UISaves_FormatUpdated(uint32_t seconds, char *out, size_t capacity)
{
	static const unsigned monthDays[12] = {31, 28, 31, 30, 31, 30,
		31, 31, 30, 31, 30, 31};
	unsigned year = 2000u, month = 0u;
	uint32_t days = seconds / 86400u;
	unsigned rest = seconds % 86400u;
	unsigned span;

	if(out == NULL || capacity == 0u) return false;
	out[0] = '\0';
	if(seconds == 0u || seconds == UINT32_MAX || capacity < 17u) return false;
	while(days >= (span = leap(year) ? 366u : 365u)) {
		days -= span;
		year++;
	}
	while(days >= (span = monthDays[month] +
		(month == 1u && leap(year) ? 1u : 0u))) {
		days -= span;
		month++;
	}
	snprintf(out, capacity, "%04u-%02u-%02u %02u:%02u", year, month + 1u,
		(unsigned)days + 1u, rest / 3600u, rest / 60u % 60u);
	return true;
}

bool UISaves_StatsAdd(uiSavesGameStats_t *stats,
	const uint8_t entry[UI_SAVES_ENTRY_SIZE], const char gameId[6],
	unsigned source)
{
	unsigned blocks;
	uint32_t updated;

	if(stats == NULL || entry == NULL || gameId == NULL ||
		source >= UI_SAVES_STATS_SOURCES || memcmp(entry, gameId, 6u) != 0) return false;
	blocks = UISaves_Blocks(entry);
	if(blocks == 0u || blocks > 2043u || stats->saves == UINT32_MAX ||
		stats->sourceSaves[source] == UINT32_MAX ||
		stats->blocks > UINT32_MAX - blocks) return false;
	stats->saves++;
	stats->blocks += blocks;
	stats->sourceSaves[source]++;
	updated = UISaves_UpdatedSeconds(entry);
	if(updated != 0u && (!stats->updatedKnown || updated > stats->latestUpdated)) {
		stats->latestUpdated = updated;
		stats->updatedKnown = true;
	}
	return true;
}
