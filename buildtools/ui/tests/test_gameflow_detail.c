/*
 * Host test command (run from repository root):
 * cc -std=c11 -Wall -Wextra -Werror -Wconversion \
 *   -Wsign-conversion -pedantic -Icube/swiss/source/gui \
 *   buildtools/ui/tests/test_gameflow_detail.c \
 *   cube/swiss/source/gui/ui_gameflow_detail.c \
 *   cube/swiss/source/gui/ui_game_history.c \
 *   -o /tmp/test_gameflow_detail && /tmp/test_gameflow_detail
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_gameflow_detail.h"

#define CHECK(condition) do { \
	if(!(condition)) { \
		fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, \
			#condition); \
		exit(EXIT_FAILURE); \
	} \
} while(0)

static char *copyString(const char *source)
{
	size_t length = strlen(source) + 1u;
	char *copy = malloc(length);

	CHECK(copy != NULL);
	memcpy(copy, source, length);
	return copy;
}

static uiGameflowDetailSnapshot_t buildSnapshot(void)
{
	uint8_t *banner = malloc(UI_GAMEFLOW_DETAIL_BANNER_BYTES);
	char *title = copyString("Super Mario Sunshine");
	char *company = copyString("Nintendo");
	char *description = copyString("Clean the island and recover the Shine Sprites "
		"in a bright platform adventure with FLUDD.\nSecond paragraph.");
	char *firstName = copyString("Infinite Water");
	char *secondName = copyString("Moon Jump");
	uiGameflowDetailCheatSource_t cheats[] = {
		{firstName, true}, {"All Shines", false}, {secondName, true},
		{"No Damage", true}
	};
	uiGameflowDetailSource_t source = {
		.generation = 77u,
		.focusIndex = 4u,
		.gameId = "GMSE01",
		.title = title,
		.company = company,
		.facts = "GMSE01  |  1.35 GB  |  DISC 1",
		.description = description,
		.playHistoryAvailable = true,
		.lastPlayedUnixSeconds = UI_GAME_HISTORY_MIN_TIME + 45000u,
		.saveStatus = UI_GAME_SAVE_NOT_CHECKED,
		.banner = banner,
		.bannerSize = UI_GAMEFLOW_DETAIL_BANNER_BYTES,
		.cheats = cheats,
		.cheatCount = sizeof(cheats) / sizeof(cheats[0]),
		.enabledCheatBytes = 96u,
		.cheatCapacityBytes = 2048u,
		.flags = UI_GAMEFLOW_DETAIL_CHEATS_KNOWN |
			UI_GAMEFLOW_DETAIL_CAN_SETTINGS |
			UI_GAMEFLOW_DETAIL_CAN_CHEATS |
			UI_GAMEFLOW_DETAIL_CAN_LIBRARY |
			UI_GAMEFLOW_DETAIL_CAN_AUTOLOAD |
			UI_GAMEFLOW_DETAIL_IS_AUTOLOAD |
			UI_GAMEFLOW_DETAIL_CAN_VERIFY |
			UI_GAMEFLOW_DETAIL_CAN_CLEAN_BOOT |
			UI_GAMEFLOW_DETAIL_AUDIO_STREAMING |
			UI_GAMEFLOW_DETAIL_HAS_DISC_TWO
	};
	uiGameflowDetailSnapshot_t snapshot;

	CHECK(banner != NULL && title != NULL && company != NULL &&
		description != NULL && firstName != NULL && secondName != NULL);
	memset(banner, 0x5a, UI_GAMEFLOW_DETAIL_BANNER_BYTES);
	CHECK(UIGameflowDetail_Build(&snapshot, &source));

	/* Destroy every borrowed source before examining the retained copy. */
	memset(banner, 0, UI_GAMEFLOW_DETAIL_BANNER_BYTES);
	free(banner);
	free(title);
	free(company);
	free(description);
	free(firstName);
	free(secondName);
	return snapshot;
}

