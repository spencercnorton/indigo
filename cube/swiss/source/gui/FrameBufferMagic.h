/* -----------------------------------------------------------
      FrameBufferMagic.h - Framebuffer routines with GX
	      - by emu_kidid & sepp256

      Version 1.0 11/11/2009
        - Initial Code
   ----------------------------------------------------------- */
   
#ifndef FRAMEBUFFERMAGIC_H
#define FRAMEBUFFERMAGIC_H

#include <gccore.h>
#include "deviceHandler.h"
#include "ui_gameflow_detail.h"
#include "ui_gameflow.h"
#include "ui_home.h"
#include "ui_presentation.h"
#include "ui_color.h"
#include "ui_settings_layout.h"

#define D_WARN  0
#define D_INFO  1
#define D_FAIL  2
#define D_PASS  3
#define B_NOSELECT 0
#define B_SELECTED 1

#define BUTTON_COLOUR_INNER 0x2088207C
#define BUTTON_COLOUR_OUTER COLOR_SILVER

#define PROGRESS_BOX_WIDTH  600
#define PROGRESS_BOX_HEIGHT 125
#define PROGRESS_BOX_BOTTOMLEFT 0
#define PROGRESS_BOX_TOPLEFT 1

#include "images_tpl.h"
#include "images.h"
#include "buttons_tpl.h"
#include "buttons.h"

typedef struct uiDrawObj {
    int type;
	void *data;
	struct uiDrawObj *child;
	bool disposed;
} uiDrawObj_t;

/* Enough records for the Grid layout's window, five rows of five
 * (UI_GAMEFLOW_LIBRARY_GRID_WINDOW); the carousels fill 7 of them. */
#define UI_GAMEFLOW_RENDER_SLOTS 25u
#define UI_GAMEFLOW_TITLE_LENGTH 96u
#define UI_GAMEFLOW_COMPANY_LENGTH 64u
#define UI_GAMEFLOW_FACTS_LENGTH 64u
/* Spotlight's description: from the card's descriptions file (up to 300
 * characters), else a disc banner's 128, which need not end in a NUL. */
#define UI_GAMEFLOW_DESCRIPTION_LENGTH 320u
/* ...shown in this many lines of Spotlight's column. */
#define UI_GAMEFLOW_DESCRIPTION_LINES 6u

#define UI_GAMEFLOW_CARD_VALID       (1u << 0)
#define UI_GAMEFLOW_CARD_HAS_BANNER  (1u << 1)
#define UI_GAMEFLOW_CARD_AUTOLOAD    (1u << 2)
#define UI_GAMEFLOW_CARD_HIDDEN      (1u << 3)
#define UI_GAMEFLOW_CARD_PARENT      (1u << 4)
#define UI_GAMEFLOW_CARD_FOLDER      (1u << 5)
#define UI_GAMEFLOW_CARD_CUSTOM      (1u << 6) /* the game has settings of its own */
/* An app (gui/apps.c), not a game: its poster is its own picture, and a
 * snapshot of apps is the Apps screen. */
#define UI_GAMEFLOW_CARD_APP         (1u << 7)

/* Pointer-free menu-thread record. Its fixed 6400-byte stride keeps every
 * inline RGB5A3 banner 32-byte aligned when the snapshot is memalign(32). */
typedef struct {
	u8 banner[BNR_PIXELDATA_LEN];
	char title[UI_GAMEFLOW_TITLE_LENGTH];
	char company[UI_GAMEFLOW_COMPANY_LENGTH];
	char facts[UI_GAMEFLOW_FACTS_LENGTH];
	char gameId[8];
	u64 size;
	u32 libraryIndex;
	/* Place on the ring, or the row in a grid (0 is the focused row). */
	s8 relativeSlot;
	u8 flags;
	u8 column;	/* grid only */
	u8 reserved[9];
} uiGameflowCardSnapshot_t;

