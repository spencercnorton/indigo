#define _POSIX_C_SOURCE 200809L
#include "ui_game_history.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char header[] = "SWISS_PLAY_HISTORY 1\n";

static bool validId(const char *id, size_t length)
{
	size_t i;
	if(id == NULL || length != 6u) return false;
	for(i = 0u; i < length; ++i) {
		if(!((id[i] >= 'A' && id[i] <= 'Z') ||
			(id[i] >= '0' && id[i] <= '9'))) return false;
	}
	return true;
}

static uint32_t checksum(const char *data, size_t size)
{
	uint32_t hash = UINT32_C(2166136261);
	size_t i;
	for(i = 0u; i < size; ++i) {
		hash ^= (unsigned char)data[i];
		hash *= UINT32_C(16777619);
	}
	return hash;
}

void UIGameHistory_Init(uiGameHistory_t *history, bool available)
{
	if(history == NULL) return;
	memset(history, 0, sizeof(*history));
	history->available = available;
}

bool UIGameHistory_ValidTime(uint64_t unixSeconds)
{
	return unixSeconds >= UI_GAME_HISTORY_MIN_TIME &&
		unixSeconds <= UI_GAME_HISTORY_MAX_TIME;
}

uint64_t UIGameHistory_Find(const uiGameHistory_t *history,
	const char *gameId, size_t gameIdLength)
{
	size_t i;
	if(history == NULL || !history->available ||
		history->count > UI_GAME_HISTORY_CAPACITY || !validId(gameId, gameIdLength)) return 0u;
	for(i = 0u; i < history->count; ++i) {
		if(memcmp(history->entries[i].gameId, gameId, 6u) == 0)
			return history->entries[i].unixSeconds;
	}
	return 0u;
}

bool UIGameHistory_Record(uiGameHistory_t *history, const char *gameId,
	size_t gameIdLength, uint64_t unixSeconds)
{
	size_t i;
	size_t index;
	if(history == NULL || !history->available ||
		history->count > UI_GAME_HISTORY_CAPACITY || !validId(gameId, gameIdLength) ||
		!UIGameHistory_ValidTime(unixSeconds) || history->generation == UINT64_MAX) return false;
	index = history->count;
	for(i = 0u; i < history->count; ++i) {
		if(memcmp(history->entries[i].gameId, gameId, 6u) == 0) { index = i; break; }
	}
	if(index == UI_GAME_HISTORY_CAPACITY) {
		index = 0u;
		for(i = 1u; i < history->count; ++i)
			if(history->entries[i].unixSeconds < history->entries[index].unixSeconds) index = i;
	} else if(index == history->count) {
		++history->count;
	}
	memcpy(history->entries[index].gameId, gameId, 6u);
	history->entries[index].gameId[6] = '\0';
	history->entries[index].unixSeconds = unixSeconds;
	++history->generation;
	return true;
}

int UIGameHistory_SelectSlot(const uiGameHistory_t *first, const uiGameHistory_t *second)
{
	bool a = first != NULL && first->available;
	bool b = second != NULL && second->available;
	if(!a && !b) return -1;
	return b && (!a || second->generation > first->generation) ? 1 : 0;
}

size_t UIGameHistory_Serialize(const uiGameHistory_t *history,
	char *out, size_t capacity)
{
	size_t used = sizeof(header) - 1u;
	size_t i;
	int written;
	if(out == NULL || capacity == 0u) return 0u;
	out[0] = '\0';
	if(history == NULL || !history->available ||
		history->count > UI_GAME_HISTORY_CAPACITY || capacity <= used) return 0u;
	memcpy(out, header, used + 1u);
	written = snprintf(out + used, capacity - used, "Generation=%" PRIu64 "\n", history->generation);
	if(written < 0 || (size_t)written >= capacity - used) goto fail;
	used += (size_t)written;
	for(i = 0u; i < history->count; ++i) {
		const uiGameHistoryEntry_t *entry = &history->entries[i];
		if(!validId(entry->gameId, 6u) || !UIGameHistory_ValidTime(entry->unixSeconds)) goto fail;
		written = snprintf(out + used, capacity - used, "%.6s=%" PRIu64 "\n",
			entry->gameId, entry->unixSeconds);
		if(written < 0 || (size_t)written >= capacity - used) goto fail;
		used += (size_t)written;
	}
	written = snprintf(out + used, capacity - used, "Checksum=%08" PRIX32 "\n",
		checksum(out, used));
	if(written < 0 || (size_t)written >= capacity - used) goto fail;
	return used + (size_t)written;
fail:
	out[0] = '\0';
	return 0u;
}