static void testPointerFreeCopyAndMatch(void)
{
	uiGameflowDetailSnapshot_t snapshot = buildSnapshot();
	size_t i;

	CHECK(strcmp(snapshot.title, "Super Mario Sunshine") == 0);
	CHECK(strcmp(snapshot.company, "Nintendo") == 0);
	CHECK(strstr(snapshot.lastPlayedText, "2001") != NULL);
	CHECK(strcmp(snapshot.saveStatusText, "Check in game") == 0);
	CHECK(strcmp(snapshot.enabledCheatNames[0], "Infinite Water") == 0);
	CHECK(strcmp(snapshot.enabledCheatNames[1], "Moon Jump") == 0);
	CHECK(strcmp(snapshot.enabledCheatNames[2], "No Damage") == 0);
	CHECK(snapshot.cheatCount == 4u);
	CHECK(snapshot.enabledCheatCount == 3u);
	CHECK(snapshot.enabledCheatBytes == 96u);
	CHECK(snapshot.cheatCapacityBytes == 2048u);
	CHECK((snapshot.flags & UI_GAMEFLOW_DETAIL_HAS_BANNER) != 0u);
	CHECK(((uintptr_t)snapshot.banner & 31u) == 0u);
	for(i = 0u; i < UI_GAMEFLOW_DETAIL_BANNER_BYTES; ++i) {
		CHECK(snapshot.banner[i] == 0x5au);
	}
	CHECK(snapshot.description[0][0] != '\0');
	CHECK(strcmp(snapshot.statusText,
		"AUDIO STREAMING   DISC 2 READY   AUTOLOAD") == 0);
	CHECK(strcmp(snapshot.cheatSummary,
		"3 of 4 enabled") == 0);
	CHECK(strcmp(snapshot.cheatPreview, "Infinite Water  +2 MORE") == 0);
	CHECK(strcmp(snapshot.launchLabel, "A  LAUNCH GAME") == 0);
	CHECK(strcmp(snapshot.primaryActions,
		"A  LAUNCH   B  LIBRARY   X  SETTINGS   Y  CHEATS") == 0);
	CHECK(strcmp(snapshot.advancedLineOne,
		"Z  AUTOLOAD ON   R  VERIFY") == 0);
	CHECK(strcmp(snapshot.advancedLineTwo, "L+A  CLEAN BOOT") == 0);
	CHECK(strstr(snapshot.primaryActions, "AUTOLOAD") == NULL);
	CHECK(strstr(snapshot.primaryActions, "VERIFY") == NULL);
	CHECK(strstr(snapshot.primaryActions, "CLEAN") == NULL);
	CHECK(sizeof(snapshot) < 8192u);
	CHECK(UIGameflowDetail_Matches(&snapshot, 77u, 4u, "GMSE01", 6u));
	CHECK(!UIGameflowDetail_Matches(&snapshot, 78u, 4u, "GMSE01", 6u));
	CHECK(!UIGameflowDetail_Matches(&snapshot, 77u, 5u, "GMSE01", 6u));
	CHECK(!UIGameflowDetail_Matches(&snapshot, 77u, 4u, "GALE01", 6u));
	CHECK(!UIGameflowDetail_Matches(&snapshot, 77u, 4u, "GMSE0", 5u));
}

static void testNoCheatsClearsCapability(void)
{
	uiGameflowDetailSource_t source = {
		.gameId = "GALE01",
		.title = "Super Smash Bros. Melee",
		.flags = UI_GAMEFLOW_DETAIL_HAS_BANNER |
			UI_GAMEFLOW_DETAIL_CHEATS_KNOWN |
			UI_GAMEFLOW_DETAIL_CAN_CHEATS |
			UI_GAMEFLOW_DETAIL_CAN_SETTINGS
	};
	uiGameflowDetailSnapshot_t snapshot;

	CHECK(UIGameflowDetail_Build(&snapshot, &source));
	CHECK(strcmp(snapshot.lastPlayedText, "History unavailable") == 0);
	CHECK(strcmp(snapshot.saveStatusText, "Check in game") == 0);
	CHECK(snapshot.cheatCount == 0u);
	CHECK(snapshot.enabledCheatCount == 0u);
	CHECK((snapshot.flags & UI_GAMEFLOW_DETAIL_CHEATS_KNOWN) != 0u);
	CHECK((snapshot.flags & UI_GAMEFLOW_DETAIL_HAS_BANNER) == 0u);
	CHECK((snapshot.flags & UI_GAMEFLOW_DETAIL_CAN_CHEATS) == 0u);
	CHECK(strcmp(snapshot.cheatSummary, "NO CHEATS FOUND") == 0);
	CHECK(snapshot.cheatPreview[0] == '\0');
	CHECK(strstr(snapshot.primaryActions, "Y  CHEATS") == NULL);
	CHECK(UIGameflowDetail_ResolveAction(&snapshot,
		UI_GAMEFLOW_DETAIL_INPUT_Y) == UI_GAMEFLOW_DETAIL_ACTION_NONE);
}

