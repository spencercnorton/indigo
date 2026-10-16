#ifndef UI_MANUAL_H
#define UI_MANUAL_H

/* ui_manual: a game's manual, as Game Details opens it from its Manual row.
 * The file is /swiss/ui/manuals/<GAMEID>.pak (or <first four>.pak for every
 * publisher's copy of a game), pages that buildtools/ui/manual_pack.py made
 * from a PDF; its format is in docs/MANUALS.md. This file is pure: it checks
 * a manual's header, index and pages, decides which page each of the
 * viewer's two page slots holds, and places a page on the screen. manual.c
 * reads the card and draws. */

#include <stdbool.h>
#include <stdint.h>

#define UI_MANUAL_HEADER_BYTES 64u
#define UI_MANUAL_ENTRY_BYTES 32u
#define UI_MANUAL_MAX_PAGES 256u
/* A page at full width is one texel a pixel of the 640 x 480 stage. */
#define UI_MANUAL_WIDTH 640u
/* The tallest page: two pages (and their halves) stay under 1 MB. */
#define UI_MANUAL_MAX_HEIGHT 1152u
#define UI_MANUAL_FORMAT_I4 0u
#define UI_MANUAL_FORMAT_CMPR 14u
/* GX draws at most 1024 rows of a texture: a taller page draws in two. */
#define UI_MANUAL_STRIP_ROWS 1024u
/* The viewer's two page slots: the page shown and the next one read ahead. */
#define UI_MANUAL_SLOTS 2u
/* Frames a slot rests after it stops showing, so the GPU has finished
 * drawing it before a new page is read into it. */
#define UI_MANUAL_GPU_FRAMES 2u
/* The bar under the page: its number and the buttons. */
#define UI_MANUAL_BAR 30u

/* What the console TV pass may change, each one constant here:
 * - a page's size on a 16:9 screen: with Menu Widescreen on, a full-width
 *   page keeps its shape (the stage's 4:3) unless this is 1, when it grows
 *   to the screen's whole width (4/3 bigger, more to scroll);
 * - colour or gray, and margins trimmed or not, are manual_pack.py's
 *   (CMPR and trimmed by default; --gray, --no-trim). */
#define UI_MANUAL_FILL_WIDESCREEN 0

typedef struct {
	uint32_t pages;
	uint32_t indexBytes;	/* pages x UI_MANUAL_ENTRY_BYTES, after the header */
	uint32_t dataOffset;	/* the first page's record */
	uint32_t recordBytes;	/* a page and its half */
	uint32_t pageBytes;	/* the page alone: its half follows it */
	uint16_t height;	/* every page is a 640 x height texture */
	uint8_t format;		/* UI_MANUAL_FORMAT_ */
} uiManualInfo_t;

typedef struct {
	uint32_t crc;
	uint16_t width;		/* the page inside its 640 x height texture */
	uint16_t height;
} uiManualPage_t;

/* The header; fileSize is the file's real size. False for a file this
 * Indigo doesn't read: nothing past the header need be read then. */
bool UIManual_Header(const uint8_t *header, uint32_t fileSize,
	uiManualInfo_t *info);
/* The index, info->indexBytes of it, against the CRC the header holds; on
 * success pages[0..info->pages) are its pages. */
bool UIManual_Index(const uint8_t *header, const uint8_t *index,
	const uiManualInfo_t *info, uiManualPage_t *pages);
/* Where page n's record (info->recordBytes) starts in the file. */
uint32_t UIManual_RecordOffset(const uiManualInfo_t *info, uint32_t n);
/* A page's record against its CRC. */
bool UIManual_RecordIntact(const uiManualInfo_t *info,
	const uiManualPage_t *page, const uint8_t *record);

/* The two page slots. The page shown is read first, then the one after it
 * in the way the pages last turned. One read at a time; a slot that just
 * stopped showing rests UI_MANUAL_GPU_FRAMES before it takes another page.
 * Frame counts wrap. */
typedef enum {
	UI_MANUAL_SLOT_EMPTY = 0,
	UI_MANUAL_SLOT_READING,
	UI_MANUAL_SLOT_READY,
	UI_MANUAL_SLOT_FAILED
} uiManualSlotState_t;

typedef struct {
	uint32_t page;
	uint32_t restUntil;	/* a frame count, while resting */
	uint8_t state;
	uint8_t resting;
} uiManualSlot_t;

typedef struct {
	uint32_t pages;
	uint32_t page;		/* the page shown, from 0 */
	int32_t step;		/* +1 or -1: the next page read ahead */
	int32_t shown;		/* the slot drawn, or -1 */
	uiManualSlot_t slot[UI_MANUAL_SLOTS];
} uiManualReader_t;

void UIManual_ReaderInit(uiManualReader_t *reader, uint32_t pages);
/* Shows page (the last one past the end); now is the frame count. */
void UIManual_Go(uiManualReader_t *reader, uint32_t page, uint32_t now);
/* The next read, if one is due: the slot to read into and the page. The
 * slot is READING until UIManual_ReadDone. */
bool UIManual_NextRead(uiManualReader_t *reader, uint32_t now,
	uint32_t *slot, uint32_t *page);
void UIManual_ReadDone(uiManualReader_t *reader, uint32_t slot, bool ok,
	uint32_t now);
/* The shown page's slot when it is ready, else -1. */
int32_t UIManual_Shown(const uiManualReader_t *reader);
/* The shown page couldn't be read (a damaged page or card). */
bool UIManual_ShownFailed(const uiManualReader_t *reader);

/* Where a page draws on the 640 x 480 stage. Full width: the page from its
 * top, scroll rows of it above the screen, zoom 1 (or 4/3 to fill a 16:9
 * screen); whole: its half copy fitted above the bar. */
typedef struct {
	float x, y, width, height;
} uiManualRect_t;

uint32_t UIManual_MaxScroll(const uiManualPage_t *page, float zoom);
uiManualRect_t UIManual_Place(const uiManualPage_t *page, bool whole,
	uint32_t scroll, float zoom);

/* Memory guard: the viewer opens only with room for its two pages and this
 * much besides, and gives all of it back when it closes. */
#define UI_MANUAL_MEMORY_MARGIN (256u * 1024u)
bool UIManual_Fits(uint32_t freeBytes, const uiManualInfo_t *info);

/* What the viewer draws: the menu thread's copy, handed to the video thread
 * whenever it changes. */
typedef struct {
	const uint8_t *texels;	/* the shown page's record, NULL while it is read */
	uint32_t pageBytes;	/* the page's texture; its half follows */
	uint16_t height;	/* the page's texture is 640 x height */
	uint16_t pageWidth;
	uint16_t pageHeight;
	uint8_t format;
	bool whole;
	uint32_t scroll;
	float zoom;
	char counter[24];	/* "PAGE 3 / 24" */
	char message[64];	/* why there's no page: still reading, or damaged */
	char hints[96];
} uiManualView_t;

#endif
