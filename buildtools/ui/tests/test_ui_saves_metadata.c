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

static void displayTitles(void)
{
	uint8_t entry[64] = {0};
	char title[64], shortText[5];
	static const uint8_t unsafe[] = " \tNight\n\r  Circuit\177 2  ";
	static const uint8_t cp1252[] = "POK\351MON \231";
	static const uint8_t utf8[] = "POK\303\251MON \342\204\242";
	static const uint8_t fallbackNames[4][6] = {
		{'G','U','G','E','6','9'}, {'G','Y','Q','E','0','1'},
		{'G','C','6','E','0','1'}, {'G','M','4','E','0','1'}
	};
	static const char *const expected[] = {"Need for Speed: Underground 2",
		"Mario Superstar Baseball", "Pokemon Colosseum", "Mario Kart: Double Dash!!"};
	size_t i;

	assert(UISaves_DisplayText(title, sizeof(title), unsafe, sizeof(unsafe)));
	assert(!strcmp(title, "Night Circuit 2"));
	assert(UISaves_DisplayText(title, sizeof(title), cp1252, sizeof(cp1252)));
	assert(!memcmp(title, cp1252, sizeof(cp1252)));
	assert(UISaves_DisplayText(title, sizeof(title), utf8, sizeof(utf8)));
	assert(!memcmp(title, cp1252, sizeof(cp1252)));
	assert(!UISaves_DisplayText(title, sizeof(title), (const uint8_t *)" \t\r\n", 4u));
	assert(title[0] == '\0');
	assert(!UISaves_DisplayText(NULL, 4u, unsafe, sizeof(unsafe)));
	assert(!UISaves_DisplayText(title, 0u, unsafe, sizeof(unsafe)));
	assert(!UISaves_DisplayText(title, sizeof(title), NULL, 32u));
	assert(UISaves_DisplayText(shortText, sizeof(shortText), (const uint8_t *)"abcde", 5u));
	assert(!strcmp(shortText, "abcd"));

	entry[0x39] = 1u;
	memcpy(entry + 8u, "opaque_save_identifier", 22u);
	memset(entry + 0x2cu, 0xff, 4u);
	memset(entry + 0x3cu, 0xff, 4u);
	for(i = 0u; i < 4u; i++) {
		memcpy(entry, fallbackNames[i], 6u);
		assert(UISaves_DisplayTitle(title, sizeof(title), entry, NULL, 0u));
		assert(!strcmp(title, expected[i]));
		entry[3] = 'P';
		assert(UISaves_DisplayTitle(title, sizeof(title), entry, NULL, 0u));
		assert(!strcmp(title, expected[i]));
		assert(UISaves_DisplayTitle(title, sizeof(title), entry, unsafe, sizeof(unsafe)));
		assert(!strcmp(title, "Night Circuit 2"));
	}
	memcpy(entry, "GUGE70", 6u); /* Different maker must not inherit a title. */
	assert(UISaves_DisplayTitle(title, sizeof(title), entry, NULL, 0u));
	assert(!strcmp(title, "opaque save identifier"));
	memcpy(entry, "GUGE69", 6u);
	entry[0x39] = 0u; /* An invalid directory entry cannot select the catalog. */
	assert(UISaves_DisplayTitle(title, sizeof(title), entry, NULL, 0u));
	assert(!strcmp(title, "opaque save identifier"));
	assert(!UISaves_DisplayTitle(title, sizeof(title), NULL, NULL, 0u));
	assert(title[0] == '\0');
	memset(entry + 8u, 'x', 32u); /* Full, unterminated directory name stays bounded. */
	assert(UISaves_DisplayTitle(title, sizeof(title), entry, NULL, 0u));
	assert(strlen(title) == 32u);
}

