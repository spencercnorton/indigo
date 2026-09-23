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
#define PROGRESS_BOX_TOPRIGHT 1

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

#define UI_GAMEFLOW_RENDER_SLOTS 7u
#define UI_GAMEFLOW_TITLE_LENGTH 96u
#define UI_GAMEFLOW_COMPANY_LENGTH 64u
#define UI_GAMEFLOW_FACTS_LENGTH 64u

#define UI_GAMEFLOW_CARD_VALID       (1u << 0)
#define UI_GAMEFLOW_CARD_HAS_BANNER  (1u << 1)
#define UI_GAMEFLOW_CARD_AUTOLOAD    (1u << 2)
#define UI_GAMEFLOW_CARD_HIDDEN      (1u << 3)
#define UI_GAMEFLOW_CARD_PARENT      (1u << 4)
#define UI_GAMEFLOW_CARD_FOLDER      (1u << 5)

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
	s8 relativeSlot;
	u8 flags;
	u8 reserved[10];
} uiGameflowCardSnapshot_t;

typedef struct {
	uiGameflowSelectionSnapshot_t selection;
	u32 recordCount;
	char deviceName[64];
	/* Keep records at offset 96 after the explicit snapTransition field. */
	u8 reserved[8];
	uiGameflowCardSnapshot_t records[UI_GAMEFLOW_RENDER_SLOTS];
} uiGameflowRenderSnapshot_t;

enum TextureId
{
	TEX_BACKDROP=0,
	TEX_SWISS,
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
uiDrawObj_t* DrawSettingsFocus(int x1, int y1, int x2, int y2);
uiDrawObj_t* DrawStyledLabel(int x, int y, const char *string, float size, int align, GXColor color);
uiDrawObj_t* DrawStyledLabelWithCaret(int x, int y, const char *string, float size, int align, GXColor color, int caretPosition);
uiDrawObj_t* DrawLabel(int x, int y, const char *string);
uiDrawObj_t* DrawFadingLabel(int x, int y, const char *string, float size);
uiDrawObj_t* DrawDynamicLabel(int x, int y, const char *(*getString)(void), float size, int align, GXColor color);
uiDrawObj_t* DrawHome(void);
uiDrawObj_t* DrawDeviceSelectorCard(DEVICEHANDLER_INTERFACE *device,
	bool destination, bool showAllDevices, bool inAdvanced);
uiDrawObj_t* DrawTooltip(const char *tooltip);
uiDrawObj_t* DrawTitleBar();
uiDrawObj_t* DrawGameflow(const uiGameflowRenderSnapshot_t *snapshot);
void DrawGameflowRequestPosters(DEVICEHANDLER_INTERFACE *device,
	const uiGameflowRenderSnapshot_t *snapshot);
bool DrawGameflowPollPosters(void);
void DrawGameflowCancelPosters(void);
void DrawUpdateProgressBar(uiDrawObj_t *evt, int percent);
void DrawUpdateProgressBarDetail(uiDrawObj_t *evt, int percent, int speed, int timestart, int timeremain);
void DrawUpdateProgressLoading(uiDrawObj_t *evt, int increment);
bool DrawUpdatePresentation(uiDrawObj_t *evt,
	const uiPresentationSnapshot_t *snapshot);
void DrawUpdateHome(const uiHomeState_t *state,
	uiHomeCapabilities_t capabilities, const char *sourceName);
void DrawUpdateFileBrowserButton(uiDrawObj_t *evt, int mode);
bool DrawUpdateGameflow(uiDrawObj_t *evt,
	const uiGameflowRenderSnapshot_t *snapshot);
bool DrawSetGameflowMode(uiDrawObj_t *evt, uiGameflowMode_t mode);
bool DrawUpdateGameflowDetail(uiDrawObj_t *evt,
	const uiGameflowDetailSnapshot_t *snapshot);
void DrawClearGameflowDetail(uiDrawObj_t *evt);
void DrawAddChild(uiDrawObj_t *parent, uiDrawObj_t *child);
uiDrawObj_t* DrawPublish(uiDrawObj_t *evt);
uiDrawObj_t* DrawRepublish(uiDrawObj_t *old, uiDrawObj_t *new);
void DrawDispose(uiDrawObj_t *evt);
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

#endif
