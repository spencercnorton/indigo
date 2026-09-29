/*
 * Host test for ui_about.c: the Spotlight layout's game descriptions, read
 * from /swiss/ui/descriptions.txt.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_about.h"

#define CHECK(condition) do { \
	if(!(condition)) { \
		fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, \
			#condition); \
		exit(EXIT_FAILURE); \
	} \
} while(0)

static uiAboutEntry_t entries[UI_ABOUT_MAX_GAMES];

/* A hand-written file: a comment, a blank line, Windows line endings, a
 * tab, a line with a bad ID, one with only an ID, a game twice, out of
 * order, and no NUL or newline at the end. */
static const char FILE_TEXT[] =
	"# Indigo game descriptions\r\n"
	"\r\n"
	"GMSE01 Clean up Isle Delfino.\r\n"
	"GALE01\tA brawl among friends.\n"
	"gale01 lower case is no ID\n"
	"GAFE01\n"
	"GAFE01 Welcome to Animal Crossing.\n"
	"GALE01 A second line for Melee, which loses.\n"
	"GZLE01 Sail the Great Sea.";

static void testIndexAndFind(void)
{
	uiAbout_t about;
	char out[64];

	CHECK(UIAbout_Index(&about, FILE_TEXT, sizeof(FILE_TEXT) - 1u, entries,
		UI_ABOUT_MAX_GAMES) == 4u);
	CHECK(UIAbout_Find(&about, "GMSE01", out, sizeof(out)));
	CHECK(strcmp(out, "Clean up Isle Delfino.") == 0);
	CHECK(UIAbout_Find(&about, "GALE01", out, sizeof(out)));
	CHECK(strcmp(out, "A brawl among friends.") == 0);
	CHECK(UIAbout_Find(&about, "GAFE01", out, sizeof(out)));
	CHECK(strcmp(out, "Welcome to Animal Crossing.") == 0);
	/* The last line has no newline and is kept whole. */
	CHECK(UIAbout_Find(&about, "GZLE01", out, sizeof(out)));
	CHECK(strcmp(out, "Sail the Great Sea.") == 0);
}

static void testFallbacksAndMisses(void)
{
	uiAbout_t about;
	char out[8];

	UIAbout_Index(&about, FILE_TEXT, sizeof(FILE_TEXT) - 1u, entries,
		UI_ABOUT_MAX_GAMES);
	/* Another publisher's disc of the same game and region. */
	CHECK(UIAbout_Find(&about, "GMSE8P", out, sizeof(out)));
	/* Cut to the room there is, and always ended. */
	CHECK(strcmp(out, "Clean u") == 0);
	/* Another region is another game here: none of it. */
	CHECK(!UIAbout_Find(&about, "GMSP01", out, sizeof(out)));
	CHECK(out[0] == '\0');
	CHECK(!UIAbout_Find(&about, "gmse01", out, sizeof(out)));
	CHECK(!UIAbout_Find(&about, "GMS", out, sizeof(out)));
	CHECK(!UIAbout_Find(&about, NULL, out, sizeof(out)));
	CHECK(!UIAbout_Find(NULL, "GMSE01", out, sizeof(out)));
	CHECK(!UIAbout_Find(&about, "GMSE01", out, 0u));
}

static void testControlCharactersAndLimits(void)
{
	static const char text[] = "GTEE01 Wax\tyour board.\x01\n";
	static char big[UI_ABOUT_MAX_BYTES + 1u];
	uiAboutEntry_t two[2];
	uiAbout_t about;
	char out[64];

	CHECK(UIAbout_Index(&about, text, sizeof(text) - 1u, entries,
		UI_ABOUT_MAX_GAMES) == 1u);
	CHECK(UIAbout_Find(&about, "GTEE01", out, sizeof(out)));
	CHECK(strcmp(out, "Wax your board. ") == 0);
	/* Games past the room given are left out, never written past it. */
	CHECK(UIAbout_Index(&about, FILE_TEXT, sizeof(FILE_TEXT) - 1u, two, 2u) == 2u);
	/* A file over the limit, or nothing at all, finds nothing. */
	memset(big, 'A', sizeof(big));
	CHECK(UIAbout_Index(&about, big, sizeof(big), entries, UI_ABOUT_MAX_GAMES) == 0u);
	CHECK(!UIAbout_Find(&about, "AAAAAA", out, sizeof(out)));
	CHECK(UIAbout_Index(&about, NULL, 10u, entries, UI_ABOUT_MAX_GAMES) == 0u);
	CHECK(UIAbout_Index(&about, "", 0u, entries, UI_ABOUT_MAX_GAMES) == 0u);
}

int main(void)
{
	testIndexAndFind();
	testFallbacksAndMisses();
	testControlCharactersAndLimits();
	puts("ui_about: descriptions indexed, found, bounded and cleaned");
	return EXIT_SUCCESS;
}
