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

static uint8_t captionByte(const uint8_t *text, size_t length, size_t *at)
{
	uint8_t value = text[(*at)++];

	/* The UI font is Windows-1252. These pairs cover its Latin-1 letters;
	 * a byte that is not followed by UTF-8 continuation stays unchanged. */
	if((value == 0xc2u || value == 0xc3u) && *at < length &&
		(text[*at] & 0xc0u) == 0x80u) {
		unsigned code = ((unsigned)value & 0x1fu) << 6 |
			((unsigned)text[(*at)++] & 0x3fu);

		return code >= 0xa0u ? (uint8_t)code : (uint8_t)' ';
	}
	if(value == 0xe2u && *at + 1u < length) {
		uint8_t middle = text[*at], last = text[*at + 1u], mapped = 0u;

		if(middle == 0x80u) {
			switch(last) {
				case 0x98u: mapped = 0x91u; break;
				case 0x99u: mapped = 0x92u; break;
				case 0x9cu: mapped = 0x93u; break;
				case 0x9du: mapped = 0x94u; break;
				case 0x93u: mapped = 0x96u; break;
				case 0x94u: mapped = 0x97u; break;
				case 0xa6u: mapped = 0x85u; break;
				default: break;
			}
		}
		else if(middle == 0x82u && last == 0xacu) mapped = 0x80u;
		else if(middle == 0x84u && last == 0xa2u) mapped = 0x99u;
		if(mapped != 0u) {
			*at += 2u;
			return mapped;
		}
	}
	return value;
}

bool UISaves_DisplayText(char *out, size_t capacity, const uint8_t *text,
	size_t length)
{
	size_t at = 0u, written = 0u;
	bool space = false;

	if(out == NULL || capacity == 0u) return false;
	out[0] = '\0';
	if(text == NULL) return false;
	while(at < length && text[at] != 0u && written + 1u < capacity) {
		uint8_t value = captionByte(text, length, &at);

		if(value < 0x20u || value == 0x7fu || value == 0x81u ||
			value == 0x8du || value == 0x8fu || value == 0x90u ||
			value == 0x9du || value == 0xa0u || value == ' ') {
			space = written > 0u;
			continue;
		}
		if(space && written + 2u < capacity) out[written++] = ' ';
		space = false;
		out[written++] = (char)value;
	}
	out[written] = '\0';
	return written > 0u;
}

bool UISaves_DisplayTitle(char *out, size_t capacity,
	const uint8_t entry[UI_SAVES_ENTRY_SIZE], const uint8_t *comment,
	size_t commentLength)
{
	static const struct {
		char game[4], maker[3];
		const char *title;
	} fallback[] = {
		{"GUG", "69", "Need for Speed: Underground 2"},
		{"GYQ", "01", "Mario Superstar Baseball"},
		{"GC6", "01", "Pokemon Colosseum"},
		{"GM4", "01", "Mario Kart: Double Dash!!"}
	};
	uint8_t name[UI_SAVES_NAME_LENGTH];
	size_t i;
	bool identity = entry != NULL && UISaves_Blocks(entry) > 0u &&
		UISaves_Blocks(entry) <= 2043u;

	if(commentLength > UI_SAVES_NAME_LENGTH) commentLength = UI_SAVES_NAME_LENGTH;
	if(UISaves_DisplayText(out, capacity, comment, commentLength)) return true;
	if(entry == NULL) return false;
	for(i = 0u; i < 6u; i++) {
		identity = identity && ((entry[i] >= 'A' && entry[i] <= 'Z') ||
			(entry[i] >= '0' && entry[i] <= '9'));
	}
	for(i = 0u; i < sizeof(fallback) / sizeof(fallback[0]); i++) {
		if(identity && !memcmp(entry, fallback[i].game, 3u) &&
			!memcmp(entry + 4u, fallback[i].maker, 2u) &&
			entry[3] >= 'A' && entry[3] <= 'Z') {
			return UISaves_DisplayText(out, capacity,
				(const uint8_t *)fallback[i].title, strlen(fallback[i].title));
		}
	}
	memcpy(name, entry + 8u, sizeof(name));
	for(i = 0u; i < sizeof(name); i++) {
		if(name[i] == '_') name[i] = ' ';
	}
	return UISaves_DisplayText(out, capacity, name, sizeof(name));
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
