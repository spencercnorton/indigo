#define _POSIX_C_SOURCE 200809L
#include "ui_game_history.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void roundtrip_and_failures(void)
{
	uiGameHistory_t a, b;
	char text[UI_GAME_HISTORY_FILE_CAPACITY];
	char original[UI_GAME_HISTORY_FILE_CAPACITY];
	size_t n, i;
	UIGameHistory_Init(&a, true);
	assert(UIGameHistory_Record(&a, "GMSE01", 6u, UI_GAME_HISTORY_MIN_TIME));
	assert(UIGameHistory_Record(&a, "GALE01", 6u, UI_GAME_HISTORY_MAX_TIME));
	assert(UIGameHistory_Record(&a, "GMSE01", 6u, UI_GAME_HISTORY_MIN_TIME + 60u));
	assert(a.count == 2u && a.generation == 3u);
	n = UIGameHistory_Serialize(&a, text, sizeof(text));
	assert(n > 0u && n < sizeof(text));
	assert(UIGameHistory_Parse(&b, text, n));
	assert(b.count == a.count && b.generation == a.generation);
	assert(UIGameHistory_Find(&b, "GMSE01", 6u) == UI_GAME_HISTORY_MIN_TIME + 60u);
	assert(UIGameHistory_Find(&b, "GALE01", 6u) == UI_GAME_HISTORY_MAX_TIME);
	assert(UIGameHistory_Find(&b, "GP7E01", 6u) == 0u);
	memcpy(original, text, n + 1u);
	/* Every truncation and single-byte corruption must fail closed. */
	for(i = 0u; i < n; ++i) {
		assert(!UIGameHistory_Parse(&b, original, i) && !b.available);
		memcpy(text, original, n + 1u);
		text[i] ^= 1;
		assert(!UIGameHistory_Parse(&b, text, n) && !b.available);
	}
	assert(UIGameHistory_Serialize(&a, text, 8u) == 0u && text[0] == '\0');
	assert(!UIGameHistory_Parse(&b, NULL, n));
	assert(!UIGameHistory_Parse(NULL, original, n));
	/* A checksum is not enough: invalid identities/epochs/duplicates reject. */
	a.entries[1] = a.entries[0];
	n = UIGameHistory_Serialize(&a, text, sizeof(text));
	assert(n != 0u && !UIGameHistory_Parse(&b, text, n));
	a.entries[1].gameId[0] = '!';
	assert(UIGameHistory_Serialize(&a, text, sizeof(text)) == 0u);
	UIGameHistory_Init(&a, true);
	n = UIGameHistory_Serialize(&a, text, sizeof(text));
	assert(n != 0u && UIGameHistory_Parse(&b, text, n));
	assert(b.available && b.count == 0u && b.generation == 0u);
}

static void bounds_and_recovery(void)
{
	uiGameHistory_t a, b;
	char id[7];
	unsigned int i;
	UIGameHistory_Init(&a, true);
	assert(!UIGameHistory_Record(&a, "GMSE01", 5u, UI_GAME_HISTORY_MIN_TIME));
	assert(!UIGameHistory_Record(&a, "game01", 6u, UI_GAME_HISTORY_MIN_TIME));
	assert(!UIGameHistory_Record(&a, "GMSE01", 6u, 0u));
	assert(!UIGameHistory_Record(&a, "GMSE01", 6u, UI_GAME_HISTORY_MIN_TIME - 1u));
	assert(!UIGameHistory_Record(&a, "GMSE01", 6u, UI_GAME_HISTORY_MAX_TIME + 1u));
	assert(!UIGameHistory_Record(&a, "GMSE01", 6u, UINT64_MAX));
	for(i = 0u; i < UI_GAME_HISTORY_CAPACITY; ++i) {
		(void)snprintf(id, sizeof(id), "G%05u", i);
		assert(UIGameHistory_Record(&a, id, 6u, UI_GAME_HISTORY_MIN_TIME + i));
	}
	b = a;
	assert(UIGameHistory_Record(&b, "GMSE01", 6u, UI_GAME_HISTORY_MAX_TIME));
	assert(b.count == UI_GAME_HISTORY_CAPACITY);
	assert(UIGameHistory_Find(&b, "G00000", 6u) == 0u);
	assert(UIGameHistory_Find(&b, "G00001", 6u) != 0u);
	assert(UIGameHistory_SelectSlot(&a, &b) == 1);
	assert(UIGameHistory_SelectSlot(&b, &a) == 0);
	b.available = false; /* interrupted newer write leaves older valid slot */
	assert(UIGameHistory_SelectSlot(&a, &b) == 0);
	a.available = false;
	assert(UIGameHistory_SelectSlot(&a, &b) == -1);
	assert(UIGameHistory_SelectSlot(NULL, NULL) == -1);
	assert(!UIGameHistory_Record(&a, "GMSE01", 6u, UI_GAME_HISTORY_MIN_TIME));
	a.available = true;
	a.generation = UINT64_MAX;
	assert(!UIGameHistory_Record(&a, "GMSE01", 6u, UI_GAME_HISTORY_MIN_TIME));
}

static void presentation(void)
{
	char text[64];
	assert(setenv("TZ", "UTC", 1) == 0);
	tzset();
	UIGameHistory_Format(text, sizeof(text), false, UI_GAME_HISTORY_MIN_TIME);
	assert(strcmp(text, "History unavailable") == 0);
	UIGameHistory_Format(text, sizeof(text), true, 0u);
	assert(strcmp(text, "No play recorded") == 0);
	UIGameHistory_Format(text, sizeof(text), true, 1u);
	assert(strcmp(text, "Date unavailable") == 0);
	UIGameHistory_Format(text, sizeof(text), true, UI_GAME_HISTORY_MIN_TIME);
	assert(strcmp(text, "Jan 1, 2001  12:00 AM") == 0);
	UIGameHistory_Format(text, sizeof(text), true, UI_GAME_HISTORY_MIN_TIME + 45000u);
	assert(strcmp(text, "Jan 1, 2001  12:30 PM") == 0);
	UIGameHistory_Format(text, 4u, true, UI_GAME_HISTORY_MIN_TIME);
	assert(text[3] == '\0');
	assert(strcmp(UIGameHistory_SaveStatus(UI_GAME_SAVE_NOT_CHECKED), "Check in game") == 0);
	assert(strcmp(UIGameHistory_SaveStatus(UI_GAME_SAVE_UNAVAILABLE), "Status unavailable") == 0);
}

int main(void)
{
	roundtrip_and_failures();
	bounds_and_recovery();
	presentation();
	puts("Game history model: PASS");
	return 0;
}