static void testPresentationVariants(void)
{
	uiGameflowDetailCheatSource_t cheat = {"Invincibility", false};
	uiGameflowDetailSource_t source = {
		.gameId = "GM4E01",
		.title = "Mario Kart: Double Dash!!",
		.cheats = &cheat,
		.cheatCount = 1u,
		.enabledCheatBytes = UINT32_MAX,
		.cheatCapacityBytes = UINT32_MAX,
		.flags = UI_GAMEFLOW_DETAIL_CAN_SETTINGS |
			UI_GAMEFLOW_DETAIL_CAN_CHEATS |
			UI_GAMEFLOW_DETAIL_CAN_CLEAN_BOOT |
			UI_GAMEFLOW_DETAIL_CLEAN_BOOT_DEFAULT
	};
	uiGameflowDetailSnapshot_t snapshot;

	CHECK(UIGameflowDetail_Build(&snapshot, &source));
	CHECK(strcmp(snapshot.cheatSummary, "STATUS UNAVAILABLE") == 0);
	CHECK(snapshot.cheatPreview[0] == '\0');
	CHECK(strcmp(snapshot.launchLabel, "A  CLEAN BOOT") == 0);
	CHECK(strcmp(snapshot.primaryActions,
		"A  LAUNCH   X  SETTINGS   Y  CHEATS") == 0);
	CHECK(snapshot.advancedLineOne[0] == '\0');
	CHECK(strcmp(snapshot.advancedLineTwo, "L+A  CLEAN BOOT") == 0);

	source.flags |= UI_GAMEFLOW_DETAIL_CHEATS_KNOWN;
	CHECK(UIGameflowDetail_Build(&snapshot, &source));
	CHECK(strcmp(snapshot.cheatSummary,
		"0 of 1 enabled") == 0);
	CHECK(strcmp(snapshot.cheatPreview, "Y  Choose cheats") == 0);
}

static void testDescriptionTruncationIsVisible(void)
{
	const char *description =
		"One two three four five six seven eight nine ten eleven twelve "
		"thirteen fourteen fifteen sixteen seventeen eighteen nineteen "
		"twenty twenty-one twenty-two twenty-three twenty-four twenty-five "
		"twenty-six twenty-seven twenty-eight twenty-nine thirty and beyond.";
	uiGameflowDetailSource_t source = {
		.gameId = "GALE01",
		.title = "Super Smash Bros. Melee",
		.description = description,
		.playHistoryAvailable = true,
		.lastPlayedUnixSeconds = UI_GAME_HISTORY_MIN_TIME + 45000u,
		.saveStatus = UI_GAME_SAVE_NOT_CHECKED,
		.flags = UI_GAMEFLOW_DETAIL_CHEATS_KNOWN
	};
	uiGameflowDetailSnapshot_t snapshot;
	size_t length;

	CHECK(UIGameflowDetail_Build(&snapshot, &source));
	CHECK(snapshot.description[0][0] != '\0');
	CHECK(snapshot.description[1][0] != '\0');
	CHECK(snapshot.description[2][0] != '\0');
	length = strlen(snapshot.description[2]);
	CHECK(length >= 3u);
	CHECK(strcmp(&snapshot.description[2][length - 3u], "...") == 0);
}

static void testControllerPrecedence(void)
{
	uiGameflowDetailSnapshot_t snapshot = buildSnapshot();

	CHECK(UIGameflowDetail_ResolveAction(&snapshot,
		UI_GAMEFLOW_DETAIL_INPUT_A) == UI_GAMEFLOW_DETAIL_ACTION_BOOT);
	CHECK(UIGameflowDetail_ResolveAction(&snapshot,
		UI_GAMEFLOW_DETAIL_INPUT_A | UI_GAMEFLOW_DETAIL_INPUT_L) ==
		UI_GAMEFLOW_DETAIL_ACTION_CLEAN_BOOT);
	CHECK(UIGameflowDetail_ResolveAction(&snapshot,
		UI_GAMEFLOW_DETAIL_INPUT_A | UI_GAMEFLOW_DETAIL_INPUT_B |
		UI_GAMEFLOW_DETAIL_INPUT_Y) == UI_GAMEFLOW_DETAIL_ACTION_BOOT);
	CHECK(UIGameflowDetail_ResolveAction(&snapshot,
		UI_GAMEFLOW_DETAIL_INPUT_B | UI_GAMEFLOW_DETAIL_INPUT_X) ==
		UI_GAMEFLOW_DETAIL_ACTION_LIBRARY);
	CHECK(UIGameflowDetail_ResolveAction(&snapshot,
		UI_GAMEFLOW_DETAIL_INPUT_R | UI_GAMEFLOW_DETAIL_INPUT_X) ==
		UI_GAMEFLOW_DETAIL_ACTION_VERIFY);
	CHECK(UIGameflowDetail_ResolveAction(&snapshot,
		UI_GAMEFLOW_DETAIL_INPUT_X | UI_GAMEFLOW_DETAIL_INPUT_Y) ==
		UI_GAMEFLOW_DETAIL_ACTION_SETTINGS);
	CHECK(UIGameflowDetail_ResolveAction(&snapshot,
		UI_GAMEFLOW_DETAIL_INPUT_Z | UI_GAMEFLOW_DETAIL_INPUT_Y) ==
		UI_GAMEFLOW_DETAIL_ACTION_AUTOLOAD);
	CHECK(UIGameflowDetail_ResolveAction(&snapshot,
		UI_GAMEFLOW_DETAIL_INPUT_Y) == UI_GAMEFLOW_DETAIL_ACTION_CHEATS);
	snapshot.flags &= ~(uint32_t)(UI_GAMEFLOW_DETAIL_CAN_LIBRARY |
		UI_GAMEFLOW_DETAIL_CAN_CLEAN_BOOT);
	CHECK(UIGameflowDetail_ResolveAction(&snapshot,
		UI_GAMEFLOW_DETAIL_INPUT_B) == UI_GAMEFLOW_DETAIL_ACTION_NONE);
	CHECK(UIGameflowDetail_ResolveAction(&snapshot,
		UI_GAMEFLOW_DETAIL_INPUT_A | UI_GAMEFLOW_DETAIL_INPUT_L) ==
		UI_GAMEFLOW_DETAIL_ACTION_BOOT);
}

