#include <string.h>

#include "ui_gameflow_resolver.h"

static unsigned char asciiLower(unsigned char value)
{
	return value >= 'A' && value <= 'Z' ?
		(unsigned char)(value + ('a' - 'A')) : value;
}

static bool asciiEquals(const char *left, const char *right)
{
	size_t i;

	if(left == NULL || right == NULL) {
		return false;
	}
	for(i = 0u; left[i] != '\0' && right[i] != '\0'; ++i) {
		if(asciiLower((unsigned char)left[i]) !=
			asciiLower((unsigned char)right[i])) {
			return false;
		}
	}
	return left[i] == '\0' && right[i] == '\0';
}

static bool validGameId(const char gameId[UI_GAMEFLOW_RESOLVER_ID_LENGTH + 1u])
{
	size_t i;

	if(gameId == NULL) {
		return false;
	}
	for(i = 0u; i < UI_GAMEFLOW_RESOLVER_ID_LENGTH; ++i) {
		unsigned char value = (unsigned char)gameId[i];
		if(!((value >= 'A' && value <= 'Z') ||
			(value >= '0' && value <= '9'))) {
			return false;
		}
	}
	return gameId[UI_GAMEFLOW_RESOLVER_ID_LENGTH] == '\0';
}

static bool validEntryType(uiGameflowLibraryEntryType_t type)
{
	return type == UI_GAMEFLOW_LIBRARY_ENTRY_OTHER ||
		type == UI_GAMEFLOW_LIBRARY_ENTRY_SPECIAL ||
		type == UI_GAMEFLOW_LIBRARY_ENTRY_FILE ||
		type == UI_GAMEFLOW_LIBRARY_ENTRY_DIRECTORY;
}

static bool canonicalGameName(const char *name)
{
	return asciiEquals(name, "game.fdi") || asciiEquals(name, "game.gcm") ||
		asciiEquals(name, "game.iso") || asciiEquals(name, "game.tgc");
}

static bool validMatchingCandidate(const uiGameflowResolverEntry_t *entry,
	const uiGameflowResolverFolder_t *folder)
{
	return entry->type == UI_GAMEFLOW_LIBRARY_ENTRY_FILE &&
		UIGameflowLibrary_IsGameImageName(entry->name) &&
		entry->headerValid && validGameId(entry->gameId) &&
		entry->discNumber <= 1u &&
		memcmp(entry->gameId, folder->gameId,
			UI_GAMEFLOW_RESOLVER_ID_LENGTH) == 0;
}

static void resetResult(uiGameflowResolverResult_t *result)
{
	memset(result, 0, sizeof(*result));
	result->primarySourceIndex = UI_GAMEFLOW_RESOLVER_NO_SOURCE;
	result->oppositeDiscSourceIndex = UI_GAMEFLOW_RESOLVER_NO_SOURCE;
}

bool UIGameflowResolver_IsOppositeDisc(
	const uiGameflowResolverEntry_t *primary,
	const uiGameflowResolverEntry_t *candidate)
{
	return primary != NULL && candidate != NULL && primary->headerValid &&
		candidate->headerValid && validGameId(primary->gameId) &&
		validGameId(candidate->gameId) && primary->discNumber <= 1u &&
		candidate->discNumber <= 1u &&
		primary->discNumber != candidate->discNumber &&
		primary->version == candidate->version &&
		memcmp(primary->gameId, candidate->gameId,
			UI_GAMEFLOW_RESOLVER_ID_LENGTH) == 0;
}

static bool duplicateSupportedSource(
	const uiGameflowResolverEntry_t *entries, size_t entryCount)
{
	size_t i;
	size_t j;

	for(i = 0u; i < entryCount; ++i) {
		if(entries[i].type != UI_GAMEFLOW_LIBRARY_ENTRY_FILE ||
			!UIGameflowLibrary_IsGameImageName(entries[i].name)) {
			continue;
		}
		for(j = i + 1u; j < entryCount; ++j) {
			if(entries[j].type == UI_GAMEFLOW_LIBRARY_ENTRY_FILE &&
				UIGameflowLibrary_IsGameImageName(entries[j].name) &&
				entries[i].sourceIndex == entries[j].sourceIndex) {
				return true;
			}
		}
	}
	return false;
}

