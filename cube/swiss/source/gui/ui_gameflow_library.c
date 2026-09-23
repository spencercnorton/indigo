#include <string.h>

#include "ui_gameflow_library.h"

static unsigned char asciiLower(unsigned char value)
{
	return value >= 'A' && value <= 'Z' ?
		(unsigned char)(value + ('a' - 'A')) : value;
}

static size_t trimmedPathLength(const char *path)
{
	size_t length = path != NULL ? strlen(path) : 0u;
	while(length > 0u && path[length - 1u] == '/') {
		length--;
	}
	return length;
}

static bool asciiPrefixEquals(const char *left, const char *right,
	size_t length)
{
	size_t i;

	if(left == NULL || right == NULL) {
		return false;
	}
	for(i = 0u; i < length; ++i) {
		if(asciiLower((unsigned char)left[i]) !=
			asciiLower((unsigned char)right[i])) {
			return false;
		}
	}
	return true;
}

static bool suffixEquals(const char *name, const char *suffix)
{
	size_t nameLength;
	size_t suffixLength;
	size_t i;

	if(name == NULL || suffix == NULL) {
		return false;
	}
	nameLength = strlen(name);
	suffixLength = strlen(suffix);
	if(nameLength <= suffixLength) {
		return false;
	}
	for(i = 0u; i < suffixLength; ++i) {
		if(asciiLower((unsigned char)name[nameLength - suffixLength + i]) !=
			asciiLower((unsigned char)suffix[i])) {
			return false;
		}
	}
	return true;
}

bool UIGameflowLibrary_IsGameImageName(const char *name)
{
	return suffixEquals(name, ".fdi") || suffixEquals(name, ".gcm") ||
		suffixEquals(name, ".iso") || suffixEquals(name, ".tgc");
}

bool UIGameflowLibrary_ParseGameFolderName(const char *name, char gameId[7],
	char *title, size_t titleSize)
{
	size_t length;
	size_t titleLength;
	size_t i;

	if(name == NULL) {
		return false;
	}
	length = strlen(name);
	if(length < 10u || name[length - 8u] != '[' || name[length - 1u] != ']' ||
		name[length - 9u] != ' ') {
		return false;
	}
	titleLength = length - 9u;
	if(titleLength == 0u || name[titleLength - 1u] == ' ' ||
		memchr(name, '/', length) != NULL || memchr(name, '\\', length) != NULL) {
		return false;
	}
	for(i = 0u; i < 6u; ++i) {
		unsigned char value = (unsigned char)name[length - 7u + i];
		if(!((value >= 'A' && value <= 'Z') ||
			(value >= '0' && value <= '9'))) {
			return false;
		}
	}

	if(gameId != NULL) {
		memcpy(gameId, &name[length - 7u], 6u);
		gameId[6] = '\0';
	}
	if(title != NULL && titleSize > 0u) {
		size_t copyLength = titleLength < titleSize - 1u ?
			titleLength : titleSize - 1u;
		memcpy(title, name, copyLength);
		title[copyLength] = '\0';
	}
	return true;
}

bool UIGameflowLibrary_EntryEligible(uiGameflowLibraryMode_t mode,
	uint32_t index, uiGameflowLibraryEntryType_t type, const char *name)
{
	if(type == UI_GAMEFLOW_LIBRARY_ENTRY_SPECIAL) {
		return index == 0u;
	}
	if(mode == UI_GAMEFLOW_LIBRARY_IMAGE_FILES) {
		return type == UI_GAMEFLOW_LIBRARY_ENTRY_FILE &&
			UIGameflowLibrary_IsGameImageName(name);
	}
	if(mode == UI_GAMEFLOW_LIBRARY_GAME_FOLDERS) {
		return type == UI_GAMEFLOW_LIBRARY_ENTRY_DIRECTORY &&
			UIGameflowLibrary_ParseGameFolderName(name, NULL, NULL, 0u);
	}
	return false;
}

