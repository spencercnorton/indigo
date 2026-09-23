#include <stdio.h>
#include <string.h>

#include "ui_gameflow_detail.h"

#define UI_GAMEFLOW_DETAIL_WRAP_COLUMNS 48u

static bool validGameId(const char *gameId)
{
	size_t i;

	if(gameId == NULL) {
		return false;
	}
	for(i = 0u; i < UI_GAMEFLOW_DETAIL_ID_LENGTH; ++i) {
		unsigned char value = (unsigned char)gameId[i];
		if(!((value >= 'A' && value <= 'Z') ||
			(value >= '0' && value <= '9'))) {
			return false;
		}
	}
	return gameId[UI_GAMEFLOW_DETAIL_ID_LENGTH] == '\0';
}

static size_t boundedLength(const char *text, size_t limit)
{
	size_t length = 0u;

	while(text != NULL && length < limit && text[length] != '\0') {
		++length;
	}
	return length;
}

static void copyText(char *destination, size_t capacity, const char *source)
{
	size_t length;

	if(destination == NULL || capacity == 0u) {
		return;
	}
	if(source == NULL) {
		destination[0] = '\0';
		return;
	}
	length = boundedLength(source, capacity - 1u);
	memcpy(destination, source, length);
	destination[length] = '\0';
}

static void appendText(char *destination, size_t capacity, const char *text)
{
	size_t length;
	size_t textLength;

	if(destination == NULL || capacity == 0u || text == NULL) {
		return;
	}
	length = boundedLength(destination, capacity);
	if(length >= capacity - 1u) {
		return;
	}
	if(length != 0u) {
		const char separator[] = "   ";
		size_t available = capacity - length - 1u;
		size_t copyLength = sizeof(separator) - 1u;

		if(copyLength > available) {
			copyLength = available;
		}
		memcpy(&destination[length], separator, copyLength);
		length += copyLength;
		destination[length] = '\0';
	}
	textLength = boundedLength(text, capacity);
	if(textLength > capacity - length - 1u) {
		textLength = capacity - length - 1u;
	}
	memcpy(&destination[length], text, textLength);
	destination[length + textLength] = '\0';
}

static const char *skipWhitespace(const char *text)
{
	while(text != NULL && (*text == ' ' || *text == '\t' ||
		*text == '\r' || *text == '\n')) {
		++text;
	}
	return text;
}

static void markDescriptionTruncated(char *line, size_t capacity)
{
	size_t length;

	if(line == NULL || capacity < 4u) {
		return;
	}
	length = boundedLength(line, capacity - 1u);
	if(length + 3u < capacity &&
		length + 3u <= UI_GAMEFLOW_DETAIL_WRAP_COLUMNS) {
		memcpy(&line[length], "...", 4u);
		return;
	}
	if(length >= 3u) {
		memcpy(&line[length - 3u], "...", 4u);
	}
}

static void wrapDescription(
	char lines[UI_GAMEFLOW_DETAIL_DESCRIPTION_LINES]
		[UI_GAMEFLOW_DETAIL_DESCRIPTION_CAPACITY], const char *description)
{
	const char *cursor = skipWhitespace(description);
	size_t line;

	for(line = 0u; line < UI_GAMEFLOW_DETAIL_DESCRIPTION_LINES &&
		cursor != NULL && *cursor != '\0'; ++line) {
		const char *end = cursor;
		const char *lastSpace = NULL;
		size_t length = 0u;

		while(*end != '\0' && *end != '\r' && *end != '\n' &&
			length < UI_GAMEFLOW_DETAIL_WRAP_COLUMNS) {
			if(*end == ' ' || *end == '\t') {
				lastSpace = end;
			}
			++end;
			++length;
		}
		if(*end != '\0' && *end != '\r' && *end != '\n' &&
			lastSpace != NULL && lastSpace > cursor) {
			end = lastSpace;
			length = (size_t)(end - cursor);
		}
		while(length > 0u && (cursor[length - 1u] == ' ' ||
			cursor[length - 1u] == '\t')) {
			--length;
		}
		if(length >= UI_GAMEFLOW_DETAIL_DESCRIPTION_CAPACITY) {
			length = UI_GAMEFLOW_DETAIL_DESCRIPTION_CAPACITY - 1u;
		}
		memcpy(lines[line], cursor, length);
		lines[line][length] = '\0';
		cursor = skipWhitespace(end);
	}
	if(line == UI_GAMEFLOW_DETAIL_DESCRIPTION_LINES && cursor != NULL &&
		*cursor != '\0') {
		markDescriptionTruncated(
			lines[UI_GAMEFLOW_DETAIL_DESCRIPTION_LINES - 1u],
			UI_GAMEFLOW_DETAIL_DESCRIPTION_CAPACITY);
	}
}