typedef struct {
	uiGameflowSelectionSnapshot_t selection;
	u32 recordCount;
	char deviceName[64];
	u8 layout;	/* uiGameflowLayout_t */
	u8 columns;	/* the grid's row length, 0 for a ring */
	u8 reserved[6];
	/* Spotlight: the selected game's description. Keeps the records at
	 * offset 416, a multiple of 32. */
	char description[UI_GAMEFLOW_DESCRIPTION_LENGTH];
	uiGameflowCardSnapshot_t records[UI_GAMEFLOW_RENDER_SLOTS];
} uiGameflowRenderSnapshot_t;

enum TextureId
{
	TEX_SWISS=0,
	TEX_GCDVDSMALL,
	TEX_SDSMALL,
	TEX_HDD,
	TEX_QOOB,
	TEX_WODEIMG,
	TEX_BTNHILIGHT,
	TEX_BTNDEVICE,
	TEX_BTNSETTINGS,
	TEX_BTNINFO,
	TEX_BTNREFRESH,
	TEX_BTNEXIT,
	TEX_MEMCARD,
	TEX_WIIKEY,
	TEX_SYSTEM,
	TEX_USBGECKO,
	TEX_BBA,
	TEX_CHECKED,
	TEX_UNCHECKED,
	TEX_STAR,
	TEX_GCLOADER,
	TEX_M2LOADER,
	TEX_ETH2GC,
	TEX_FLIPPY,
	TEX_GCNET,
	TEX_GCODE,
	TEX_KUNAIGC
};

extern GXTexObj ntscjTexObj;
extern GXTexObj ntscuTexObj;
extern GXTexObj palTexObj;
extern GXTexObj dirimgTexObj;
extern GXTexObj dolimgTexObj;
extern GXTexObj dolcliimgTexObj;
extern GXTexObj elfimgTexObj;
extern GXTexObj fileimgTexObj;
extern GXTexObj fpkgimgTexObj;
extern GXTexObj gcmimgTexObj;
extern GXTexObj mp3imgTexObj;
extern GXTexObj tgcimgTexObj;

typedef struct kbBtn_ {
    int supportedEntryMode;
	char *val;
} kbBtn;

#define ENTRYMODE_ALPHA 	(1)
#define ENTRYMODE_NUMERIC 	(1<<1)
#define ENTRYMODE_IP	 	(1<<2)
#define ENTRYMODE_MASKED 	(1<<3)
#define ENTRYMODE_FILE	 	(1<<4)

uiDrawObj_t* DrawImage(int textureId, int x, int y, int width, int height, int depth, float s1, float s2, float t1, float t2, int centered);
uiDrawObj_t* DrawTexObj(GXTexObj *texObj, int x, int y, int width, int height, int depth, float s1, float s2, float t1, float t2, int centered);
uiDrawObj_t* DrawProgressBar(bool indeterminate, int percent, const char *message);
uiDrawObj_t* DrawProgressLoading(int miniModePos);
uiDrawObj_t* DrawContainer();
uiDrawObj_t* DrawMessageBox(int type, const char *message);
uiDrawObj_t* DrawPresentation(const uiPresentationSnapshot_t *snapshot);
uiDrawObj_t* DrawSelectableButton(int x1, int y1, int x2, int y2, const char *message, int mode);
uiDrawObj_t* DrawEmptyBox(int x1, int y1, int x2, int y2);
uiDrawObj_t* DrawEmptyColouredBox(int x1, int y1, int x2, int y2, GXColor colour);
uiDrawObj_t* DrawTransparentBox(int x1, int y1, int x2, int y2);
uiDrawObj_t* DrawStyledLabel(int x, int y, const char *string, float size, int align, GXColor color);
uiDrawObj_t* DrawHintLabel(int x, int y, const char *string, float size, int align, GXColor color);
/* A hint line's width at scale 1, button icons included. */
int GetHintSizeInPixels(const char *text);
float GetHintScaleToFitInWidthWithMax(const char *text, int width, float maximum);
uiDrawObj_t* DrawStyledLabelWithCaret(int x, int y, const char *string, float size, int align, GXColor color, int caretPosition);
uiDrawObj_t* DrawLabel(int x, int y, const char *string);
uiDrawObj_t* DrawFadingLabel(int x, int y, const char *string, float size);
uiDrawObj_t* DrawDynamicLabel(int x, int y, const char *(*getString)(void), float size, int align, GXColor color);
uiDrawObj_t* DrawHome(void);
/* The Source picker, and Copy/Move's destination: one event while it is
 * open. Each update copies what the picker shows of every listed device (its
 * name, picture, what it can do, where it plugs in and whether it was
 * detected), so the video thread never reads a device handler. travel is
 * the focus, unwrapped: a step changes it by one and the row slides that
 * way. */