int main(void)
{
	uint8_t entry[UI_SAVES_ENTRY_SIZE] = {0}, other[UI_SAVES_ENTRY_SIZE];
	uiSavesGameStats_t stats = {0};
	uiSavesStatsFiles_t files = {0};
	char text[17];
	unsigned i;
	displayTitles();

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
	memcpy(entry + 8, "SuperSmashBros0110290334", 24u);
	entry[0x39] = 11u;
	write32(entry + 0x28, 100u);
	assert(UISaves_UpdatedSeconds(entry) == 100u);
	assert(UISaves_StatsAdd(&stats, &files, entry, "GALE01", 2u));
	assert(stats.saves == 1u && stats.files == 1u && stats.blocks == 11u &&
		stats.sourceSaves[2] == 1u);
	assert(stats.updatedKnown && stats.latestUpdated == 100u);
	assert(!UISaves_StatsAdd(&stats, &files, entry, "GALE02", 0u));
	assert(!UISaves_StatsAdd(&stats, &files, entry, "GALP01", 0u));
	/* An older copy of the same file is a copy, not more blocks... */
	write32(entry + 0x28, 75u);
	entry[0x39] = 3u;
	assert(UISaves_StatsAdd(&stats, &files, entry, "GALE01", 0u));
	assert(stats.saves == 2u && stats.files == 1u && stats.blocks == 11u &&
		stats.latestUpdated == 100u);
	assert(stats.sourceSaves[0] == 1u && stats.sourceSaves[1] == 0u);
	/* ...and a newer one's blocks take the place of the file's. */
	write32(entry + 0x28, 125u);
	assert(UISaves_StatsAdd(&stats, &files, entry, "GALE01", 1u));
	assert(stats.saves == 3u && stats.files == 1u && stats.blocks == 3u);
	assert(stats.latestUpdated == 125u && stats.sourceSaves[1] == 1u);
	write32(entry + 0x28, UINT32_MAX);
	assert(UISaves_UpdatedSeconds(entry) == 0u);
	entry[0x39] = 9u;
	assert(UISaves_StatsAdd(&stats, &files, entry, "GALE01", 2u));
	assert(stats.latestUpdated == 125u && stats.blocks == 3u);
	/* Another name is another file of the game: its blocks add. A name
	 * compares as the card does, up to its end. */
	assert(UISaves_SameFile(entry, entry) && !UISaves_SameFile(entry, NULL));
	memcpy(other, entry, sizeof(other));
	other[8 + 24 + 1] = 'x';
	assert(UISaves_SameFile(entry, other));
	other[8 + 23] = '5';
	assert(!UISaves_SameFile(entry, other));
	memcpy(other, entry, sizeof(other));
	other[5] = '2';
	assert(!UISaves_SameFile(entry, other));
	memcpy(other, entry, sizeof(other));
	memcpy(other + 8, "SuperSmashBros0110290335", 24u);
	assert(UISaves_StatsAdd(&stats, &files, other, "GALE01", 2u));
	assert(stats.saves == 5u && stats.files == 2u && stats.blocks == 12u);
	entry[0x39] = 0u;
	assert(!UISaves_StatsAdd(&stats, &files, entry, "GALE01", 2u));
	entry[0x38] = 0xffu;
	entry[0x39] = 0xffu;
	assert(!UISaves_StatsAdd(&stats, &files, entry, "GALE01", 2u));
	assert(!UISaves_StatsAdd(&stats, &files, entry, "GALE01", 3u));
	assert(!UISaves_StatsAdd(NULL, &files, entry, "GALE01", 0u));
	assert(!UISaves_StatsAdd(&stats, NULL, entry, "GALE01", 0u));
	assert(!UISaves_StatsAdd(&stats, &files, NULL, "GALE01", 0u));
	assert(!UISaves_StatsAdd(&stats, &files, entry, NULL, 0u));
	entry[0x38] = 0u;
	entry[0x39] = 1u;
	stats.blocks = UINT32_MAX;
	assert(!UISaves_StatsAdd(&stats, &files, entry, "GALE01", 0u));
	/* Past the files it can tell apart, each more counts as a file and the
	 * scan says it is partial. */
	memset(&stats, 0, sizeof(stats));
	memset(&files, 0, sizeof(files));
	for(i = 0u; i <= UI_SAVES_STATS_FILES; i++) {
		other[8] = (uint8_t)('A' + i);
		assert(UISaves_StatsAdd(&stats, &files, other, "GALE01", 2u));
		assert(stats.partial == (i == UI_SAVES_STATS_FILES));
	}
	assert(stats.files == UI_SAVES_STATS_FILES + 1u && files.count == UI_SAVES_STATS_FILES);
	puts("save metadata: bounded titles, encoding, exact identity, timestamps and totals by file PASS");
	return 0;
}