bool UIGameHistory_Parse(uiGameHistory_t *history, const char *data, size_t size)
{
	uiGameHistory_t parsed;
	size_t offset = sizeof(header) - 1u;
	uint64_t generation = 0u;
	bool generationSeen = false;
	UIGameHistory_Init(history, false);
	if(history == NULL || data == NULL || size >= UI_GAME_HISTORY_FILE_CAPACITY ||
		size < offset + 18u || memcmp(data, header, offset) != 0) return false;
	UIGameHistory_Init(&parsed, true);
	while(offset < size) {
		size_t end = offset;
		size_t i;
		uint64_t value = 0u;
		while(end < size && data[end] != '\n') ++end;
		if(end == size) return false;
		if(!generationSeen) {
			if(end - offset < 12u || memcmp(data + offset, "Generation=", 11u) != 0) return false;
			for(i = offset + 11u; i < end; ++i) {
				unsigned char digit = (unsigned char)data[i];
				if(digit < '0' || digit > '9' || generation > (UINT64_MAX - (uint64_t)(digit - '0')) / 10u) return false;
				generation = generation * 10u + (uint64_t)(digit - '0');
			}
			generationSeen = true;
			offset = end + 1u;
			continue;
		}
		if(end - offset == 17u && memcmp(data + offset, "Checksum=", 9u) == 0) {
			uint32_t expected = 0u;
			if(end + 1u != size) return false;
			for(i = offset + 9u; i < end; ++i) {
				unsigned char digit = (unsigned char)data[i];
				if(digit >= '0' && digit <= '9') digit = (unsigned char)(digit - '0');
				else if(digit >= 'A' && digit <= 'F') digit = (unsigned char)(digit - 'A' + 10);
				else return false;
				expected = (expected << 4) | digit;
			}
			if(expected != checksum(data, offset)) return false;
			if(generation < parsed.count) return false;
			parsed.generation = generation;
			*history = parsed;
			return true;
		}
		if(end - offset < 8u || data[offset + 6u] != '=' ||
			!validId(data + offset, 6u) || parsed.count >= UI_GAME_HISTORY_CAPACITY ||
			UIGameHistory_Find(&parsed, data + offset, 6u) != 0u) return false;
		for(i = offset + 7u; i < end; ++i) {
			unsigned char digit = (unsigned char)data[i];
			if(digit < '0' || digit > '9' || value > (UINT64_MAX - (uint64_t)(digit - '0')) / 10u) return false;
			value = value * 10u + (uint64_t)(digit - '0');
		}
		if(!UIGameHistory_Record(&parsed, data + offset, 6u, value)) return false;
		offset = end + 1u;
	}
	return false;
}

void UIGameHistory_Format(char *out, size_t capacity, bool available,
	uint64_t unixSeconds)
{
	static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	time_t value = (time_t)unixSeconds;
	struct tm calendar;
	const char *fallback = !available ? "History unavailable" :
		(unixSeconds == 0u ? "No play recorded" : "Date unavailable");
	if(out == NULL || capacity == 0u) return;
	if(available && UIGameHistory_ValidTime(unixSeconds) && value >= 0 &&
		(uint64_t)value == unixSeconds && localtime_r(&value, &calendar) != NULL &&
		calendar.tm_mon >= 0 && calendar.tm_mon < 12) {
		int hour = calendar.tm_hour % 12;
		(void)snprintf(out, capacity, "%s %d, %d  %d:%02d %s",
			months[calendar.tm_mon], calendar.tm_mday, calendar.tm_year + 1900,
			hour == 0 ? 12 : hour, calendar.tm_min, calendar.tm_hour >= 12 ? "PM" : "AM");
	} else {
		(void)snprintf(out, capacity, "%s", fallback);
	}
}

const char *UIGameHistory_SaveStatus(uiGameSaveStatus_t status)
{
	switch(status) {
		case UI_GAME_SAVE_CONFIRMED: return "Save data found";
		case UI_GAME_SAVE_NOT_FOUND: return "No save found";
		case UI_GAME_SAVE_NOT_CHECKED: return "Check in game";
		default: return "Status unavailable";
	}
}
