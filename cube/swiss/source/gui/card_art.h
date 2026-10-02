#ifndef CARD_ART_H
#define CARD_ART_H

#include <gccore.h>
#include <stdbool.h>
#include <stdint.h>

/* card_art: posters made on the console, for the cards the poster pack has
 * none for: an app (Apps) or a folder of games (the Library, with Library
 * Folders on). A card's poster is its own picture, a PNG on the card, or its
 * name when it has none or the picture can't be used.
 *
 * Every picture is held to ui_png's limits: a file over UI_PNG_MAX_FILE is
 * never read, and ui_png refuses a picture over UI_PNG_MAX_SIDE a side and
 * makes any other one within UI_PNG_MAX_WORK. The posters of the cards on
 * screen, UI_APPS_ART_SLOTS of them, take one block of
 * UI_APPS_ART_SLOTS * UI_PNG_POSTER_BYTES, set aside by CardArt_Open. They
 * are made one at a time on a thread of their own, below the menus'
 * priority, so a big picture never holds up a button.
 *
 * One screen uses it at a time: Apps, or the Library's listing on screen.
 * Menu thread, except CardArt_Poster. */

typedef struct {
	/* The card's picture's size in bytes, or 0 when it has none (or it
	 * failed before and isn't to be tried again). */
	uint32_t (*pictureSize)(int32_t card);
	/* Reads the card's picture, size bytes of it, into data; false when it
	 * can't. On the poster thread when threadSafe, else on the menu thread
	 * between frames. */
	bool (*readPicture)(int32_t card, uint8_t *data, uint32_t size);
	/* The card's name, drawn as its poster when it has no picture. */
	const char *(*name)(int32_t card);
	/* After the card's picture was tried: whether it made a poster. Not
	 * called for a poster that was stopped. */
	void (*verdict)(int32_t card, bool ok);
	/* Whether the device may be read from another thread. */
	bool threadSafe;
} cardArtSource_t;

/* Starts making posters for source's cards. Without memory for the
 * posters, there are none: every card shows its name as it does today.
 * Open already, it takes the new source in place of the old one: the old
 * cards' posters go, the memory stays. */
void CardArt_Open(const cardArtSource_t *source);
/* The cards on screen, nearest first: the posters to keep or make. */
void CardArt_Want(const int32_t *cards, uint32_t count);
/* Hands the poster thread the next poster to make. Call it in the frames
 * that wait for a button. */
void CardArt_Poll(void);
/* Stops making posters (the one in hand stops between rows), keeping those
 * made: before anything that needs the device or the memory, a launch above
 * all. CardArt_Resume goes on. */
void CardArt_Pause(void);
void CardArt_Resume(void);
/* Stops, forgets every poster and gives the memory back. */
void CardArt_Close(void);
/* The card's poster, or NULL while it has none. Video thread, with the
 * video lock held, as _DrawGameflow has it. */
GXTexObj *CardArt_Poster(int32_t card);
/* How many milliseconds ago the card's poster was made, or 0 while it has
 * none; as CardArt_Poster. */
u32 CardArt_PosterAgeMs(int32_t card);

#endif