static void buildPresentation(uiGameflowDetailSnapshot_t *snapshot)
{
	uint32_t flags = snapshot->flags;

	if((flags & UI_GAMEFLOW_DETAIL_AUDIO_STREAMING) != 0u) {
		appendText(snapshot->statusText, sizeof(snapshot->statusText),
			"AUDIO STREAMING");
	}
	if((flags & UI_GAMEFLOW_DETAIL_HAS_DISC_TWO) != 0u) {
		appendText(snapshot->statusText, sizeof(snapshot->statusText),
			"DISC 2 READY");
	}
	if((flags & UI_GAMEFLOW_DETAIL_IS_AUTOLOAD) != 0u) {
		appendText(snapshot->statusText, sizeof(snapshot->statusText),
			"AUTOLOAD");
	}

	if((flags & UI_GAMEFLOW_DETAIL_CHEATS_KNOWN) == 0u) {
		copyText(snapshot->cheatSummary, sizeof(snapshot->cheatSummary),
			"STATUS UNAVAILABLE");
	}
	else if(snapshot->cheatCount == 0u) {
		copyText(snapshot->cheatSummary, sizeof(snapshot->cheatSummary),
			"NO CHEATS FOUND");
	}
	else {
		(void)snprintf(snapshot->cheatSummary,
			sizeof(snapshot->cheatSummary),
			"%lu of %lu enabled",
			(unsigned long)snapshot->enabledCheatCount,
			(unsigned long)snapshot->cheatCount);
		if(snapshot->enabledCheatCount == 0u) {
			copyText(snapshot->cheatPreview,
				sizeof(snapshot->cheatPreview), "Y  Choose cheats");
		}
		else if(snapshot->enabledCheatCount == 1u) {
			copyText(snapshot->cheatPreview,
				sizeof(snapshot->cheatPreview),
				snapshot->enabledCheatNames[0]);
		}
		else {
			(void)snprintf(snapshot->cheatPreview,
				sizeof(snapshot->cheatPreview), "%s  +%lu MORE",
				snapshot->enabledCheatNames[0],
				(unsigned long)(snapshot->enabledCheatCount - 1u));
		}
	}

	copyText(snapshot->launchLabel, sizeof(snapshot->launchLabel),
		(flags & UI_GAMEFLOW_DETAIL_CLEAN_BOOT_DEFAULT) != 0u ?
		"A  CLEAN BOOT" : "A  LAUNCH GAME");
	appendText(snapshot->primaryActions, sizeof(snapshot->primaryActions),
		"A  LAUNCH");
	if((flags & UI_GAMEFLOW_DETAIL_CAN_LIBRARY) != 0u) {
		appendText(snapshot->primaryActions,
			sizeof(snapshot->primaryActions), "B  LIBRARY");
	}
	if((flags & UI_GAMEFLOW_DETAIL_CAN_SETTINGS) != 0u) {
		appendText(snapshot->primaryActions,
			sizeof(snapshot->primaryActions), "X  SETTINGS");
	}
	if((flags & UI_GAMEFLOW_DETAIL_CAN_CHEATS) != 0u) {
		appendText(snapshot->primaryActions,
			sizeof(snapshot->primaryActions), "Y  CHEATS");
	}

	if((flags & UI_GAMEFLOW_DETAIL_CAN_AUTOLOAD) != 0u) {
		appendText(snapshot->advancedLineOne,
			sizeof(snapshot->advancedLineOne),
			(flags & UI_GAMEFLOW_DETAIL_IS_AUTOLOAD) != 0u ?
			"Z  AUTOLOAD ON" : "Z  AUTOLOAD");
	}
	if((flags & UI_GAMEFLOW_DETAIL_CAN_VERIFY) != 0u) {
		appendText(snapshot->advancedLineOne,
			sizeof(snapshot->advancedLineOne), "R  VERIFY");
	}
	if((flags & UI_GAMEFLOW_DETAIL_CAN_CLEAN_BOOT) != 0u) {
		copyText(snapshot->advancedLineTwo,
			sizeof(snapshot->advancedLineTwo), "L+A  CLEAN BOOT");
	}
}