uiGameflowLibraryLocation_t UIGameflowLibrary_Locate(
	const char *gamesRoot, const char *currentPath)
{
	size_t rootLength = trimmedPathLength(gamesRoot);
	size_t pathLength = trimmedPathLength(currentPath);
	const char *leaf;

	if(rootLength == 0u || pathLength < rootLength ||
		!asciiPrefixEquals(gamesRoot, currentPath, rootLength)) {
		return UI_GAMEFLOW_LIBRARY_LOCATION_NONE;
	}
	if(pathLength == rootLength) {
		return UI_GAMEFLOW_LIBRARY_LOCATION_ROOT;
	}
	if(pathLength <= rootLength + 1u || currentPath[rootLength] != '/' ||
		currentPath[pathLength] != '\0') {
		return UI_GAMEFLOW_LIBRARY_LOCATION_NONE;
	}
	leaf = &currentPath[rootLength + 1u];
	if(memchr(leaf, '/', pathLength - rootLength - 1u) != NULL ||
		!UIGameflowLibrary_ParseGameFolderName(leaf, NULL, NULL, 0u)) {
		return UI_GAMEFLOW_LIBRARY_LOCATION_NONE;
	}
	return UI_GAMEFLOW_LIBRARY_LOCATION_STRICT_LEAF;
}

bool UIGameflowLibrary_IsGamesRootEntry(const char *gamesRoot,
	uiGameflowLibraryEntryType_t type, const char *entryPath)
{
	return type == UI_GAMEFLOW_LIBRARY_ENTRY_DIRECTORY &&
		UIGameflowLibrary_Locate(gamesRoot, entryPath) ==
			UI_GAMEFLOW_LIBRARY_LOCATION_ROOT;
}

bool UIGameflowLibrary_ShouldStartHome(bool hasAutoload, bool hasRecent,
	bool recentAutoEnabled)
{
	return !hasAutoload && !(hasRecent && recentAutoEnabled);
}

void UIGameflowLibrary_ClassifierInit(uiGameflowLibraryClassifier_t *state,
	uiGameflowLibraryLocation_t location)
{
	if(state == NULL) {
		return;
	}
	memset(state, 0, sizeof(*state));
	state->location = location;
	state->valid = location == UI_GAMEFLOW_LIBRARY_LOCATION_ROOT ||
		location == UI_GAMEFLOW_LIBRARY_LOCATION_STRICT_LEAF;
	if(location == UI_GAMEFLOW_LIBRARY_LOCATION_STRICT_LEAF) {
		state->mode = UI_GAMEFLOW_LIBRARY_IMAGE_FILES;
	}
}

bool UIGameflowLibrary_ClassifierAdd(uiGameflowLibraryClassifier_t *state,
	uiGameflowLibraryEntryType_t type, const char *name)
{
	uiGameflowLibraryMode_t entryMode = UI_GAMEFLOW_LIBRARY_NONE;
	uint32_t index;

	if(state == NULL || !state->valid) {
		return false;
	}
	index = state->entryCount++;
	if(state->location == UI_GAMEFLOW_LIBRARY_LOCATION_ROOT &&
		type != UI_GAMEFLOW_LIBRARY_ENTRY_SPECIAL) {
		if(type == UI_GAMEFLOW_LIBRARY_ENTRY_FILE) {
			entryMode = UI_GAMEFLOW_LIBRARY_IMAGE_FILES;
		}
		else if(type == UI_GAMEFLOW_LIBRARY_ENTRY_DIRECTORY) {
			entryMode = UI_GAMEFLOW_LIBRARY_GAME_FOLDERS;
		}
		if(entryMode == UI_GAMEFLOW_LIBRARY_NONE ||
			(state->mode != UI_GAMEFLOW_LIBRARY_NONE &&
			state->mode != entryMode)) {
			state->valid = false;
			return false;
		}
		state->mode = entryMode;
	}
	if(!UIGameflowLibrary_EntryEligible(state->mode, index, type, name)) {
		state->valid = false;
		return false;
	}
	if(type != UI_GAMEFLOW_LIBRARY_ENTRY_SPECIAL) {
		state->hasGame = true;
	}
	return true;
}

uiGameflowLibraryMode_t UIGameflowLibrary_ClassifierFinish(
	const uiGameflowLibraryClassifier_t *state)
{
	return state != NULL && state->valid && state->hasGame ?
		state->mode : UI_GAMEFLOW_LIBRARY_NONE;
}

int UIGameflowLibrary_SelectBrowser(uiGameflowLibraryMode_t mode,
	int requestedBrowser, int retainedBrowser)
{
	if(mode == UI_GAMEFLOW_LIBRARY_IMAGE_FILES ||
		mode == UI_GAMEFLOW_LIBRARY_GAME_FOLDERS) {
		return retainedBrowser;
	}
	return requestedBrowser;
}

