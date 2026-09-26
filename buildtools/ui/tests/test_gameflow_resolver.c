/*
 * Host test command (run from the repository root):
 * cc -std=c11 -Wall -Wextra -Werror -Wconversion -Wsign-conversion \
 *   -pedantic -Icube/swiss/source/gui \
 *   buildtools/ui/tests/test_gameflow_resolver.c \
 *   cube/swiss/source/gui/ui_gameflow_resolver.c \
 *   cube/swiss/source/gui/ui_gameflow_library.c \
 *   -o /tmp/test_gameflow_resolver && /tmp/test_gameflow_resolver
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_gameflow_resolver.h"

#define CHECK(condition) do { \
	if(!(condition)) { \
		fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, \
			#condition); \
		exit(EXIT_FAILURE); \
	} \
} while(0)

static uiGameflowResolverFolder_t makeFolder(const char *gameId)
{
	uiGameflowResolverFolder_t folder;

	memset(&folder, 0, sizeof(folder));
	CHECK(gameId != NULL);
	CHECK(strlen(gameId) == UI_GAMEFLOW_RESOLVER_ID_LENGTH);
	memcpy(folder.gameId, gameId, UI_GAMEFLOW_RESOLVER_ID_LENGTH);
	return folder;
}

static uiGameflowResolverEntry_t makeEntry(uint32_t sourceIndex,
	uiGameflowLibraryEntryType_t type, const char *name, bool headerValid,
	const char *gameId, uint8_t discNumber, uint8_t version)
{
	uiGameflowResolverEntry_t entry;
	size_t nameLength;

	memset(&entry, 0, sizeof(entry));
	entry.sourceIndex = sourceIndex;
	entry.type = type;
	entry.headerValid = headerValid;
	entry.discNumber = discNumber;
	entry.version = version;
	CHECK(name != NULL);
	nameLength = strlen(name);
	CHECK(nameLength < sizeof(entry.name));
	memcpy(entry.name, name, nameLength + 1u);
	if(gameId != NULL) {
		CHECK(strlen(gameId) == UI_GAMEFLOW_RESOLVER_ID_LENGTH);
		memcpy(entry.gameId, gameId, UI_GAMEFLOW_RESOLVER_ID_LENGTH);
	}
	return entry;
}

static void checkFailureReset(const uiGameflowResolverResult_t *result)
{
	CHECK(result->primarySourceIndex == UI_GAMEFLOW_RESOLVER_NO_SOURCE);
	CHECK(result->oppositeDiscSourceIndex == UI_GAMEFLOW_RESOLVER_NO_SOURCE);
	CHECK(!result->hasOppositeDisc);
}

static void testCanonicalPreferred(void)
{
	uiGameflowResolverFolder_t folder = makeFolder("GMSE01");
	uiGameflowResolverEntry_t entries[] = {
		makeEntry(4u, UI_GAMEFLOW_LIBRARY_ENTRY_FILE, "backup.iso", true,
			"GMSE01", 0u, 0u),
		makeEntry(9u, UI_GAMEFLOW_LIBRARY_ENTRY_FILE, "GAME.GCM", true,
			"GMSE01", 0u, 0u),
		makeEntry(12u, UI_GAMEFLOW_LIBRARY_ENTRY_FILE, "notes.txt", false,
			NULL, 0u, 0u)
	};
	uiGameflowResolverResult_t result;

	CHECK(UIGameflowResolver_Resolve(&folder, entries,
		sizeof(entries) / sizeof(entries[0]), &result) ==
		UI_GAMEFLOW_RESOLVE_OK);
	CHECK(result.primarySourceIndex == 9u);
	CHECK(result.primaryDiscNumber == 0u);
	CHECK(!result.hasOppositeDisc);
	CHECK(strcmp(result.gameId, "GMSE01") == 0);
}

static void testOnlyImage(void)
{
	uiGameflowResolverFolder_t folder = makeFolder("GALE01");
	uiGameflowResolverEntry_t entry = makeEntry(41u,
		UI_GAMEFLOW_LIBRARY_ENTRY_FILE, "second-disc.tgc", true,
		"GALE01", 1u, 7u);
	uiGameflowResolverResult_t result;

	CHECK(UIGameflowResolver_Resolve(&folder, &entry, 1u, &result) ==
		UI_GAMEFLOW_RESOLVE_OK);
	CHECK(result.primarySourceIndex == 41u);
	CHECK(result.primaryDiscNumber == 1u);
	CHECK(result.version == 7u);
	CHECK(!result.hasOppositeDisc);
}

static void testCanonicalDiscOnePreferred(void)
{
	uiGameflowResolverFolder_t folder = makeFolder("G4BE08");
	uiGameflowResolverEntry_t entries[] = {
		makeEntry(30u, UI_GAMEFLOW_LIBRARY_ENTRY_FILE, "disc-one.iso", true,
			"G4BE08", 0u, 4u),
		makeEntry(31u, UI_GAMEFLOW_LIBRARY_ENTRY_FILE, "game.tgc", true,
			"G4BE08", 1u, 4u)
	};
	uiGameflowResolverResult_t result;

	CHECK(UIGameflowResolver_Resolve(&folder, entries, 2u, &result) ==
		UI_GAMEFLOW_RESOLVE_OK);
	CHECK(result.primarySourceIndex == 31u);
	CHECK(result.primaryDiscNumber == 1u);
	CHECK(result.hasOppositeDisc);
	CHECK(result.oppositeDiscSourceIndex == 30u);
}

static void testAmbiguity(void)
{
	uiGameflowResolverFolder_t folder = makeFolder("GMSE01");
	uiGameflowResolverEntry_t entries[] = {
		makeEntry(2u, UI_GAMEFLOW_LIBRARY_ENTRY_FILE, "first.iso", true,
			"GMSE01", 0u, 0u),
		makeEntry(3u, UI_GAMEFLOW_LIBRARY_ENTRY_FILE, "backup.gcm", true,
			"GMSE01", 0u, 0u)
	};
	uiGameflowResolverResult_t result;

	CHECK(UIGameflowResolver_Resolve(&folder, entries, 2u, &result) ==
		UI_GAMEFLOW_RESOLVE_AMBIGUOUS);
	checkFailureReset(&result);

	entries[0] = makeEntry(2u, UI_GAMEFLOW_LIBRARY_ENTRY_FILE, "game.iso",
		true, "GMSE01", 0u, 0u);
	entries[1] = makeEntry(3u, UI_GAMEFLOW_LIBRARY_ENTRY_FILE, "GAME.gcm",
		true, "GMSE01", 0u, 0u);
	CHECK(UIGameflowResolver_Resolve(&folder, entries, 2u, &result) ==
		UI_GAMEFLOW_RESOLVE_AMBIGUOUS);
	checkFailureReset(&result);
}

static void testNoImage(void)
{
	uiGameflowResolverFolder_t folder = makeFolder("GMSE01");
	uiGameflowResolverEntry_t entries[] = {
		makeEntry(0u, UI_GAMEFLOW_LIBRARY_ENTRY_SPECIAL, "..", false,
			NULL, 0u, 0u),
		makeEntry(1u, UI_GAMEFLOW_LIBRARY_ENTRY_DIRECTORY, "extras", false,
			NULL, 0u, 0u),
		makeEntry(2u, UI_GAMEFLOW_LIBRARY_ENTRY_FILE, "readme.txt", false,
			NULL, 0u, 0u)
	};
	uiGameflowResolverResult_t result;

	CHECK(UIGameflowResolver_Resolve(&folder, entries, 3u, &result) ==
		UI_GAMEFLOW_RESOLVE_NO_IMAGE);
	checkFailureReset(&result);
	CHECK(UIGameflowResolver_Resolve(&folder, NULL, 0u, &result) ==
		UI_GAMEFLOW_RESOLVE_NO_IMAGE);
}

static void testIdMismatch(void)
{
	uiGameflowResolverFolder_t folder = makeFolder("GMSE01");
	uiGameflowResolverEntry_t entry = makeEntry(1u,
		UI_GAMEFLOW_LIBRARY_ENTRY_FILE, "game.iso", true, "GALE01", 0u, 0u);
	uiGameflowResolverResult_t result;

	CHECK(UIGameflowResolver_Resolve(&folder, &entry, 1u, &result) ==
		UI_GAMEFLOW_RESOLVE_ID_MISMATCH);
	checkFailureReset(&result);
}

static void testDiscPair(void)
{
	uiGameflowResolverFolder_t folder = makeFolder("G4BE08");
	uiGameflowResolverEntry_t entries[] = {
		makeEntry(22u, UI_GAMEFLOW_LIBRARY_ENTRY_FILE, "Resident Evil 4 D2.iso",
			true, "G4BE08", 1u, 3u),
		makeEntry(11u, UI_GAMEFLOW_LIBRARY_ENTRY_FILE, "Resident Evil 4 D1.iso",
			true, "G4BE08", 0u, 3u)
	};
	uiGameflowResolverResult_t result;

	CHECK(UIGameflowResolver_Resolve(&folder, entries, 2u, &result) ==
		UI_GAMEFLOW_RESOLVE_OK);
	CHECK(result.primarySourceIndex == 11u);
	CHECK(result.primaryDiscNumber == 0u);
	CHECK(result.hasOppositeDisc);
	CHECK(result.oppositeDiscSourceIndex == 22u);
	CHECK(result.version == 3u);
}

static void testOppositeDiscPredicate(void)
{
	uiGameflowResolverEntry_t primary = makeEntry(1u,
		UI_GAMEFLOW_LIBRARY_ENTRY_FILE, "game.iso", true,
		"G4BE08", 0u, 3u);
	uiGameflowResolverEntry_t candidate = makeEntry(2u,
		UI_GAMEFLOW_LIBRARY_ENTRY_FILE, "disc2.iso", true,
		"G4BE08", 1u, 3u);

	CHECK(UIGameflowResolver_IsOppositeDisc(&primary, &candidate));
	candidate.version = 4u;
	CHECK(!UIGameflowResolver_IsOppositeDisc(&primary, &candidate));
	candidate.version = 3u;
	candidate.gameId[0] = 'X';
	CHECK(!UIGameflowResolver_IsOppositeDisc(&primary, &candidate));
	candidate.gameId[0] = 'G';
	candidate.discNumber = 0u;
	CHECK(!UIGameflowResolver_IsOppositeDisc(&primary, &candidate));
	candidate.discNumber = 1u;
	candidate.headerValid = false;
	CHECK(!UIGameflowResolver_IsOppositeDisc(&primary, &candidate));
	CHECK(!UIGameflowResolver_IsOppositeDisc(NULL, &candidate));
}

static void testParentAndDirectoriesIgnored(void)
{
	uiGameflowResolverFolder_t folder = makeFolder("GMSE01");
	uiGameflowResolverEntry_t entries[] = {
		makeEntry(0u, UI_GAMEFLOW_LIBRARY_ENTRY_SPECIAL, "game.iso", false,
			NULL, 0u, 0u),
		makeEntry(1u, UI_GAMEFLOW_LIBRARY_ENTRY_DIRECTORY, "game.gcm", false,
			NULL, 0u, 0u),
		makeEntry(2u, UI_GAMEFLOW_LIBRARY_ENTRY_OTHER, "game.fdi", false,
			NULL, 0u, 0u),
		makeEntry(3u, UI_GAMEFLOW_LIBRARY_ENTRY_FILE, "sunshine.iso", true,
			"GMSE01", 0u, 0u)
	};
	uiGameflowResolverResult_t result;

	CHECK(UIGameflowResolver_Resolve(&folder, entries, 4u, &result) ==
		UI_GAMEFLOW_RESOLVE_OK);
	CHECK(result.primarySourceIndex == 3u);
	CHECK(!result.hasOppositeDisc);
}

static void testDeterministicOrder(void)
{
	uiGameflowResolverFolder_t folder = makeFolder("G4BE08");
	uiGameflowResolverEntry_t first[] = {
		makeEntry(80u, UI_GAMEFLOW_LIBRARY_ENTRY_FILE, "disc-two.iso", true,
			"G4BE08", 1u, 2u),
		makeEntry(10u, UI_GAMEFLOW_LIBRARY_ENTRY_DIRECTORY, "game.iso", false,
			NULL, 0u, 0u),
		makeEntry(70u, UI_GAMEFLOW_LIBRARY_ENTRY_FILE, "disc-one.iso", true,
			"G4BE08", 0u, 2u),
		makeEntry(5u, UI_GAMEFLOW_LIBRARY_ENTRY_SPECIAL, "..", false,
			NULL, 0u, 0u)
	};
	uiGameflowResolverEntry_t second[] = {
		first[3], first[2], first[0], first[1]
	};
	uiGameflowResolverResult_t left;
	uiGameflowResolverResult_t right;

	CHECK(UIGameflowResolver_Resolve(&folder, first, 4u, &left) ==
		UI_GAMEFLOW_RESOLVE_OK);
	CHECK(UIGameflowResolver_Resolve(&folder, second, 4u, &right) ==
		UI_GAMEFLOW_RESOLVE_OK);
	CHECK(left.primarySourceIndex == right.primarySourceIndex);
	CHECK(left.oppositeDiscSourceIndex == right.oppositeDiscSourceIndex);
	CHECK(left.primarySourceIndex == 70u);
	CHECK(left.oppositeDiscSourceIndex == 80u);
}

static void testMetadataAndVersionRejection(void)
{
	uiGameflowResolverFolder_t folder = makeFolder("GMSE01");
	uiGameflowResolverEntry_t entries[] = {
		makeEntry(1u, UI_GAMEFLOW_LIBRARY_ENTRY_FILE, "broken.iso", false,
			NULL, 0u, 0u),
		makeEntry(2u, UI_GAMEFLOW_LIBRARY_ENTRY_FILE, "valid.iso", true,
			"GMSE01", 0u, 0u)
	};
	uiGameflowResolverResult_t result;

	CHECK(UIGameflowResolver_Resolve(&folder, entries, 2u, &result) ==
		UI_GAMEFLOW_RESOLVE_INVALID_METADATA);
	checkFailureReset(&result);

	entries[0] = makeEntry(1u, UI_GAMEFLOW_LIBRARY_ENTRY_FILE, "disc-one.iso",
		true, "GMSE01", 0u, 0u);
	entries[1] = makeEntry(2u, UI_GAMEFLOW_LIBRARY_ENTRY_FILE, "disc-two.iso",
		true, "GMSE01", 1u, 1u);
	CHECK(UIGameflowResolver_Resolve(&folder, entries, 2u, &result) ==
		UI_GAMEFLOW_RESOLVE_AMBIGUOUS);
	checkFailureReset(&result);
}

static void testInputValidation(void)
{
	uiGameflowResolverFolder_t folder = makeFolder("GMSE01");
	uiGameflowResolverEntry_t entry = makeEntry(1u,
		UI_GAMEFLOW_LIBRARY_ENTRY_FILE, "game.iso", true, "GMSE01", 0u, 0u);
	uiGameflowResolverResult_t result;

	memset(&result, 0x5a, sizeof(result));
	CHECK(UIGameflowResolver_Resolve(NULL, &entry, 1u, &result) ==
		UI_GAMEFLOW_RESOLVE_INVALID_ARGUMENT);
	checkFailureReset(&result);
	CHECK(UIGameflowResolver_Resolve(&folder, NULL, 1u, &result) ==
		UI_GAMEFLOW_RESOLVE_INVALID_ARGUMENT);
	checkFailureReset(&result);
	CHECK(UIGameflowResolver_Resolve(&folder, &entry, 1u, NULL) ==
		UI_GAMEFLOW_RESOLVE_INVALID_ARGUMENT);

	memset(entry.name, 'x', sizeof(entry.name));
	CHECK(UIGameflowResolver_Resolve(&folder, &entry, 1u, &result) ==
		UI_GAMEFLOW_RESOLVE_INVALID_ARGUMENT);
	checkFailureReset(&result);
}

int main(void)
{
	testCanonicalPreferred();
	testOnlyImage();
	testCanonicalDiscOnePreferred();
	testAmbiguity();
	testNoImage();
	testIdMismatch();
	testDiscPair();
	testOppositeDiscPredicate();
	testParentAndDirectoriesIgnored();
	testDeterministicOrder();
	testMetadataAndVersionRejection();
	testInputValidation();
	puts("ui_gameflow resolver tests passed");
	return EXIT_SUCCESS;
}
