#ifndef UI_GAME_HISTORY_H
#define UI_GAME_HISTORY_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UI_GAME_HISTORY_CAPACITY 128u
#define UI_GAME_HISTORY_FILE_CAPACITY 4096u
#define UI_GAME_HISTORY_MIN_TIME UINT64_C(978307200) /* 2001-01-01 */
#define UI_GAME_HISTORY_MAX_TIME UINT64_C(4102444799) /* 2099-12-31 */

typedef struct {
	char gameId[7];
	uint64_t unixSeconds;
} uiGameHistoryEntry_t;

typedef struct {
	uiGameHistoryEntry_t entries[UI_GAME_HISTORY_CAPACITY];
	size_t count;
	uint64_t generation;
	bool available;
} uiGameHistory_t;

typedef enum {
	UI_GAME_SAVE_NOT_CHECKED = 0,
	UI_GAME_SAVE_CONFIRMED,
	UI_GAME_SAVE_NOT_FOUND,
	UI_GAME_SAVE_UNAVAILABLE
} uiGameSaveStatus_t;

/* Pure, bounded metadata model. Recording belongs only at successful Swiss
 * game handoff, never selection, browsing, A presses, or failed setup. */
void UIGameHistory_Init(uiGameHistory_t *history, bool available);
bool UIGameHistory_ValidTime(uint64_t unixSeconds);
bool UIGameHistory_Record(uiGameHistory_t *history, const char *gameId,
	size_t gameIdLength, uint64_t unixSeconds);
uint64_t UIGameHistory_Find(const uiGameHistory_t *history,
	const char *gameId, size_t gameIdLength);
bool UIGameHistory_Parse(uiGameHistory_t *history, const char *data, size_t size);
/* Select newest valid generation; -1 means no valid slot. */
int UIGameHistory_SelectSlot(const uiGameHistory_t *first, const uiGameHistory_t *second);
size_t UIGameHistory_Serialize(const uiGameHistory_t *history,
	char *out, size_t capacity);
void UIGameHistory_Format(char *out, size_t capacity, bool available,
	uint64_t unixSeconds);
const char *UIGameHistory_SaveStatus(uiGameSaveStatus_t status);
#endif