uiDrawObj_t* DrawDeviceSelector(bool destination);
void DrawUpdateDeviceSelector(uiDrawObj_t *selector,
	DEVICEHANDLER_INTERFACE *const *listed, int count, int travel,
	const DEVICEHANDLER_INTERFACE *current,
	const DEVICEHANDLER_INTERFACE *settings, bool showAllDevices,
	bool inAdvanced);
uiDrawObj_t* DrawTooltip(const char *tooltip);
uiDrawObj_t* DrawTitleBar();
uiDrawObj_t* DrawGameflow(const uiGameflowRenderSnapshot_t *snapshot);
void DrawGameflowRequestPosters(DEVICEHANDLER_INTERFACE *device,
	const uiGameflowRenderSnapshot_t *snapshot);
bool DrawGameflowPollPosters(void);
void DrawGameflowCancelPosters(void);
/* Spotlight's description of a game, from swiss/ui/descriptions.txt on
 * device: read whole the first time it is asked for there, and again after
 * DrawGameflowCancelPosters. False, with out empty, when there is none.
 * Menu thread only. */
bool DrawGameflowDescription(DEVICEHANDLER_INTERFACE *device,
	const char *gameId, char *out, size_t capacity);
void DrawUpdateProgressBar(uiDrawObj_t *evt, int percent);
void DrawUpdateProgressBarDetail(uiDrawObj_t *evt, int percent, int speed, int timestart, int timeremain);
void DrawUpdateProgressLoading(uiDrawObj_t *evt, int increment);
bool DrawUpdatePresentation(uiDrawObj_t *evt,
	const uiPresentationSnapshot_t *snapshot);
/* What the UI calls a device: the disc drive is the "Game Disc", not
 * upstream's "DVD". */
const char *DeviceDisplayName(const DEVICEHANDLER_INTERFACE *device);
void DrawUpdateHome(const uiHomeState_t *state,
	uiHomeCapabilities_t capabilities, const char *sourceName);
void DrawUpdateFileBrowserButton(uiDrawObj_t *evt, int mode);
bool DrawUpdateGameflow(uiDrawObj_t *evt,
	const uiGameflowRenderSnapshot_t *snapshot);
bool DrawSetGameflowMode(uiDrawObj_t *evt, uiGameflowMode_t mode);
/* The Detail row the D-pad rests on. */
bool DrawSetGameflowDetailFocus(uiDrawObj_t *evt,
	uiGameflowDetailFocus_t focus);
/* While Launch mode is on, the launch screen shows message as its step and
 * this returns true; otherwise it returns false and the caller shows its own
 * box. DrawProgressBar sends it every looping bar's message. */
bool DrawLaunchStep(const char *message);
bool DrawUpdateGameflowDetail(uiDrawObj_t *evt,
	const uiGameflowDetailSnapshot_t *snapshot);
void DrawClearGameflowDetail(uiDrawObj_t *evt);
void DrawAddChild(uiDrawObj_t *parent, uiDrawObj_t *child);
/* Runs change on the menu thread with the video thread held off, so what a
 * frame draws never changes half way (Apps' posters). change must not
 * block, draw or call anything that takes the video lock. */
void DrawWithVideoLocked(void (*change)(void *context), void *context);
uiDrawObj_t* DrawPublish(uiDrawObj_t *evt);
uiDrawObj_t* DrawRepublish(uiDrawObj_t *old, uiDrawObj_t *new);
void DrawDispose(uiDrawObj_t *evt);
/* Settings: one page event for the whole session, drawn from a snapshot in
 * the cheat browser's language. Each update copies the snapshot and keeps
 * the screen in the colors it was built with (colors: the Menu, Backdrop and
 * Wave Color settings, one per UI_COLOR_LAYER_), in one step, until the page
 * is disposed. */