bool UIGameflowDetail_Build(uiGameflowDetailSnapshot_t *snapshot,
	const uiGameflowDetailSource_t *source)
{
	size_t i;
	size_t enabledNameIndex = 0u;

	if(snapshot == NULL) {
		return false;
	}
	memset(snapshot, 0, sizeof(*snapshot));
	if(source == NULL || !validGameId(source->gameId) ||
		source->title == NULL || source->title[0] == '\0' ||
		(source->cheatCount != 0u && source->cheats == NULL)) {
		return false;
	}

	snapshot->generation = source->generation;
	snapshot->focusIndex = source->focusIndex;
	snapshot->flags =
		(source->flags & ~(uint32_t)UI_GAMEFLOW_DETAIL_HAS_BANNER) |
		UI_GAMEFLOW_DETAIL_VALID;
	memcpy(snapshot->gameId, source->gameId,
		UI_GAMEFLOW_DETAIL_ID_LENGTH + 1u);
	copyText(snapshot->title, sizeof(snapshot->title), source->title);
	copyText(snapshot->company, sizeof(snapshot->company), source->company);
	copyText(snapshot->facts, sizeof(snapshot->facts), source->facts);
	wrapDescription(snapshot->description, source->description);

	if(source->banner != NULL &&
		source->bannerSize == UI_GAMEFLOW_DETAIL_BANNER_BYTES) {
		memcpy(snapshot->banner, source->banner,
			UI_GAMEFLOW_DETAIL_BANNER_BYTES);
		snapshot->flags |= UI_GAMEFLOW_DETAIL_HAS_BANNER;
	}

	if(source->cheatCount > UINT32_MAX) {
		snapshot->cheatCount = UINT32_MAX;
	}
	else {
		snapshot->cheatCount = (uint32_t)source->cheatCount;
	}
	for(i = 0u; i < source->cheatCount; ++i) {
		if(!source->cheats[i].enabled) {
			continue;
		}
		if(snapshot->enabledCheatCount != UINT32_MAX) {
			++snapshot->enabledCheatCount;
		}
		if(enabledNameIndex < UI_GAMEFLOW_DETAIL_ENABLED_NAMES) {
			copyText(snapshot->enabledCheatNames[enabledNameIndex],
				sizeof(snapshot->enabledCheatNames[enabledNameIndex]),
				source->cheats[i].name != NULL ?
				source->cheats[i].name : "Unnamed cheat");
			++enabledNameIndex;
		}
	}
	snapshot->enabledCheatBytes = source->enabledCheatBytes;
	snapshot->cheatCapacityBytes = source->cheatCapacityBytes;
	if(snapshot->cheatCount == 0u) {
		snapshot->flags &= ~(uint32_t)UI_GAMEFLOW_DETAIL_CAN_CHEATS;
	}
	UIGameHistory_Format(snapshot->lastPlayedText, sizeof(snapshot->lastPlayedText),
		source->playHistoryAvailable, source->lastPlayedUnixSeconds);
	copyText(snapshot->saveStatusText, sizeof(snapshot->saveStatusText),
		UIGameHistory_SaveStatus(source->saveStatus));
	buildPresentation(snapshot);
	return true;
}

bool UIGameflowDetail_Matches(const uiGameflowDetailSnapshot_t *snapshot,
	uint32_t generation, uint32_t focusIndex, const char *gameId,
	size_t gameIdLength)
{
	return snapshot != NULL &&
		(snapshot->flags & UI_GAMEFLOW_DETAIL_VALID) != 0u &&
		snapshot->generation == generation &&
		snapshot->focusIndex == focusIndex &&
		gameId != NULL && gameIdLength == UI_GAMEFLOW_DETAIL_ID_LENGTH &&
		memcmp(snapshot->gameId, gameId,
			UI_GAMEFLOW_DETAIL_ID_LENGTH) == 0;
}

uiGameflowDetailAction_t UIGameflowDetail_ResolveAction(
	const uiGameflowDetailSnapshot_t *snapshot, uint32_t input)
{
	uint32_t flags;

	if(snapshot == NULL ||
		(snapshot->flags & UI_GAMEFLOW_DETAIL_VALID) == 0u) {
		return UI_GAMEFLOW_DETAIL_ACTION_NONE;
	}
	flags = snapshot->flags;
	if((input & UI_GAMEFLOW_DETAIL_INPUT_A) != 0u) {
		if((input & UI_GAMEFLOW_DETAIL_INPUT_L) != 0u &&
			(flags & UI_GAMEFLOW_DETAIL_CAN_CLEAN_BOOT) != 0u) {
			return UI_GAMEFLOW_DETAIL_ACTION_CLEAN_BOOT;
		}
		return UI_GAMEFLOW_DETAIL_ACTION_BOOT;
	}
	if((input & UI_GAMEFLOW_DETAIL_INPUT_B) != 0u &&
		(flags & UI_GAMEFLOW_DETAIL_CAN_LIBRARY) != 0u) {
		return UI_GAMEFLOW_DETAIL_ACTION_LIBRARY;
	}
	if((input & UI_GAMEFLOW_DETAIL_INPUT_R) != 0u &&
		(flags & UI_GAMEFLOW_DETAIL_CAN_VERIFY) != 0u) {
		return UI_GAMEFLOW_DETAIL_ACTION_VERIFY;
	}
	if((input & UI_GAMEFLOW_DETAIL_INPUT_X) != 0u &&
		(flags & UI_GAMEFLOW_DETAIL_CAN_SETTINGS) != 0u) {
		return UI_GAMEFLOW_DETAIL_ACTION_SETTINGS;
	}
	if((input & UI_GAMEFLOW_DETAIL_INPUT_Z) != 0u &&
		(flags & UI_GAMEFLOW_DETAIL_CAN_AUTOLOAD) != 0u) {
		return UI_GAMEFLOW_DETAIL_ACTION_AUTOLOAD;
	}
	if((input & UI_GAMEFLOW_DETAIL_INPUT_Y) != 0u &&
		(flags & UI_GAMEFLOW_DETAIL_CAN_CHEATS) != 0u) {
		return UI_GAMEFLOW_DETAIL_ACTION_CHEATS;
	}
	return UI_GAMEFLOW_DETAIL_ACTION_NONE;
}