bool UIGameflowLibrary_UsesRetainedDetail(uiGameflowLibraryMode_t mode,
	uiGameflowLibraryEntryType_t type)
{
	return (mode == UI_GAMEFLOW_LIBRARY_GAME_FOLDERS &&
			type == UI_GAMEFLOW_LIBRARY_ENTRY_DIRECTORY) ||
		(mode == UI_GAMEFLOW_LIBRARY_IMAGE_FILES &&
			type == UI_GAMEFLOW_LIBRARY_ENTRY_FILE);
}

uiGameflowLibraryArtwork_t UIGameflowLibrary_ChooseArtwork(
	bool posterReady, bool bannerReady)
{
	if(posterReady) {
		return UI_GAMEFLOW_LIBRARY_ART_POSTER;
	}
	if(bannerReady) {
		return UI_GAMEFLOW_LIBRARY_ART_BANNER;
	}
	return UI_GAMEFLOW_LIBRARY_ART_EMBLEM;
}

bool UIGameflowLibrary_BuildFallbackLayout(float visualSlot,
	uiGameflowLibraryFallbackLayout_t *layout)
{
	float distance;

	if(layout == NULL || visualSlot != visualSlot) {
		return false;
	}
	distance = visualSlot < 0.0f ? -visualSlot : visualSlot;
	if(distance >= 1.5f) {
		return false;
	}

	layout->contentInset = 0.055f;
	layout->bannerLeft = 0.12f;
	layout->bannerTop = 0.145f;
	layout->bannerRight = 0.88f;
	layout->bannerBottom = 0.335f;
	layout->motifCenterX = 0.50f;
	layout->motifCenterY = 0.36f;
	layout->motifRadius = 0.16f;
	layout->identityTop = 0.655f;
	layout->idBaseline = 0.765f;
	layout->regionBaseline = 0.855f;
	/* Identity belongs only to the centered card. During travel it fades
	 * before the neighboring card can claim the same visual hierarchy. */
	layout->identityAlpha = 1.0f - distance * 2.5f;
	if(layout->identityAlpha < 0.0f) {
		layout->identityAlpha = 0.0f;
	}
	return true;
}

const char *UIGameflowLibrary_RegionLabel(const char *gameId)
{
	size_t i;
	char region;

	if(gameId == NULL) {
		return "GAMECUBE";
	}
	for(i = 0u; i < 4u; ++i) {
		if(gameId[i] == '\0') {
			return "GAMECUBE";
		}
	}
	region = gameId[3];
	switch(region) {
		case 'E':
			return "NTSC-U";
		case 'J':
			return "NTSC-J";
		case 'K':
		case 'T':
			return "NTSC-K";
		case 'P':
		case 'D':
		case 'F':
		case 'H':
		case 'I':
		case 'L':
		case 'M':
		case 'R':
		case 'S':
		case 'U':
		case 'X':
		case 'Y':
			return "PAL";
		case 'A':
			return "REGION FREE";
		case 'C':
		case 'W':
			return "NTSC";
		default:
			return "GAMECUBE";
	}
}

size_t UIGameflowLibrary_BuildWindow(uint32_t itemCount,
	uint32_t selectedIndex, uiGameflowDirection_t directionHint,
	uiGameflowLibraryWindowSlot_t slots[7])
{
	static const int8_t relativeOrder[UI_GAMEFLOW_LIBRARY_WINDOW] = {
		0, -1, 1, -2, 2, -3, 3
	};
	size_t count = 0u;
	size_t i;

	if(slots == NULL || itemCount == 0u) {
		return 0u;
	}
	if(selectedIndex >= itemCount) {
		selectedIndex = itemCount - 1u;
	}

	for(i = 0u; i < UI_GAMEFLOW_LIBRARY_WINDOW; ++i) {
		int8_t relativeSlot = relativeOrder[i];
		int64_t candidate;
		uint32_t index;
		size_t j;

		if(itemCount == 2u && i == 1u &&
			directionHint == UI_GAMEFLOW_DIRECTION_PREVIOUS) {
			relativeSlot = 1;
		}
		candidate = (int64_t)selectedIndex + relativeSlot;

		while(candidate < 0) {
			candidate += itemCount;
		}
		index = (uint32_t)(candidate % itemCount);
		for(j = 0u; j < count; ++j) {
			if(slots[j].index == index) {
				break;
			}
		}
		if(j != count) {
			continue;
		}
		slots[count].index = index;
		slots[count].relativeSlot = relativeSlot;
		if(++count == itemCount) {
			break;
		}
	}
	return count;
}