static void testInvalidInputResetsSnapshot(void)
{
	uiGameflowDetailSnapshot_t snapshot;
	uiGameflowDetailSource_t source = {
		.gameId = "bad-id",
		.title = "Invalid"
	};

	memset(&snapshot, 0xff, sizeof(snapshot));
	CHECK(!UIGameflowDetail_Build(&snapshot, &source));
	CHECK(snapshot.flags == 0u);
	CHECK(snapshot.title[0] == '\0');
}

/* A game's own settings get their own line, like its cheats: how many and
 * the first, or how to set some. The X action stays plain, so the count
 * isn't said twice. */
static void testCustomSettingsLine(void)
{
	uiGameflowDetailSource_t source = {
		.gameId = "GALE01",
		.title = "Super Smash Bros. Melee",
		.facts = "GALE01",
		.description = "",
		.saveStatus = UI_GAME_SAVE_NOT_CHECKED,
		.flags = UI_GAMEFLOW_DETAIL_CHEATS_KNOWN |
			UI_GAMEFLOW_DETAIL_CAN_SETTINGS | UI_GAMEFLOW_DETAIL_CAN_LIBRARY,
		.customSettings = 2u,
		.firstCustomSetting = "Force Video Mode: 480p"
	};
	uiGameflowDetailSnapshot_t snapshot;

	CHECK(UIGameflowDetail_Build(&snapshot, &source));
	CHECK(strcmp(snapshot.primaryActions,
		"A  LAUNCH   B  LIBRARY   X  SETTINGS") == 0);
	CHECK(strcmp(snapshot.settingsSummary, "2 custom") == 0);
	CHECK(strcmp(snapshot.settingsPreview,
		"Force Video Mode: 480p  +1 MORE") == 0);
	source.customSettings = 1u;
	CHECK(UIGameflowDetail_Build(&snapshot, &source));
	CHECK(strcmp(snapshot.settingsSummary, "1 custom") == 0);
	CHECK(strcmp(snapshot.settingsPreview, "Force Video Mode: 480p") == 0);
	/* The name comes from settings.c; without one the line still reads. */
	source.firstCustomSetting = NULL;
	CHECK(UIGameflowDetail_Build(&snapshot, &source));
	CHECK(strcmp(snapshot.settingsPreview, "Custom settings") == 0);
	source.customSettings = 0u;
	CHECK(UIGameflowDetail_Build(&snapshot, &source));
	CHECK(strcmp(snapshot.primaryActions,
		"A  LAUNCH   B  LIBRARY   X  SETTINGS") == 0);
	CHECK(strcmp(snapshot.settingsSummary, "Game Defaults") == 0);
	CHECK(strcmp(snapshot.settingsPreview, "X  Change for this game") == 0);
	/* Without the X action there is no settings line to offer. */
	source.flags &= ~(uint32_t)UI_GAMEFLOW_DETAIL_CAN_SETTINGS;
	source.customSettings = 3u;
	source.firstCustomSetting = "Force Video Mode: 480p";
	CHECK(UIGameflowDetail_Build(&snapshot, &source));
	CHECK(strstr(snapshot.primaryActions, "SETTINGS") == NULL);
	CHECK(snapshot.settingsSummary[0] == '\0');
	CHECK(snapshot.settingsPreview[0] == '\0');
}

int main(void)
{
	testPointerFreeCopyAndMatch();
	testCustomSettingsLine();
	testNoCheatsClearsCapability();
	testPresentationVariants();
	testDescriptionTruncationIsVisible();
	testControllerPrecedence();
	testInvalidInputResetsSnapshot();
	puts("gameflow detail tests passed");
	return EXIT_SUCCESS;
}