uiGameflowResolveStatus_t UIGameflowResolver_Resolve(
	const uiGameflowResolverFolder_t *folder,
	const uiGameflowResolverEntry_t *entries, size_t entryCount,
	uiGameflowResolverResult_t *result)
{
	size_t supportedCount = 0u;
	size_t candidateCount = 0u;
	size_t canonicalCount = 0u;
	size_t discZeroCount = 0u;
	size_t canonicalIndex = 0u;
	size_t discZeroIndex = 0u;
	size_t primaryIndex = 0u;
	size_t oppositeIndex = 0u;
	size_t oppositeCount = 0u;
	bool invalidMetadata = false;
	bool idMismatch = false;
	size_t i;

	if(result == NULL) {
		return UI_GAMEFLOW_RESOLVE_INVALID_ARGUMENT;
	}
	resetResult(result);
	if(folder == NULL || !validGameId(folder->gameId) ||
		(entries == NULL && entryCount != 0u)) {
		return UI_GAMEFLOW_RESOLVE_INVALID_ARGUMENT;
	}

	for(i = 0u; i < entryCount; ++i) {
		const uiGameflowResolverEntry_t *entry = &entries[i];

		if(!validEntryType(entry->type) ||
			memchr(entry->name, '\0', sizeof(entry->name)) == NULL) {
			return UI_GAMEFLOW_RESOLVE_INVALID_ARGUMENT;
		}
		if(entry->type != UI_GAMEFLOW_LIBRARY_ENTRY_FILE ||
			!UIGameflowLibrary_IsGameImageName(entry->name)) {
			continue;
		}
		++supportedCount;
		if(entry->sourceIndex == UI_GAMEFLOW_RESOLVER_NO_SOURCE) {
			return UI_GAMEFLOW_RESOLVE_INVALID_ARGUMENT;
		}
		if(!entry->headerValid || !validGameId(entry->gameId) ||
			entry->discNumber > 1u) {
			invalidMetadata = true;
			continue;
		}
		if(memcmp(entry->gameId, folder->gameId,
			UI_GAMEFLOW_RESOLVER_ID_LENGTH) != 0) {
			idMismatch = true;
			continue;
		}

		++candidateCount;
		if(canonicalGameName(entry->name)) {
			canonicalIndex = i;
			++canonicalCount;
		}
		if(entry->discNumber == 0u) {
			if(discZeroCount == 0u) {
				discZeroIndex = i;
			}
			++discZeroCount;
		}
	}

	if(duplicateSupportedSource(entries, entryCount)) {
		return UI_GAMEFLOW_RESOLVE_INVALID_ARGUMENT;
	}
	if(invalidMetadata) {
		return UI_GAMEFLOW_RESOLVE_INVALID_METADATA;
	}
	if(idMismatch) {
		return UI_GAMEFLOW_RESOLVE_ID_MISMATCH;
	}
	if(supportedCount == 0u || candidateCount == 0u) {
		return UI_GAMEFLOW_RESOLVE_NO_IMAGE;
	}
	if(canonicalCount > 1u) {
		return UI_GAMEFLOW_RESOLVE_AMBIGUOUS;
	}

	if(canonicalCount == 1u) {
		primaryIndex = canonicalIndex;
	}
	else {
		if(candidateCount == 1u) {
			for(i = 0u; i < entryCount; ++i) {
				if(validMatchingCandidate(&entries[i], folder)) {
					primaryIndex = i;
					break;
				}
			}
		}
		else if(discZeroCount != 1u) {
			return UI_GAMEFLOW_RESOLVE_AMBIGUOUS;
		}
		else {
			primaryIndex = discZeroIndex;
		}
	}

	for(i = 0u; i < entryCount; ++i) {
		const uiGameflowResolverEntry_t *entry = &entries[i];
		const uiGameflowResolverEntry_t *primary = &entries[primaryIndex];
		uint8_t oppositeDisc = primary->discNumber == 0u ? 1u : 0u;

		if(i != primaryIndex && validMatchingCandidate(entry, folder) &&
			entry->discNumber == oppositeDisc &&
			entry->version == primary->version) {
			oppositeIndex = i;
			++oppositeCount;
		}
	}
	if(oppositeCount > 1u || (canonicalCount == 0u &&
		candidateCount != 1u + oppositeCount)) {
		return UI_GAMEFLOW_RESOLVE_AMBIGUOUS;
	}

	result->primarySourceIndex = entries[primaryIndex].sourceIndex;
	result->primaryDiscNumber = entries[primaryIndex].discNumber;
	result->version = entries[primaryIndex].version;
	memcpy(result->gameId, folder->gameId,
		UI_GAMEFLOW_RESOLVER_ID_LENGTH + 1u);
	if(oppositeCount == 1u) {
		result->hasOppositeDisc = true;
		result->oppositeDiscSourceIndex = entries[oppositeIndex].sourceIndex;
	}
	return UI_GAMEFLOW_RESOLVE_OK;
}