uiDrawObj_t* DrawSettingsPage(const uiSetPageSnapshot_t *snapshot);
void DrawUpdateSettingsPage(uiDrawObj_t *page,
	const uiSetPageSnapshot_t *snapshot, const int colors[UI_COLOR_LAYERS]);
/* The value list over the page. With previewLayer >= 0 the screen shows
 * previewColor, a value of that layer's setting, while the list has it
 * focused; the page's next update ends it. */
uiDrawObj_t* DrawSettingsList(const uiSetListSnapshot_t *snapshot);
void DrawUpdateSettingsList(uiDrawObj_t *list,
	const uiSetListSnapshot_t *snapshot, int previewLayer, int previewColor);
/* A row's help (its tooltip) as a card over the page. */
uiDrawObj_t* DrawSettingsHelp(const char *help);

/* Memory Cards (saves.c): one page event, drawn from this snapshot in the
 * cheat browser's language. A row carries its banner inline, and the rows
 * come first, so each banner and its palette stay 32-byte aligned in the
 * memalign(32) event, as the Library's cards do. */
#define UI_SAVES_PAGE_ROWS 6
#define UI_SAVES_PAGE_TABS 3

typedef enum {
	UI_SAVES_ROW_SAVE = 0,
	UI_SAVES_ROW_FOLDER,
	UI_SAVES_ROW_PARENT
} uiSavesRowKind_t;

typedef struct {
	/* RGB5A3, or CI8 then its 256-color palette */
	u8 banner[CARD_BANNER_W * CARD_BANNER_H * 2];
	char title[64];
	char blocks[24];
	u8 bannerFormat;	/* CARD_BANNER_NONE, _CI or _RGB */
	u8 kind;		/* uiSavesRowKind_t */
	u8 reserved[6];
} uiSavesPageRow_t;

typedef struct {
	uiSavesPageRow_t rows[UI_SAVES_PAGE_ROWS];
	char title[32];
	char tabs[UI_SAVES_PAGE_TABS][16];
	char status[48];
	char section[96];
	char position[16];
	char detail[96];
	char empty[2][96];
	char hint[2][64];	/* left, in ink, and right */
	s32 count;
	s32 first;
	s8 tabCount;		/* 0: the folder chooser, no tabs */
	s8 tab;
	s8 rowCount;
	s8 focusRow;
	u8 warning;		/* detail is a warning */
	u8 reserved[3];
	u32 list;		/* changes with the list shown: the focus snaps */
} uiSavesPageSnapshot_t;

uiDrawObj_t* DrawSavesPage(const uiSavesPageSnapshot_t *snapshot);
void DrawUpdateSavesPage(uiDrawObj_t *page,
	const uiSavesPageSnapshot_t *snapshot);
uiDrawObj_t* DrawFileBrowserButton(int x1, int y1, int x2, int y2, const char *message, file_handle *file, int mode);
uiDrawObj_t* DrawFileBrowserButtonMeta(int x1, int y1, int x2, int y2, const char *message, file_handle *file, int mode);
uiDrawObj_t* DrawFileCarouselEntry(int x1, int y1, int x2, int y2, const char *message, file_handle *file, int distFromMiddle);
uiDrawObj_t* DrawVertScrollBar(int x, int y, int width, int height, float scrollPercent, int scrollHeight);
void DrawArgsSelector(const char *fileName);
void DrawCheatsSelector(const char *fileName);
void DrawGetTextEntry(int entryMode, const char *label, void *src, int size);
void DrawInit(GXRModeObj *videoMode, bool black);
void DrawLoadBackdrop(DEVICEHANDLER_INTERFACE *device);
void DrawShutdown();
void DrawVideoMode(GXRModeObj *videoMode);
void DrawVideoModeDefer(bool defer);

#endif
