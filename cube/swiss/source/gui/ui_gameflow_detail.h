#ifndef UI_GAMEFLOW_DETAIL_H
#define UI_GAMEFLOW_DETAIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ui_game_history.h"

#define UI_GAMEFLOW_DETAIL_ID_LENGTH 6u
#define UI_GAMEFLOW_DETAIL_TITLE_CAPACITY 96u
#define UI_GAMEFLOW_DETAIL_COMPANY_CAPACITY 64u
#define UI_GAMEFLOW_DETAIL_FACTS_CAPACITY 96u
#define UI_GAMEFLOW_DETAIL_DESCRIPTION_LINES 3u
#define UI_GAMEFLOW_DETAIL_DESCRIPTION_CAPACITY 80u
#define UI_GAMEFLOW_DETAIL_ENABLED_NAMES 3u
#define UI_GAMEFLOW_DETAIL_CHEAT_NAME_CAPACITY 64u
#define UI_GAMEFLOW_DETAIL_PRESENTATION_CAPACITY 96u
#define UI_GAMEFLOW_DETAIL_LAUNCH_LABEL_CAPACITY 48u
#define UI_GAMEFLOW_DETAIL_ADVANCED_CAPACITY 64u
#define UI_GAMEFLOW_DETAIL_BANNER_BYTES 6144u

typedef enum {
	UI_GAMEFLOW_DETAIL_VALID = 1u << 0,
	UI_GAMEFLOW_DETAIL_HAS_BANNER = 1u << 1,
	UI_GAMEFLOW_DETAIL_CHEATS_KNOWN = 1u << 2,
	UI_GAMEFLOW_DETAIL_CAN_SETTINGS = 1u << 3,
	UI_GAMEFLOW_DETAIL_CAN_CHEATS = 1u << 4,
	UI_GAMEFLOW_DETAIL_CAN_LIBRARY = 1u << 5,
	UI_GAMEFLOW_DETAIL_CAN_AUTOLOAD = 1u << 6,
	UI_GAMEFLOW_DETAIL_IS_AUTOLOAD = 1u << 7,
	UI_GAMEFLOW_DETAIL_CAN_VERIFY = 1u << 8,
	UI_GAMEFLOW_DETAIL_CAN_CLEAN_BOOT = 1u << 9,
	UI_GAMEFLOW_DETAIL_CLEAN_BOOT_DEFAULT = 1u << 10,
	UI_GAMEFLOW_DETAIL_AUDIO_STREAMING = 1u << 11,
	UI_GAMEFLOW_DETAIL_HAS_DISC_TWO = 1u << 12
} uiGameflowDetailFlags_t;

typedef struct {
	const char *name;
	bool enabled;
} uiGameflowDetailCheatSource_t;

/* Ephemeral menu-thread input. Build copies every byte that survives into the
 * retained event; none of these pointers may be stored or used by drawing. */
typedef struct {
	uint32_t generation;
	uint32_t focusIndex;
	const char *gameId;
	const char *title;
	const char *company;
	const char *facts;
	const char *description;
	uint64_t lastPlayedUnixSeconds;
	bool playHistoryAvailable;
	uiGameSaveStatus_t saveStatus;
	const uint8_t *banner;
	size_t bannerSize;
	const uiGameflowDetailCheatSource_t *cheats;
	size_t cheatCount;
	uint32_t enabledCheatBytes;
	uint32_t cheatCapacityBytes;
	uint32_t flags;
} uiGameflowDetailSource_t;

/* Fixed, pointer-free video-thread payload. The banner begins on a cache-line
 * boundary when this object itself is 32-byte aligned (as EV_GAMEFLOW is). */
typedef struct {
	uint32_t generation;
	uint32_t focusIndex;
	uint32_t flags;
	uint32_t cheatCount;
	uint32_t enabledCheatCount;
	uint32_t enabledCheatBytes;
	uint32_t cheatCapacityBytes;
	char gameId[UI_GAMEFLOW_DETAIL_ID_LENGTH + 1u];
	char title[UI_GAMEFLOW_DETAIL_TITLE_CAPACITY];
	char company[UI_GAMEFLOW_DETAIL_COMPANY_CAPACITY];
	char lastPlayedText[64];
	char saveStatusText[48];
	char facts[UI_GAMEFLOW_DETAIL_FACTS_CAPACITY];
	char description[UI_GAMEFLOW_DETAIL_DESCRIPTION_LINES]
		[UI_GAMEFLOW_DETAIL_DESCRIPTION_CAPACITY];
	char enabledCheatNames[UI_GAMEFLOW_DETAIL_ENABLED_NAMES]
		[UI_GAMEFLOW_DETAIL_CHEAT_NAME_CAPACITY];
	/* Menu-thread presentation copy. Dynamic strings are formatted once by
	 * Build so the vsync-locked renderer never assembles labels per frame. */
	char statusText[UI_GAMEFLOW_DETAIL_PRESENTATION_CAPACITY];
	char cheatSummary[UI_GAMEFLOW_DETAIL_PRESENTATION_CAPACITY];
	char cheatPreview[UI_GAMEFLOW_DETAIL_PRESENTATION_CAPACITY];
	char launchLabel[UI_GAMEFLOW_DETAIL_LAUNCH_LABEL_CAPACITY];
	char primaryActions[UI_GAMEFLOW_DETAIL_PRESENTATION_CAPACITY];
	char advancedLineOne[UI_GAMEFLOW_DETAIL_ADVANCED_CAPACITY];
	char advancedLineTwo[UI_GAMEFLOW_DETAIL_ADVANCED_CAPACITY];
	uint8_t banner[UI_GAMEFLOW_DETAIL_BANNER_BYTES]
		__attribute__((aligned(32)));
} uiGameflowDetailSnapshot_t;

typedef enum {
	UI_GAMEFLOW_DETAIL_INPUT_A = 1u << 0,
	UI_GAMEFLOW_DETAIL_INPUT_B = 1u << 1,
	UI_GAMEFLOW_DETAIL_INPUT_X = 1u << 2,
	UI_GAMEFLOW_DETAIL_INPUT_Y = 1u << 3,
	UI_GAMEFLOW_DETAIL_INPUT_Z = 1u << 4,
	UI_GAMEFLOW_DETAIL_INPUT_R = 1u << 5,
	UI_GAMEFLOW_DETAIL_INPUT_L = 1u << 6
} uiGameflowDetailInput_t;

typedef enum {
	UI_GAMEFLOW_DETAIL_ACTION_NONE = 0,
	UI_GAMEFLOW_DETAIL_ACTION_BOOT,
	UI_GAMEFLOW_DETAIL_ACTION_CLEAN_BOOT,
	UI_GAMEFLOW_DETAIL_ACTION_LIBRARY,
	UI_GAMEFLOW_DETAIL_ACTION_SETTINGS,
	UI_GAMEFLOW_DETAIL_ACTION_CHEATS,
	UI_GAMEFLOW_DETAIL_ACTION_AUTOLOAD,
	UI_GAMEFLOW_DETAIL_ACTION_VERIFY
} uiGameflowDetailAction_t;

bool UIGameflowDetail_Build(uiGameflowDetailSnapshot_t *snapshot,
	const uiGameflowDetailSource_t *source);
bool UIGameflowDetail_Matches(const uiGameflowDetailSnapshot_t *snapshot,
	uint32_t generation, uint32_t focusIndex, const char *gameId,
	size_t gameIdLength);
uiGameflowDetailAction_t UIGameflowDetail_ResolveAction(
	const uiGameflowDetailSnapshot_t *snapshot, uint32_t input);

#endif
