#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ui_saves_metadata.h"

static void write32(uint8_t *p, uint32_t n)
{
	p[0] = (uint8_t)(n >> 24);
	p[1] = (uint8_t)(n >> 16);
	p[2] = (uint8_t)(n >> 8);
	p[3] = (uint8_t)n;
}

static void date(uint32_t seconds, const char *want)
{
	char text[17];
	assert(UISaves_FormatUpdated(seconds, text, sizeof(text)));
	assert(!strcmp(text, want));
}

int main(void)
{
	uint8_t entry[UI_SAVES_ENTRY_SIZE] = {0};
	uiSavesGameStats_t stats = {0};
	char text[17];
	unsigned i;

	date(1u, "2000-01-01 00:00");
	date(5101260u, "2000-02-29 01:01");
	date(1200798840u, "2038-01-19 03:14");
	date(3160857600u, "2100-03-01 00:00");
	date(UINT32_MAX - 1u, "2136-02-07 06:28");
	memset(text, 'x', sizeof(text));
	assert(!UISaves_FormatUpdated(0u, text, sizeof(text)) && text[0] == '\0');
	assert(!UISaves_FormatUpdated(UINT32_MAX, text, sizeof(text)) && text[0] == '\0');
	assert(!UISaves_FormatUpdated(1u, text, 16u) && text[0] == '\0');
	assert(!UISaves_FormatUpdated(1u, NULL, 17u));
	assert(!UISaves_FormatUpdated(1u, text, 0u));
	assert(UISaves_UpdatedSeconds(NULL) == 0u);
	for(i = 0u; i < 10000u; i++) {
		uint32_t value = i * 429491u + 1u;
		assert(UISaves_FormatUpdated(value, text, sizeof(text)));
		assert(strlen(text) == 16u);
	}
	memcpy(entry, "GALE01", 6u);
	entry[0x39] = 11u;
	write32(entry + 0x28, 100u);
	assert(UISaves_UpdatedSeconds(entry) == 100u);
	assert(UISaves_StatsAdd(&stats, entry, "GALE01", 2u));
	assert(stats.saves == 1u && stats.blocks == 11u && stats.sourceSaves[2] == 1u);
	assert(stats.updatedKnown && stats.latestUpdated == 100u);
	assert(!UISaves_StatsAdd(&stats, entry, "GALE02", 0u));
	assert(!UISaves_StatsAdd(&stats, entry, "GALP01", 0u));
	write32(entry + 0x28, 75u);
	entry[0x39] = 3u;
	assert(UISaves_StatsAdd(&stats, entry, "GALE01", 0u));
	assert(stats.saves == 2u && stats.blocks == 14u && stats.latestUpdated == 100u);
	assert(stats.sourceSaves[0] == 1u && stats.sourceSaves[1] == 0u);
	write32(entry + 0x28, 125u);
	assert(UISaves_StatsAdd(&stats, entry, "GALE01", 1u));
	assert(stats.latestUpdated == 125u && stats.sourceSaves[1] == 1u);
	write32(entry + 0x28, UINT32_MAX);
	assert(UISaves_UpdatedSeconds(entry) == 0u);
	assert(UISaves_StatsAdd(&stats, entry, "GALE01", 2u));
	assert(stats.latestUpdated == 125u);
	entry[0x39] = 0u;
	assert(!UISaves_StatsAdd(&stats, entry, "GALE01", 2u));
	entry[0x38] = 0xffu;
	entry[0x39] = 0xffu;
	assert(!UISaves_StatsAdd(&stats, entry, "GALE01", 2u));
	assert(!UISaves_StatsAdd(&stats, entry, "GALE01", 3u));
	assert(!UISaves_StatsAdd(NULL, entry, "GALE01", 0u));
	assert(!UISaves_StatsAdd(&stats, NULL, "GALE01", 0u));
	assert(!UISaves_StatsAdd(&stats, entry, NULL, 0u));
	entry[0x38] = 0u;
	entry[0x39] = 1u;
	stats.blocks = UINT32_MAX;
	assert(!UISaves_StatsAdd(&stats, entry, "GALE01", 0u));
	puts("save metadata: exact identity, timestamps, calendar and totals PASS");
	return 0;
}
