/* Fuzz the play-history file (swiss/indigo/history): whatever is on the card,
 * Parse must not overrun, and a history it accepts must come back unchanged
 * through Serialize and Parse. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ui_game_history.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	uiGameHistory_t history, again;
	char file[UI_GAME_HISTORY_FILE_CAPACITY];
	char *copy = malloc(size ? size : 1);

	if (!copy)
		return 0;
	memcpy(copy, data, size);
	UIGameHistory_Init(&history, true);
	if (UIGameHistory_Parse(&history, copy, size)) {
		if (history.count > UI_GAME_HISTORY_CAPACITY)
			abort();
		for (size_t i = 0; i < history.count; i++) {
			if (memchr(history.entries[i].gameId, '\0', sizeof(history.entries[i].gameId)) == NULL ||
			    !UIGameHistory_ValidTime(history.entries[i].unixSeconds))
				abort();
		}
		size_t length = UIGameHistory_Serialize(&history, file, sizeof(file));
		if (length == 0 || length > sizeof(file))
			abort();
		UIGameHistory_Init(&again, true);
		if (!UIGameHistory_Parse(&again, file, length) || again.count != history.count ||
		    again.generation != history.generation)
			abort();
		for (size_t i = 0; i < history.count; i++) {
			if (strcmp(again.entries[i].gameId, history.entries[i].gameId) != 0 ||
			    again.entries[i].unixSeconds != history.entries[i].unixSeconds)
				abort();
		}
	}
	free(copy);
	return 0;
}
