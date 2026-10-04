#ifndef UI_SAVES_H
#define UI_SAVES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Memory Cards (Home > System > Memory Cards): the pure half. saves.c asks
 * the card and SD drivers for the bytes; these decide what the bytes are and
 * where a save may go. */

#define UI_SAVES_ENTRY_SIZE 64u	/* a card's directory entry, a .gci's header */
#define UI_SAVES_BLOCK_SIZE 8192u
#define UI_SAVES_NAME_LENGTH 32u	/* CARD_FILENAMELEN */

/* The places a save can go, in the order the destination list shows them. */
typedef enum {
	UI_SAVES_PLACE_SLOT_A = 0,
	UI_SAVES_PLACE_SLOT_B,
	UI_SAVES_PLACE_FOLDER,	/* Settings > Storage > Save Folder */
	UI_SAVES_PLACE_CHOOSE,	/* another folder, chosen then */
	UI_SAVES_PLACE_COUNT
} uiSavesPlace_t;

/* A .gci is the card's directory entry, then the save's blocks. Action
 * Replay (.sav, "DATELGC_SAVE") and GameShark (.gcs, "GCSAVE") files put
 * their own header first. Copies the entry to entry as a .gci has it and
 * returns where the blocks start, or 0 when the file isn't one whole save:
 * its length must be the entry's block count exactly. */
size_t UISaves_FindEntry(const uint8_t *file, size_t length,
	uint8_t entry[UI_SAVES_ENTRY_SIZE]);

/* FindEntry from the first headLength bytes of a file fileLength long: the
 * first UI_SAVES_HEAD_SIZE bytes hold any wrapper's header and the entry. */
#define UI_SAVES_HEAD_SIZE 0x150u
size_t UISaves_FindEntryPrefix(const uint8_t *head, size_t headLength,
	size_t fileLength, uint8_t entry[UI_SAVES_ENTRY_SIZE]);

/* The blocks the entry says the save has. */
unsigned UISaves_Blocks(const uint8_t entry[UI_SAVES_ENTRY_SIZE]);

/* "01-GALE-SuperSmashBros0110290334.gci", the maker, game code and card
 * name, as Dolphin names a GCI folder's files, so a folder of them can be a
 * Dolphin memory card. A character a FAT name can't hold becomes '_'. */
void UISaves_FileName(char *out, size_t capacity,
	const uint8_t entry[UI_SAVES_ENTRY_SIZE]);

/* "name.gci" as "name_2.gci": the attempt'th name for a copy when the first
 * is taken (attempt 1 is the name itself). */
void UISaves_NumberedName(char *out, size_t capacity, const char *name,
	int attempt);

/* Where a save in from can go: a slot with a card that isn't from, the Save
 * Folder unless the save is in it, and any folder when there is an SD card
 * to hold one. Returns how many it wrote to out. */
int UISaves_Destinations(uiSavesPlace_t from, bool cardA, bool cardB,
	bool folders, bool inSaveFolder, uiSavesPlace_t out[UI_SAVES_PLACE_COUNT]);

/* ------------------------------------------------------------------------
 * A save's art: its banner, its animated icon and its comment, where the
 * entry puts them in the save's data (the bytes after the entry), read as
 * Dolphin reads them. Fields come from the entry's own bytes, never through
 * libogc2's offset_icon[] (which steps an RGB5A3 frame 3072 bytes, not 2048)
 * or its CARD_GetIconSpeed and CARD_SetIconSpeed (wrong bits, wrong field).
 * --------------------------------------------------------------------- */
#define UI_SAVES_ICON_FRAMES 8u		/* CARD_MAXICONS */
#define UI_SAVES_ICON_BYTES 2048u	/* a 32x32 frame as RGB5A3 */
#define UI_SAVES_BANNER_BYTES 6144u	/* the 96x32 banner as RGB5A3 */
#define UI_SAVES_ART_STEPS 14u		/* 8 frames, then 6 back in a bounce */
#define UI_SAVES_ART_MAX_END 65536u	/* the most of a save its art is read from */
#define UI_SAVES_ART_TICK_HZ 15u	/* speed 1 holds a frame 4 VSyncs at 60 Hz */
#define UI_SAVES_ART_BANNER (-1)	/* ToRgb5a3's banner, not a frame */
#define UI_SAVES_ART_BLANK 0xFFu	/* a step that shows nothing */

/* A picture's pixels, as the entry's two-bit fields name them. */
typedef enum {
	UI_SAVES_ART_NONE = 0,
	UI_SAVES_ART_CI8_SHARED,	/* the frames' palette follows the last frame */
	UI_SAVES_ART_RGB5A3,
	UI_SAVES_ART_CI8_OWN		/* its own palette follows it, as a CI8 banner's */
} uiSavesArtFormat_t;

typedef struct {
	uint32_t end;		/* the bytes of the save's data the art spans */
	uint32_t bannerAt;
	uint32_t frameAt[UI_SAVES_ICON_FRAMES];
	uint32_t paletteAt;	/* the shared palette, when a frame uses it */
	uint32_t commentAt;
	uint8_t bannerFormat;	/* NONE, RGB5A3 or CI8_OWN */
	uint8_t frameFormat[UI_SAVES_ICON_FRAMES];	/* NONE: shows a later frame */
	uint8_t frames;		/* frames with a speed, up to the first without */
	uint8_t steps;		/* the timeline's steps, 0 when there is no icon */
	uint8_t stepFrame[UI_SAVES_ART_STEPS];	/* a frame with pixels, or BLANK */
	uint8_t stepHold[UI_SAVES_ART_STEPS];	/* in ticks: 1, 2 or 3 */
	uint8_t period;		/* the ticks in one turn of the timeline */
	bool comment;		/* commentAt holds the comment's two 32-byte lines */
} uiSavesArt_t;

/* Where entry's banner, icon frames and comment lie in a save whose data is
 * dataLength bytes. A part that runs past the data, or past
 * UI_SAVES_ART_MAX_END, is left out on its own. An icon is up to 8 frames,
 * ending at the first with no speed; a frame with no pixels shows the next
 * one that has them, or nothing; banner_fmt bit 0x04 plays the frames back
 * again in reverse; and a first frame with no pixels means no icon at all.
 * Returns whether any of it is there to read (art->end bytes). */
bool UISaves_ArtLayout(const uint8_t entry[UI_SAVES_ENTRY_SIZE],
	size_t dataLength, uiSavesArt_t *art);

/* Frame number frame (or UI_SAVES_ART_BANNER) of the save's data, length
 * bytes of it, as 4x4 tiles of RGB5A3 texels, GX_TF_RGB5A3's layout: an
 * RGB5A3 picture as it is, a CI8 one looked up in its palette, so drawing it
 * needs no TLUT. Writes UI_SAVES_ICON_BYTES (UI_SAVES_BANNER_BYTES for the
 * banner); false when the picture has no pixels or lies outside length. */
bool UISaves_ToRgb5a3(const uint8_t *data, size_t length,
	const uiSavesArt_t *art, int frame, uint8_t *out);

/* The frame the icon shows at tick (UI_SAVES_ART_TICK_HZ ticks a second),
 * or -1 for nothing. Every icon runs on the same clock. */
int UISaves_ArtStep(const uiSavesArt_t *art, uint32_t tick);

#endif
