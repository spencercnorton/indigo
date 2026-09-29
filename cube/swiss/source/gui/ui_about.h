#ifndef UI_ABOUT_H
#define UI_ABOUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Game descriptions for the Spotlight layout, from /swiss/ui/descriptions.txt:
 * one game per line, its six-character game ID, a space or a tab, then the
 * description, in Windows-1252 like a disc banner's text. Lines starting with
 * # and lines without a valid ID are skipped, and the file need not be sorted:
 * a hand-written one works. The first line for a game wins.
 *
 * Pure: the caller reads the file and owns its bytes and the entries.
 */
#define UI_ABOUT_MAX_BYTES (1024u * 1024u)
#define UI_ABOUT_MAX_GAMES 4096u

typedef struct {
	char id[6];
	uint16_t length;
	uint32_t offset;
} uiAboutEntry_t;

typedef struct {
	const char *data;
	const uiAboutEntry_t *entries;
	size_t count;
} uiAbout_t;

/* Index size bytes of data (no NUL needed) into entries, which has room for
 * capacity games; a game past that is left out. Returns how many games it
 * found: 0 for an empty or unusable file, which then finds nothing. */
size_t UIAbout_Index(uiAbout_t *about, const char *data, size_t size,
	uiAboutEntry_t *entries, size_t capacity);

/* Copy a game's description into out (bounded, NUL-ended, control
 * characters as spaces): the exact six-character ID first, else the first
 * game sharing its first four characters, the same game and region. False,
 * with out empty, when neither is there. */
bool UIAbout_Find(const uiAbout_t *about, const char *gameId, char *out,
	size_t capacity);

#endif
