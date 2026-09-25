/* -----------------------------------------------------------
      FrameBufferMagic.c - Framebuffer routines with GX
	      - by emu_kidid & sepp256

      Version 1.0 11/11/2009
        - Initial Code
   ----------------------------------------------------------- */

#include <fnmatch.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <gccore.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>
#include <ogc/exi.h>
#include <gctypes.h>
#include <ogc/lwp_watchdog.h>
#include "deviceHandler.h"
#include "FrameBufferMagic.h"
#include "IPLFontWrite.h"
#include "filemeta.h"
#include "swiss.h"
#include "main.h"
#include "util.h"
#include "ata.h"
#include "btns.h"
#include "dolparameters.h"
#include "cheats.h"
#include "indigo_background.h"
#include "ui_anim.h"
#include "ui_clock.h"
#include "ui_color.h"
#include "ui_perf.h"
#include "ui_scene.h"
#include "ui_hint.h"
#include "ui_assets.h"
#include "ui_command_rail.h"
#include "ui_home_layout.h"
#include "ui_home_text.h"
#include "ui_settings_focus.h"
#include "ui_gameflow_detail.h"
#include "ui_gameflow_library.h"
#include "ui_cheats.h"

#define GUI_MSGBOX_ALPHA 225
#define GUI_PANEL_ALPHA 150	// Phase 2: translucent content panels (config-gated; dialogs stay at GUI_MSGBOX_ALPHA)
#define SETTINGS_FOCUS_CONTINUITY_FRAMES 8u

TPLFile imagesTPL;
TPLFile buttonsTPL;
TPLFile backdropTPL;
GXTexObj backdropTexObj;
GXTlutObj backdropTlutObj;
GXTexObj backdropIndTexObj;
GXTexObj bannerMaskTexObj;
GXTexObj swissTexObj;
GXTexObj gcdvdsmallTexObj;
GXTexObj sdsmallTexObj;
GXTlutObj sdsmallTlutObj;
GXTexObj hddTexObj;
GXTlutObj hddTlutObj;
GXTexObj qoobTexObj;
GXTlutObj qoobTlutObj;
GXTexObj qoobIndTexObj;
GXTexObj wodeimgTexObj;
GXTexObj usbgeckoTexObj;
GXTlutObj usbgeckoTlutObj;
GXTexObj memcardTexObj;
GXTlutObj memcardTlutObj;
GXTexObj memcardIndTexObj;
GXTexObj bbaTexObj;
GXTexObj wiikeyTexObj;
GXTexObj systemTexObj;
GXTexObj btnhilightTexObj;
GXTexObj btndeviceTexObj;
GXTexObj btnsettingsTexObj;
GXTexObj btninfoTexObj;
GXTexObj btnrefreshTexObj;
GXTexObj btnexitTexObj;
GXTexObj boxinnerTexObj;
GXTexObj boxouterTexObj;
GXTexObj ntscjTexObj;
GXTexObj ntscuTexObj;
GXTexObj palTexObj;
GXTexObj checkedTexObj;
GXTexObj uncheckedTexObj;
GXTexObj loadingTexObj;
GXTexObj starTexObj;
GXTexObj dirimgTexObj;
GXTexObj dolimgTexObj;
GXTexObj dolcliimgTexObj;
GXTexObj elfimgTexObj;
GXTexObj fileimgTexObj;
GXTexObj fpkgimgTexObj;
GXTexObj gcmimgTexObj;
GXTexObj mp3imgTexObj;
GXTexObj tgcimgTexObj;
GXTexObj gcloaderTexObj;
GXTexObj m2loaderTexObj;
GXTexObj eth2gcTexObj;
GXTexObj flippyTexObj;
GXTexObj gcnetTexObj;
GXTexObj kunaigcTexObj;

static char fbTextBuffer[256];

// Video threading vars
#define VIDEO_STACK_SIZE (64*1024)
#define VIDEO_PRIORITY LWP_PRIO_HIGHEST
static char  video_thread_stack[VIDEO_STACK_SIZE] ATTRIBUTE_ALIGN (8);
static lwp_t video_thread = LWP_THREAD_NULL;
static mutex_t _videomutex = LWP_MUTEX_NULL;
static bool sceneRenderingEnabled;
static u32 videoFrameSerial;
/* While a Settings page is up, the screen keeps the Menu Color that page was
 * drawn with (DrawPinMenuColor); disposing the page lets it go. While the
 * Menu Color list is open, the color it has focused shows instead. */
static uiDrawObj_t *menuColorPage;
static int menuColorPinned = -1;
static int menuColorPreview = -1;

typedef struct {
	uiClockFrame_t clock;
	time_t sampledSecond;
	int hour;
	int minute;
	int second;
	char timeText[9];
	char temperatureText[8];
	s8 coreTemperature;
	bool civilSecondSampled;
	bool civilTimeAvailable;
	bool temperatureSampled;
} uiSystemInstrument_t;

static uiSystemInstrument_t systemInstrument;
static void _UpdateSystemInstrument(void);
static indigoPadFrame_t padInstrument;
static u32 settingsFocusLastDrawFrame;
static uiSettingsFocusState_t settingsFocusState;
static file_handle posterPackFile;
static DEVICEHANDLER_INTERFACE *posterPackDevice;
static bool posterPackAttempted;
static bool posterPackFileOwned;
static bool gameflowResetRegistered;
static s32 _GameflowOnReset(s32 final);
static sys_resetinfo gameflowResetInfo = {
	{NULL, NULL}, _GameflowOnReset, 0
};

static uiMotionMode_t _CurrentMotionMode(void);

enum VideoEventType
{
	EV_TEXOBJ = 0,
	EV_MSGBOX,
	EV_IMAGE,
	EV_BACKGROUND,
	EV_PROGRESS,
	EV_SELECTABLEBUTTON,
	EV_EMPTYBOX,
	EV_TRANSPARENTBOX,
	EV_FILEBROWSERBUTTON,
	EV_VERTSCROLLBAR,
	EV_STYLEDLABEL,
	EV_CONTAINER,
	EV_HOME,
	EV_DEVICESELECTOR,
	EV_TOOLTIP,
	EV_TITLEBAR,
	EV_GAMEFLOW,
	EV_PRESENTATION,
	EV_SETTINGSFOCUS,
	EV_CHEATS
};

char * typeStrings[] = {"TexObj", "MsgBox", "Image", "Background", "Progress", "SelectableButton", "EmptyBox", "TransparentBox",
						"FileBrowserButton", "VertScrollbar", "StyledLabel", "Container", "Home", "DeviceSelector", "Tooltip", "TitleBar", "Gameflow", "Presentation", "SettingsFocus", "Cheats"};

typedef struct drawTexObjEvent {
	GXTexObj *texObj;
	int x;
	int y;
	int width;
	int height;
	int depth;
	float s1;
	float s2;
	float t1;
	float t2;
} drawTexObjEvent_t;

typedef struct drawVertScrollbarEvent {
	int x;
	int y;
	int width;
	int height;
	float scrollPercent;
	int scrollHeight;
} drawVertScrollbarEvent_t;

typedef struct drawImageEvent {
	int textureId;
	int x;
	int y;
	int width;
	int height;
	int depth;
	float s1;
	float s2;
	float t1;
	float t2;
} drawImageEvent_t;

typedef struct drawStyledLabelEvent {
	int x;
	int y;
	const char *(*getString)(void);
	char *string;
	float size;
	int align;
	GXColor color;
	int fadingDirection;
	bool showCaret;
	int caretPosition;
	GXColor caretColor;
	bool hint;	/* draw button names as icons */
} drawStyledLabelEvent_t;

typedef struct drawSelectableButtonEvent {
	int x1;
	int y1;
	int x2;
	int y2;
	int mode;
	char *msg;
} drawSelectableButtonEvent_t;

typedef struct drawBoxEvent {
	int x1;
	int y1;
	int x2;
	int y2;
	GXColor backfill;
} drawBoxEvent_t;

typedef struct drawSettingsFocusEvent {
	uiSetLayoutRect_t target;
} drawSettingsFocusEvent_t;

typedef struct drawFileBrowserButtonEvent {
	int x1;
	int y1;
	int x2;
	int y2;
	char *displayName;
	file_handle *file;
	int mode;
	int alpha;
	bool isAutoLoadEntry;
	bool isCarousel;	// Draw this as a full "card" style
	int distFromMiddle;	// 0 = full, -1 spine only but large then gradually getting smaller as dist increases from 0
} drawFileBrowserButtonEvent_t;

typedef struct drawHomeEvent {
	uiHomeState_t state;
	uiHomeCapabilities_t capabilities;
	uiHomeLayout_t layout;
	bool visible;
	bool layoutValid;
	char sourceName[64];
	char heading[96];
	char command[96];
	float focusScale[UI_HOME_FACE_COUNT];
	float rowSelectedScale[2];
	float rowIdleScale[2];
	float headingScale;
	float commandScale;
	float contextCommandScale;
	float confirmCommandScale;
	float consequenceScale;
} drawHomeEvent_t;

static const char homeContextCommand[] =
	"D-PAD  SELECT    A  OPEN    B  BACK";
static const char homeConfirmCommand[] =
	"\213  \233  CHOOSE    A  SELECT    B  CANCEL";
static const char homeRestartConsequence[] =
	"RELOADS INDIGO AND ENDS THIS SESSION";

typedef struct drawDeviceSelectorEvent {
	DEVICEHANDLER_INTERFACE *device;
	bool destination;
	bool showAllDevices;
	bool inAdvanced;
	bool exiSpeed;
	bool available;
	char capability[64];
	char actionHint[64];
	char auxiliaryHint[48];
	float deviceNameScale;
	float capabilityScale;
	float actionScale;
	float auxiliaryScale;
} drawDeviceSelectorEvent_t;

typedef struct drawTooltipEvent {
	char *tooltip;
} drawTooltipEvent_t;

typedef struct drawGameflowDetailPresentation {
	float titleScale;
	float companyScale;
	float factsScale;
	float statusScale;
	float lastPlayedScale;
	float saveStatusScale;
	float cheatSummaryScale;
	float cheatPreviewScale;
	float launchScale;
	float primaryActionsScale;
	float advancedLineOneScale;
	float advancedLineTwoScale;
	GXColor accent;
} drawGameflowDetailPresentation_t;

typedef struct drawGameflowCardPresentation {
	float titleScale;
	float companyScale;
	float factsScale;
} drawGameflowCardPresentation_t;

typedef struct drawGameflowEvent {
	uiGameflowRenderSnapshot_t snapshot;
	uiGameflowState_t state;
	GXTexObj bannerTexObj[UI_GAMEFLOW_RENDER_SLOTS];
	uiGameflowDetailSnapshot_t detail;
	drawGameflowDetailPresentation_t detailPresentation;
	drawGameflowCardPresentation_t
		cardPresentation[UI_GAMEFLOW_RENDER_SLOTS];
	GXTexObj detailBannerTexObj;
} drawGameflowEvent_t;

typedef struct drawPresentationEvent {
	uiPresentationSnapshot_t snapshot;
	float titleScale;
	float messageScale;
	float detailScale;
	float actionScale;
} drawPresentationEvent_t;

_Static_assert(sizeof(uiGameflowCardSnapshot_t) % 32u == 0u,
	"Gameflow record stride must preserve banner alignment");
_Static_assert(offsetof(uiGameflowRenderSnapshot_t, records) % 32u == 0u,
	"Gameflow records must begin at a cache-line boundary");
_Static_assert(offsetof(drawGameflowEvent_t, detail) % 32u == 0u,
	"Gameflow detail snapshot must begin at a cache-line boundary");
_Static_assert(offsetof(uiGameflowDetailSnapshot_t, banner) % 32u == 0u,
	"Gameflow detail banner must begin at a cache-line boundary");

typedef struct drawMsgBoxEvent {
	int type;
} drawMsgBoxEvent_t;

typedef struct drawProgressEvent {
	bool indeterminate;
	bool miniMode;
	int miniModePos;
	int miniModeAlpha;
	int percent;
	int speed;	// in bytes
	int timestart;
	int timeremain;
} drawProgressEvent_t;

typedef struct uiDrawObjQueue {
	struct uiDrawObj *event;
	struct uiDrawObjQueue *next;
} uiDrawObjQueue_t;

static uiDrawObjQueue_t *videoEventQueue = NULL;
static uiDrawObj_t *buttonPanel = NULL;

static void drawInit(void);
static void _DrawHintText(int x, int y, const char *text, float scale, int align,
	GXColor color);
static void _DrawSimpleBox(int x, int y, int width, int height, int depth,
	GXColor fillColor, GXColor borderColor);

#if UI_PERF_CAPTURE
static void _DrawPerfOverlay(void)
{
	static char summary[128] = "PERF capture warming up";
	static u32 framesUntilRefresh = 1;
	uiPerfSnapshot_t snapshot;
	u64 workP99;
	u64 backgroundP99;
	u64 periodP99;

	if(--framesUntilRefresh == 0) {
		UIPerf_Snapshot(&snapshot);
		workP99 = UIPerf_PercentileUs(&snapshot.metrics[UI_PERF_METRIC_FRAME_WORK], 99);
		backgroundP99 = UIPerf_PercentileUs(
			&snapshot.metrics[UI_PERF_METRIC_BACKGROUND_CPU_SUBMIT], 99);
		periodP99 = UIPerf_PercentileUs(&snapshot.metrics[UI_PERF_METRIC_FRAME_PERIOD], 99);
		snprintf(summary, sizeof(summary),
			"PERF p99 work %llu.%02llums  bg %llu.%02llums  cadence %llu.%02llums  drops %llu",
			(unsigned long long)(workP99 / 1000),
			(unsigned long long)((workP99 % 1000) / 10),
			(unsigned long long)(backgroundP99 / 1000),
			(unsigned long long)((backgroundP99 % 1000) / 10),
			(unsigned long long)(periodP99 / 1000),
			(unsigned long long)((periodP99 % 1000) / 10),
			(unsigned long long)snapshot.metrics[UI_PERF_METRIC_FRAME_PERIOD].thresholdExceedances);
		framesUntilRefresh = 60;
	}

	drawInit();
	_DrawSimpleBox(118, 82, 404, 22, 0,
		(GXColor) {7, 6, 24, 218}, (GXColor) {135, 124, 209, 150});
	drawString(320, 94, summary, 0.42f, ALIGN_CENTER, defaultColor);
}
#endif

// Add root level uiDrawObj_t
static uiDrawObj_t* addVideoEvent(uiDrawObj_t *event) {
	// First entry, make it root
	if(videoEventQueue == NULL) {
		videoEventQueue = calloc(1, sizeof(uiDrawObjQueue_t));
		videoEventQueue->event = event;
		//print_debug("Added first event %08X (type %s)\n", (u32)videoEventQueue, typeStrings[event->type]);
		return event;
	}
	
	uiDrawObjQueue_t *current = videoEventQueue;
    while (current->next != NULL) {
        current = current->next;
    }
    current->next = calloc(1, sizeof(uiDrawObjQueue_t));
	current->next->event = event;
	event->disposed = false;
	//print_debug("Added a new event %08X (type %s)\n", (u32)event, typeStrings[event->type]);
	return event;
}

static void clearNestedEvent(uiDrawObj_t *event) {
	if(event && !event->disposed) {
		print_debug("Event was not disposed!!\n");
		print_debug("Event %08X (type %s)\n", (u32)event, typeStrings[event->type]);
	}
	
	if(event->child && event->child != event) {
		clearNestedEvent(event->child);
	}
	//print_debug("Dispose nested event %08X\n", (u32)event);
	if(event && event->data) {
		// Free any attached data
		if(event->type == EV_STYLEDLABEL) {
			if(((drawStyledLabelEvent_t*)event->data)->string) {
				//print_debug("Clear Nested EV_STYLEDLABEL\n");
				free(((drawStyledLabelEvent_t*)event->data)->string);
			}
		}
		else if(event->type == EV_FILEBROWSERBUTTON) {
			if(((drawFileBrowserButtonEvent_t*)event->data)->displayName) {
				//print_debug("Clear Nested EV_FILEBROWSERBUTTON\n");
				free(((drawFileBrowserButtonEvent_t*)event->data)->displayName);
			}
			if(((drawFileBrowserButtonEvent_t*)event->data)->file) {
				if(((drawFileBrowserButtonEvent_t*)event->data)->file->meta) {
					if(((drawFileBrowserButtonEvent_t*)event->data)->file->meta->banner) {
						free(((drawFileBrowserButtonEvent_t*)event->data)->file->meta->banner);
					}
					free(((drawFileBrowserButtonEvent_t*)event->data)->file->meta);
				}
				free(((drawFileBrowserButtonEvent_t*)event->data)->file);
			}
		}
		else if(event->type == EV_SELECTABLEBUTTON) {
			if(((drawSelectableButtonEvent_t*)event->data)->msg) {
				//print_debug("Clear Nested EV_SELECTABLEBUTTON\n");
				free(((drawSelectableButtonEvent_t*)event->data)->msg);
			}
		}
		else if(event->type == EV_TOOLTIP) {
			if(((drawTooltipEvent_t*)event->data)->tooltip) {
				//print_debug("Clear Nested EV_TOOLTIP\n");
				free(((drawTooltipEvent_t*)event->data)->tooltip);
			}
		}
		//print_debug("Clear Nested event->data\n");
		free(event->data);
	}
	if(event) {
		//print_debug("Clear event\n");
		memset(event, 0, sizeof(uiDrawObj_t));
		free(event);
	}
}

static void disposeEvent(uiDrawObj_t *event) {
	if(videoEventQueue == NULL) {
		return;
	}

	// See if this is in our root event queue
	uiDrawObjQueue_t *current = videoEventQueue->next;
	uiDrawObjQueue_t *previous = videoEventQueue;
	// First node is what we're after.
	while (current != NULL) {
		if(current->event == event) {
			//print_debug("Disposing event %08X\n", (u32)current);
			clearNestedEvent(current->event);
			previous->next = current->next;
			free(current);
		}
		else {
			previous = current;
		}
		current = previous->next;
	}
}


static void init_textures() 
{
	TPL_OpenTPLFromMemory(&imagesTPL, (void *)images_tpl, images_tpl_size);
	TPL_OpenTPLFromMemory(&buttonsTPL, (void *)buttons_tpl, buttons_tpl_size);
	TPL_GetTextureCI(&imagesTPL, backdrop, &backdropTexObj, &backdropTlutObj, GX_TLUT0);
	GX_InitTexObjUserData(&backdropTexObj, &backdropTlutObj);
	TPL_GetTexture(&imagesTPL, backdrop_ind, &backdropIndTexObj);
	GX_InitTexObjUserData(&backdropIndTexObj, &backdropTexObj);
	TPL_GetTexture(&imagesTPL, banner_mask, &bannerMaskTexObj);
	TPL_GetTexture(&imagesTPL, swissimg, &swissTexObj);
	TPL_GetTexture(&imagesTPL, gcdvdsmall, &gcdvdsmallTexObj);
	TPL_GetTextureCI(&imagesTPL, sdsmall, &sdsmallTexObj, &sdsmallTlutObj, GX_TLUT0);
	GX_InitTexObjUserData(&sdsmallTexObj, &sdsmallTlutObj);
	TPL_GetTextureCI(&imagesTPL, hddimg, &hddTexObj, &hddTlutObj, GX_TLUT0);
	GX_InitTexObjUserData(&hddTexObj, &hddTlutObj);
	TPL_GetTextureCI(&imagesTPL, qoobimg, &qoobTexObj, &qoobTlutObj, GX_TLUT0);
	GX_InitTexObjUserData(&qoobTexObj, &qoobTlutObj);
	TPL_GetTexture(&imagesTPL, qoobimg_ind, &qoobIndTexObj);
	TPL_GetTexture(&imagesTPL, wodeimg, &wodeimgTexObj);
	TPL_GetTexture(&imagesTPL, wiikeyimg, &wiikeyTexObj);
	TPL_GetTexture(&imagesTPL, systemimg, &systemTexObj);
	TPL_GetTextureCI(&imagesTPL, memcardimg, &memcardTexObj, &memcardTlutObj, GX_TLUT0);
	GX_InitTexObjUserData(&memcardTexObj, &memcardTlutObj);
	TPL_GetTexture(&imagesTPL, memcardimg_ind, &memcardIndTexObj);
	TPL_GetTextureCI(&imagesTPL, usbgeckoimg, &usbgeckoTexObj, &usbgeckoTlutObj, GX_TLUT0);
	GX_InitTexObjUserData(&usbgeckoTexObj, &usbgeckoTlutObj);
	TPL_GetTexture(&imagesTPL, bbaimg, &bbaTexObj);
	TPL_GetTexture(&buttonsTPL, btnhilight, &btnhilightTexObj);
	TPL_GetTexture(&buttonsTPL, btndevice, &btndeviceTexObj);
	TPL_GetTexture(&buttonsTPL, btnsettings, &btnsettingsTexObj);
	TPL_GetTexture(&buttonsTPL, btninfo, &btninfoTexObj);
	TPL_GetTexture(&buttonsTPL, btnrefresh, &btnrefreshTexObj);
	TPL_GetTexture(&buttonsTPL, btnexit, &btnexitTexObj);
	TPL_GetTexture(&buttonsTPL, boxinner, &boxinnerTexObj);
	TPL_GetTexture(&buttonsTPL, boxouter, &boxouterTexObj);
	TPL_GetTexture(&imagesTPL, ntscjimg, &ntscjTexObj);
	TPL_GetTexture(&imagesTPL, ntscuimg, &ntscuTexObj);
	TPL_GetTexture(&imagesTPL, palimg, &palTexObj);
	TPL_GetTexture(&buttonsTPL, checked_32, &checkedTexObj);
	TPL_GetTexture(&buttonsTPL, unchecked_32, &uncheckedTexObj);
	TPL_GetTexture(&buttonsTPL, loading_16, &loadingTexObj);
	TPL_GetTexture(&buttonsTPL, star_16, &starTexObj);
	TPL_GetTexture(&imagesTPL, dirimg, &dirimgTexObj);
	TPL_GetTexture(&imagesTPL, dolimg, &dolimgTexObj);
	TPL_GetTexture(&imagesTPL, dolcliimg, &dolcliimgTexObj);
	TPL_GetTexture(&imagesTPL, elfimg, &elfimgTexObj);
	TPL_GetTexture(&imagesTPL, fileimg, &fileimgTexObj);
	TPL_GetTexture(&imagesTPL, fpkgimg, &fpkgimgTexObj);
	TPL_GetTexture(&imagesTPL, gcmimg, &gcmimgTexObj);
	TPL_GetTexture(&imagesTPL, mp3img, &mp3imgTexObj);
	TPL_GetTexture(&imagesTPL, tgcimg, &tgcimgTexObj);
	TPL_GetTexture(&imagesTPL, gcloaderimg, &gcloaderTexObj);
	TPL_GetTexture(&imagesTPL, m2loaderimg, &m2loaderTexObj);
	TPL_GetTexture(&imagesTPL, eth2gcimg, &eth2gcTexObj);
	TPL_GetTexture(&imagesTPL, flippyimg, &flippyTexObj);
	TPL_GetTexture(&imagesTPL, gcnetimg, &gcnetTexObj);
	TPL_GetTexture(&imagesTPL, kunaigcimg, &kunaigcTexObj);
}

static void drawInit()
{
	Mtx44 GXprojection2D;
	Mtx GXmodelView2D;

	// Reset various parameters from gfx plugin
	GX_SetCoPlanar(GX_DISABLE);
	GX_SetClipMode(GX_CLIP_ENABLE);
	GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);

	guMtxIdentity(GXmodelView2D);
	GX_LoadTexMtxImm(GXmodelView2D,GX_TEXMTX0,GX_MTX2x4);
	GX_LoadPosMtxImm(GXmodelView2D,GX_PNMTX0);
	guOrtho(GXprojection2D, 0, 480, 0, 640, 0, 1);
	GX_LoadProjectionMtx(GXprojection2D, GX_ORTHOGRAPHIC);

	GX_SetZMode(GX_DISABLE,GX_ALWAYS,GX_FALSE);

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_PNMTXIDX, GX_PNMTX0);
	GX_SetVtxDesc(GX_VA_TEX0MTXIDX, GX_TEXMTX0);
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	//set vertex attribute formats here
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	//enable textures
	GX_SetNumChans (1);
	GX_SetNumTexGens (1);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_DISABLE, 0, 0);

	GX_SetNumIndStages (0);
	GX_SetNumTevStages (2);
	GX_SetTevOrder (GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetTevColorIn (GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_RASC, GX_CC_ZERO);
	GX_SetTevColorOp (GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	GX_SetTevAlphaIn (GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_TEXA, GX_CA_RASA, GX_CA_ZERO);
	GX_SetTevAlphaOp (GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	GX_SetTevDirect (GX_TEVSTAGE0);
	GX_SetTevOrder (GX_TEVSTAGE1, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetTevColorIn (GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_CPREV, GX_CC_RASA, GX_CC_ZERO);
	GX_SetTevColorOp (GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	GX_SetTevAlphaIn (GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_APREV);
	GX_SetTevAlphaOp (GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	GX_SetTevDirect (GX_TEVSTAGE1);

	//set blend mode
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_INVSRCALPHA, GX_LO_CLEAR); //Fix src alpha
	GX_SetColorUpdate(GX_ENABLE);
//	GX_SetAlphaUpdate(GX_ENABLE);
//	GX_SetDstAlpha(GX_DISABLE, 0xFF);
	//set cull mode
	GX_SetCullMode (GX_CULL_NONE);
}

static void _drawRect(int x, int y, int width, int height, int depth, GXColor color, float s0, float s1, float t0, float t1)
{
	UIColor_Apply(&color.r, &color.g, &color.b);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		GX_Position3f32((float) x,(float) y,(float) depth );
		GX_Color4u8(color.r, color.g, color.b, color.a);
		GX_TexCoord2f32(s0,t0);
		GX_Position3f32((float) (x+width),(float) y,(float) depth );
		GX_Color4u8(color.r, color.g, color.b, color.a);
		GX_TexCoord2f32(s1,t0);
		GX_Position3f32((float) (x+width),(float) (y+height),(float) depth );
		GX_Color4u8(color.r, color.g, color.b, color.a);
		GX_TexCoord2f32(s1,t1);
		GX_Position3f32((float) x,(float) (y+height),(float) depth );
		GX_Color4u8(color.r, color.g, color.b, color.a);
		GX_TexCoord2f32(s0,t1);
	GX_End();
}

static void _putFlatVertex(float x, float y, GXColor color)
{
	UIColor_Apply(&color.r, &color.g, &color.b);
	GX_Position3f32(x, y, 0.0f);
	GX_Color4u8(color.r, color.g, color.b, color.a);
	GX_TexCoord2f32(0.0f, 0.0f);
}

static void _putFlatRect(float x, float y, float width, float height, GXColor color)
{
	_putFlatVertex(x, y, color);
	_putFlatVertex(x + width, y, color);
	_putFlatVertex(x + width, y + height, color);
	_putFlatVertex(x, y + height, color);
}

static void _DrawSimpleBox(int x, int y, int width, int height, int depth, GXColor fillColor, GXColor borderColor) 
{
	//Adjust for blank texture border
	x-=4; y-=4; width+=8; height+=8;
	
	GX_InvalidateTexAll();
	GX_LoadTexObj(&boxinnerTexObj, GX_TEXMAP0);

	_drawRect(x, y, width/2, height/2, depth, fillColor, 0.0f, ((float)width/32), 0.0f, ((float)height/32));
	_drawRect(x+(width/2), y, width/2, height/2, depth, fillColor, ((float)width/32), 0.0f, 0.0f, ((float)height/32));
	_drawRect(x, y+(height/2), width/2, height/2, depth, fillColor, 0.0f, ((float)width/32), ((float)height/32), 0.0f);
	_drawRect(x+(width/2), y+(height/2), width/2, height/2, depth, fillColor, ((float)width/32), 0.0f, ((float)height/32), 0.0f);

	GX_InvalidateTexAll();
	GX_LoadTexObj(&boxouterTexObj, GX_TEXMAP0);

	_drawRect(x, y, width/2, height/2, depth, borderColor, 0.0f, ((float)width/32), 0.0f, ((float)height/32));
	_drawRect(x+(width/2), y, width/2, height/2, depth, borderColor, ((float)width/32), 0.0f, 0.0f, ((float)height/32));
	_drawRect(x, y+(height/2), width/2, height/2, depth, borderColor, 0.0f, ((float)width/32), ((float)height/32), 0.0f);
	_drawRect(x+(width/2), y+(height/2), width/2, height/2, depth, borderColor, ((float)width/32), 0.0f, ((float)height/32), 0.0f);
}

// Internal
static void _DrawImageNow(int textureId, int x, int y, int width, int height, int depth,
		float s1, float s2, float t1, float t2, int centered, u8 opacity) {
	u16 ss = 0, ts = 0;
	GXTexObj *texObj = NULL;
	GXTexObj *indTexObj = NULL;
	GXColor color = (GXColor) {255,255,255,255};
	
	switch(textureId)
	{
		case TEX_BACKDROP:
			switch(GX_GetTexObjFmt(&backdropTexObj)) {
				case GX_TF_CI4:
				case GX_TF_CI8:
				case GX_TF_CI14:
					if(GX_GetTlutObjFmt(&backdropTlutObj) != GX_TL_IA8) {
						texObj = &backdropTexObj;
						break;
					}
				case GX_TF_IA4:
				case GX_TF_IA8:
					GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXA, GX_CC_TEXC, GX_CC_RASC, GX_CC_ZERO);
					GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
					
					texObj = &backdropTexObj; color = (GXColor) {0,0,255,255};
					break;
				default:
					texObj = &backdropTexObj;
					break;
			}
			if(GX_GetTexObjUserData(&backdropIndTexObj) == texObj) {
				indTexObj = &backdropIndTexObj;
				ss = 640; ts = 480;
			}
			// Phase 2: subtle GameCube-menu-style backdrop drift. Runs once per frame
			// on the vsync-locked video thread (no timer needed — same pattern as the
			// Phase 1 highlight tween). Config-gated; when off, the original full-frame
			// coords pass through unchanged => byte-identical to stock.
			if(!swissSettings.disableUIAnimations &&
				!swissSettings.reduceUIAnimations &&
				!swissSettings.disableAnimatedBackdrop) {
				float bgPhase = UIAnim_Seconds() * 0.72f;
				const float M = 0.03f;           // inset margin keeps texcoords in [0,1]: no edge smear, wrap-agnostic
				float dx = M * sinf(bgPhase);
				float dy = M * cosf(bgPhase * 0.9f);
				s1 = M + dx; s2 = (1.0f - M) + dx;
				t1 = M + dy; t2 = (1.0f - M) + dy;
			}
			break;
		case TEX_SWISS:
			texObj = &swissTexObj;
			break;
		case TEX_GCDVDSMALL:
			texObj = &gcdvdsmallTexObj;
			break;
		case TEX_SDSMALL:
			texObj = &sdsmallTexObj;
			break;
		case TEX_HDD:
			texObj = &hddTexObj;
			break;
		case TEX_QOOB:
			texObj = &qoobTexObj;
			indTexObj = &qoobIndTexObj;
			ss = 96; ts = 102;
			break;
		case TEX_WODEIMG:
			texObj = &wodeimgTexObj; color = (GXColor) {216,216,216,255};
			break;
		case TEX_USBGECKO:
			texObj = &usbgeckoTexObj;
			break;
		case TEX_WIIKEY:
			texObj = &wiikeyTexObj; color = (GXColor) {216,216,216,255};
			break;
		case TEX_SYSTEM:
			texObj = &systemTexObj;
			break;
		case TEX_MEMCARD:
			texObj = &memcardTexObj;
			indTexObj = &memcardIndTexObj;
			ss = 80; ts = 92;
			break;
		case TEX_BBA:
			texObj = &bbaTexObj;
			break;
		case TEX_BTNHILIGHT:
			GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_RASC, GX_CC_ZERO);
			GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
			
			texObj = &btnhilightTexObj; color = (GXColor) {127,134,255,255};
			break;
		case TEX_BTNDEVICE:
			texObj = &btndeviceTexObj;
			break;
		case TEX_BTNSETTINGS:
			texObj = &btnsettingsTexObj;
			break;
		case TEX_BTNINFO:
			texObj = &btninfoTexObj;
			break;
		case TEX_BTNREFRESH:
			texObj = &btnrefreshTexObj;
			break;
		case TEX_BTNEXIT:
			texObj = &btnexitTexObj;
			break;
		case TEX_CHECKED:
			texObj = &checkedTexObj; color = (GXColor) {0,128,0,255};
			break;
		case TEX_UNCHECKED:
			texObj = &uncheckedTexObj; color = (GXColor) {87,87,87,255};
			ss = 32; ts = 32;
			break;
		case TEX_STAR:
			texObj = &starTexObj; color = (GXColor) {255,255,0,255};
			ss = 16;
			break;
		case TEX_GCLOADER:
			texObj = &gcloaderTexObj; color = (GXColor) {216,216,216,255};
			ts = 76;
			break;
		case TEX_M2LOADER:
			texObj = &m2loaderTexObj;
			break;
		case TEX_ETH2GC:
			texObj = &eth2gcTexObj; color = (GXColor) {216,216,216,255};
			break;
		case TEX_FLIPPY:
			texObj = &flippyTexObj; color = (GXColor) {216,216,216,255};
			t1 -= 18.0f/40.0f;
			break;
		case TEX_GCNET:
			texObj = &gcnetTexObj; color = (GXColor) {216,216,216,255};
			break;
		case TEX_GCODE:
			texObj = &gcloaderTexObj; color = (GXColor) {216,216,216,255};
			t1 -= 12.0f/88.0f;
			break;
		case TEX_KUNAIGC:
			texObj = &kunaigcTexObj; color = (GXColor) {216,216,216,255};
			break;
	}
	
	if(!ss) ss = GX_GetTexObjWidth(texObj);
	if(!ts) ts = GX_GetTexObjHeight(texObj);
	GX_SetTexCoordScaleManually(GX_TEXCOORD0, GX_ENABLE, ss, ts);
	
	GX_InvalidateTexAll();
	GXTlutObj *tlutObj = GX_GetTexObjUserData(texObj);
	if(tlutObj) GX_LoadTlut(tlutObj, GX_GetTexObjTlut(texObj));
	GX_LoadTexObj(texObj, GX_TEXMAP0);
	
	if(indTexObj) {
		GX_LoadTexObj(indTexObj, GX_TEXMAP1);
		
		GX_SetNumIndStages(1);
		GX_SetIndTexOrder(GX_INDTEXSTAGE0, GX_TEXCOORD0, GX_TEXMAP1);
		GX_SetIndTexCoordScale(GX_INDTEXSTAGE0, GX_ITS_16, GX_ITS_16);
		
		switch(GX_GetTexObjFmt(indTexObj)) {
			case GX_TF_I8:
				GX_SetTevIndTile(GX_TEVSTAGE0, GX_INDTEXSTAGE0, 16, 16, 16, 0, GX_ITF_8, GX_ITM_0, GX_ITB_NONE, GX_ITBA_OFF);
				GX_SetTevIndRepeat(GX_TEVSTAGE1);
				break;
			case GX_TF_IA8:
				GX_SetTevIndTile(GX_TEVSTAGE0, GX_INDTEXSTAGE0, 16, 16, 16, 16, GX_ITF_8, GX_ITM_0, GX_ITB_NONE, GX_ITBA_OFF);
				GX_SetTevIndRepeat(GX_TEVSTAGE1);
				break;
		}
	}
	
	color.a = (u8)(((u16)color.a * opacity) / 255);
	_drawRect(x, y, width, height, depth, color, s1, s2, t1, t2);
}

// Internal
static void _DrawImage(uiDrawObj_t *evt) {
	drawImageEvent_t *data = (drawImageEvent_t*)evt->data;
	_DrawImageNow(data->textureId, data->x, data->y, data->width, data->height,
		data->depth, data->s1, data->s2, data->t1, data->t2, 0, 255);
}

/* The icon Settings chose for each Home face, in uiHomeFace_t order. */
static void _HomeFaceIcons(int icons[UI_HOME_FACE_COUNT])
{
	icons[UI_HOME_FACE_LIBRARY] = swissSettings.libraryIcon;
	icons[UI_HOME_FACE_SOURCE] = swissSettings.sourceIcon;
	icons[UI_HOME_FACE_SETTINGS] = swissSettings.settingsIcon;
	icons[UI_HOME_FACE_SYSTEM] = swissSettings.systemIcon;
}

static void _DrawBackground(uiDrawObj_t *evt)
{
	bool decorativeAnimated = _CurrentMotionMode() == UI_MOTION_FULL;
	int icons[UI_HOME_FACE_COUNT];

	UI_PERF_BEGIN(backgroundStart);

	(void)evt;
	_HomeFaceIcons(icons);
	/* The glass copies the frame it is drawn in. */
	IndigoBackground_SetFramebuffer(getVideoMode()->fbWidth,
		getVideoMode()->efbHeight);
	IndigoBackground_Draw(UIAnim_Seconds(),
		decorativeAnimated && !swissSettings.disableAnimatedBackdrop,
		decorativeAnimated,
		UIScene_Frame(), &systemInstrument.clock, &padInstrument, icons);
	/* The background uses a raster-only TEV stage; never leak that state. */
	drawInit();
	UI_PERF_END(UI_PERF_METRIC_BACKGROUND_CPU_SUBMIT, backgroundStart);
}

static uiDrawObj_t* DrawBackground(void)
{
	uiDrawObj_t *event = calloc(1, sizeof(uiDrawObj_t));
	event->type = EV_BACKGROUND;
	return event;
}
const char *DeviceDisplayName(const DEVICEHANDLER_INTERFACE *device)
{
	return device == &__device_dvd ? "Game Disc" : device->deviceName;
}


static void _DrawDeviceSelectorCard(uiDrawObj_t *evt)
{
	drawDeviceSelectorEvent_t *data = (drawDeviceSelectorEvent_t*)evt->data;
	DEVICEHANDLER_INTERFACE *device = data->device;
	float reveal = UIScene_Frame()->chromeProgress;
	int offsetY;
	u8 alpha;
	GXColor primary;
	GXColor secondary;
	GXColor muted;

	if(!device || reveal <= 0.0f) {
		return;
	}
	if(reveal > 1.0f) {
		reveal = 1.0f;
	}
	offsetY = (int)((1.0f - reveal) * 20.0f);
	alpha = (u8)(255.0f * reveal);
	primary = (GXColor) {246, 243, 255, alpha};
	secondary = (GXColor) {216, 207, 255, (u8)(alpha * 0.96f)};
	muted = (GXColor) {178, 166, 224, (u8)(alpha * 0.88f)};

	/* The IPL font remains a crisp screen-space overlay. Thin rails visually
	 * attach it to the cube without perspective-distorting text at 480i. */
	drawStringMedium(320, 70 + offsetY,
		data->destination ? "DEST" : "SOURCE",
		0.62f, ALIGN_CENTER, secondary);
	if(data->inAdvanced) {
		drawStringMedium(174, 220, "\213", 0.90f, ALIGN_RIGHT, muted);
		drawStringMedium(502, 220, "\233", 0.90f, ALIGN_LEFT, muted);
	}
	else {
		drawStringMedium(174, 220, "\213", 0.90f, ALIGN_RIGHT, secondary);
		drawStringMedium(502, 220, "\233", 0.90f, ALIGN_LEFT, secondary);
	}

	drawStringMedium(320, 379 + offsetY, DeviceDisplayName(device),
		data->deviceNameScale, ALIGN_CENTER, primary);
	drawStringMedium(320, 398 + offsetY, data->capability,
		data->capabilityScale, ALIGN_CENTER,
		data->available ? secondary : muted);
	_DrawHintText(320, 419 + offsetY, data->actionHint,
		data->actionScale,
		ALIGN_CENTER, muted);

	_DrawHintText(28, 446,
		data->showAllDevices ? "Z  ONLY" : "Z  ALL",
		0.48f, ALIGN_LEFT,
		data->showAllDevices ? primary : muted);
	_DrawHintText(320, 446, "B  BACK", 0.48f, ALIGN_CENTER, muted);
	if(data->auxiliaryHint[0]) {
		_DrawHintText(612, 446, data->auxiliaryHint, data->auxiliaryScale,
			ALIGN_RIGHT,
			data->inAdvanced ? primary : muted);
	}
}

uiDrawObj_t* DrawDeviceSelectorCard(DEVICEHANDLER_INTERFACE *device,
		bool destination, bool showAllDevices, bool inAdvanced)
{
	drawDeviceSelectorEvent_t *eventData = calloc(1, sizeof(drawDeviceSelectorEvent_t));
	uiDrawObj_t *event = calloc(1, sizeof(uiDrawObj_t));

	eventData->device = device;
	eventData->destination = destination;
	eventData->showAllDevices = showAllDevices;
	eventData->inAdvanced = inAdvanced;
	eventData->exiSpeed = !!swissSettings.exiSpeed;
	eventData->available = deviceHandler_getDeviceAvailable(device);
	if(!eventData->available) {
		strcpy(eventData->capability, "NOT DETECTED");
	}
	else if(inAdvanced && (device->features & FEAT_EXI_SPEED)) {
		snprintf(eventData->capability, sizeof(eventData->capability),
			"EXI  %s", eventData->exiSpeed ? "27 MHz" : "13.5 MHz");
	}
	else if(device->features & FEAT_BOOT_GCM) {
		strcpy(eventData->capability,
			(device->features & FEAT_AUDIO_STREAMING) ?
			"BOOT + STREAM" : "BOOT READY");
	}
	else {
		strcpy(eventData->capability, "FILES READY");
	}
	if(inAdvanced) {
		strcpy(eventData->actionHint, "A  DONE");
	}
	else if(!eventData->available) {
		strcpy(eventData->actionHint, "A  TRY");
	}
	else if(destination) {
		strcpy(eventData->actionHint, "A  SELECT");
	}
	else {
		strcpy(eventData->actionHint, "A  OPEN");
	}
	if((device->features & FEAT_EXI_SPEED) && device->details) {
		strcpy(eventData->auxiliaryHint, "X  EXI  \267  Y  INFO");
	}
	else if(device->features & FEAT_EXI_SPEED) {
		strcpy(eventData->auxiliaryHint, "X  EXI");
	}
	else if(device->details) {
		strcpy(eventData->auxiliaryHint, "Y  INFO");
	}
	eventData->deviceNameScale =
		GetTextScaleToFitInWidthWithMax(DeviceDisplayName(device), 280, 0.76f);
	eventData->capabilityScale =
		GetTextScaleToFitInWidthWithMax(eventData->capability, 300, 0.52f);
	eventData->actionScale = GetHintScaleToFitInWidthWithMax(eventData->actionHint, 280, 0.50f);
	eventData->auxiliaryScale = eventData->auxiliaryHint[0] ?
		GetHintScaleToFitInWidthWithMax(eventData->auxiliaryHint, 190, 0.48f) : 0.0f;
	event->type = EV_DEVICESELECTOR;
	event->data = eventData;
	return event;
}

// External
uiDrawObj_t* DrawImage(int textureId, int x, int y, int width, int height, int depth, float s1, float s2, float t1, float t2, int centered)
{
	drawImageEvent_t *eventData = calloc(1, sizeof(drawImageEvent_t));
	eventData->textureId = textureId;
	eventData->x = centered ? ((int) x - width/2) : x;
	eventData->y = y;
	eventData->width = width;
	eventData->height = height;
	eventData->depth = depth;
	eventData->s1 = s1;
	eventData->s2 = s2;
	eventData->t1 = t1;
	eventData->t2 = t2;
	uiDrawObj_t *event = calloc(1, sizeof(uiDrawObj_t));
	event->type = EV_IMAGE;
	event->data = eventData;
	return event;
}

// Internal
static void _DrawTexObjNow(GXTexObj *texObj, int x, int y, int width, int height, int depth, float s1, float s2, float t1, float t2, int centered)
{
	if(GX_GetTexObjMagFilt(texObj) == GX_NEAR) {
		GX_SetNumTevStages(1);
		GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	}
	GX_InvalidateTexAll();
	GXTlutObj *tlutObj = GX_GetTexObjUserData(texObj);
	if(tlutObj) GX_LoadTlut(tlutObj, GX_GetTexObjTlut(texObj));
	GX_LoadTexObj(texObj, GX_TEXMAP0);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		GX_Position3f32((float) x,(float) y,(float) depth );
		GX_Color4u8(255, 255, 255, 255);
		GX_TexCoord2f32(s1,t1);
		GX_Position3f32((float) (x+width),(float) y,(float) depth );
		GX_Color4u8(255, 255, 255, 255);
		GX_TexCoord2f32(s2,t1);
		GX_Position3f32((float) (x+width),(float) (y+height),(float) depth );
		GX_Color4u8(255, 255, 255, 255);
		GX_TexCoord2f32(s2,t2);
		GX_Position3f32((float) x,(float) (y+height),(float) depth );
		GX_Color4u8(255, 255, 255, 255);
		GX_TexCoord2f32(s1,t2);
	GX_End();
}

// Internal
static void _DrawTexObj(uiDrawObj_t *evt)
{
	drawTexObjEvent_t *data = (drawTexObjEvent_t*)evt->data;
	_DrawTexObjNow(data->texObj, data->x, data->y, data->width, data->height, data->depth, data->s1, data->s2, data->t1, data->t2, 0);
}

// External
uiDrawObj_t* DrawTexObj(GXTexObj *texObj, int x, int y, int width, int height, int depth, float s1, float s2, float t1, float t2, int centered)
{
	drawTexObjEvent_t *eventData = calloc(1, sizeof(drawTexObjEvent_t));
	eventData->texObj = texObj;
	eventData->x = centered ? ((int) x - width/2) : x;
	eventData->y = y;
	eventData->width = width;
	eventData->height = height;
	eventData->depth = depth;
	eventData->s1 = s1;
	eventData->s2 = s2;
	eventData->t1 = t1;
	eventData->t2 = t2;
	uiDrawObj_t *event = calloc(1, sizeof(uiDrawObj_t));
	event->type = EV_TEXOBJ;
	event->data = eventData;
	return event;
}

// Internal
static void _DrawProgressBar(uiDrawObj_t *evt) {
	
	drawProgressEvent_t *data = (drawProgressEvent_t*)evt->data;
	int x1 = ((640/2) - (PROGRESS_BOX_WIDTH/2));
	int x2 = ((640/2) + (PROGRESS_BOX_WIDTH/2));
	int y1 = ((480/2) - (PROGRESS_BOX_HEIGHT/2));
	int y2 = ((480/2) + (PROGRESS_BOX_HEIGHT/2));

  	GXColor fillColor = (GXColor) {0,0,0,GUI_MSGBOX_ALPHA}; //black
  	GXColor noColor = (GXColor) {0,0,0,0}; //blank
	GXColor borderColor = (GXColor) {200,200,200,GUI_MSGBOX_ALPHA}; //silver
	GXColor progressBarColor = (GXColor) {255,128,0,GUI_MSGBOX_ALPHA}; //orange
	GXColor progressBarIndColor = (GXColor) {0xb3,0xd9,0xff,GUI_MSGBOX_ALPHA}; //orange
	
	if(data->miniMode) {	
		int x = 30, y = 420;
		if(data->miniModePos == PROGRESS_BOX_TOPRIGHT) {
			x = 535; y = 95;
		}
		GXColor loadingColor = (GXColor) {255,255,255,data->miniModeAlpha};
		int numSegments = (data->percent*8)/100;
		data->percent += (data->percent + 2 > 200 ? -200 : 2);
		if(data->speed != 0) {
			data->miniModeAlpha = MIN(255, data->miniModeAlpha + 3);
		}
		else {
			data->miniModeAlpha = MAX(0, data->miniModeAlpha - 3);
		}
		GX_InvalidateTexAll();
		GX_LoadTexObj(&loadingTexObj, GX_TEXMAP0);
		_drawRect(x-8, y-8, 16, 16, 0, loadingColor, (float) (numSegments)/8, (float) (numSegments+1)/8, 0.0f, 1.0f);
		drawString(x+8, y, "Loading\205", 0.55f, ALIGN_LEFT, loadingColor);
		return;
	}
	_DrawSimpleBox( x1, y1, x2-x1, y2-y1, 0, fillColor, borderColor);		
	
	int middleY = (y2+y1)/2;
	if(data->indeterminate) {
		data->percent += (data->percent + 2 == 400 ? -398 : 2);
		int multiplier = (PROGRESS_BOX_WIDTH-20)/100;
		int progressBarWidth = multiplier*100;
		int progressStart = 0;
		int progressSize = 0;
		if(data->percent < 100) {
			progressStart = 0;
			progressSize = data->percent%100;
		}
		else if(data->percent >= 100 && data->percent < 200) {
			progressStart = (data->percent%100);
			progressSize = 100-progressStart;
		}
		else if(data->percent >= 200 && data->percent < 300) {
			progressStart = 100-(data->percent%100);
			progressSize = 100-progressStart;
		}
		else {
			progressStart = 0;
			progressSize = 100-(data->percent%100);
		}
		
		_DrawSimpleBox( (640/2 - progressBarWidth/2), y1+20,
				(multiplier*100), 20, 0, noColor, borderColor); 
		_DrawSimpleBox( (640/2 - progressBarWidth/2) + (progressStart*multiplier),
				y1+20,
				(multiplier*progressSize),
				20, 0, progressBarIndColor, noColor);
	}
	else {
		int multiplier = (PROGRESS_BOX_WIDTH-20)/100;
		int progressBarWidth = multiplier*100;
		_DrawSimpleBox( (640/2 - progressBarWidth/2), y1+20,
				(multiplier*100), 20, 0, noColor, borderColor); 
		_DrawSimpleBox( (640/2 - progressBarWidth/2), y1+20,
				(multiplier*data->percent), 20, 0, progressBarColor, noColor); 
		sprintf(fbTextBuffer,"%d%%", data->percent);
		bool displaySpeed = data->speed != 0;
		drawString(displaySpeed ? (x1 + 80) : (640/2), middleY+30, fbTextBuffer, 1.0f, ALIGN_CENTER, defaultColor);
		if(displaySpeed) {
			formatBytes(fbTextBuffer, data->speed, 0, true);
			strcat(fbTextBuffer, "/s");
			drawString(x1 + 280, middleY+30, fbTextBuffer, 1.0f, ALIGN_CENTER, defaultColor);
			sprintf(fbTextBuffer,"Elapsed: %02i:%02i:%02i", data->timestart / 3600, (data->timestart / 60)%60,  data->timestart % 60);
			drawString(x1 + 500, middleY+30, fbTextBuffer, 0.65f, ALIGN_CENTER, defaultColor);
			sprintf(fbTextBuffer,"Remain: %02i:%02i:%02i", data->timeremain / 3600, (data->timeremain / 60)%60,  data->timeremain % 60);
			drawString(x1 + 500, middleY+45, fbTextBuffer, 0.65f, ALIGN_CENTER, defaultColor);
		}
	}	
}

// External
uiDrawObj_t* DrawProgressBar(bool indeterminate, int percent, const char *message) {
	drawProgressEvent_t *eventData = calloc(1, sizeof(drawProgressEvent_t));
	eventData->percent = percent;
	eventData->indeterminate = indeterminate;
	uiDrawObj_t *event = calloc(1, sizeof(uiDrawObj_t));
	event->type = EV_PROGRESS;
	event->data = eventData;
	if(message && strlen(message) > 0) {
		sprintf(txtbuffer, "%s", message);
		// Add child component(s) for label(s)
		char *tok = strtok(txtbuffer,"\n");
		int y1 = ((480/2) - (PROGRESS_BOX_HEIGHT/2));
		int y2 = ((480/2) + (PROGRESS_BOX_HEIGHT/2));
		int middleY = (y2+y1)/2;
		while(tok != NULL) {
			DrawAddChild(event, DrawStyledLabel(640/2, middleY, tok, 1.0f, ALIGN_CENTER, defaultColor));
			tok = strtok(NULL,"\n");
			middleY+=24;
		}
	}
	return event;
}

uiDrawObj_t* DrawProgressLoading(int miniModePos) {
	drawProgressEvent_t *eventData = calloc(1, sizeof(drawProgressEvent_t));
	eventData->miniMode = true;
	eventData->miniModePos = miniModePos;
	uiDrawObj_t *event = calloc(1, sizeof(uiDrawObj_t));
	event->type = EV_PROGRESS;
	event->data = eventData;
	return event;
}

// Internal
static void _DrawMessageBox(uiDrawObj_t *evt) {
	int x1 = ((640/2) - (PROGRESS_BOX_WIDTH/2));
	int x2 = ((640/2) + (PROGRESS_BOX_WIDTH/2));
	int y1 = ((480/2) - (PROGRESS_BOX_HEIGHT/2));
	int y2 = ((480/2) + (PROGRESS_BOX_HEIGHT/2));
	
  	GXColor fillColor = (GXColor) {0,0,0,GUI_MSGBOX_ALPHA}; //black
	GXColor borderColor = (GXColor) {200,200,200,GUI_MSGBOX_ALPHA}; //silver
	
	_DrawSimpleBox( x1, y1, x2-x1, y2-y1, 0, fillColor, borderColor); 
}	

// External
uiDrawObj_t* DrawMessageBox(int type, const char *msg)
{
	drawMsgBoxEvent_t *eventData = calloc(1, sizeof(drawMsgBoxEvent_t));
	eventData->type = type;
	uiDrawObj_t *event = calloc(1, sizeof(uiDrawObj_t));
	event->type = EV_MSGBOX;
	event->data = eventData;
	
	// Add child component(s) for label(s)
	sprintf(txtbuffer, "%s", msg);
	/* "Press A to continue." and the like become a line of button icons. */
	char hint[UI_HINT_LABEL_CAPACITY];
	bool hasHint = UIHint_SplitPrompt(txtbuffer, hint, sizeof(hint)) != 0;
	char *tok = strtok(txtbuffer,"\n");
	int y1 = ((480/2) - (PROGRESS_BOX_HEIGHT/2));
	int y2 = ((480/2) + (PROGRESS_BOX_HEIGHT/2));
	int middleY = y2-y1 < 23 ? y1+3 : (y2+y1)/2-12;
	while(tok != NULL) {
		uiDrawObj_t *lineLabel = DrawStyledLabel(640/2, middleY, tok, 1.0f, ALIGN_CENTER, defaultColor);
		tok = strtok(NULL,"\n");
		middleY+=24;
		DrawAddChild(event, lineLabel);
	}
	if(hasHint) {
		DrawAddChild(event, DrawHintLabel(640/2, middleY, hint, 0.8f, ALIGN_CENTER, defaultColor));
	}
	
	return event;
}

static GXColor _PresentationAccent(uiPresentationKind_t kind)
{
	switch(kind) {
		case UI_PRESENTATION_EMPTY:
			return (GXColor) {142, 128, 202, 255};
		case UI_PRESENTATION_LOADING:
			return (GXColor) {182, 170, 242, 255};
		case UI_PRESENTATION_RECOVERABLE_ERROR:
			return (GXColor) {243, 126, 145, 255};
		case UI_PRESENTATION_INFORMATION:
			return (GXColor) {117, 181, 218, 255};
		default:
			return (GXColor) {142, 128, 202, 255};
	}
}

static void _DrawPresentation(uiDrawObj_t *evt)
{
	drawPresentationEvent_t *data = (drawPresentationEvent_t*)evt->data;
	GXColor transparent = {0, 0, 0, 0};
	GXColor scrim = {4, 3, 15, 184};
	GXColor shadow = {2, 1, 10, 194};
	GXColor panel = {10, 8, 31, 246};
	GXColor border = {135, 122, 199, 205};
	GXColor primary = {244, 239, 255, 255};
	GXColor secondary = {184, 174, 225, 238};
	GXColor muted = {153, 145, 190, 225};
	GXColor accent;
	uiMotionMode_t motionMode;
	u32 activeCell = 0u;
	u32 i;

	if(data == NULL) {
		return;
	}
	accent = _PresentationAccent(data->snapshot.kind);
	motionMode = _CurrentMotionMode();
	if(data->snapshot.kind == UI_PRESENTATION_LOADING &&
		motionMode != UI_MOTION_OFF) {
		float rate = motionMode == UI_MOTION_REDUCED ? 2.0f : 5.0f;

		activeCell = (u32)(UIAnim_Seconds() * rate) % 3u;
	}

	drawInit();
	_DrawSimpleBox(0, 0, 640, 480, 0, scrim, transparent);
	_DrawSimpleBox(68, 115, 504, 254, 0, shadow, transparent);
	_DrawSimpleBox(72, 111, 496, 254, 0, panel, border);
	_DrawSimpleBox(72, 111, 6, 254, 0, accent, transparent);
	_DrawSimpleBox(102, 202, 436, 1, 0,
		(GXColor) {135, 122, 199, 118}, transparent);

	/* Three code-native cells make state visible without relying on hue. The
	 * loading variant advances one bright cell; all other kinds stay still. */
	for(i = 0u; i < 3u; ++i) {
		GXColor cell = accent;

		if(data->snapshot.kind == UI_PRESENTATION_LOADING &&
			motionMode != UI_MOTION_OFF) {
			cell.a = i == activeCell ? 255u : 72u;
		}
		else if(data->snapshot.kind == UI_PRESENTATION_LOADING) {
			cell.a = 172u;
		}
		else {
			cell.a = (u8)(216u - i * 48u);
		}
		_DrawSimpleBox(492 + (int)i * 16, 143, 10, 10, 0, cell,
			transparent);
	}

	drawStringMedium(104, 151,
		UIPresentation_KindLabel(data->snapshot.kind), 0.38f, ALIGN_LEFT,
		accent);
	drawStringMedium(104, 185, data->snapshot.title, data->titleScale,
		ALIGN_LEFT, primary);
	drawStringMedium(104, 231, data->snapshot.message, data->messageScale,
		ALIGN_LEFT, secondary);
	if(data->snapshot.detail[0] != '\0') {
		drawStringMedium(104, 263, data->snapshot.detail, data->detailScale,
			ALIGN_LEFT, muted);
	}
	if(data->snapshot.action[0] != '\0') {
		drawStringMedium(320, 332, data->snapshot.action, data->actionScale,
			ALIGN_CENTER, primary);
	}
	drawInit();
}

static bool _PreparePresentation(drawPresentationEvent_t *data,
	const uiPresentationSnapshot_t *snapshot)
{
	if(data == NULL || !UIPresentation_Valid(snapshot)) {
		return false;
	}
	memset(data, 0, sizeof(*data));
	data->snapshot.kind = snapshot->kind;
	data->titleScale = UIHomeText_CopyFitted(data->snapshot.title,
		sizeof(data->snapshot.title), snapshot->title, 392, 0.76f,
		GetTextSizeInPixels, NULL);
	data->messageScale = UIHomeText_CopyFitted(data->snapshot.message,
		sizeof(data->snapshot.message), snapshot->message, 432, 0.54f,
		GetTextSizeInPixels, NULL);
	data->detailScale = UIHomeText_CopyFitted(data->snapshot.detail,
		sizeof(data->snapshot.detail), snapshot->detail, 432, 0.46f,
		GetTextSizeInPixels, NULL);
	data->actionScale = UIHomeText_CopyFitted(data->snapshot.action,
		sizeof(data->snapshot.action), snapshot->action, 432, 0.50f,
		GetTextSizeInPixels, NULL);
	return UIPresentation_Valid(&data->snapshot);
}

uiDrawObj_t* DrawPresentation(const uiPresentationSnapshot_t *snapshot)
{
	drawPresentationEvent_t *eventData;
	uiDrawObj_t *event;

	if(!UIPresentation_Valid(snapshot)) {
		return NULL;
	}
	eventData = calloc(1, sizeof(*eventData));
	event = calloc(1, sizeof(*event));
	if(eventData == NULL || event == NULL ||
		!_PreparePresentation(eventData, snapshot)) {
		free(eventData);
		free(event);
		return NULL;
	}
	event->type = EV_PRESENTATION;
	event->data = eventData;
	return event;
}

bool DrawUpdatePresentation(uiDrawObj_t *evt,
	const uiPresentationSnapshot_t *snapshot)
{
	drawPresentationEvent_t next;
	bool updated = false;

	if(evt == NULL || !_PreparePresentation(&next, snapshot)) {
		return false;
	}
	LWP_MutexLock(_videomutex);
	if(!evt->disposed && evt->type == EV_PRESENTATION && evt->data != NULL) {
		memcpy(evt->data, &next, sizeof(next));
		updated = true;
	}
	LWP_MutexUnlock(_videomutex);
	return updated;
}

// Internal
static void _DrawSelectableButton(uiDrawObj_t *evt) {
	drawSelectableButtonEvent_t *data = (drawSelectableButtonEvent_t*)evt->data;
	int x1 = data->x1;
	int x2 = data->x2;
	GXColor selectColor = (GXColor) {96,107,164,GUI_MSGBOX_ALPHA}; //bluish
	GXColor noColor = (GXColor) {0,0,0,0}; //black
	GXColor borderColor = (GXColor) {200,200,200,GUI_MSGBOX_ALPHA}; //silver
	
	int borderSize = 4;
	//determine length of the text ourselves if x2 == -1
	x2 = (x2 == -1) ? GetTextSizeInPixels(data->msg)+x1+(borderSize*2)+6 : x2;
	//Draw Text and backfill (if selected)
	if(data->mode==B_SELECTED) {
		_DrawSimpleBox( x1, data->y1, x2-x1, data->y2-data->y1+2, 0, selectColor, borderColor);
	}
	else {
		_DrawSimpleBox( x1, data->y1, x2-x1, data->y2-data->y1+2, 0, noColor, borderColor);
	}
	
	if(data->msg) {
		float scale = GetTextScaleToFitInWidth(data->msg, (x2-x1)-(borderSize*2)-6);
		// Adjust font when we can't fit vertically too
		int availHeight = data->y2 - data->y1 - 4;
		if(GetFontHeight(scale) > availHeight) {
			int fullHeight = GetFontHeight(1.0f);
			scale = (float)availHeight / (float)fullHeight;
		}
		drawString(data->x1+borderSize+3, data->y1+(data->y2-data->y1)/2, data->msg, scale, ALIGN_LEFT, defaultColor);
	}
}

// External
uiDrawObj_t* DrawSelectableButton(int x1, int y1, int x2, int y2, const char *message, int mode)
{	
	drawSelectableButtonEvent_t *eventData = calloc(1, sizeof(drawSelectableButtonEvent_t));
	eventData->x1 = x1;
	eventData->y1 = y1;
	eventData->x2 = x2;
	eventData->y2 = y2;
	eventData->mode = mode;
	if(message) {
		eventData->msg = strdup(message);
	}
	uiDrawObj_t *event = calloc(1, sizeof(uiDrawObj_t));
	event->type = EV_SELECTABLEBUTTON;
	event->data = eventData;
	return event;
}

// Internal
static void _DrawTooltip(uiDrawObj_t *evt) {

	drawTooltipEvent_t *data = (drawTooltipEvent_t*)evt->data;

	if(data->tooltip/* && data->tooltiptime >= 50*/) {
		//int alpha = data->tooltiptime*4 > 255 ? 255 : data->tooltiptime;
		int alpha = 255;
		int borderSize = 4;
		GXColor borderColorTT = (GXColor) {255,255,255,alpha};
		GXColor backColorTT = (GXColor) {122,122,122,alpha}; //grey
		int numLines = 1;
		char *strPtr = data->tooltip;
		for (numLines=1; strPtr[numLines]; strPtr[numLines]=='\n' ? numLines++ : *strPtr++);
		int height = numLines*26;
		int tooltipY1 = (getVideoMode()->efbHeight / 2) - (height/2);
		// TODO centre on Y based on total size.
		int tooltipX1 = 25, tooltipX2 = getVideoMode()->fbWidth-25, tooltipY2 = tooltipY1+height;
		_DrawSimpleBox( tooltipX1, tooltipY1-6, tooltipX2-tooltipX1, (tooltipY2-tooltipY1)+6, 0, backColorTT, borderColorTT);
		
		// Write each line
		strPtr = data->tooltip;
		int curLine = 0;
		while(numLines) {
			float scale = GetTextScaleToFitInWidthWithMax(strPtr, (tooltipX2-tooltipX1)-(borderSize*2)-6, 0.75f);
			drawString(tooltipX1+borderSize+3, tooltipY1+11+(curLine*25), strPtr, scale, ALIGN_LEFT, borderColorTT);
			numLines--;
			curLine++;
			// Increment to the next line if we have one.
			if(numLines > 0) {
				while(*strPtr != '\n') strPtr++;
				strPtr++;
			}
		}
	}
}

// External
uiDrawObj_t* DrawTooltip(const char *tooltip) {
	drawTooltipEvent_t *eventData = calloc(1, sizeof(drawTooltipEvent_t));
	if(tooltip && strlen(tooltip) > 0) {
		eventData->tooltip = strdup(tooltip);
	}
	else {
		eventData->tooltip = NULL;
	}
	uiDrawObj_t *event = calloc(1, sizeof(uiDrawObj_t));
	event->type = EV_TOOLTIP;
	event->data = eventData;
	return event;
}

// Internal
static void _DrawStyledLabel(uiDrawObj_t *evt) {
	drawStyledLabelEvent_t *data = (drawStyledLabelEvent_t*)evt->data;
	const char *string = data->getString ? data->getString() : data->string;
	
	if(data->showCaret) {
		// blink the caret
		if(data->fadingDirection) {
			data->caretColor.a += (data->fadingDirection * 20);
			if(data->caretColor.a >= 255) { data->fadingDirection = -1; data->caretColor.a = 255; }
			else if(data->caretColor.a <= 15) { data->fadingDirection = 1; data->caretColor.a = 0; }
		}		
		drawStringWithCaret(data->x, data->y, string, data->size, data->align, data->color, data->caretPosition, data->caretColor);
	}
	else {
		if(data->fadingDirection) {
			data->color.a += data->fadingDirection;
			if(data->color.a >= 255) data->fadingDirection = -1;
			else if(data->color.a <= 15) data->fadingDirection = 1;
		}
		if(data->hint) {
			_DrawHintText(data->x, data->y, string, data->size, data->align,
				data->color);
		}
		else {
			drawStringMedium(data->x, data->y, string, data->size, data->align,
				data->color);
		}
	}
}

// External
uiDrawObj_t* DrawStyledLabel(int x, int y, const char *string, float size, int align, GXColor color)
{	
	drawStyledLabelEvent_t *eventData = calloc(1, sizeof(drawStyledLabelEvent_t));
	eventData->x = x;
	eventData->y = y;
	if(string && strlen(string) > 0) {
		eventData->string = strdup(string);
	}
	else {
		eventData->string = NULL;
	}
	eventData->size = size;
	eventData->align = align;
	eventData->color = color;
	uiDrawObj_t *event = calloc(1, sizeof(uiDrawObj_t));
	event->type = EV_STYLEDLABEL;
	event->data = eventData;
	return event;
}

// External: a label whose button names ("A  OPEN") draw as button icons.
uiDrawObj_t* DrawHintLabel(int x, int y, const char *string, float size, int align, GXColor color)
{
	uiDrawObj_t *event = DrawStyledLabel(x, y, string, size, align, color);
	((drawStyledLabelEvent_t*)event->data)->hint = true;
	return event;
}

// External
uiDrawObj_t* DrawStyledLabelWithCaret(int x, int y, const char *string, float size, int align, GXColor color, int caretPosition)
{	
	uiDrawObj_t *event = DrawStyledLabel(x, y, string, size, align, color);
	drawStyledLabelEvent_t *eventData = (drawStyledLabelEvent_t*)event->data;
	eventData->caretPosition = caretPosition;
	eventData->showCaret = true;
	eventData->fadingDirection = 1;
	eventData->caretColor = eventData->color;
	return event;
}

// External
uiDrawObj_t* DrawLabel(int x, int y, const char *string)
{	
	drawStyledLabelEvent_t *eventData = calloc(1, sizeof(drawStyledLabelEvent_t));
	eventData->x = x;
	eventData->y = y;
	if(string && strlen(string) > 0) {
		eventData->string = strdup(string);
	}
	else {
		eventData->string = NULL;
	}
	eventData->size = 1.0f;
	eventData->align = ALIGN_LEFT;
	eventData->color = defaultColor;
	uiDrawObj_t *event = calloc(1, sizeof(uiDrawObj_t));
	event->type = EV_STYLEDLABEL;
	event->data = eventData;
	return event;
}

// External
uiDrawObj_t* DrawFadingLabel(int x, int y, const char *string, float size)
{	
	drawStyledLabelEvent_t *eventData = calloc(1, sizeof(drawStyledLabelEvent_t));
	eventData->x = x;
	eventData->y = y;
	if(string && strlen(string) > 0) {
		eventData->string = strdup(string);
	}
	else {
		eventData->string = NULL;
	}
	eventData->size = size;
	eventData->align = ALIGN_LEFT;
	eventData->color = (GXColor) {255, 255, 255, 0};
	eventData->fadingDirection = 1;
	uiDrawObj_t *event = calloc(1, sizeof(uiDrawObj_t));
	event->type = EV_STYLEDLABEL;
	event->data = eventData;
	return event;
}

// External
uiDrawObj_t* DrawDynamicLabel(int x, int y, const char *(*getString)(void), float size, int align, GXColor color)
{	
	drawStyledLabelEvent_t *eventData = calloc(1, sizeof(drawStyledLabelEvent_t));
	eventData->x = x;
	eventData->y = y;
	eventData->getString = getString;
	eventData->size = size;
	eventData->align = align;
	eventData->color = color;
	uiDrawObj_t *event = calloc(1, sizeof(uiDrawObj_t));
	event->type = EV_STYLEDLABEL;
	event->data = eventData;
	return event;
}

// External (this is used to tie objects together, think of it as an invisible panel)
uiDrawObj_t* DrawContainer()
{
	uiDrawObj_t *event = calloc(1, sizeof(uiDrawObj_t));
	event->type = EV_CONTAINER;
	return event;
}

// Internal
static void _DrawFileBrowserButton(uiDrawObj_t *evt) {
	
	drawFileBrowserButtonEvent_t *data = (drawFileBrowserButtonEvent_t*)evt->data;
	int borderSize = 4;	
	if(data->isCarousel) {	
		// Not selected
		GXColor noColor 	= (GXColor) {0,0,0,128};
		GXColor borderColor = (GXColor) {200,200,200,GUI_MSGBOX_ALPHA}; //Silver
		// Large middle entry currently being displayed, verbose info
		if(data->distFromMiddle == 0) {

			_DrawSimpleBox(data->x1, data->y1, data->x2-data->x1, data->y2-data->y1, 0, noColor, borderColor);
			
			int x_mid = data->x2-((data->x2-data->x1)/2);
			int bnr_width = 96;
			int bnr_height = 32;
			file_handle *file = data->file;
			// Draw banner if there is one
			if(file->meta && (file->meta->banner || file->meta->fileTypeTexObj)) {
				GXTexObj *texObj = (file->meta->banner ? &file->meta->bannerTexObj : file->meta->fileTypeTexObj);
				bnr_width *= (file->meta->banner ? 2 : 1);
				bnr_height *= (file->meta->banner ? 2 : 1);
				if(file->meta->banner) {
					GX_SetNumTevStages(1);
					GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
				}
				GX_InvalidateTexAll();
				GXTlutObj *tlutObj = GX_GetTexObjUserData(texObj);
				if(tlutObj) GX_LoadTlut(tlutObj, GX_GetTexObjTlut(texObj));
				GX_LoadTexObj(texObj, GX_TEXMAP0);
				int bnr_x = x_mid - (bnr_width/2);
				GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
					GX_Position3f32((float) bnr_x,(float) data->y1+borderSize+40, 0.0f );
					GX_Color4u8(255, 255, 255, data->alpha);
					GX_TexCoord2f32(0.0f,0.0f);
					GX_Position3f32((float) (bnr_x+bnr_width),(float) data->y1+borderSize+40,0.0f );
					GX_Color4u8(255, 255, 255, data->alpha);
					GX_TexCoord2f32(1.0f,0.0f);
					GX_Position3f32((float) (bnr_x+bnr_width),(float) (data->y1+borderSize+40+bnr_height),0.0f );
					GX_Color4u8(255, 255, 255, data->alpha);
					GX_TexCoord2f32(1.0f,1.0f);
					GX_Position3f32((float) bnr_x,(float) (data->y1+borderSize+40+bnr_height),0.0f );
					GX_Color4u8(255, 255, 255, data->alpha);
					GX_TexCoord2f32(0.0f,1.0f);
				GX_End();
				
				if(data->isAutoLoadEntry) {
					drawInit();
					_DrawImageNow(TEX_STAR, bnr_x+bnr_width-16, data->y1+borderSize+40,
						16, 16, 0, 0.0f, 1.0f, 0.0f, 1.0f, 0, 255);
				}
				
				// Company
				sprintf(fbTextBuffer, "%.*s", BNR_FULL_TEXT_LEN, file->meta->bannerDesc.fullCompany);
				float scale = GetTextScaleToFitInWidth(fbTextBuffer,(data->x2-data->x1)-(borderSize*2));
				drawString(x_mid, data->y1+(borderSize*2)+40+bnr_height+20, fbTextBuffer, scale, ALIGN_CENTER, defaultColor);
				
				// Description
				sprintf(fbTextBuffer, "%.*s", BNR_DESC_LEN, file->meta->bannerDesc.description);
				char* rest = &fbTextBuffer[0];
				char* tok;
				int line = 0;
				while ((tok = strtok_r (rest,"\r\n", &rest))) {
					scale = GetTextScaleToFitInWidthWithMax(tok,(data->x2-data->x1)-(borderSize*2), !line ? 1.0f : scale);
					drawString(x_mid, data->y1+(borderSize*2)+40+bnr_height+60+(line*scale*24), tok, scale, ALIGN_CENTER, defaultColor);
					line++;
				}
			}
			// Region
			if(file->meta && file->meta->regionTexObj) {
				drawString(data->x2 - 44, data->y2-(borderSize+41), "Region: ", 0.45f, ALIGN_RIGHT, defaultColor);
				drawInit();
				_DrawTexObjNow(file->meta->regionTexObj, data->x2 - 44, data->y2-(borderSize+50), 32, 20, 0, 0.0f, 1.0f, 0.0f, 1.0f, 0);
			}
			
			// fullGameName displays some titles with incorrect encoding, use displayName instead
			float scale = GetTextScaleToFitInWidth(data->displayName, (data->x2-data->x1)-(borderSize*2));
			drawString(x_mid, data->y1+(borderSize*2)+10, data->displayName, scale, ALIGN_CENTER, defaultColor);
			
			// Print specific stats
			if(file->fileType==IS_FILE) {
				if(file->device == &__device_wode) {
					ISOInfo_t* isoInfo = (ISOInfo_t*)&file->other;
					sprintf(fbTextBuffer,"Partition: %i, ISO: %i", isoInfo->iso_partition,isoInfo->iso_number);
				}
				else if(file->device == &__device_card_a || file->device == &__device_card_b) {
					formatBytes(stpcpy(fbTextBuffer, "Size: "), file->size, 8192, false);
				}
				else if(file->device == &__device_qoob) {
					formatBytes(stpcpy(fbTextBuffer, "Size: "), file->size, 65536, false);
				}
				else {
					formatBytes(stpcpy(fbTextBuffer, "Size: "), file->size, 0, !(file->device->location & LOC_SYSTEM));
				}
				drawString(data->x2-(borderSize+8), data->y2-(borderSize+19), fbTextBuffer, 0.45f, ALIGN_RIGHT, defaultColor);
			}
		}
		else {
			// Vertical
			_DrawSimpleBox(data->x1, data->y1, data->x2-data->x1, data->y2-data->y1, 0, noColor, borderColor);
			int bnr_width = 72;
			int bnr_height = 24;
			int x_start = (data->x2-((data->x2-data->x1)/2)) - (bnr_height/2);
			// Draw banner if there is one
			file_handle *file = data->file;
			if(file->meta && (file->meta->banner || file->meta->fileTypeTexObj)) {
				GXTexObj *texObj = (file->meta->banner ? &file->meta->bannerTexObj : file->meta->fileTypeTexObj);
				if(file->meta->banner) {
					GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD0, GX_TEXMAP1, GX_COLOR0A0);
					GX_SetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_CPREV);
					GX_SetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_APREV, GX_CA_TEXA, GX_CA_ZERO);
					GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
				}
				GX_InvalidateTexAll();
				GXTlutObj *tlutObj = GX_GetTexObjUserData(texObj);
				if(tlutObj) GX_LoadTlut(tlutObj, GX_GetTexObjTlut(texObj));
				GX_LoadTexObj(texObj, GX_TEXMAP0);
				GX_LoadTexObj(&bannerMaskTexObj, GX_TEXMAP1);
				GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
					GX_Position3f32((float)x_start,(float) data->y2-borderSize, 0.0f ); // bottom left
					GX_Color4u8(255, 255, 255, data->alpha);
					GX_TexCoord2f32(0.0f,0.0f);
					GX_Position3f32((float)x_start,(float) data->y2-bnr_width-borderSize,0.0f );	// top left
					GX_Color4u8(255, 255, 255, data->alpha);
					GX_TexCoord2f32(1.0f,0.0f);
					GX_Position3f32((float)x_start+bnr_height,(float) data->y2-bnr_width-borderSize,0.0f );	// top right
					GX_Color4u8(255, 255, 255, data->alpha);
					GX_TexCoord2f32(1.0f,1.0f);
					GX_Position3f32((float)x_start+bnr_height,(float) data->y2 - borderSize,0.0f );	// bottom right
					GX_Color4u8(255, 255, 255, data->alpha);
					GX_TexCoord2f32(0.0f,1.0f);
				GX_End();
				
				if(data->isAutoLoadEntry) {
					drawInit();
					_DrawImageNow(TEX_STAR, x_start, data->y2-bnr_width-borderSize,
						12, 12, 0, 0.0f, 1.0f, 0.0f, 1.0f, 0, 255);
				}
			}
			// fullGameName displays some titles with incorrect encoding, use displayName instead
			drawStringEllipsis(data->x1+(data->x2-data->x1)/2, data->y2-bnr_width-5-borderSize, data->displayName, 0.5f, ALIGN_LEFT, defaultColor, true, (data->y2-bnr_width-5-borderSize) - (data->y1 + (borderSize*2)));
		}
	}
	else {
		
		// Not selected
		GXColor noColor 	= (GXColor) {0,0,0,0};
		GXColor selectColor = (GXColor) {46,57,104,GUI_MSGBOX_ALPHA}; 	//bluish
		GXColor borderColor = (GXColor) {200,200,200,GUI_MSGBOX_ALPHA}; //Silver

		_DrawSimpleBox(data->x1, data->y1, data->x2-data->x1, data->y2-data->y1, 
					0, data->mode == B_SELECTED ? selectColor : noColor, borderColor);
		
		// Draw banner if there is one
		file_handle *file = data->file;
		if(file->meta && (file->meta->banner || file->meta->fileTypeTexObj)) {
			GXTexObj *texObj = (file->meta->banner ? &file->meta->bannerTexObj : file->meta->fileTypeTexObj);
			if(file->meta->banner) {
				GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD0, GX_TEXMAP1, GX_COLOR0A0);
				GX_SetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_CPREV);
				GX_SetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_APREV, GX_CA_TEXA, GX_CA_ZERO);
				GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
			}
			GX_InvalidateTexAll();
			GXTlutObj *tlutObj = GX_GetTexObjUserData(texObj);
			if(tlutObj) GX_LoadTlut(tlutObj, GX_GetTexObjTlut(texObj));
			GX_LoadTexObj(texObj, GX_TEXMAP0);
			GX_LoadTexObj(&bannerMaskTexObj, GX_TEXMAP1);
			GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
				GX_Position3f32((float) data->x1+7,(float) data->y1+4, 0.0f );
				GX_Color4u8(255, 255, 255, data->alpha);
				GX_TexCoord2f32(0.0f,0.0f);
				GX_Position3f32((float) (data->x1+7+96),(float) data->y1+4,0.0f );
				GX_Color4u8(255, 255, 255, data->alpha);
				GX_TexCoord2f32(1.0f,0.0f);
				GX_Position3f32((float) (data->x1+7+96),(float) (data->y1+4+32),0.0f );
				GX_Color4u8(255, 255, 255, data->alpha);
				GX_TexCoord2f32(1.0f,1.0f);
				GX_Position3f32((float) data->x1+7,(float) (data->y1+4+32),0.0f );
				GX_Color4u8(255, 255, 255, data->alpha);
				GX_TexCoord2f32(0.0f,1.0f);
			GX_End();
			
			if(data->isAutoLoadEntry) {
				drawInit();
				_DrawImageNow(TEX_STAR, data->x1+7+96-16, data->y1+4,
					16, 16, 0, 0.0f, 1.0f, 0.0f, 1.0f, 0, 255);
			}
		}
		if(file->meta && file->meta->regionTexObj) {
			drawInit();
			_DrawTexObjNow(file->meta->regionTexObj, data->x2 - 39, data->y1+borderSize+1, 32, 20, 0, 0.0f, 1.0f, 0.0f, 1.0f, 0);
		}

		// fullGameName displays some titles with incorrect encoding, use displayName instead
		if(data->mode == B_SELECTED) {
			float scale = GetTextScaleToFitInWidthWithMax(data->displayName, (data->x2-data->x1-8-96-39)-(borderSize*2), 0.6f);
			drawString(data->x1+borderSize+8+96, data->y1+(data->y2-data->y1)/2, data->displayName, scale, ALIGN_LEFT, defaultColor);
		} else {
			drawStringEllipsis(data->x1+borderSize+8+96, data->y1+(data->y2-data->y1)/2, data->displayName, 0.6f, ALIGN_LEFT, defaultColor, false, (data->x2-data->x1-8-96-39)-(borderSize*2));
		}
		
		// Print specific stats
		if(file->fileType==IS_FILE) {
			if(file->device == &__device_wode) {
				ISOInfo_t* isoInfo = (ISOInfo_t*)&file->other;
				sprintf(fbTextBuffer,"Partition: %i, ISO: %i", isoInfo->iso_partition,isoInfo->iso_number);
			}
			else if(file->device == &__device_card_a || file->device == &__device_card_b) {
				formatBytes(fbTextBuffer, file->size, 8192, false);
			}
			else if(file->device == &__device_qoob) {
				formatBytes(fbTextBuffer, file->size, 65536, false);
			}
			else {
				formatBytes(fbTextBuffer, file->size, 0, !(file->device->location & LOC_SYSTEM));
			}
			drawString(data->x2-(borderSize+3), data->y1+borderSize+26, fbTextBuffer, 0.45f, ALIGN_RIGHT, defaultColor);
		}
	}
}

// External
uiDrawObj_t* DrawFileBrowserButton(int x1, int y1, int x2, int y2, const char *message, file_handle *file, int mode)
{
	drawFileBrowserButtonEvent_t *eventData = calloc(1, sizeof(drawFileBrowserButtonEvent_t));
	eventData->x1 = x1;
	eventData->y1 = y1;
	eventData->x2 = x2;
	eventData->y2 = y2;
	eventData->displayName = strdup(message);
	eventData->mode = mode;
	eventData->file = calloc(1, sizeof(file_handle));
	memcpy(eventData->file, file, sizeof(file_handle));
	if(eventData->file->meta) {
		eventData->file->meta = calloc(1, sizeof(file_meta));
		memcpy(eventData->file->meta, file->meta, sizeof(file_meta));
		if(eventData->file->meta->banner && eventData->file->meta->bannerSum != 0xFFFF) {
			// Make a copy cause we want this one to be killed off when the display event is disposed
			eventData->file->meta->banner = memalign(32, eventData->file->meta->bannerSize);
			memcpy(eventData->file->meta->banner, file->meta->banner, eventData->file->meta->bannerSize);
			DCFlushRange(eventData->file->meta->banner, eventData->file->meta->bannerSize);
			GX_InitTexObjData(&eventData->file->meta->bannerTexObj, eventData->file->meta->banner);
			if(GX_GetTexObjUserData(&eventData->file->meta->bannerTexObj) == &file->meta->bannerTlutObj) {
				void *img_ptr;
				u16 wd, ht;
				u8 fmt, wrap_s, wrap_t, mipmap;
				GX_GetTexObjAll(&eventData->file->meta->bannerTexObj, &img_ptr, &wd, &ht, &fmt, &wrap_s, &wrap_t, &mipmap);
				GX_InitTlutObjData(&eventData->file->meta->bannerTlutObj, img_ptr + GX_GetTexBufferSize(wd, ht, fmt, mipmap, 0));
				GX_InitTexObjUserData(&eventData->file->meta->bannerTexObj, &eventData->file->meta->bannerTlutObj);
			}
		}
		else {
			eventData->file->meta->banner = NULL;
			eventData->file->meta->bannerSize = 0;
		}
		if(eventData->file->meta->displayName == file->meta->bannerDesc.gameName) {
			eventData->file->meta->displayName = eventData->file->meta->bannerDesc.gameName;
		}
		else if(eventData->file->meta->displayName == file->meta->bannerDesc.fullGameName) {
			eventData->file->meta->displayName = eventData->file->meta->bannerDesc.fullGameName;
		}
	}
	// Hide extension when rendering certain files
	if(eventData->file->fileType == IS_FILE) {
		char *fileName = endsWith(eventData->file->name, eventData->displayName);
		char *start = fileName ? eventData->displayName : getRelativeName(eventData->file->name);
		char *end;
		if((end = endsWith(start,".dol"))
			|| (end = endsWith(start,".dol+cli"))
			|| (end = endsWith(start,".elf"))
			|| (end = endsWith(start,".fdi"))
			|| (end = endsWith(start,".gci"))
			|| (end = endsWith(start,".gcm.gcm"))
			|| (end = endsWith(start,".gcm"))
			|| (end = endsWith(start,".gcs"))
			|| (end = endsWith(start,".nkit.iso.iso"))
			|| (end = endsWith(start,".nkit.iso"))
			|| (end = endsWith(start,".iso.iso"))
			|| (end = endsWith(start,".iso"))
			|| (end = endsWith(start,".mp3"))
			|| (end = endsWith(start,".sav"))
			|| (end = endsWith(start,".tgc"))) {
			if(fileName) {
				*end = '\0';
			}
			else if(memmem(eventData->displayName, strlen(eventData->displayName), start, end - start)) {
				end = mempcpy(eventData->displayName, start, end - start);
				*end = '\0';
			}
		}
	}
	eventData->alpha = (eventData->file->fileAttrib & ATTRIB_HIDDEN) || *getRelativeName(eventData->file->name) == '.' ? 128 : 255;
	eventData->isAutoLoadEntry = !strcmp(swissSettings.autoload, file->name) || !fnmatch(swissSettings.autoload, file->name, FNM_PATHNAME | FNM_PREFIX_DIRS);
	
	uiDrawObj_t *event = calloc(1, sizeof(uiDrawObj_t));
	event->type = EV_FILEBROWSERBUTTON;
	event->data = eventData;
	return event;
}

uiDrawObj_t* DrawFileBrowserButtonMeta(int x1, int y1, int x2, int y2, const char *message, file_handle *file, int mode) {
	if(file->meta && file->meta->displayName) {
		message = file->meta->displayName;
	}
	return DrawFileBrowserButton(x1, y1, x2, y2, message, file, mode);
}

uiDrawObj_t* DrawFileCarouselEntry(int x1, int y1, int x2, int y2, const char *message, file_handle *file, int distFromMiddle) {
	uiDrawObj_t* event = DrawFileBrowserButtonMeta(x1, y1, x2, y2, message, file, B_SELECTED);
	drawFileBrowserButtonEvent_t *data = (drawFileBrowserButtonEvent_t*)event->data;
	data->isCarousel = true;
	data->distFromMiddle = distFromMiddle;
	//print_debug("message %s dist = %i x: (%i -> %i) y: (%i -> %i)\n", message, distFromMiddle, x1, x2, y1, y2);
	return event;
}

// Internal
static void _DrawEmptyBox(uiDrawObj_t *evt) {
	drawBoxEvent_t *data = (drawBoxEvent_t*)evt->data;
	
	GXColor borderColor = (GXColor) {200,200,200,GUI_MSGBOX_ALPHA}; //Silver
	
	_DrawSimpleBox(data->x1, data->y1, data->x2-data->x1, data->y2-data->y1, 0, data->backfill, borderColor);
}

// External
uiDrawObj_t* DrawEmptyBox(int x1, int y1, int x2, int y2) 
{
	int borderSize;
	borderSize = (y2-y1) <= 30 ? 3 : 10;
	x1-=borderSize;x2+=borderSize;y1-=borderSize;y2+=borderSize;
	
	drawBoxEvent_t *eventData = calloc(1, sizeof(drawBoxEvent_t));
	eventData->x1 = x1;
	eventData->y1 = y1;
	eventData->x2 = x2;
	eventData->y2 = y2;
	eventData->backfill = (GXColor) {0,0,0, swissSettings.disablePanelTransparency ? GUI_MSGBOX_ALPHA : GUI_PANEL_ALPHA}; //Black — Phase 2: translucent unless disabled
	uiDrawObj_t *event = calloc(1, sizeof(uiDrawObj_t));
	event->type = EV_EMPTYBOX;
	event->data = eventData;
	return event;
}

// External
uiDrawObj_t* DrawEmptyColouredBox(int x1, int y1, int x2, int y2, GXColor colour) 
{
	int borderSize;
	borderSize = (y2-y1) <= 30 ? 3 : 10;
	x1-=borderSize;x2+=borderSize;y1-=borderSize;y2+=borderSize;
	
	drawBoxEvent_t *eventData = calloc(1, sizeof(drawBoxEvent_t));
	eventData->x1 = x1;
	eventData->y1 = y1;
	eventData->x2 = x2;
	eventData->y2 = y2;
	eventData->backfill = colour;
	uiDrawObj_t *event = calloc(1, sizeof(uiDrawObj_t));
	event->type = EV_EMPTYBOX;
	event->data = eventData;
	return event;
}

// Internal
static void _DrawTransparentBox(uiDrawObj_t *evt) {
	drawBoxEvent_t *data = (drawBoxEvent_t*)evt->data;
	
	GXColor borderColor = (GXColor) {200,200,200,GUI_MSGBOX_ALPHA}; //Silver
	
	_DrawSimpleBox( data->x1, data->y1, data->x2-data->x1, data->y2-data->y1, 0, data->backfill, borderColor);
}

// External
uiDrawObj_t* DrawTransparentBox(int x1, int y1, int x2, int y2) 
{
	int borderSize;
	borderSize = (y2-y1) <= 30 ? 3 : 10;
	x1-=borderSize;x2+=borderSize;y1-=borderSize;y2+=borderSize;

	drawBoxEvent_t *eventData = calloc(1, sizeof(drawBoxEvent_t));
	eventData->x1 = x1;
	eventData->y1 = y1;
	eventData->x2 = x2;
	eventData->y2 = y2;
	eventData->backfill = (GXColor) {0,0,0,0};
	uiDrawObj_t *event = calloc(1, sizeof(uiDrawObj_t));
	event->type = EV_TRANSPARENTBOX;
	event->data = eventData;
	return event;
}

uiDrawObj_t* DrawSettingsFocus(int x1, int y1, int x2, int y2)
{
	int borderSize = (y2 - y1) <= 30 ? 3 : 10;
	drawSettingsFocusEvent_t *eventData =
		calloc(1, sizeof(drawSettingsFocusEvent_t));
	uiDrawObj_t *event = calloc(1, sizeof(uiDrawObj_t));

	eventData->target.x = (short)(x1 - borderSize);
	eventData->target.y = (short)(y1 - borderSize);
	eventData->target.w = (short)(x2 - x1 + borderSize * 2);
	eventData->target.h = (short)(y2 - y1 + borderSize * 2);
	event->type = EV_SETTINGSFOCUS;
	event->data = eventData;
	return event;
}

typedef struct systemDialPoint {
	float x;
	float y;
} systemDialPoint_t;

static const systemDialPoint_t systemDialCircle[24] = {
	{1.000000f, 0.000000f}, {0.965926f, 0.258819f},
	{0.866025f, 0.500000f}, {0.707107f, 0.707107f},
	{0.500000f, 0.866025f}, {0.258819f, 0.965926f},
	{0.000000f, 1.000000f}, {-0.258819f, 0.965926f},
	{-0.500000f, 0.866025f}, {-0.707107f, 0.707107f},
	{-0.866025f, 0.500000f}, {-0.965926f, 0.258819f},
	{-1.000000f, 0.000000f}, {-0.965926f, -0.258819f},
	{-0.866025f, -0.500000f}, {-0.707107f, -0.707107f},
	{-0.500000f, -0.866025f}, {-0.258819f, -0.965926f},
	{0.000000f, -1.000000f}, {0.258819f, -0.965926f},
	{0.500000f, -0.866025f}, {0.707107f, -0.707107f},
	{0.866025f, -0.500000f}, {0.965926f, -0.258819f}
};

static void _SetupRasterColor(void)
{
	GX_SetNumTexGens(0);
	GX_SetNumIndStages(0);
	GX_SetNumTevStages(1);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_ENABLE, GX_TEVPREV);
	GX_SetTevDirect(GX_TEVSTAGE0);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
}

/* ------------------------------------------------------------------------
 * Button icons in hint lines. "A  OPEN" draws the A button, then "OPEN",
 * like a game's on-screen prompts. Shapes are drawn like the title-bar dial:
 * a solid core and a one-pixel feathered rim instead of MSAA. Colours are
 * the GameCube controller's; letters use the console's own font.
 * --------------------------------------------------------------------- */
#define HINT_POINTS_MAX 40
#define HINT_QUARTER_TURN 1.5707963f

static void _HintVertex(float x, float y, GXColor color)
{
	GX_Position3f32(x, y, 0.0f);
	GX_Color4u8(color.r, color.g, color.b, color.a);
	GX_TexCoord2f32(0.0f, 0.0f);
}

/* A convex outline round (cx, cy): a fan for the core, then a rim pushed
 * out one pixel along each corner's normal, fading to clear. */
static void _HintShape(float cx, float cy, const float (*points)[2], int count,
	GXColor color)
{
	GXColor clear = color;
	int i;

	clear.a = 0;
	GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT0, (u16)(count + 2));
	_HintVertex(cx, cy, color);
	for(i = 0; i <= count; i++) {
		_HintVertex(points[i % count][0], points[i % count][1], color);
	}
	GX_End();
	GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT0, (u16)((count + 1) * 2));
	for(i = 0; i <= count; i++) {
		const float *prev = points[(i + count - 1) % count];
		const float *here = points[i % count];
		const float *next = points[(i + 1) % count];
		float nx = next[1] - prev[1];
		float ny = prev[0] - next[0];
		float length = sqrtf(nx * nx + ny * ny);

		if(length <= 0.0f) {
			nx = here[0] - cx;
			ny = here[1] - cy;
			length = sqrtf(nx * nx + ny * ny);
		}
		if(length > 0.0f) {
			nx /= length;
			ny /= length;
		}
		if(nx * (here[0] - cx) + ny * (here[1] - cy) < 0.0f) {
			nx = -nx;
			ny = -ny;
		}
		_HintVertex(here[0], here[1], color);
		_HintVertex(here[0] + nx, here[1] + ny, clear);
	}
	GX_End();
}

/* A rectangle with round corners; radius = half the height makes a pill. */
static void _HintRoundRect(float cx, float cy, float width, float height,
	float radius, GXColor color)
{
	static const int steps = 6;
	float points[HINT_POINTS_MAX][2];
	float halfW = width / 2.0f - radius;
	float halfH = height / 2.0f - radius;
	int corner, i, count = 0;

	for(corner = 0; corner < 4; corner++) {
		float ox = (corner == 0 || corner == 3) ? halfW : -halfW;
		float oy = (corner < 2) ? halfH : -halfH;

		for(i = 0; i <= steps; i++) {
			float angle = HINT_QUARTER_TURN * ((float)corner + (float)i / (float)steps);

			points[count][0] = cx + ox + radius * cosf(angle);
			points[count][1] = cy + oy + radius * sinf(angle);
			count++;
		}
	}
	_HintShape(cx, cy, points, count, color);
}

static void _HintDisc(float cx, float cy, float radius, GXColor color)
{
	_HintRoundRect(cx, cy, radius * 2.0f, radius * 2.0f, radius, color);
}

static GXColor _HintAlpha(GXColor color, u8 alpha)
{
	color.a = (u8)(((u16)color.a * alpha) / 255u);
	return color;
}

/* A regular octagon, flat side up: the control stick's gate. */
static void _HintOctagon(float cx, float cy, float radius, GXColor color)
{
	float points[8][2];
	int i;

	for(i = 0; i < 8; i++) {
		float angle = HINT_QUARTER_TURN * ((float)i + 0.5f) / 2.0f;

		points[i][0] = cx + radius * cosf(angle);
		points[i][1] = cy + radius * sinf(angle);
	}
	_HintShape(cx, cy, points, 8, color);
}

static void _HintLetter(float cx, float cy, const char *letter, float scale,
	GXColor color)
{
	drawStringMediumUntinted((int)(cx + 0.5f), (int)(cy + 0.5f), letter, scale,
		ALIGN_CENTER, color);
	drawInit();
	_SetupRasterColor();
}

/* One icon centred on (cx, cy), size tall and width wide. Letters are about
 * the label's own size so they stay legible on a 480-line screen. */
static void _DrawHintGlyph(uiHintGlyph_t glyph, float cx, float cy, float size,
	float width, float scale, u8 alpha)
{
	const GXColor grey = _HintAlpha((GXColor) {202, 202, 214, 255}, alpha);
	const GXColor ink = _HintAlpha((GXColor) {42, 40, 56, 255}, alpha);
	const GXColor white = _HintAlpha((GXColor) {255, 255, 255, 255}, alpha);

	drawInit();
	_SetupRasterColor();
	switch(glyph) {
		case UI_HINT_GLYPH_A:
			_HintDisc(cx, cy, width / 2.0f, _HintAlpha((GXColor) {0, 170, 122, 255}, alpha));
			_HintLetter(cx, cy, "A", scale, white);
			break;
		case UI_HINT_GLYPH_B:
			_HintDisc(cx, cy, width / 2.0f, _HintAlpha((GXColor) {222, 48, 56, 255}, alpha));
			_HintLetter(cx, cy, "B", scale * 0.9f, white);
			break;
		case UI_HINT_GLYPH_X:
			_HintRoundRect(cx, cy, width, size, width / 2.0f, grey);
			_HintLetter(cx, cy, "X", scale * 0.9f, ink);
			break;
		case UI_HINT_GLYPH_Y:
			_HintRoundRect(cx, cy, width, size * 0.78f, size * 0.39f, grey);
			_HintLetter(cx, cy, "Y", scale * 0.9f, ink);
			break;
		case UI_HINT_GLYPH_Z:
			_HintRoundRect(cx, cy, width, size * 0.8f, size * 0.22f,
				_HintAlpha((GXColor) {122, 86, 214, 255}, alpha));
			_HintLetter(cx, cy, "Z", scale * 0.86f, white);
			break;
		case UI_HINT_GLYPH_L:
		case UI_HINT_GLYPH_R:
			_HintRoundRect(cx, cy, width, size * 0.8f, size * 0.26f, grey);
			_HintLetter(cx, cy, glyph == UI_HINT_GLYPH_L ? "L" : "R",
				scale * 0.86f, ink);
			break;
		case UI_HINT_GLYPH_START:
			_HintRoundRect(cx, cy, width, size * 0.86f, size * 0.43f, grey);
			_HintLetter(cx, cy, "START", scale * UI_HINT_START_TEXT_SCALE, ink);
			break;
		case UI_HINT_GLYPH_STICK:
			/* The stick from above: its octagonal gate, then the cap. */
			_HintOctagon(cx, cy, size * 0.54f,
				_HintAlpha((GXColor) {104, 102, 124, 255}, alpha));
			_HintDisc(cx, cy, size * 0.3f, grey);
			break;
		case UI_HINT_GLYPH_DPAD:
			_HintRoundRect(cx, cy, size, size * 0.36f, size * 0.08f, grey);
			_HintRoundRect(cx, cy, size * 0.36f, size, size * 0.08f, grey);
			break;
		default:
			break;
	}
}

int GetHintSizeInPixels(const char *text)
{
	return (int)ceilf(UIHint_LineWidth(text, GetFontHeight(1.0f), 1.0f,
		GetTextSizeInPixels));
}

/* The largest scale up to max at which a hint line fits width, like
 * GetTextScaleToFitInWidthWithMax with the button icons measured. */
float GetHintScaleToFitInWidthWithMax(const char *text, int width, float maximum)
{
	int natural = GetHintSizeInPixels(text);

	return natural > 0 && (float)natural * maximum > (float)width ?
		(float)width / (float)natural : maximum;
}

/* One hint line: button names become icons, the rest stays text. x is the
 * line's left edge, centre or right edge as align says; y is its middle. */
static void _DrawHintText(int x, int y, const char *text, float scale, int align,
	GXColor color)
{
	uiHintItem_t items[UI_HINT_MAX_ITEMS];
	char label[UI_HINT_MAX_ITEMS][UI_HINT_LABEL_CAPACITY];
	float size = UIHint_GlyphSize(GetFontHeight(1.0f), scale);
	float total = 0.0f;
	float cursor;
	int count = UIHint_Parse(text, items, UI_HINT_MAX_ITEMS);
	int i, g;

	for(i = 0; i < count; i++) {
		total += UIHint_ItemWidth(&items[i], size, scale, GetTextSizeInPixels,
			label[i], sizeof(label[i])) + (i > 0 ? size * UI_HINT_ITEM_GAP : 0.0f);
	}
	cursor = (float)x - (align == ALIGN_CENTER ? total / 2.0f :
		(align == ALIGN_RIGHT ? total : 0.0f));
	for(i = 0; i < count; i++) {
		for(g = 0; g < items[i].glyphCount; g++) {
			float width = UIHint_GlyphWidth(items[i].glyph[g], size, scale,
				GetTextSizeInPixels);

			if(g > 0 && items[i].chord) {
				/* A chord: the two buttons are held together. */
				drawStringMedium((int)(cursor + size * UI_HINT_ALTERNATIVE_GAP + 0.5f), y,
					"+", scale, ALIGN_LEFT, color);
			}
			if(g > 0) {
				cursor += UIHint_JoinWidth(&items[i], size, scale, GetTextSizeInPixels);
			}
			_DrawHintGlyph(items[i].glyph[g], cursor + width / 2.0f, (float)y, size,
				width, scale, color.a);
			cursor += width;
		}
		if(label[i][0] != '\0') {
			if(items[i].glyphCount > 0) {
				cursor += size * UI_HINT_LABEL_GAP;
			}
			drawStringMedium((int)(cursor + 0.5f), y, label[i], scale, ALIGN_LEFT, color);
			cursor += (float)GetTextSizeInPixels(label[i]) * scale;
		}
		cursor += size * UI_HINT_ITEM_GAP;
	}
}

static void _PutSystemDialVertex(float centerX, float centerY, float radius,
		const systemDialPoint_t *point, GXColor color)
{
	UIColor_Apply(&color.r, &color.g, &color.b);
	GX_Position3f32(centerX + (point->x * radius), centerY + (point->y * radius), 0.0f);
	GX_Color4u8(color.r, color.g, color.b, color.a);
	GX_TexCoord2f32(0.0f, 0.0f);
}

static void _DrawSystemRing(float centerX, float centerY, float radius,
		float thickness, int start, int segments, GXColor color)
{
	/* Three joined bands provide one native pixel of coverage at both edges;
	 * preserve the original stroke's integrated width without MSAA. */
	float core = fmaxf(0.0f, thickness - 0.5f);
	float offsets[4] = {-core - 1.0f, -core, core, core + 1.0f};
	for(int band = 0; band < 3; band++) {
		GXColor a = color, b = color;
		if(band == 0) a.a = 0;
		if(band == 2) b.a = 0;
		GX_Begin(GX_TRIANGLESTRIP, GX_VTXFMT0, (segments + 1) * 2);
		for(int i = 0; i <= segments; i++) {
			const systemDialPoint_t *point = &systemDialCircle[(start + i) % 24];
			_PutSystemDialVertex(centerX, centerY, radius + offsets[band], point, a);
			_PutSystemDialVertex(centerX, centerY, radius + offsets[band + 1], point, b);
		}
		GX_End();
	}
}

static void _DrawSystemDial(float centerX, float centerY, s8 coreTemperature,
		const uiClockFrame_t *clock, u8 opacity)
{
	int temperatureSegments = coreTemperature < 20 ? 0 :
		(coreTemperature > 80 ? 24 : (coreTemperature - 20) * 24 / 60);

	drawInit();
	_SetupRasterColor();
	_DrawSystemRing(centerX, centerY, 19.0f, 0.8f, 0, 24,
		(GXColor) {122, 112, 201, (u8)((62 * opacity) / 255)});
	if(temperatureSegments > 0) {
		_DrawSystemRing(centerX, centerY, 16.0f, 1.45f, 18, temperatureSegments,
			(GXColor) {183, 171, 238, (u8)((196 * opacity) / 255)});
	}
	if(clock != NULL && clock->available) {
		float markerX = clock->secondX;
		float markerY = -clock->secondY;
		GXColor marker = {226, 221, 255, 255};
		UIColor_Apply(&marker.r, &marker.g, &marker.b);
		GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
			GX_Position3f32(centerX + markerX * 18.0f - 1.5f, centerY + markerY * 18.0f - 1.5f, 0.0f);
			GX_Color4u8(marker.r, marker.g, marker.b, (230 * opacity) / 255); GX_TexCoord2f32(0.0f, 0.0f);
			GX_Position3f32(centerX + markerX * 18.0f + 1.5f, centerY + markerY * 18.0f - 1.5f, 0.0f);
			GX_Color4u8(marker.r, marker.g, marker.b, (210 * opacity) / 255); GX_TexCoord2f32(0.0f, 0.0f);
			GX_Position3f32(centerX + markerX * 18.0f + 1.5f, centerY + markerY * 18.0f + 1.5f, 0.0f);
			GX_Color4u8(marker.r, marker.g, marker.b, (180 * opacity) / 255); GX_TexCoord2f32(0.0f, 0.0f);
			GX_Position3f32(centerX + markerX * 18.0f - 1.5f, centerY + markerY * 18.0f + 1.5f, 0.0f);
			GX_Color4u8(marker.r, marker.g, marker.b, (210 * opacity) / 255); GX_TexCoord2f32(0.0f, 0.0f);
		GX_End();
	}
	drawInit();
}

static void _UpdateSystemInstrument(void)
{
	struct timeval now;

	if(gettimeofday(&now, NULL) != 0) {
		/* Civil time and thermal telemetry are independent instruments. */
		if(!systemInstrument.temperatureSampled) {
			systemInstrument.coreTemperature = SYS_GetCoreTemperature();
			if(systemInstrument.coreTemperature >= 0) {
				(void)snprintf(systemInstrument.temperatureText,
					sizeof(systemInstrument.temperatureText), "%i\260C",
					systemInstrument.coreTemperature);
			}
			else {
				systemInstrument.temperatureText[0] = '\0';
			}
			systemInstrument.temperatureSampled = true;
		}
		(void)UIClock_Compose(&systemInstrument.clock, -1, -1, -1.0f);
		systemInstrument.civilSecondSampled = false;
		systemInstrument.civilTimeAvailable = false;
		memcpy(systemInstrument.timeText, "--:--:--", 9u);
		return;
	}
	if(!systemInstrument.civilSecondSampled ||
		now.tv_sec != systemInstrument.sampledSecond) {
		struct tm localTime;
		if(localtime_r(&now.tv_sec, &localTime) != NULL) {
			systemInstrument.hour = localTime.tm_hour;
				systemInstrument.minute = localTime.tm_min;
				systemInstrument.second = localTime.tm_sec;
				systemInstrument.civilTimeAvailable = true;
				/* Refresh on every sampled second so timezone/RTC corrections in
				 * the same minute cannot leave stale text. */
				(void)strftime(systemInstrument.timeText,
					sizeof(systemInstrument.timeText), "%H:%M:%S", &localTime);
		}
		else {
			systemInstrument.civilTimeAvailable = false;
			memcpy(systemInstrument.timeText, "--:--:--", 9u);
		}
		systemInstrument.coreTemperature = SYS_GetCoreTemperature();
		if(systemInstrument.coreTemperature >= 0) {
			(void)snprintf(systemInstrument.temperatureText,
				sizeof(systemInstrument.temperatureText), "%i\260C",
				systemInstrument.coreTemperature);
		}
		else {
			systemInstrument.temperatureText[0] = '\0';
		}
		systemInstrument.temperatureSampled = true;
		systemInstrument.sampledSecond = now.tv_sec;
		systemInstrument.civilSecondSampled = true;
	}
	if(systemInstrument.civilTimeAvailable) {
		(void)UIClock_Compose(&systemInstrument.clock,
			systemInstrument.hour, systemInstrument.minute,
			(float)systemInstrument.second +
				(float)now.tv_usec / 1000000.0f);
	}
	else {
		(void)UIClock_Compose(&systemInstrument.clock, -1, -1, -1.0f);
	}
}

// Internal
static void _DrawTitleBar(uiDrawObj_t *evt) {
	float reveal = UIScene_Frame()->chromeProgress;
	int offsetY;
	GXColor textColor;

	(void)evt;
	if(reveal <= 0.0f) {
		return;
	}
	if(reveal > 1.0f) {
		reveal = 1.0f;
	}
	offsetY = (int)((reveal - 1.0f) * 18.0f);
	textColor = (GXColor) {209, 201, 255, (u8)(232.0f * reveal)};

	/* A single pre-traversal instrument snapshot owns both header and cube. */
	_DrawSystemDial(600.0f, 43.0f + offsetY,
		systemInstrument.coreTemperature, &systemInstrument.clock,
		(u8)(255.0f * reveal));
	if(systemInstrument.temperatureText[0]) {
		drawStringMedium(600, 43 + offsetY,
			systemInstrument.temperatureText,
			0.42f, ALIGN_CENTER, textColor);
	}
	if(systemInstrument.clock.available) {
		drawStringMedium(568, 43 + offsetY, systemInstrument.timeText,
			0.54f, ALIGN_RIGHT, textColor);
	}
}

// External
uiDrawObj_t* DrawTitleBar()
{
	uiDrawObj_t *event = calloc(1, sizeof(uiDrawObj_t));
	event->type = EV_TITLEBAR;
	return event;
}

static uiMotionMode_t _CurrentMotionMode(void)
{
	/* Backdrop animation is decorative. Disabling it must not silently weaken
	 * primary navigation, focus, or scene-transition motion. */
	return UIMotion_ModeFromFlags(swissSettings.disableUIAnimations,
		swissSettings.reduceUIAnimations);
}

/* Retained settings focus is deliberately local to one appended event type.
 * The immutable event supplies only a target rectangle; the video thread owns
 * and advances the bounded springs. Page objects are rebuilt after each input,
 * so a short continuity window carries motion across replacement while a real
 * exit/re-entry snaps cleanly to the new target. */
static void _DrawSettingsFocus(uiDrawObj_t *evt)
{
	drawSettingsFocusEvent_t *data =
		(drawSettingsFocusEvent_t*)evt->data;
	uiSettingsFocusFrame_t frame;
	uiMotionMode_t mode = _CurrentMotionMode();
	GXColor fillColor = swissSettings.disablePanelTransparency ?
		(GXColor) {78, 62, 158, 154} : (GXColor) {68, 50, 148, 92};
	GXColor borderColor = (GXColor) {211, 202, 255, 210};
	int x;
	int y;
	int width;
	int height;

	if(!settingsFocusState.initialized ||
		!UISettingsFocus_IsContinuous(videoFrameSerial,
			settingsFocusLastDrawFrame,
			SETTINGS_FOCUS_CONTINUITY_FRAMES)) {
		UISettingsFocus_Init(&settingsFocusState, &data->target);
	}
	else {
		UISettingsFocus_Retarget(&settingsFocusState, &data->target, mode);
	}
	UISettingsFocus_Update(&settingsFocusState, UIAnim_Delta(), mode, &frame);
	settingsFocusLastDrawFrame = videoFrameSerial;
	x = (int)(frame.x + 0.5f);
	y = (int)(frame.y + 0.5f);
	width = (int)(frame.w + 0.5f);
	height = (int)(frame.h + 0.5f);
	_DrawSimpleBox(x, y, width, height, 0, fillColor, borderColor);
}

typedef struct gameflowPoint {
	float x;
	float y;
} gameflowPoint_t;

typedef struct gameflowQuad {
	gameflowPoint_t point[4];
} gameflowQuad_t;

typedef struct gameflowRenderCard {
	const uiGameflowCardSnapshot_t *record;
	u32 recordIndex;
	float visualSlot;
	float presence;
	gameflowQuad_t quad;
} gameflowRenderCard_t;

static const gameflowQuad_t gameflowSlotPoses[7] = {
	{{{-28.0f, 185.0f}, {10.0f, 174.0f}, {10.0f, 244.0f}, {-28.0f, 233.0f}}},
	{{{32.0f, 159.0f}, {52.0f, 151.0f}, {52.0f, 266.0f}, {32.0f, 258.0f}}},
	{{{78.0f, 132.0f}, {208.0f, 121.0f}, {208.0f, 295.0f}, {78.0f, 284.0f}}},
	{{{230.0f, 88.0f}, {410.0f, 88.0f}, {410.0f, 328.0f}, {230.0f, 328.0f}}},
	{{{432.0f, 121.0f}, {562.0f, 132.0f}, {562.0f, 284.0f}, {432.0f, 295.0f}}},
	{{{588.0f, 151.0f}, {608.0f, 159.0f}, {608.0f, 258.0f}, {588.0f, 266.0f}}},
	{{{630.0f, 174.0f}, {668.0f, 185.0f}, {668.0f, 233.0f}, {630.0f, 244.0f}}}
};

static float _GameflowClamp(float value, float minimum, float maximum)
{
	if(value < minimum) {
		return minimum;
	}
	if(value > maximum) {
		return maximum;
	}
	return value;
}

static float _GameflowRound(float value)
{
	return floorf(value + 0.5f);
}

static gameflowPoint_t _GameflowLerpPoint(gameflowPoint_t from,
	gameflowPoint_t to, float progress)
{
	gameflowPoint_t point = {
		from.x + (to.x - from.x) * progress,
		from.y + (to.y - from.y) * progress
	};
	return point;
}

static gameflowQuad_t _GameflowSamplePose(float slot)
{
	gameflowQuad_t result;
	float clamped = _GameflowClamp(slot, -3.0f, 3.0f);
	int lower = (int)floorf(clamped);
	int upper = lower < 3 ? lower + 1 : lower;
	float progress = clamped - (float)lower;
	int i;

	for(i = 0; i < 4; ++i) {
		result.point[i] = _GameflowLerpPoint(
			gameflowSlotPoses[lower + 3].point[i],
			gameflowSlotPoses[upper + 3].point[i], progress);
		result.point[i].x = _GameflowRound(result.point[i].x);
		result.point[i].y = _GameflowRound(result.point[i].y);
	}
	return result;
}

static gameflowPoint_t _GameflowQuadPoint(const gameflowQuad_t *quad,
	float u, float v)
{
	gameflowPoint_t top = _GameflowLerpPoint(quad->point[0], quad->point[1], u);
	gameflowPoint_t bottom = _GameflowLerpPoint(quad->point[3], quad->point[2], u);
	return _GameflowLerpPoint(top, bottom, v);
}

static gameflowQuad_t _GameflowInsetQuad(const gameflowQuad_t *quad,
	float horizontal, float vertical)
{
	gameflowQuad_t result = {{
		_GameflowQuadPoint(quad, horizontal, vertical),
		_GameflowQuadPoint(quad, 1.0f - horizontal, vertical),
		_GameflowQuadPoint(quad, 1.0f - horizontal, 1.0f - vertical),
		_GameflowQuadPoint(quad, horizontal, 1.0f - vertical)
	}};
	return result;
}

static gameflowQuad_t _GameflowRectQuad(const gameflowQuad_t *quad,
	float left, float top, float right, float bottom)
{
	gameflowQuad_t result = {{
		_GameflowQuadPoint(quad, left, top),
		_GameflowQuadPoint(quad, right, top),
		_GameflowQuadPoint(quad, right, bottom),
		_GameflowQuadPoint(quad, left, bottom)
	}};
	return result;
}

static float _GameflowPresence(float visualSlot)
{
	static const float presence[4] = {1.0f, 0.72f, 0.52f, 0.0f};
	float distance = fabsf(visualSlot);
	int lower;
	int upper;

	if(distance >= 3.0f) {
		return 0.0f;
	}
	lower = (int)floorf(distance);
	upper = lower + 1;
	return presence[lower] + (presence[upper] - presence[lower]) *
		(distance - (float)lower);
}

static u8 _GameflowAlpha(float value)
{
	return (u8)(_GameflowClamp(value, 0.0f, 255.0f) + 0.5f);
}

static void _GameflowPutVertex(gameflowPoint_t point, GXColor color)
{
	UIColor_Apply(&color.r, &color.g, &color.b);
	GX_Position3f32(point.x, point.y, 0.0f);
	GX_Color4u8(color.r, color.g, color.b, color.a);
	GX_TexCoord2f32(0.0f, 0.0f);
}

static void _GameflowPutQuad(const gameflowQuad_t *quad, GXColor top,
	GXColor bottom)
{
	_GameflowPutVertex(quad->point[0], top);
	_GameflowPutVertex(quad->point[1], top);
	_GameflowPutVertex(quad->point[2], bottom);
	_GameflowPutVertex(quad->point[3], bottom);
}

static void _GameflowPutBorder(const gameflowQuad_t *outer,
	const gameflowQuad_t *inner, GXColor color)
{
	int i;
	for(i = 0; i < 4; ++i) {
		int next = (i + 1) % 4;
		_GameflowPutVertex(outer->point[i], color);
		_GameflowPutVertex(outer->point[next], color);
		_GameflowPutVertex(inner->point[next], color);
		_GameflowPutVertex(inner->point[i], color);
	}
}

static GXColor _GameflowAccent(const uiGameflowCardSnapshot_t *record,
	u8 alpha)
{
	static const GXColor palette[4] = {
		{114, 101, 186, 255}, {82, 112, 171, 255},
		{129, 93, 164, 255}, {83, 127, 150, 255}
	};
	u32 hash = 2166136261u;
	const unsigned char *text = (const unsigned char *)(record->gameId[0] ?
		record->gameId : record->title);
	GXColor color;

	while(*text) {
		hash = (hash ^ *text++) * 16777619u;
	}
	color = palette[hash & 3u];
	if(record->flags & UI_GAMEFLOW_CARD_PARENT) {
		color = (GXColor) {102, 103, 130, 255};
	}
	color.a = alpha;
	return color;
}

static const uiGameflowCardSnapshot_t *_GameflowFindRecord(
	const uiGameflowRenderSnapshot_t *snapshot, u32 libraryIndex,
	u32 *recordIndex)
{
	u32 i;
	for(i = 0u; i < snapshot->recordCount; ++i) {
		if((snapshot->records[i].flags & UI_GAMEFLOW_CARD_VALID) &&
			snapshot->records[i].libraryIndex == libraryIndex) {
			if(recordIndex != NULL) {
				*recordIndex = i;
			}
			return &snapshot->records[i];
		}
	}
	return NULL;
}

static void _GameflowDrawBanner(const gameflowQuad_t *quad,
	GXTexObj *texture, u8 alpha)
{
	drawInit();
	GX_SetNumTevStages(1);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	GX_InvalidateTexAll();
	GX_LoadTexObj(texture, GX_TEXMAP0);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		GX_Position3f32(quad->point[0].x, quad->point[0].y, 0.0f);
		GX_Color4u8(255, 255, 255, alpha);
		GX_TexCoord2f32(0.0f, 0.0f);
		GX_Position3f32(quad->point[1].x, quad->point[1].y, 0.0f);
		GX_Color4u8(255, 255, 255, alpha);
		GX_TexCoord2f32(1.0f, 0.0f);
		GX_Position3f32(quad->point[2].x, quad->point[2].y, 0.0f);
		GX_Color4u8(255, 255, 255, alpha);
		GX_TexCoord2f32(1.0f, 1.0f);
		GX_Position3f32(quad->point[3].x, quad->point[3].y, 0.0f);
		GX_Color4u8(255, 255, 255, alpha);
		GX_TexCoord2f32(0.0f, 1.0f);
	GX_End();
}

static void _GameflowDrawPoster(const gameflowRenderCard_t *card,
	GXTexObj *texture, float reveal)
{
	gameflowQuad_t content = _GameflowInsetQuad(&card->quad,
		0.033333f, 0.033333f);
	u8 alpha = _GameflowAlpha(255.0f * card->presence * reveal);

	/* The pack stores 192x256 retail-cover content on a 256x256 GX canvas.
	 * Stop at s=0.75 so the edge-extended right padding is never shown. */
	drawInit();
	GX_SetNumTevStages(1);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA,
		GX_LO_CLEAR);
	GX_InvalidateTexAll();
	GX_LoadTexObj(texture, GX_TEXMAP0);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
		GX_Position3f32(content.point[0].x, content.point[0].y, 0.0f);
		GX_Color4u8(255, 255, 255, alpha);
		GX_TexCoord2f32(0.0f, 0.0f);
		GX_Position3f32(content.point[1].x, content.point[1].y, 0.0f);
		GX_Color4u8(255, 255, 255, alpha);
		GX_TexCoord2f32(0.75f, 0.0f);
		GX_Position3f32(content.point[2].x, content.point[2].y, 0.0f);
		GX_Color4u8(255, 255, 255, alpha);
		GX_TexCoord2f32(0.75f, 1.0f);
		GX_Position3f32(content.point[3].x, content.point[3].y, 0.0f);
		GX_Color4u8(255, 255, 255, alpha);
		GX_TexCoord2f32(0.0f, 1.0f);
	GX_End();

	/* A following card may still be on its BNR/procedural fallback while
	 * this poster finishes loading. Do not leak the bound cover texture or
	 * textured TEV state into that code-native geometry. */
	drawInit();
	_SetupRasterColor();
}

/* A game with settings of its own: a small sliders mark, the Settings
 * face's emblem, in the top-right corner of its cover. */
static void _GameflowDrawCustomMark(const gameflowRenderCard_t *card, float reveal)
{
	static const float knob[3] = {0.66f, 0.28f, 0.50f};
	gameflowPoint_t top = _GameflowQuadPoint(&card->quad, 1.0f, 0.0f);
	gameflowPoint_t bottom = _GameflowQuadPoint(&card->quad, 1.0f, 1.0f);
	gameflowPoint_t center = _GameflowQuadPoint(&card->quad, 0.85f, 0.085f);
	float size = fabsf(bottom.y - top.y) * 0.13f;
	float bar = fmaxf(1.4f, size * 0.08f);
	u8 alpha = _GameflowAlpha(255.0f * card->presence * reveal);
	GXColor plate = {22, 17, 46, (u8)((alpha * 220u) / 255u)};
	GXColor line = {206, 196, 255, alpha};
	int k;

	UIColor_Apply(&plate.r, &plate.g, &plate.b);
	UIColor_Apply(&line.r, &line.g, &line.b);
	drawInit();
	_SetupRasterColor();
	_HintRoundRect(center.x, center.y, size, size, size * 0.24f, plate);
	for(k = 0; k < 3; k++) {
		float y = center.y + (float)(k - 1) * size * 0.25f;

		_HintRoundRect(center.x, y, size * 0.64f, bar, bar / 2.0f, line);
		_HintRoundRect(center.x - size * 0.32f + knob[k] * size * 0.64f, y,
			size * 0.17f, size * 0.17f, size * 0.05f, line);
	}
}

static void _GameflowDrawFallback(const gameflowRenderCard_t *card,
	uiGameflowLibraryArtwork_t artwork, GXTexObj *bannerTexture,
	float reveal)
{
	uiGameflowLibraryFallbackLayout_t layout;
	gameflowQuad_t surface;
	gameflowQuad_t upperFacet;
	gameflowQuad_t lowerFacet;
	gameflowQuad_t spine;
	gameflowQuad_t identity;
	gameflowQuad_t identityRule;
	GXColor accent;
	GXColor surfaceTop;
	GXColor surfaceBottom;
	GXColor facetUpper;
	GXColor facetLower;
	GXColor dark;
	u8 alpha;

	if(!UIGameflowLibrary_BuildFallbackLayout(card->visualSlot, &layout)) {
		return;
	}
	alpha = _GameflowAlpha(255.0f * card->presence * reveal);
	accent = _GameflowAccent(card->record,
		_GameflowAlpha(220.0f * card->presence * reveal));
	surfaceTop = accent;
	surfaceTop.r = (u8)((surfaceTop.r + 24u) / 2u);
	surfaceTop.g = (u8)((surfaceTop.g + 20u) / 2u);
	surfaceTop.b = (u8)((surfaceTop.b + 54u) / 2u);
	surfaceTop.a = _GameflowAlpha(178.0f * card->presence * reveal);
	surfaceBottom = (GXColor) {7, 6, 23,
		_GameflowAlpha(238.0f * card->presence * reveal)};
	facetUpper = accent;
	facetUpper.a = _GameflowAlpha(68.0f * card->presence * reveal);
	facetLower = (GXColor) {92, 76, 151,
		_GameflowAlpha(76.0f * card->presence * reveal)};
	dark = (GXColor) {6, 5, 19,
		_GameflowAlpha(218.0f * card->presence * reveal)};

	surface = _GameflowRectQuad(&card->quad, layout.contentInset,
		layout.contentInset, 1.0f - layout.contentInset,
		1.0f - layout.contentInset);
	upperFacet = (gameflowQuad_t) {{
		_GameflowQuadPoint(&card->quad, 0.055f, 0.10f),
		_GameflowQuadPoint(&card->quad, 0.945f, 0.37f),
		_GameflowQuadPoint(&card->quad, 0.945f, 0.51f),
		_GameflowQuadPoint(&card->quad, 0.055f, 0.24f)
	}};
	lowerFacet = (gameflowQuad_t) {{
		_GameflowQuadPoint(&card->quad, 0.055f, 0.52f),
		_GameflowQuadPoint(&card->quad, 0.945f, 0.34f),
		_GameflowQuadPoint(&card->quad, 0.945f, 0.55f),
		_GameflowQuadPoint(&card->quad, 0.055f, 0.73f)
	}};
	spine = _GameflowRectQuad(&card->quad, 0.075f, 0.08f, 0.105f,
		0.92f);
	identity = _GameflowRectQuad(&card->quad, 0.12f,
		layout.identityTop, 0.88f, 0.91f);
	identityRule = _GameflowRectQuad(&card->quad, 0.12f,
		layout.identityTop, 0.88f, layout.identityTop + 0.018f);

	drawInit();
	_SetupRasterColor();
	GX_Begin(GX_QUADS, GX_VTXFMT0, 24);
		_GameflowPutQuad(&surface, surfaceTop, surfaceBottom);
		_GameflowPutQuad(&upperFacet, facetUpper, facetUpper);
		_GameflowPutQuad(&lowerFacet, facetLower, facetLower);
		_GameflowPutQuad(&spine, accent, accent);
		_GameflowPutQuad(&identity, dark, dark);
		_GameflowPutQuad(&identityRule, accent, accent);
	GX_End();

	if(artwork == UI_GAMEFLOW_LIBRARY_ART_BANNER && bannerTexture != NULL) {
		gameflowQuad_t bannerFrame = _GameflowRectQuad(&card->quad,
			layout.bannerLeft - 0.025f, layout.bannerTop - 0.02f,
			layout.bannerRight + 0.025f, layout.bannerBottom + 0.02f);
		gameflowQuad_t banner = _GameflowRectQuad(&card->quad,
			layout.bannerLeft, layout.bannerTop, layout.bannerRight,
			layout.bannerBottom);
		GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
			_GameflowPutQuad(&bannerFrame, dark, dark);
		GX_End();
		_GameflowDrawBanner(&banner, bannerTexture, alpha);
		drawInit();
		_SetupRasterColor();
	}
	else {
		float radius = layout.motifRadius;
		float verticalRadius = radius * 0.72f;
		gameflowQuad_t diamond = {{
			_GameflowQuadPoint(&card->quad, layout.motifCenterX,
				layout.motifCenterY - verticalRadius),
			_GameflowQuadPoint(&card->quad, layout.motifCenterX + radius,
				layout.motifCenterY),
			_GameflowQuadPoint(&card->quad, layout.motifCenterX,
				layout.motifCenterY + verticalRadius),
			_GameflowQuadPoint(&card->quad, layout.motifCenterX - radius,
				layout.motifCenterY)
		}};
		gameflowQuad_t core = {{
			_GameflowQuadPoint(&card->quad, layout.motifCenterX,
				layout.motifCenterY - verticalRadius * 0.55f),
			_GameflowQuadPoint(&card->quad,
				layout.motifCenterX + radius * 0.55f,
				layout.motifCenterY),
			_GameflowQuadPoint(&card->quad, layout.motifCenterX,
				layout.motifCenterY + verticalRadius * 0.55f),
			_GameflowQuadPoint(&card->quad,
				layout.motifCenterX - radius * 0.55f,
				layout.motifCenterY)
		}};
		GX_Begin(GX_QUADS, GX_VTXFMT0, 8);
			_GameflowPutQuad(&diamond, accent, accent);
			_GameflowPutQuad(&core, dark, dark);
		GX_End();
	}

	if(layout.identityAlpha > 0.001f) {
		const char *identityText = card->record->gameId[0] ?
			card->record->gameId :
			(card->record->flags & UI_GAMEFLOW_CARD_PARENT ?
				"RETURN" : "GAME DISC");
		const char *regionText = card->record->flags & UI_GAMEFLOW_CARD_PARENT ?
			"GAME LIBRARY" :
			UIGameflowLibrary_RegionLabel(card->record->gameId);
		gameflowPoint_t idPoint = _GameflowQuadPoint(&card->quad, 0.5f,
			layout.idBaseline);
		gameflowPoint_t regionPoint = _GameflowQuadPoint(&card->quad, 0.5f,
			layout.regionBaseline);
		GXColor primary = {239, 234, 255,
			_GameflowAlpha(244.0f * card->presence * reveal *
			layout.identityAlpha)};
		GXColor secondary = {177, 168, 220,
			_GameflowAlpha(218.0f * card->presence * reveal *
			layout.identityAlpha)};
		drawStringMedium((int)_GameflowRound(idPoint.x),
			(int)_GameflowRound(idPoint.y), identityText, 0.44f,
			ALIGN_CENTER, primary);
		drawStringMedium((int)_GameflowRound(regionPoint.x),
			(int)_GameflowRound(regionPoint.y), regionText, 0.31f,
			ALIGN_CENTER, secondary);
	}
	drawInit();
	_SetupRasterColor();
}

static GXTexObj *_GameflowPosterTexture(
	const uiGameflowCardSnapshot_t *record)
{
	uiPosterHandle_t handle;
	uiPosterResult_t result;

	/* _DrawGameflow runs under _videomutex. Query and Peek deliberately do
	 * not lock and the borrowed texture is consumed before that lock drops. */
	result = UIAssets_Query(record->gameId,
		strnlen(record->gameId, sizeof(record->gameId)),
		(record->flags & UI_GAMEFLOW_CARD_HAS_BANNER) != 0u, &handle);
	if(result != UI_POSTER_EXACT && result != UI_POSTER_UNIVERSAL) {
		return NULL;
	}
	return UIAssets_Peek(handle);
}

static void _GameflowDrawMetadata(const uiGameflowCardSnapshot_t *record,
	const drawGameflowCardPresentation_t *presentation, float alpha,
	float reveal)
{
	GXColor primary = {246, 243, 255, _GameflowAlpha(255.0f * alpha * reveal)};
	GXColor secondary = {190, 181, 231, _GameflowAlpha(220.0f * alpha * reveal)};

	if(record == NULL || presentation == NULL || alpha <= 0.001f) {
		return;
	}
	drawStringMedium(320, 350, record->title, presentation->titleScale,
		ALIGN_CENTER, primary);
	if(record->company[0] && !(record->flags &
		(UI_GAMEFLOW_CARD_PARENT | UI_GAMEFLOW_CARD_FOLDER))) {
		drawStringMedium(320, 376, record->company,
			presentation->companyScale, ALIGN_CENTER, secondary);
	}
}

static float _GameflowPrepareDetailText(char *text, size_t capacity,
	int width, float maximum, float floor)
{
	char original[UI_GAMEFLOW_DETAIL_PRESENTATION_CAPACITY];
	size_t length;
	size_t keep;
	float scale;

	if(text == NULL || capacity == 0u || text[0] == '\0') {
		return 0.0f;
	}
	if(capacity > sizeof(original)) {
		capacity = sizeof(original);
	}
	text[capacity - 1u] = '\0';
	length = strnlen(text, capacity);
	scale = GetTextScaleToFitInWidthWithMax(text, width, maximum);
	if(scale >= floor) {
		return scale;
	}
	memcpy(original, text, length + 1u);
	for(keep = length; keep > 0u; --keep) {
		size_t candidate = keep - 1u;

		if(candidate + 3u >= capacity) {
			continue;
		}
		memcpy(text, original, candidate);
		memcpy(&text[candidate], "...", 4u);
		if((float)GetTextSizeInPixels(text) * floor <= (float)width) {
			break;
		}
	}
	if(keep == 0u && capacity >= 4u) {
		memcpy(text, "...", 4u);
	}
	scale = GetTextScaleToFitInWidthWithMax(text, width, maximum);
	return scale < floor ? floor : scale;
}

static void _GameflowPrepareCardPresentation(drawGameflowEvent_t *data,
	u32 recordIndex)
{
	uiGameflowCardSnapshot_t *record;
	drawGameflowCardPresentation_t *presentation;

	if(data == NULL || recordIndex >= data->snapshot.recordCount ||
		recordIndex >= UI_GAMEFLOW_RENDER_SLOTS) {
		return;
	}
	record = &data->snapshot.records[recordIndex];
	presentation = &data->cardPresentation[recordIndex];
	presentation->titleScale = _GameflowPrepareDetailText(record->title,
		sizeof(record->title), 520, 0.78f, 0.50f);
	presentation->companyScale = _GameflowPrepareDetailText(record->company,
		sizeof(record->company), 420, 0.50f, 0.42f);
	presentation->factsScale = _GameflowPrepareDetailText(record->facts,
		sizeof(record->facts), 500, 0.44f, 0.42f);
}

static void _GameflowPrepareDetailPresentation(drawGameflowEvent_t *data)
{
	drawGameflowDetailPresentation_t *presentation;
	u8 coverRed;
	u8 coverGreen;
	u8 coverBlue;

	if(data == NULL) {
		return;
	}
	presentation = &data->detailPresentation;
	memset(presentation, 0, sizeof(*presentation));
	presentation->accent = (GXColor) {135, 120, 207, 255};
	/* Publication already owns _videomutex. Resolve the immutable pack accent
	 * once here instead of searching the pack index every presented frame.
	 * Only Indigo takes the cover's tint: mixed with a warm cover, the accent
	 * can leave the family Menu Color recolors and would stay indigo. */
	if(swissSettings.uiColor == UI_COLOR_INDIGO &&
		UIAssets_DominantColor(data->detail.gameId,
			UI_GAMEFLOW_DETAIL_ID_LENGTH, &coverRed, &coverGreen, &coverBlue)) {
		presentation->accent.r =
			(u8)(((u16)presentation->accent.r * 2u + coverRed) / 3u);
		presentation->accent.g =
			(u8)(((u16)presentation->accent.g * 2u + coverGreen) / 3u);
		presentation->accent.b =
			(u8)(((u16)presentation->accent.b * 2u + coverBlue) / 3u);
	}
	presentation->titleScale = _GameflowPrepareDetailText(
		data->detail.title, sizeof(data->detail.title), 310, 0.72f, 0.50f);
	presentation->companyScale = _GameflowPrepareDetailText(
		data->detail.company, sizeof(data->detail.company), 310, 0.50f, 0.46f);
	presentation->factsScale = _GameflowPrepareDetailText(
		data->detail.facts, sizeof(data->detail.facts), 310, 0.46f, 0.46f);
	presentation->statusScale = _GameflowPrepareDetailText(
		data->detail.statusText, sizeof(data->detail.statusText),
		310, 0.44f, 0.44f);
	presentation->lastPlayedScale = _GameflowPrepareDetailText(
		data->detail.lastPlayedText, sizeof(data->detail.lastPlayedText),
		310, 0.46f, 0.46f);
	presentation->saveStatusScale = _GameflowPrepareDetailText(
		data->detail.saveStatusText, sizeof(data->detail.saveStatusText),
		310, 0.46f, 0.46f);
	presentation->cheatSummaryScale = _GameflowPrepareDetailText(
		data->detail.cheatSummary, sizeof(data->detail.cheatSummary),
		294, 0.46f, 0.46f);
	presentation->cheatPreviewScale = _GameflowPrepareDetailText(
		data->detail.cheatPreview, sizeof(data->detail.cheatPreview),
		294, 0.46f, 0.46f);
	presentation->launchScale = _GameflowPrepareDetailText(
		data->detail.launchLabel, sizeof(data->detail.launchLabel),
		278, 0.56f, 0.50f);
	presentation->primaryActionsScale = _GameflowPrepareDetailText(
		data->detail.primaryActions, sizeof(data->detail.primaryActions),
		590, 0.46f, 0.46f);
	/* ponytail: the shortcut, launch and cheat hints fit by their text
	 * width. Their strings are fixed and the icons leave room (the widest,
	 * "Z  AUTOLOAD ON   R  VERIFY", draws about 115 px of 164); measure with
	 * GetHintSizeInPixels if a longer one is added. */
	presentation->advancedLineOneScale = _GameflowPrepareDetailText(
		data->detail.advancedLineOne, sizeof(data->detail.advancedLineOne),
		164, 0.42f, 0.42f);
	presentation->advancedLineTwoScale = _GameflowPrepareDetailText(
		data->detail.advancedLineTwo, sizeof(data->detail.advancedLineTwo),
		164, 0.42f, 0.42f);
}

static void _GameflowPutDetailPanel(int x, int y, int width, int height,
	int edgeWidth, GXColor glow, GXColor fill, GXColor edge)
{
	_GameflowPutVertex((gameflowPoint_t) {(float)x - 3.0f,
		(float)y - 3.0f}, glow);
	_GameflowPutVertex((gameflowPoint_t) {(float)x + (float)width + 3.0f,
		(float)y - 3.0f}, glow);
	_GameflowPutVertex((gameflowPoint_t) {(float)x + (float)width + 3.0f,
		(float)y + (float)height + 3.0f}, glow);
	_GameflowPutVertex((gameflowPoint_t) {(float)x - 3.0f,
		(float)y + (float)height + 3.0f}, glow);
	_GameflowPutVertex((gameflowPoint_t) {(float)x, (float)y}, fill);
	_GameflowPutVertex((gameflowPoint_t) {(float)x + (float)width,
		(float)y}, fill);
	_GameflowPutVertex((gameflowPoint_t) {(float)x + (float)width,
		(float)y + (float)height}, fill);
	_GameflowPutVertex((gameflowPoint_t) {(float)x,
		(float)y + (float)height}, fill);
	_GameflowPutVertex((gameflowPoint_t) {(float)x, (float)y}, edge);
	_GameflowPutVertex((gameflowPoint_t) {(float)x + (float)edgeWidth,
		(float)y}, edge);
	_GameflowPutVertex((gameflowPoint_t) {(float)x + (float)edgeWidth,
		(float)y + (float)height}, edge);
	_GameflowPutVertex((gameflowPoint_t) {(float)x,
		(float)y + (float)height}, edge);
}

static void _GameflowDrawDetailPlanes(
	const uiGameflowDetailSnapshot_t *detail,
	const drawGameflowDetailPresentation_t *presentation,
	const uiGameflowFrame_t *frame, float alpha)
{
	bool hasAdvanced = detail->advancedLineOne[0] != '\0' ||
		detail->advancedLineTwo[0] != '\0';
	float focus = _CurrentMotionMode() == UI_MOTION_FULL ?
		0.86f + 0.14f * sinf(UIAnim_Seconds() * 3.5f) : 1.0f;
	GXColor panelGlow = presentation->accent;
	GXColor panelFill = {8, 7, 25, _GameflowAlpha(222.0f * alpha)};
	GXColor panelEdge = presentation->accent;
	GXColor insetGlow = presentation->accent;
	GXColor insetFill = {16, 12, 46, _GameflowAlpha(226.0f * alpha)};
	GXColor insetEdge = presentation->accent;
	GXColor ctaGlow = presentation->accent;
	GXColor ctaFill = frame->launchProgress > 0.02f ?
		(GXColor) {58, 46, 126, _GameflowAlpha(244.0f * alpha)} :
		(GXColor) {43, 33, 101, _GameflowAlpha(238.0f * alpha)};
	GXColor ctaEdge = {
		(u8)(((u16)presentation->accent.r + 255u) / 2u),
		(u8)(((u16)presentation->accent.g + 255u) / 2u),
		(u8)(((u16)presentation->accent.b + 255u) / 2u),
		_GameflowAlpha(255.0f * alpha * focus)};
	u16 panelCount = hasAdvanced ? 4u : 3u;

	panelGlow.a = _GameflowAlpha(34.0f * alpha);
	panelEdge.a = _GameflowAlpha(172.0f * alpha);
	insetGlow.a = _GameflowAlpha(22.0f * alpha);
	insetEdge.a = _GameflowAlpha(112.0f * alpha);
	ctaGlow.a = _GameflowAlpha(92.0f * alpha * focus);
	drawInit();
	_SetupRasterColor();
	GX_Begin(GX_QUADS, GX_VTXFMT0, (u16)(panelCount * 12u));
		_GameflowPutDetailPanel(246, 76, 358, 328, 2,
			panelGlow, panelFill, panelEdge);
		_GameflowPutDetailPanel(260, 281, 330, 59, 2,
			insetGlow, insetFill, insetEdge);
		_GameflowPutDetailPanel(260, 348, 330, 43, 4,
			ctaGlow, ctaFill, ctaEdge);
		if(hasAdvanced) {
			_GameflowPutDetailPanel(48, 350, 180, 52, 2,
				insetGlow, insetFill, insetEdge);
		}
	GX_End();
	drawInit();
}

static void _GameflowDrawDetailDashboard(
	const uiGameflowDetailSnapshot_t *detail,
	const drawGameflowDetailPresentation_t *presentation,
	const uiGameflowFrame_t *frame, float reveal,
	const uiCommandRailFrame_t *commandRail)
{
	const char *launchText;
	float launchScale;
	float alpha;
	GXColor primary;
	GXColor secondary;
	GXColor muted;
	GXColor focus;

	if(detail == NULL || presentation == NULL || frame == NULL ||
		frame->detailProgress <= 0.001f) {
		return;
	}
	alpha = _GameflowClamp(frame->detailProgress * reveal, 0.0f, 1.0f);
	primary = (GXColor) {246, 243, 255, _GameflowAlpha(255.0f * alpha)};
	secondary = (GXColor) {202, 192, 244, _GameflowAlpha(235.0f * alpha)};
	muted = (GXColor) {165, 158, 201, _GameflowAlpha(218.0f * alpha)};
	focus = (GXColor) {244, 239, 255, _GameflowAlpha(255.0f * alpha)};
	_GameflowDrawDetailPlanes(detail, presentation, frame, alpha);

	drawStringMedium(264, 95, frame->launchProgress > 0.02f ?
		"LAUNCHING" : "GAME DETAIL", 0.42f, ALIGN_LEFT, secondary);
	drawStringMedium(264, 122, detail->title, presentation->titleScale,
		ALIGN_LEFT, primary);
	if(detail->company[0] != '\0') {
		drawStringMedium(264, 148, detail->company,
			presentation->companyScale, ALIGN_LEFT, secondary);
	}
	if(detail->statusText[0] != '\0') {
		drawStringMedium(264, 177, detail->statusText,
			presentation->statusScale, ALIGN_LEFT, muted);
	}

	drawStringMedium(264, 207, "LAST PLAYED", 0.42f, ALIGN_LEFT, secondary);
	drawStringMedium(264, 226, detail->lastPlayedText,
		presentation->lastPlayedScale, ALIGN_LEFT, primary);
	drawStringMedium(264, 247, "SAVE DATA", 0.42f, ALIGN_LEFT, secondary);
	drawStringMedium(264, 265, detail->saveStatusText,
		presentation->saveStatusScale, ALIGN_LEFT, muted);

	drawStringMedium(274, 293, "CHEATS", 0.42f, ALIGN_LEFT, secondary);
	drawStringMedium(274, 311, detail->cheatSummary,
		presentation->cheatSummaryScale, ALIGN_LEFT, muted);
	if(detail->cheatPreview[0] != '\0') {
		if(detail->enabledCheatCount != 0u) {
			drawStringMedium(274, 329, "\267", 0.50f, ALIGN_LEFT, focus);
			drawStringMedium(288, 329, detail->cheatPreview,
				presentation->cheatPreviewScale, ALIGN_LEFT, primary);
		}
		else {
			/* "Y  Choose cheats": a hint only while nothing is on, since
			 * cheat names are free text. */
			_DrawHintText(274, 329, detail->cheatPreview,
				presentation->cheatPreviewScale, ALIGN_LEFT, muted);
		}
	}

	launchText = frame->launchProgress > 0.02f ?
		"STARTING GAME..." : detail->launchLabel;
	launchScale = frame->launchProgress > 0.02f ?
		0.56f : presentation->launchScale;
	drawStringMedium(278, 369, "\267", 0.58f, ALIGN_LEFT, focus);
	_DrawHintText(425, 369, launchText, launchScale, ALIGN_CENTER, focus);

	if(detail->advancedLineOne[0] != '\0' ||
		detail->advancedLineTwo[0] != '\0') {
		int firstY = detail->advancedLineTwo[0] != '\0' ? 378 : 387;

		drawStringMedium(138, 359, "SHORTCUTS", 0.36f,
			ALIGN_CENTER, secondary);
		if(detail->advancedLineOne[0] != '\0') {
			_DrawHintText(138, firstY, detail->advancedLineOne,
				presentation->advancedLineOneScale, ALIGN_CENTER, muted);
		}
		if(detail->advancedLineTwo[0] != '\0') {
			_DrawHintText(138, 396, detail->advancedLineTwo,
				presentation->advancedLineTwoScale, ALIGN_CENTER, muted);
		}
	}

	if(commandRail != NULL && commandRail->owner == UI_COMMAND_RAIL_DETAIL &&
		commandRail->alpha > 0.001f) {
		GXColor command = secondary;

		command.a = _GameflowAlpha(235.0f * reveal * commandRail->alpha);
		_DrawHintText(320, 433, detail->primaryActions,
			presentation->primaryActionsScale, ALIGN_CENTER, command);
	}
	drawInit();
}

static void _DrawGameflow(uiDrawObj_t *evt)
{
	drawGameflowEvent_t *data = (drawGameflowEvent_t*)evt->data;
	const uiSceneFrame_t *scene = UIScene_Frame();
	const uiGameflowFrame_t *frame;
	gameflowRenderCard_t cards[UI_GAMEFLOW_RENDER_SLOTS];
	const uiGameflowCardSnapshot_t *selectedRecord;
	const uiGameflowCardSnapshot_t *previousRecord;
	const uiGameflowCardSnapshot_t *focusRecord;
	const uiGameflowDetailSnapshot_t *detail = NULL;
	uiCommandRailFrame_t commandRail;
	gameflowQuad_t detailPose = {{{48.0f, 96.0f}, {228.0f, 96.0f},
		{228.0f, 336.0f}, {48.0f, 336.0f}}};
	float reveal;
	float titleTravel;
	u32 count = 0u;
	u32 i;
	u32 selectedRecordIndex = 0u;
	u32 previousRecordIndex = 0u;

	if(data == NULL || (scene->scene != UI_SCENE_LIBRARY &&
		scene->scene != UI_SCENE_GAME_DETAIL)) {
		return;
	}
	reveal = _GameflowClamp(scene->chromeProgress * scene->libraryReveal,
		0.0f, 1.0f);
	if(reveal <= 0.0f) {
		return;
	}

	UIGameflow_Update(&data->state, UIAnim_Delta(), _CurrentMotionMode());
	frame = UIGameflow_Frame(&data->state);
	if(frame == NULL || !frame->hasSnapshot || frame->itemCount == 0u) {
		return;
	}
	focusRecord = _GameflowFindRecord(&data->snapshot, frame->focusIndex,
		NULL);
	if(focusRecord != NULL && UIGameflowDetail_Matches(&data->detail,
		frame->generation, frame->focusIndex, focusRecord->gameId,
		strnlen(focusRecord->gameId, sizeof(focusRecord->gameId)))) {
		detail = &data->detail;
	}

	for(i = 0u; i < data->snapshot.recordCount; ++i) {
		const uiGameflowCardSnapshot_t *record = &data->snapshot.records[i];
		float slot;
		float presence;

		if(!(record->flags & UI_GAMEFLOW_CARD_VALID)) {
			continue;
		}
		slot = (float)record->relativeSlot + frame->carouselTravel;
		presence = _GameflowPresence(slot);
		if(frame->detailProgress > 0.0f) {
			if(record->libraryIndex == frame->focusIndex) {
				presence = 1.0f;
			}
			else {
				presence *= 1.0f - frame->detailProgress;
			}
		}
		if(presence <= 0.001f) {
			continue;
		}
		cards[count].record = record;
		cards[count].recordIndex = i;
		cards[count].visualSlot = slot;
		cards[count].presence = presence *
			((record->flags & UI_GAMEFLOW_CARD_HIDDEN) ? 0.55f : 1.0f);
		cards[count].quad = _GameflowSamplePose(slot);
		if(frame->detailProgress > 0.0f &&
			record->libraryIndex == frame->focusIndex) {
			int vertex;
			for(vertex = 0; vertex < 4; ++vertex) {
				cards[count].quad.point[vertex] = _GameflowLerpPoint(
					cards[count].quad.point[vertex], detailPose.point[vertex],
					frame->detailProgress);
			}
		}
		count++;
	}

	/* Painter's order: far/sliver cards first, center-most focus last. */
	for(i = 1u; i < count; ++i) {
		gameflowRenderCard_t card = cards[i];
		u32 j = i;
		while(j > 0u && fabsf(cards[j - 1u].visualSlot) <
			fabsf(card.visualSlot)) {
			cards[j] = cards[j - 1u];
			j--;
		}
		cards[j] = card;
	}

	drawInit();
	_SetupRasterColor();
	GX_Begin(GX_QUADS, GX_VTXFMT0, (u16)(count * 4u));
	for(i = 0u; i < count; ++i) {
		u8 topAlpha = _GameflowAlpha((104.0f +
			(1.0f - _GameflowClamp(fabsf(cards[i].visualSlot), 0.0f, 1.0f)) *
			64.0f) * cards[i].presence * reveal);
		u8 bottomAlpha = _GameflowAlpha(210.0f * cards[i].presence * reveal);
		GXColor top = _GameflowAccent(cards[i].record, topAlpha);
		GXColor bottom = {10, 8, 30, bottomAlpha};
		_GameflowPutQuad(&cards[i].quad, top, bottom);
	}
	GX_End();

	GX_Begin(GX_QUADS, GX_VTXFMT0, (u16)(count * 4u));
	for(i = 0u; i < count; ++i) {
		gameflowQuad_t inner = _GameflowInsetQuad(&cards[i].quad,
			0.033333f, 0.033333f);
		GXColor top = {49, 43, 92,
			_GameflowAlpha(130.0f * cards[i].presence * reveal)};
		GXColor bottom = {12, 10, 34,
			_GameflowAlpha(190.0f * cards[i].presence * reveal)};
		_GameflowPutQuad(&inner, top, bottom);
	}
	GX_End();

	GX_Begin(GX_QUADS, GX_VTXFMT0, (u16)(count * 16u));
	for(i = 0u; i < count; ++i) {
		gameflowQuad_t inner = _GameflowInsetQuad(&cards[i].quad,
			0.018f, 0.018f);
		float focus = 1.0f - _GameflowClamp(fabsf(cards[i].visualSlot),
			0.0f, 1.0f);
		GXColor border = {220, 214, 255,
			_GameflowAlpha((112.0f + focus * 126.0f) *
			cards[i].presence * reveal)};
		_GameflowPutBorder(&cards[i].quad, &inner, border);
	}
	GX_End();

	/* Actual retail covers own the selected and near-card inset. While an
	 * exact/universal record is loading (or unavailable), retain the native
	 * BNR; use the code-native emblem only when neither texture exists. */
	for(i = 0u; i < count; ++i) {
		float distance = fabsf(cards[i].visualSlot);
		GXTexObj *posterTexture;
		GXTexObj *bannerTexture = NULL;
		uiGameflowLibraryArtwork_t artwork;

		if(distance >= 1.5f) {
			continue;
		}
		posterTexture = _GameflowPosterTexture(cards[i].record);
		if(cards[i].record->flags & UI_GAMEFLOW_CARD_HAS_BANNER) {
			bannerTexture = &data->bannerTexObj[cards[i].recordIndex];
		}
		else if(detail != NULL &&
			cards[i].record->libraryIndex == frame->focusIndex &&
			(detail->flags & UI_GAMEFLOW_DETAIL_HAS_BANNER) != 0u) {
			bannerTexture = &data->detailBannerTexObj;
		}
		artwork = UIGameflowLibrary_ChooseArtwork(posterTexture != NULL,
			bannerTexture != NULL);
		if(artwork == UI_GAMEFLOW_LIBRARY_ART_POSTER) {
			_GameflowDrawPoster(&cards[i], posterTexture, reveal);
			continue;
		}
		_GameflowDrawFallback(&cards[i], artwork, bannerTexture, reveal);
	}
	for(i = 0u; i < count; ++i) {
		if(fabsf(cards[i].visualSlot) < 1.5f &&
			(cards[i].record->flags & UI_GAMEFLOW_CARD_CUSTOM)) {
			_GameflowDrawCustomMark(&cards[i], reveal);
		}
	}

	selectedRecord = _GameflowFindRecord(&data->snapshot,
		frame->selectedIndex, &selectedRecordIndex);
	previousRecord = _GameflowFindRecord(&data->snapshot,
		frame->previousIndex, &previousRecordIndex);
	titleTravel = _GameflowClamp(fabsf(frame->carouselTravel), 0.0f, 1.0f);
	_GameflowDrawMetadata(previousRecord, previousRecord != NULL ?
		&data->cardPresentation[previousRecordIndex] : NULL,
		titleTravel * (1.0f - frame->detailProgress), reveal);
	_GameflowDrawMetadata(selectedRecord, selectedRecord != NULL ?
		&data->cardPresentation[selectedRecordIndex] : NULL,
		(1.0f - titleTravel) * (1.0f - frame->detailProgress), reveal);

	UICommandRail_Gameflow(frame->detailProgress, &commandRail);
	_GameflowDrawDetailDashboard(detail, &data->detailPresentation,
		frame, reveal, &commandRail);
	if(frame->detailProgress < 0.999f) {
		GXColor label = {177, 168, 220,
			_GameflowAlpha(180.0f * reveal *
			(1.0f - frame->detailProgress))};
		drawStringMedium(320, 70, "GAME LIBRARY", 0.50f, ALIGN_CENTER, label);
		if(commandRail.owner == UI_COMMAND_RAIL_LIBRARY &&
				commandRail.alpha > 0.001f) {
			GXColor command = label;
			command.a = _GameflowAlpha(180.0f * reveal * commandRail.alpha);
			_DrawHintText(320, 428,
				"D-PAD  BROWSE   A  OPEN   X  BACK   B  HOME",
				0.46f, ALIGN_CENTER, command);
		}
	}
	drawInit();
}

static void _DrawHomeText(int x, int y, const char *text, float scale,
		int align, GXColor color)
{
	/* drawStringMedium already adds exactly one native-pixel coverage pass.
	 * Do not stack extra font passes on the Gekko merely to fake a keyline. */
	drawStringMedium(x, y, text, scale, align, color);
}

static void _DrawHomePanel(int x, int y, int width, int height,
		float reveal, bool selected, bool danger)
{
	GXColor fill = danger ? (GXColor) {82, 20, 42, 0} :
		(GXColor) {20, 13, 58, 0};
	GXColor edge = danger ? (GXColor) {255, 132, 158, 0} :
		(GXColor) {191, 174, 255, 0};
	GXColor glow = danger ? (GXColor) {255, 75, 118, 0} :
		(GXColor) {117, 88, 244, 0};

	fill.a = (u8)((selected ? 148.0f : 72.0f) * reveal);
	edge.a = (u8)((selected ? 218.0f : 92.0f) * reveal);
	glow.a = (u8)((selected ? 84.0f : 28.0f) * reveal);
	drawInit();
	GX_SetNumTexGens(0);
	GX_SetNumIndStages(0);
	GX_SetNumTevStages(1);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL,
		GX_COLOR0A0);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO,
		GX_CC_RASC);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
		GX_ENABLE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO,
		GX_CA_RASA);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
		GX_ENABLE, GX_TEVPREV);
	GX_SetTevDirect(GX_TEVSTAGE0);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA,
		GX_LO_CLEAR);
	GX_SetZMode(GX_DISABLE, GX_ALWAYS, GX_FALSE);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 12);
		_putFlatRect((float)x - 3.0f, (float)y - 3.0f,
			(float)width + 6.0f, (float)height + 6.0f, glow);
		_putFlatRect((float)x, (float)y, (float)width, (float)height, fill);
		_putFlatRect((float)x, (float)y, selected ? 4.0f : 2.0f,
			(float)height, edge);
	GX_End();
	drawInit();
}

static void _DrawHomeModalDepth(const uiHomeLayout_t *layout, float reveal)
{
	int width = layout->modalBounds.right - layout->modalBounds.left;
	int height = layout->modalBounds.bottom - layout->modalBounds.top;
	GXColor scrim = {4, 3, 16, (u8)(104.0f * reveal)};
	GXColor card = {18, 11, 48, (u8)(220.0f * reveal)};
	GXColor edge = {169, 145, 235, (u8)(112.0f * reveal)};

	/* The confirmation surface sits above the hero scene. One darkening pass
	 * quiets the cube, orbits and silk without replacing their spatial
	 * context; the opaque lower card keeps consequence copy crisp at 480i. */
	drawInit();
	GX_SetNumTexGens(0);
	GX_SetNumIndStages(0);
	GX_SetNumTevStages(1);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL,
		GX_COLOR0A0);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO,
		GX_CC_RASC);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
		GX_ENABLE, GX_TEVPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO,
		GX_CA_RASA);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
		GX_ENABLE, GX_TEVPREV);
	GX_SetTevDirect(GX_TEVSTAGE0);
	GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA,
		GX_LO_CLEAR);
	GX_SetZMode(GX_DISABLE, GX_ALWAYS, GX_FALSE);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 12);
		_putFlatRect(0.0f, 0.0f, 640.0f, 480.0f, scrim);
		_putFlatRect((float)layout->modalBounds.left,
			(float)layout->modalBounds.top, (float)width, (float)height,
			card);
		_putFlatRect((float)layout->modalBounds.left,
			(float)layout->modalBounds.top, (float)width, 2.0f, edge);
	GX_End();
	drawInit();
}

static void _DrawHomeRoot(const drawHomeEvent_t *data,
		const uiSceneFrame_t *scene, float reveal)
{
	const uiHomeLayout_t *layout = &data->layout;
	uiHomeFace_t face = scene->homeFace;
	float progress = scene->homeFocusProgress;
	float eased;
	float incomingAlpha;
	float scale;
	float travel;
	int incomingX;
	int incomingY;
	GXColor primary = {250, 248, 255, (u8)(255.0f * reveal)};
	GXColor muted = {157, 146, 205, (u8)(184.0f * reveal)};
	GXColor incoming;
	uiMotionMode_t motionMode = _CurrentMotionMode();

	if(progress < 0.0f) progress = 0.0f;
	if(progress > 1.0f) progress = 1.0f;
	eased = progress * progress * (3.0f - 2.0f * progress);

	/* The ring names only the selected face, under the cube. Nothing sits
	 * above or beside it: a label the cube does not carry through the turn
	 * reads as detached from it. */
	incomingAlpha = 0.34f + eased * 0.66f;
	travel = UIMotion_Amplitude((float)UI_HOME_LAYOUT_SELECTED_TRAVEL,
		motionMode);
	incomingX = layout->selectedLabelCenter.x;
	incomingY = layout->selectedLabelCenter.y;
	if(scene->homeTurnAxis == UI_HOME_TURN_VERTICAL) {
		incomingY += (int)((float)scene->homeTurnDirection * (1.0f - eased) *
			UIMotion_Amplitude((float)UI_HOME_LAYOUT_SELECTED_VERTICAL_TRAVEL, motionMode));
	}
	else {
		incomingX += (int)((float)scene->homeTurnDirection * (1.0f - eased) * travel);
	}
	incoming = primary;
	incoming.a = (u8)((float)incoming.a * incomingAlpha);
	scale = data->focusScale[(int)face];
	_DrawHomeText(incomingX, incomingY,
		UIHome_FaceLabel(face), scale,
		ALIGN_CENTER, incoming);
	_DrawHintText(layout->commandCenter.x, layout->commandCenter.y,
		data->command, data->commandScale,
		ALIGN_CENTER, muted);
}

static void _DrawHomeRows(const drawHomeEvent_t *data, float reveal)
{
	const uiHomeLayout_t *layout = &data->layout;
	int rowCount = layout->rowCount;
	int row;
	GXColor primary = {250, 248, 255, (u8)(255.0f * reveal)};
	GXColor muted = {164, 153, 212, (u8)(190.0f * reveal)};

	for(row = 0; row < rowCount; ++row) {
		const uiHomeLayoutItem_t *item = &layout->rows[row];
		int width = item->panelBounds.right - item->panelBounds.left;
		int height = item->panelBounds.bottom - item->panelBounds.top;
		bool selected = item->selected;
		const char *label = UIHome_RowLabel(data->state.surface, row);
		_DrawHomePanel(item->panelBounds.left, item->panelBounds.top,
			width, height, reveal, selected, false);
		_DrawHomeText(item->labelCenter.x, item->labelCenter.y, label,
			selected ? data->rowSelectedScale[row] : data->rowIdleScale[row],
			ALIGN_CENTER, selected ? primary : muted);
	}
	_DrawHintText(layout->commandCenter.x, layout->commandCenter.y,
		homeContextCommand, data->contextCommandScale,
		ALIGN_CENTER, muted);
}

static void _DrawHomeContext(const drawHomeEvent_t *data, float reveal)
{
	GXColor title = {220, 211, 255, (u8)(234.0f * reveal)};

	_DrawHomeText(data->layout.titleCenter.x, data->layout.titleCenter.y,
		data->heading, data->headingScale,
		ALIGN_CENTER, title);
	_DrawHomeRows(data, reveal);
}

static void _DrawHomeRestartConfirm(const drawHomeEvent_t *data,
		float reveal)
{
	int row;
	GXColor title = {255, 225, 232, (u8)(250.0f * reveal)};
	GXColor primary = {255, 247, 249, (u8)(255.0f * reveal)};
	GXColor muted = {205, 169, 184, (u8)(202.0f * reveal)};
	GXColor consequence = {220, 210, 239, (u8)(224.0f * reveal)};

	_DrawHomeModalDepth(&data->layout, reveal);
	_DrawHomeText(data->layout.titleCenter.x, data->layout.titleCenter.y,
		"RESTART INDIGO?", 0.60f, ALIGN_CENTER, title);
	_DrawHomeText(data->layout.consequenceCenter.x,
		data->layout.consequenceCenter.y, homeRestartConsequence,
		data->consequenceScale, ALIGN_CENTER, consequence);
	for(row = 0; row < 2; ++row) {
		const uiHomeLayoutItem_t *item = &data->layout.options[row];
		int width = item->panelBounds.right - item->panelBounds.left;
		int height = item->panelBounds.bottom - item->panelBounds.top;
		bool selected = item->selected;
		const char *label = UIHome_RowLabel(
			UI_HOME_SURFACE_RESTART_CONFIRM, row);
		_DrawHomePanel(item->panelBounds.left, item->panelBounds.top,
			width, height, reveal, selected, row == 1);
		_DrawHomeText(item->labelCenter.x, item->labelCenter.y, label,
			selected ? 0.54f : 0.49f,
			ALIGN_CENTER, selected ? primary : muted);
	}
	_DrawHintText(data->layout.commandCenter.x, data->layout.commandCenter.y,
		homeConfirmCommand, data->confirmCommandScale,
		ALIGN_CENTER, muted);
}

// Internal
static void _DrawHome(uiDrawObj_t *evt)
{
	drawHomeEvent_t *data = (drawHomeEvent_t*)evt->data;
	const uiSceneFrame_t *scene = UIScene_Frame();
	float reveal = scene->chromeProgress;

	if(!data->visible || !data->layoutValid || reveal <= 0.0f ||
		scene->scene != UI_SCENE_HOME || scene->homeFace != data->state.face ||
		!UIHome_IsFace((int)data->state.face) ||
		!UIHome_IsSurface((int)data->state.surface)) {
		return;
	}
	if(reveal > 1.0f) reveal = 1.0f;
	if(data->state.surface == UI_HOME_SURFACE_RING) {
		_DrawHomeRoot(data, scene, reveal);
	}
	else if(data->state.surface == UI_HOME_SURFACE_RESTART_CONFIRM) {
		_DrawHomeRestartConfirm(data, reveal);
	}
	else {
		_DrawHomeContext(data, reveal);
	}
}

// External
uiDrawObj_t* DrawHome(void)
{
	uiDrawObj_t *event = calloc(1, sizeof(uiDrawObj_t));
	drawHomeEvent_t *eventData = calloc(1, sizeof(drawHomeEvent_t));
	event->type = EV_HOME;
	event->data = eventData;
	return event;
}

void DrawUpdateProgressBar(uiDrawObj_t *evt, int percent) {
	drawProgressEvent_t *data = (drawProgressEvent_t*)evt->data;
	data->percent = percent;
}

void DrawUpdateProgressBarDetail(uiDrawObj_t *evt, int percent, int speed, int timestart, int timeremain) {
	drawProgressEvent_t *data = (drawProgressEvent_t*)evt->data;
	data->percent = percent;
	data->speed = speed;
	data->timestart = timestart;
	data->timeremain = timeremain;
}

void DrawUpdateProgressLoading(uiDrawObj_t *evt, int increment) {
	drawProgressEvent_t *data = (drawProgressEvent_t*)evt->data;
	data->speed += increment;
}

static void _PrepareHomeText(drawHomeEvent_t *data)
{
	int commandWidth = data->layout.commandGlyphBounds.right -
		data->layout.commandGlyphBounds.left;
	int focusWidth = data->layout.selectedLabelBounds.right -
		data->layout.selectedLabelBounds.left;
	int titleWidth = data->layout.titleBounds.right -
		data->layout.titleBounds.left;
	char headingSource[sizeof(data->heading)];
	int face;
	int row;

	for(face = 0; face < UI_HOME_FACE_COUNT; ++face) {
		data->focusScale[face] = UIHomeText_FitScale(
			UIHome_FaceLabel((uiHomeFace_t)face),
			focusWidth, 0.78f, GetTextSizeInPixels);
	}
	if(data->capabilities.hasRecent) {
		snprintf(data->command, sizeof(data->command),
			"STICK / D-PAD  TURN    %s    START  RECENT",
			UIHome_PrimaryHint(data->state.face, data->capabilities));
	}
	else {
		snprintf(data->command, sizeof(data->command),
			"STICK / D-PAD  TURN    %s",
			UIHome_PrimaryHint(data->state.face, data->capabilities));
	}
	data->commandScale = UIHomeText_FitScale(data->command,
		commandWidth, 0.46f, GetHintSizeInPixels);
	if(data->state.surface == UI_HOME_SURFACE_SOURCE) {
		if(data->capabilities.hasSource && data->sourceName[0] != '\0') {
			snprintf(headingSource, sizeof(headingSource), "SOURCE   %s",
				data->sourceName);
		}
		else {
			strcpy(headingSource, "SOURCE   NO SOURCE");
		}
	}
	else {
		strncpy(headingSource, UIHome_SurfaceTitle(data->state.surface),
			sizeof(headingSource) - 1u);
		headingSource[sizeof(headingSource) - 1u] = '\0';
	}
	data->headingScale = UIHomeText_CopyFitted(data->heading,
		sizeof(data->heading), headingSource, titleWidth, 0.58f,
		GetTextSizeInPixels, NULL);
	for(row = 0; row < 2; ++row) {
		const char *label = UIHome_RowLabel(data->state.surface, row);
		if(row < data->layout.rowCount && label[0] != '\0') {
			int rowWidth = data->layout.rows[row].labelBounds.right -
				data->layout.rows[row].labelBounds.left;
			data->rowSelectedScale[row] = UIHomeText_FitScale(label,
				rowWidth, 0.55f, GetTextSizeInPixels);
			data->rowIdleScale[row] = UIHomeText_FitScale(label,
				rowWidth, 0.50f, GetTextSizeInPixels);
		}
		else {
			data->rowSelectedScale[row] = 0.55f;
			data->rowIdleScale[row] = 0.50f;
		}
	}
	data->contextCommandScale = UIHomeText_FitScale(homeContextCommand,
		commandWidth, 0.46f, GetHintSizeInPixels);
	data->confirmCommandScale = UIHomeText_FitScale(homeConfirmCommand,
		commandWidth, 0.46f, GetHintSizeInPixels);
	data->consequenceScale = data->state.surface ==
		UI_HOME_SURFACE_RESTART_CONFIRM ?
		UIHomeText_FitScale(homeRestartConsequence,
			data->layout.consequenceBounds.right -
				data->layout.consequenceBounds.left,
			0.50f, GetTextSizeInPixels) : 0.50f;
}

void DrawUpdateHome(const uiHomeState_t *state,
		uiHomeCapabilities_t capabilities, const char *sourceName)
{
	LWP_MutexLock(_videomutex);
	if(buttonPanel && buttonPanel->data) {
		drawHomeEvent_t *data = (drawHomeEvent_t*)buttonPanel->data;
		data->visible = state != NULL;
		data->layoutValid = false;
		if(state != NULL) {
			data->state = *state;
			data->capabilities = capabilities;
			if(sourceName != NULL) {
				strncpy(data->sourceName, sourceName,
					sizeof(data->sourceName) - 1u);
				data->sourceName[sizeof(data->sourceName) - 1u] = '\0';
			}
			else {
				data->sourceName[0] = '\0';
			}
			data->layoutValid = UIHomeLayout_Compute(&data->state,
				data->capabilities, &data->layout);
			if(data->layoutValid) {
				_PrepareHomeText(data);
			}
			else {
				data->visible = false;
			}
			/* Publish cube and semantic foreground in one video transaction. */
			UIScene_RequestHome(state);
		}
	}
	LWP_MutexUnlock(_videomutex);
}

void DrawUpdateFileBrowserButton(uiDrawObj_t *evt, int mode) {
	drawFileBrowserButtonEvent_t *data = (drawFileBrowserButtonEvent_t*)evt->data;
	data->mode = mode;
}

static bool _GameflowSnapshotValid(const uiGameflowRenderSnapshot_t *snapshot)
{
	bool hasSelected = false;
	u32 i;
	u32 j;

	if(snapshot == NULL || snapshot->selection.itemCount == 0u ||
		snapshot->selection.selectedIndex >= snapshot->selection.itemCount ||
		snapshot->recordCount == 0u ||
		snapshot->recordCount > UI_GAMEFLOW_RENDER_SLOTS ||
		snapshot->recordCount > snapshot->selection.itemCount) {
		return false;
	}
	for(i = 0u; i < snapshot->recordCount; ++i) {
		const uiGameflowCardSnapshot_t *record = &snapshot->records[i];
		if(!(record->flags & UI_GAMEFLOW_CARD_VALID) ||
			record->libraryIndex >= snapshot->selection.itemCount ||
			record->relativeSlot < -3 || record->relativeSlot > 3) {
			return false;
		}
		if(record->libraryIndex == snapshot->selection.selectedIndex &&
			record->relativeSlot == 0) {
			hasSelected = true;
		}
		for(j = i + 1u; j < snapshot->recordCount; ++j) {
			if(record->libraryIndex == snapshot->records[j].libraryIndex) {
				return false;
			}
		}
	}
	return hasSelected;
}

static void _GameflowClosePosterPackFile(void)
{
	if(posterPackFileOwned && posterPackFile.device != NULL &&
		posterPackFile.device->closeFile != NULL) {
		posterPackFile.device->closeFile(&posterPackFile);
	}
	memset(&posterPackFile, 0, sizeof(posterPackFile));
	posterPackFileOwned = false;
}

void DrawGameflowCancelPosters(void)
{
	if(!posterPackAttempted && !posterPackFileOwned) {
		return;
	}

	/* Unpublish the pack before closing its source. Video readers use the
	 * same private mutex through ui_assets' callback contract. */
	UIAssets_CancelForDeviceChange();
	_GameflowClosePosterPackFile();
	posterPackDevice = NULL;
	posterPackAttempted = false;
}

static s32 _GameflowOnReset(s32 final)
{
	if(!final) {
		/* Reset callbacks run in ascending priority. Retire the UI-owned pack
		 * while its source and video mutex are still live; device teardown is
		 * deliberately registered one priority later. */
		DrawGameflowCancelPosters();
	}
	return TRUE;
}

static bool _GameflowOpenPosterPack(DEVICEHANDLER_INTERFACE *device)
{
	uiAssetsSource_t source;
	uiAssetsSync_t sync;
	s32 result;

	if(device == NULL || device->initial == NULL ||
		_videomutex == LWP_MUTEX_NULL) {
		return false;
	}
	if(posterPackAttempted && posterPackDevice == device) {
		return UIAssets_Ready();
	}
	if(posterPackAttempted || posterPackFileOwned) {
		DrawGameflowCancelPosters();
	}

	/* Latch one attempt per mounted device. A missing/corrupt private pack
	 * must not turn an idle game-library frame into repeated device I/O. */
	posterPackDevice = device;
	posterPackAttempted = true;
	memset(&posterPackFile, 0, sizeof(posterPackFile));
	concat_path(posterPackFile.name, device->initial->name,
		"swiss/ui/posters.pak");
	posterPackFile.device = device;
	posterPackFileOwned = true;

	result = UIAssets_SourceFromFileHandle(&posterPackFile, &source);
	if(result == UI_ASSETS_OK) {
		UIAssets_SyncFromMutex(_videomutex, &sync);
		result = UIAssets_Init(&source, &sync);
	}
	if(result != UI_ASSETS_OK) {
		_GameflowClosePosterPackFile();
		return false;
	}
	return true;
}

void DrawGameflowRequestPosters(DEVICEHANDLER_INTERFACE *device,
	const uiGameflowRenderSnapshot_t *snapshot)
{
	char ids[UI_ASSETS_WINDOW][8] = {{0}};
	u32 i;

	if(!_GameflowSnapshotValid(snapshot) ||
		!_GameflowOpenPosterPack(device)) {
		return;
	}
	for(i = 0u; i < snapshot->recordCount; ++i) {
		const uiGameflowCardSnapshot_t *record = &snapshot->records[i];
		int position = (int)record->relativeSlot + 3;
		if(position < 0 || position >= UI_ASSETS_WINDOW ||
			strnlen(record->gameId, sizeof(record->gameId)) !=
				UI_ASSETS_ID_LEN) {
			continue;
		}
		memcpy(ids[position], record->gameId, UI_ASSETS_ID_LEN);
	}
	/* Fixed -3..+3 placement preserves true carousel distance even when
	 * the parent entry has no poster ID or a small library has gaps. */
	UIAssets_RequestWindow(ids, UI_ASSETS_WINDOW, 3);
}

bool DrawGameflowPollPosters(void)
{
	return UIAssets_Poll();
}

static void _GameflowCopySnapshot(drawGameflowEvent_t *data,
	const uiGameflowRenderSnapshot_t *snapshot)
{
	u32 i;

	memcpy(&data->snapshot, snapshot, sizeof(data->snapshot));
	memset(data->cardPresentation, 0, sizeof(data->cardPresentation));
	for(i = 0u; i < UI_GAMEFLOW_RENDER_SLOTS; ++i) {
		uiGameflowCardSnapshot_t *record = &data->snapshot.records[i];
		memset(&data->bannerTexObj[i], 0, sizeof(data->bannerTexObj[i]));
		if(i >= data->snapshot.recordCount) {
			continue;
		}
		_GameflowPrepareCardPresentation(data, i);
		if(!(record->flags & UI_GAMEFLOW_CARD_HAS_BANNER)) {
			continue;
		}
		DCFlushRange(record->banner, BNR_PIXELDATA_LEN);
		GX_InitTexObj(&data->bannerTexObj[i], record->banner, 96, 32,
			GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE);
		GX_InitTexObjFilterMode(&data->bannerTexObj[i], GX_LINEAR, GX_NEAR);
	}
}

uiDrawObj_t* DrawGameflow(const uiGameflowRenderSnapshot_t *snapshot)
{
	drawGameflowEvent_t *eventData;
	uiDrawObj_t *event;

	if(!_GameflowSnapshotValid(snapshot)) {
		return NULL;
	}
	eventData = memalign(32, sizeof(drawGameflowEvent_t));
	event = calloc(1, sizeof(uiDrawObj_t));
	if(eventData == NULL || event == NULL) {
		free(eventData);
		free(event);
		return NULL;
	}
	memset(eventData, 0, sizeof(*eventData));
	UIGameflow_Init(&eventData->state);
	if(!UIGameflow_ApplySnapshot(&eventData->state, &snapshot->selection,
		_CurrentMotionMode())) {
		free(eventData);
		free(event);
		return NULL;
	}
	_GameflowCopySnapshot(eventData, snapshot);
	event->type = EV_GAMEFLOW;
	event->data = eventData;
	return event;
}

bool DrawUpdateGameflow(uiDrawObj_t *evt,
	const uiGameflowRenderSnapshot_t *snapshot)
{
	bool updated = false;

	if(evt == NULL || !_GameflowSnapshotValid(snapshot)) {
		return false;
	}
	LWP_MutexLock(_videomutex);
	if(!evt->disposed && evt->type == EV_GAMEFLOW && evt->data != NULL) {
		drawGameflowEvent_t *data = (drawGameflowEvent_t*)evt->data;
		if(UIGameflow_ApplySnapshot(&data->state, &snapshot->selection,
			_CurrentMotionMode())) {
			_GameflowCopySnapshot(data, snapshot);
			updated = true;
		}
	}
	LWP_MutexUnlock(_videomutex);
	return updated;
}

bool DrawSetGameflowMode(uiDrawObj_t *evt, uiGameflowMode_t mode)
{
	bool updated = false;

	if(evt == NULL) {
		return false;
	}
	LWP_MutexLock(_videomutex);
	if(!evt->disposed && evt->type == EV_GAMEFLOW && evt->data != NULL) {
		drawGameflowEvent_t *data = (drawGameflowEvent_t*)evt->data;
		UIGameflow_SetMode(&data->state, mode, _CurrentMotionMode());
		updated = true;
	}
	LWP_MutexUnlock(_videomutex);
	return updated;
}

bool DrawUpdateGameflowDetail(uiDrawObj_t *evt,
	const uiGameflowDetailSnapshot_t *snapshot)
{
	bool updated = false;

	if(evt == NULL || snapshot == NULL ||
		(snapshot->flags & UI_GAMEFLOW_DETAIL_VALID) == 0u) {
		return false;
	}
	LWP_MutexLock(_videomutex);
	if(!evt->disposed && evt->type == EV_GAMEFLOW && evt->data != NULL) {
		drawGameflowEvent_t *data = (drawGameflowEvent_t*)evt->data;
		const uiGameflowFrame_t *frame = UIGameflow_Frame(&data->state);
		const uiGameflowCardSnapshot_t *record = frame != NULL ?
			_GameflowFindRecord(&data->snapshot, frame->focusIndex, NULL) : NULL;

		if(frame != NULL && record != NULL &&
			UIGameflowDetail_Matches(snapshot, frame->generation,
				frame->focusIndex, record->gameId,
				strnlen(record->gameId, sizeof(record->gameId)))) {
			memcpy(&data->detail, snapshot, sizeof(data->detail));
			_GameflowPrepareDetailPresentation(data);
			memset(&data->detailBannerTexObj, 0,
				sizeof(data->detailBannerTexObj));
			if((data->detail.flags & UI_GAMEFLOW_DETAIL_HAS_BANNER) != 0u) {
				DCFlushRange(data->detail.banner,
					UI_GAMEFLOW_DETAIL_BANNER_BYTES);
				GX_InitTexObj(&data->detailBannerTexObj, data->detail.banner,
					96, 32, GX_TF_RGB5A3, GX_CLAMP, GX_CLAMP, GX_FALSE);
				GX_InitTexObjFilterMode(&data->detailBannerTexObj,
					GX_LINEAR, GX_NEAR);
			}
			updated = true;
		}
	}
	LWP_MutexUnlock(_videomutex);
	return updated;
}

void DrawClearGameflowDetail(uiDrawObj_t *evt)
{
	if(evt == NULL) {
		return;
	}
	LWP_MutexLock(_videomutex);
	if(!evt->disposed && evt->type == EV_GAMEFLOW && evt->data != NULL) {
		drawGameflowEvent_t *data = (drawGameflowEvent_t*)evt->data;
		memset(&data->detail, 0, sizeof(data->detail));
		memset(&data->detailPresentation, 0,
			sizeof(data->detailPresentation));
		memset(&data->detailBannerTexObj, 0,
			sizeof(data->detailBannerTexObj));
	}
	LWP_MutexUnlock(_videomutex);
}

// Internal
static void _DrawVertScrollBar(uiDrawObj_t *evt) {
	drawVertScrollbarEvent_t *data = (drawVertScrollbarEvent_t*)evt->data;
	int x1 = data->x;
	int x2 = data->x+data->width;
	int y1 = data->y;
	int y2 = data->y+data->height;
	int scrollStartY = y1+3 + (int)((data->height-6-data->scrollHeight)*data->scrollPercent);

	if(scrollStartY > y2-3-data->scrollHeight)
		scrollStartY = y2-3-data->scrollHeight;
	
	GXColor fillColor = (GXColor) {46,57,104,GUI_MSGBOX_ALPHA}; 	//bluish
  	GXColor noColor = (GXColor) {0,0,0,0}; //blank
	GXColor borderColor = (GXColor) {200,200,200,GUI_MSGBOX_ALPHA}; //silver
	
	_DrawSimpleBox( x1, y1, x2-x1, y2-y1, 0, noColor, borderColor);
	
	_DrawSimpleBox( x1, scrollStartY,
			data->width, data->scrollHeight, 0, fillColor, borderColor); 
}

// External
uiDrawObj_t* DrawVertScrollBar(int x, int y, int width, int height, float scrollPercent, int scrollHeight) {
	scrollHeight = scrollHeight < 10 ? 10:scrollHeight;
	drawVertScrollbarEvent_t *eventData = calloc(1, sizeof(drawVertScrollbarEvent_t));
	eventData->x = x;
	eventData->y = y;
	eventData->width = width;
	eventData->height = height;
	eventData->scrollPercent = scrollPercent;
	eventData->scrollHeight = scrollHeight;
	uiDrawObj_t *event = calloc(1, sizeof(uiDrawObj_t));
	event->type = EV_VERTSCROLLBAR;
	event->data = eventData;
	return event;
}

static uiDrawObj_t* drawParameterForArgsSelector(Parameter *param, int x, int y, int selected) {

	uiDrawObj_t* container = DrawContainer();
	char *name = &param->arg.name[0];
	char *selValue = &param->values[param->currentValueIdx].name[0];
	
	int chkWidth = 32, nameWidth = 300, gapWidth = 13, paramWidth = 120;
	// [32px 10px 250px 10px 5px 80px 5px]
	// If not selected and not enabled, use greyed out font for everything
	GXColor fontColor = (param->enable || selected) ? defaultColor : deSelectedColor;

	// If selected draw that it's selected
	if(selected) DrawAddChild(container, DrawTransparentBox( x+chkWidth+gapWidth, y, getVideoMode()->fbWidth-52, y+30));
	DrawAddChild(container, DrawImage(param->enable ? TEX_CHECKED:TEX_UNCHECKED, x, y, 32, 32, 0, 0.0f, 1.0f, 0.0f, 1.0f, 0));
	// Draw the parameter Name
	DrawAddChild(container, DrawStyledLabel(x+chkWidth+gapWidth+5, y+15, name, GetTextScaleToFitInWidth(name, nameWidth-10), ALIGN_LEFT, fontColor));
	// If enabled, draw arrows indicating where in the param list we are
	if(selected && param->enable && param->num_values > 1) {
		if(param->currentValueIdx != 0) {
			DrawAddChild(container, DrawStyledLabel(x+(chkWidth+nameWidth+(gapWidth*4)), y+15, "\213", .8f, ALIGN_LEFT, defaultColor));
		}
		if(param->currentValueIdx != param->num_values-1) {
			DrawAddChild(container, DrawStyledLabel(x+(chkWidth+nameWidth+paramWidth+(gapWidth*7)), y+15, "\233", .8f, ALIGN_LEFT, defaultColor));
		}
	}
	// Draw the current value
	DrawAddChild(container, DrawStyledLabel(x+chkWidth+nameWidth+(gapWidth*6), y+15, selValue, GetTextScaleToFitInWidth(selValue, paramWidth), ALIGN_LEFT, fontColor));
	return container;
}

// External
void DrawArgsSelector(const char *fileName) {
	Parameters* params = getParameters();
	int param_selection = 0;
	int params_per_page = 6;
	
	while ((padsButtonsHeld() & BUTTON_A)){ VIDEO_WaitVSync (); }
	uiDrawObj_t *container = NULL;
	while(1) {
		uiDrawObj_t *newPanel = DrawEmptyBox(20,60, getVideoMode()->fbWidth-20, 460);
		sprintf(txtbuffer, "%s Parameters:", fileName);
		DrawAddChild(newPanel, DrawStyledLabel(25, 74, txtbuffer, GetTextScaleToFitInWidth(txtbuffer, getVideoMode()->fbWidth-50), ALIGN_LEFT, defaultColor));

		int i = 0, j = 0;
		int current_view_start = MIN(MAX(0,param_selection-params_per_page/2),MAX(0,params->num_params-params_per_page));
		int current_view_end = MIN(params->num_params, MAX(param_selection+params_per_page/2,params_per_page));
	
		int scrollBarHeight = 90+(params_per_page*20);
		int scrollBarTabHeight = (int)((float)scrollBarHeight/(float)params->num_params);
		DrawAddChild(newPanel, DrawVertScrollBar(getVideoMode()->fbWidth-45, 120, 25, scrollBarHeight, (float)((float)param_selection/(float)(params->num_params-1)),scrollBarTabHeight));
		for(i = current_view_start,j = 0; i<current_view_end; ++i,++j) {
			DrawAddChild(newPanel, drawParameterForArgsSelector(&params->parameters[i], 25, 120+j*35, i==param_selection));
		}
		// Write about the default if there is any
		DrawAddChild(newPanel, DrawTransparentBox( 35, 350, getVideoMode()->fbWidth-35, 400));
		DrawAddChild(newPanel, DrawStyledLabel(33, 354, "Default values will be used by the DOL being loaded if a", 0.8f, ALIGN_LEFT, defaultColor));
		DrawAddChild(newPanel, DrawStyledLabel(33, 374, "parameter is not enabled. Please check the documentation", 0.8f, ALIGN_LEFT, defaultColor));
		DrawAddChild(newPanel, DrawStyledLabel(33, 394, "for this DOL if you are unsure of the default values.", 0.8f, ALIGN_LEFT, defaultColor));
		DrawAddChild(newPanel, DrawHintLabel(640/2, 440, "A  Toggle Param    START  Load the DOL", 1.0f, ALIGN_CENTER, defaultColor));
		
		container = DrawRepublish(container, newPanel);
		
		while (!(padsButtonsHeld() & (BUTTON_RIGHT|BUTTON_LEFT|BUTTON_UP|BUTTON_DOWN|BUTTON_START|BUTTON_A)))
			{ VIDEO_WaitVSync (); }
		u32 btns = padsButtonsHeld();
		if((btns & (BUTTON_RIGHT|BUTTON_LEFT)) && params->parameters[param_selection].enable) {
			int curValIdx = params->parameters[param_selection].currentValueIdx;
			int maxValIdx = params->parameters[param_selection].num_values;
			curValIdx = btns & BUTTON_LEFT ? 
				((--curValIdx < 0) ? maxValIdx-1 : curValIdx):((curValIdx + 1) % maxValIdx);
			params->parameters[param_selection].currentValueIdx = curValIdx;
		}
		if(btns & (BUTTON_UP|BUTTON_DOWN)) {
			param_selection = btns & BUTTON_UP ? 
				((--param_selection < 0) ? params->num_params-1 : param_selection)
				:((param_selection + 1) % params->num_params);
		}
		if(btns & BUTTON_A) {
			params->parameters[param_selection].enable ^= 1;
		}
		if(btns & BUTTON_START) {
			break;
		}
		while (padsButtonsHeld() & (BUTTON_RIGHT|BUTTON_LEFT|BUTTON_UP|BUTTON_DOWN|BUTTON_START|BUTTON_A))
			{ VIDEO_WaitVSync (); }
	}
	DrawDispose(container);
}

/* Retained cheat screen: menu thread prepares text and selection snapshots;
 * the video thread only draws and advances the focus spring. */
typedef struct {
	char title[UI_CHEATS_TEXT_CAPACITY];
	char rows[UI_CHEATS_VISIBLE_ROWS][UI_CHEATS_TEXT_CAPACITY];
	char selectedName[3][UI_CHEATS_TEXT_CAPACITY];
	char enabledText[32];
	char positionText[32];
	char memoryText[64];
	char warning[96];
	bool enabled[UI_CHEATS_VISIBLE_ROWS];
	bool enabledOnly;
	bool advanced;
	bool debug;
	int count;
	int first;
	int rowCount;
	int focusRow;
	int memoryWidth;
} uiCheatsSnapshot_t;

typedef struct {
	uiCheatsSnapshot_t snapshot;
	uiMotionSpring_t focusY;
	bool focusInitialized;
	bool lastAdvanced;
} drawCheatsEvent_t;

static bool _CheatsEnabled(int index, const void *context)
{
	const CheatEntries *cheats = context;
	return cheats->cheat[index].enabled != 0;
}

static void _CheatsSnapshot(uiCheatsSnapshot_t *out, const char *fileName,
	const CheatEntries *cheats, int selection, bool enabledOnly, bool advanced,
	const char *warning)
{
	char text[UI_CHEATS_SOURCE_LIMIT + 1u];
	int i;
	int selectedIndex;
	int used = getEnabledCheatsSize();
	int capacity = kenobi_get_maxsize();
	memset(out, 0, sizeof(*out));
	UICheats_GameTitle(text, sizeof(text), fileName);
	UICheats_Fit(out->title, sizeof(out->title), text[0] ? text : "Game",
		548, 0.64f, GetTextSizeInPixels);
	out->enabledOnly = enabledOnly;
	out->advanced = advanced;
	out->debug = swissSettings.wiirdDebug != 0;
	out->count = UICheats_Count(cheats->num_cheats, enabledOnly,
		_CheatsEnabled, cheats);
	out->first = UICheats_WindowStart(selection, out->count);
	out->focusRow = selection - out->first;
	(void)snprintf(out->enabledText, sizeof(out->enabledText), "%d enabled",
		getEnabledCheatsCount());
	if(out->count > 0) {
		(void)snprintf(out->positionText, sizeof(out->positionText), "%d / %d",
			selection + 1, out->count);
	}
	(void)snprintf(out->memoryText, sizeof(out->memoryText),
		"Cheat memory   %d of %d bytes", used, capacity);
	if(capacity > 0 && used > 0) {
		double fraction = (double)used / (double)capacity;
		out->memoryWidth = fraction >= 1.0 ? 512 : (int)(fraction * 512.0);
	}
	UICheats_Fit(out->warning, sizeof(out->warning), warning,
		548, 0.48f, GetTextSizeInPixels);
	for(i = 0; i < UI_CHEATS_VISIBLE_ROWS; ++i) {
		int index = UICheats_Index(cheats->num_cheats, enabledOnly,
			out->first + i, _CheatsEnabled, cheats);
		if(index < 0) break;
		UICheats_Name(text, sizeof(text), cheats->cheat[index].name);
		UICheats_Fit(out->rows[i], sizeof(out->rows[i]),
			text[0] ? text : "Unnamed cheat", 454, 0.66f, GetTextSizeInPixels);
		out->enabled[i] = cheats->cheat[index].enabled != 0;
		++out->rowCount;
	}
	selectedIndex = UICheats_Index(cheats->num_cheats, enabledOnly,
		selection, _CheatsEnabled, cheats);
	UICheats_Wrap(out->selectedName,
		selectedIndex >= 0 ? cheats->cheat[selectedIndex].name : "No cheat selected",
		520, 0.58f, GetTextSizeInPixels);
}

/* Flat, lightly rounded geometry; no legacy bevel/shadow or backdrop leak. */
static void _CheatsPanel(int x, int y, int width, int height, GXColor color)
{
	const int radius = height > 8 && width > 12 ? 6 : 0;
	const int px[9] = {x + radius, x + width - radius, x + width,
		x + width, x + width - radius, x + radius, x, x, x + radius};
	const int py[9] = {y, y, y + radius, y + height - radius, y + height,
		y + height, y + height - radius, y + radius, y};
	int i;
	UIColor_Apply(&color.r, &color.g, &color.b);
	drawInit();
	_SetupRasterColor();
	GX_Begin(GX_TRIANGLEFAN, GX_VTXFMT0, 10);
		GX_Position3f32((float)x + (float)width * 0.5f,
			(float)y + (float)height * 0.5f, 0.0f);
		GX_Color4u8(color.r, color.g, color.b, color.a);
		GX_TexCoord2f32(0.0f, 0.0f);
		for(i = 0; i < 9; ++i) {
			GX_Position3f32((float)px[i], (float)py[i], 0.0f);
			GX_Color4u8(color.r, color.g, color.b, color.a);
			GX_TexCoord2f32(0.0f, 0.0f);
		}
	GX_End();
	drawInit();
}

static void _CheatsToggle(int x, int y, bool enabled)
{
	_CheatsPanel(x, y, 62, 24, enabled ? (GXColor){26, 93, 103, 255} :
		(GXColor){34, 42, 65, 255});
	drawStringMedium(x + 31, y + 12, enabled ? "ON" : "OFF", 0.60f,
		ALIGN_CENTER, enabled ? (GXColor){151, 250, 246, 255} :
		(GXColor){184, 192, 215, 255});
}

static void _DrawCheats(uiDrawObj_t *evt)
{
	drawCheatsEvent_t *data = evt->data;
	const uiCheatsSnapshot_t *s = &data->snapshot;
	const GXColor primary = {241, 244, 255, 255};
	const GXColor secondary = {173, 187, 216, 255};
	const GXColor accent = {115, 234, 234, 255};
	uiMotionMode_t motion = _CurrentMotionMode();
	float target = s->advanced ? 264.0f : (float)(148 + s->focusRow * 40);
	int focusY;
	int i;
	if(!data->focusInitialized || data->lastAdvanced != s->advanced) {
		UIMotion_SpringInit(&data->focusY, target, 25.0f);
		data->focusInitialized = true;
	}
	data->lastAdvanced = s->advanced;
	UIMotion_SpringRetarget(&data->focusY, target, motion);
	focusY = (int)lrintf(UIMotion_SpringUpdate(&data->focusY,
		UIAnim_Delta(), motion));

	_CheatsPanel(-6, -6, 652, 492, (GXColor){8, 12, 27, 254});
	_CheatsPanel(40, 30, 40, 3, accent);
	drawStringMedium(40, 63, "Cheats", 1.05f, ALIGN_LEFT, primary);
	drawStringMedium(600, 63, s->enabledText, 0.54f, ALIGN_RIGHT, accent);
	drawStringMedium(40, 98, s->title, 0.64f, ALIGN_LEFT, secondary);
	_CheatsPanel(40, 115, 560, 1, (GXColor){43, 52, 80, 255});

	if(s->advanced) {
		drawStringMedium(40, 132, "ADVANCED", 0.42f, ALIGN_LEFT, accent);
		drawStringMedium(52, 166, "Selected cheat", 0.48f, ALIGN_LEFT, secondary);
		for(i = 0; i < 3; ++i) {
			drawStringMedium(52, 192 + i * 21, s->selectedName[i], 0.58f,
				ALIGN_LEFT, primary);
		}
		_CheatsPanel(40, focusY, 560, 44, (GXColor){38, 37, 78, 255});
		_CheatsPanel(40, focusY + 6, 3, 32, accent);
		drawStringMedium(52, 286, "WiiRD Debug", 0.66f, ALIGN_LEFT, primary);
		_CheatsToggle(526, 274, s->debug);
		drawStringMedium(52, 327, "For compatible debugging tools.", 0.48f,
			ALIGN_LEFT, secondary);
		drawStringMedium(52, 356, s->memoryText, 0.54f, ALIGN_LEFT, secondary);
		_CheatsPanel(52, 376, 512, 4, (GXColor){34, 42, 65, 255});
		if(s->memoryWidth > 0) _CheatsPanel(52, 376, s->memoryWidth, 4, accent);
	}
	else {
		drawStringMedium(40, 132, s->enabledOnly ? "ENABLED ONLY" : "ALL CHEATS",
			0.42f, ALIGN_LEFT, accent);
		drawStringMedium(600, 132, s->positionText, 0.46f, ALIGN_RIGHT, secondary);
		for(i = 0; i < s->rowCount; ++i) {
			_CheatsPanel(40, 148 + i * 40, 560, 34, (GXColor){16, 23, 43, 255});
		}
		if(s->rowCount > 0) {
			_CheatsPanel(40, focusY, 560, 34, (GXColor){38, 37, 78, 255});
			_CheatsPanel(40, focusY + 4, 3, 26, accent);
		}
		for(i = 0; i < s->rowCount; ++i) {
			drawStringMedium(52, 165 + i * 40, s->rows[i], 0.66f,
				ALIGN_LEFT, primary);
			_CheatsToggle(526, 153 + i * 40, s->enabled[i]);
		}
		if(s->count > UI_CHEATS_VISIBLE_ROWS) {
			int thumb = 234 * UI_CHEATS_VISIBLE_ROWS / s->count;
			int travel;
			if(thumb < 12) thumb = 12;
			travel = (int)((int64_t)(234 - thumb) * s->first /
				(s->count - UI_CHEATS_VISIBLE_ROWS));
			_CheatsPanel(608, 148, 2, 234, (GXColor){34, 42, 65, 255});
			_CheatsPanel(608, 148 + travel, 2, thumb, accent);
		}
		if(s->rowCount == 0) {
			drawStringMedium(320, 236, s->enabledOnly ?
				"No cheats enabled" : "No cheats available", 0.76f,
				ALIGN_CENTER, primary);
			_DrawHintText(320, 269, s->enabledOnly ?
				"X  Browse all cheats" : "B  Return to your game",
				0.54f, ALIGN_CENTER, secondary);
		}
	}
	if(s->warning[0]) drawStringMedium(40, 400, s->warning, 0.48f,
		ALIGN_LEFT, (GXColor){255, 207, 139, 255});
	_CheatsPanel(40, 413, 560, 1, (GXColor){43, 52, 80, 255});
	if(s->advanced) {
		_DrawHintText(40, 435, "A  Toggle debug", 0.48f, ALIGN_LEFT, primary);
		_DrawHintText(600, 435, "B  Back", 0.48f, ALIGN_RIGHT, secondary);
	}
	else {
		_DrawHintText(40, 435, "A  Toggle", 0.48f, ALIGN_LEFT, primary);
		_DrawHintText(169, 435, "B  Done", 0.48f, ALIGN_LEFT, secondary);
		_DrawHintText(290, 435, s->enabledOnly ? "X  Show all" : "X  Enabled only",
			0.48f, ALIGN_LEFT, secondary);
		_DrawHintText(600, 435, "Z  Advanced", 0.48f, ALIGN_RIGHT, secondary);
	}
	drawInit();
}

static u32 _CheatsElapsed(u32 *lastRetrace)
{
	u32 retrace = VIDEO_GetRetraceCount();
	u32 count = retrace - *lastRetrace;
	float rate = VIDEO_GetRetraceRate();
	float elapsed;
	*lastRetrace = retrace;
	if(!isfinite(rate) || rate < 1.0f) rate = 60.0f;
	elapsed = (float)count * 1000000.0f / rate;
	return elapsed >= (float)UI_MENU_INPUT_MAX_ELAPSED_US ?
		UI_MENU_INPUT_MAX_ELAPSED_US : (u32)elapsed;
}

void DrawCheatsSelector(const char *fileName)
{
	const u32 directions = BUTTON_UP | BUTTON_DOWN | BUTTON_LEFT |
		BUTTON_RIGHT | BUTTON_L | BUTTON_R;
	const u32 buttons = directions | BUTTON_A | BUTTON_B | BUTTON_X | BUTTON_Z;
	CheatEntries *cheats = getCheats();
	uiDrawObj_t *event = calloc(1, sizeof(*event));
	drawCheatsEvent_t *data = calloc(1, sizeof(*data));
	uiMenuInputState_t menuInput;
	int selection = 0;
	bool enabledOnly = false;
	bool advanced = false;
	bool dirty = true;
	const char *warning = "";
	u32 previous = padsButtonsHeld() & buttons;
	u32 lastRetrace = VIDEO_GetRetraceCount();
	u32 repeatHeld = 0u;
	u32 repeatTime = 0u;
	bool repeated = false;
	if(event == NULL || data == NULL) { free(event); free(data); return; }
	UIMenuInput_Init(&menuInput);
	event->type = EV_CHEATS;
	event->data = data;
	_CheatsSnapshot(&data->snapshot, fileName, cheats, selection,
		enabledOnly, advanced, warning);
	DrawPublish(event);
	while(1) {
		u32 held;
		u32 pressed;
		u32 elapsed;
		u32 heldDirection;
		uiMenuInputDirection_t analog;
		int count;
		int index;
		if(dirty) {
			uiCheatsSnapshot_t snapshot;
			_CheatsSnapshot(&snapshot, fileName, cheats, selection,
				enabledOnly, advanced, warning);
			LWP_MutexLock(_videomutex);
			data->snapshot = snapshot;
			LWP_MutexUnlock(_videomutex);
			dirty = false;
		}
		VIDEO_WaitVSync();
		held = padsButtonsHeld() & buttons;
		pressed = held & ~previous;
		previous = held;
		elapsed = _CheatsElapsed(&lastRetrace);
		analog = padsMenuInputPoll(&menuInput, elapsed,
			UI_MENU_INPUT_AXIS_VERTICAL | UI_MENU_INPUT_REPEAT, held != 0u);
		heldDirection = held & directions;
		if(heldDirection != repeatHeld) {
			repeatHeld = heldDirection;
			repeatTime = 0u;
			repeated = false;
		}
		else if(heldDirection != 0u && (held & ~directions) == 0u) {
			repeatTime += elapsed;
			if(repeatTime >= (repeated ? UI_MENU_INPUT_REPEAT_US :
					UI_MENU_INPUT_INITIAL_REPEAT_US)) {
				pressed |= heldDirection;
				repeatTime = 0u;
				repeated = true;
			}
		}
		if(analog == UI_MENU_INPUT_UP) pressed |= BUTTON_UP;
		if(analog == UI_MENU_INPUT_DOWN) pressed |= BUTTON_DOWN;
		if(pressed == 0u) continue;
		count = UICheats_Count(cheats->num_cheats, enabledOnly, _CheatsEnabled, cheats);
		index = UICheats_Index(cheats->num_cheats, enabledOnly, selection,
			_CheatsEnabled, cheats);
		warning = "";
		if(pressed & BUTTON_B) {
			if(!advanced) break;
			advanced = false;
		}
		else if(pressed & BUTTON_Z) advanced = !advanced;
		else if(!advanced && (pressed & BUTTON_X)) {
			enabledOnly = !enabledOnly;
			selection = UICheats_Preserve(cheats->num_cheats, enabledOnly,
				index, _CheatsEnabled, cheats);
		}
		else if(pressed & BUTTON_A) {
			if(advanced) {
				if(swissSettings.wiirdDebug) swissSettings.wiirdDebug = 0;
				else if(cheatsCanEnableDebug()) swissSettings.wiirdDebug = 1;
				else warning = "Turn off a cheat to make room for debugging.";
			}
			else if(index >= 0) {
				cheats->cheat[index].enabled ^= 1;
				if(getEnabledCheatsSize() > kenobi_get_maxsize()) {
					cheats->cheat[index].enabled = 0;
					warning = "Not enough room. Turn off another cheat first.";
				}
				selection = UICheats_Preserve(cheats->num_cheats, enabledOnly,
					index, _CheatsEnabled, cheats);
			}
		}
		else if(!advanced) {
			if(pressed & (BUTTON_UP | BUTTON_LEFT | BUTTON_L))
				selection = UICheats_Move(selection, count, -1,
					(pressed & (BUTTON_LEFT | BUTTON_L)) != 0u);
			else if(pressed & (BUTTON_DOWN | BUTTON_RIGHT | BUTTON_R))
				selection = UICheats_Move(selection, count, 1,
					(pressed & (BUTTON_RIGHT | BUTTON_R)) != 0u);
		}
		dirty = true;
	}
	DrawDispose(event);
}


void DrawGetTextEntry(int mode, const char *label, void *src, int size) {
	
	print_debug("DrawGetTextEntry Modes: Alpha [%s] Numeric [%s] IP [%s] Masked [%s] File [%s]\n", mode & ENTRYMODE_ALPHA ? "Y":"N", mode & ENTRYMODE_NUMERIC ? "Y":"N",
																	mode & ENTRYMODE_IP ? "Y":"N", mode & ENTRYMODE_MASKED ? "Y":"N", mode & ENTRYMODE_FILE ? "Y":"N");
	char *text = calloc(1, size + 1);
	if(mode & (ENTRYMODE_ALPHA|ENTRYMODE_IP)) {
		strncpy(text, src, size);
	}
	else {
		u16 *src_int = (u16*)src;
		itoa(*src_int, text, 10);
	}
	print_debug("Text is [%s] size %i\n", text, size);
	
	int caret = strlen(text);
	int cur_row = 0;
	int cur_col = 0;
	int num_rows = 0;
	int num_per_row[5] = {0,0,0,0,0};	// number of keys per row
	int pos_for_row[5] = {0,0,0,0,0};	// X pos to start drawing keys from
	int grid_gap = 45;
	int num_txt_modes = 0;	// Number of modes the text entry chars will have, e.g. upper case, lowercase etc.
	char *txt_modes_str[] = {"lowercase", "UPPERCASE"};
	// char arrays to grab from, exact order is important
	char *ip_mode_chars = "123456789.0\b";
	char *num_mode_chars = "1234567890\b";
	char *txt_mode_chars_lower = "1234567890-=\bqwertyuiop[]\\asdfghjkl;'zxcvbnm,./`!@\a#$%";
	char *txt_mode_chars_upper = "1234567890_+\bQWERTYUIOP{}|ASDFGHJKL:\"ZXCVBNM<>?^&*\a()~";
	char *txt_mode_file_chars_lower = "1234567890-=\bqwertyuiop[]asdfghjkl;'zxcvbnm.`!\a#$%";
	char *txt_mode_file_chars_upper = "1234567890_+\bQWERTYUIOP{}ASDFGHJKL^&ZXCVBNM,@~\a()%";
	
	int cur_txt_mode = 0;
	char *gridText = NULL;
	
	// IP mode
	if(mode & ENTRYMODE_IP) {
		// 1  2  3
		// 4  5  6
		// 7  8  9
		//[.] 0  [backspace]
		num_rows = 4;
		grid_gap = 15;
		num_per_row[0] = 3;
		pos_for_row[0] = 240;
		num_per_row[1] = 3;
		pos_for_row[1] = 240;
		num_per_row[2] = 3;
		pos_for_row[2] = 240;
		num_per_row[3] = 3;
		pos_for_row[3] = 240;
		gridText = ip_mode_chars;
	}
	
	// Number only
	if((mode & ENTRYMODE_NUMERIC) && !(mode & ENTRYMODE_ALPHA)) {
		// 1  2  3
		// 4  5  6
		// 7  8  9
		// 0  [backspace]
		num_rows = 4;
		grid_gap = 15;
		num_per_row[0] = 3;
		pos_for_row[0] = 240;
		num_per_row[1] = 3;
		pos_for_row[1] = 240;
		num_per_row[2] = 3;
		pos_for_row[2] = 240;
		num_per_row[3] = 2;
		pos_for_row[3] = 240;
		gridText = num_mode_chars;
	}
	
	// Alpha only
	else if(!(mode & ENTRYMODE_NUMERIC) && (mode & ENTRYMODE_ALPHA)) {
		// TODO if we ever have to.
	}
	
	// Alphanumeric (not file)
	else if((mode & (ENTRYMODE_NUMERIC | ENTRYMODE_ALPHA)) && !(mode & ENTRYMODE_IP) && !(mode & ENTRYMODE_FILE)) {
		/* Mode 0:
		 1234567890-=<\b aka backspace>
		 qwertyuiop[]\
		 asdfghjkl;'
		 zxcvbnm,./
		 `!@<\a aka space>#$%
		
		 Mode 1:
		 1234567890_+<\b aka backspace>
		 QWERTYUIOP{}|
		 ASDFGHJKL:"
		 ZXCVBNM<>?
		 ^&*<\a aka space>()~
		*/
		
		num_txt_modes = 2;
		num_rows = 5;
		grid_gap = 10;
		num_per_row[0] = 13;
		pos_for_row[0] = 40;
		num_per_row[1] = 13;
		pos_for_row[1] = 60;
		num_per_row[2] = 11;
		pos_for_row[2] = 100;
		num_per_row[3] = 10;
		pos_for_row[3] = 120;
		num_per_row[4] = 7;
		pos_for_row[4] = 160;
	}
	
	// Alphanumeric (file)
	else if(mode & ENTRYMODE_FILE) {
		/* Mode 0:
		 1234567890-=<\b aka backspace>
		 qwertyuiop[]\
		 asdfghjkl;'
		 zxcvbnm
		 .`!<\a aka space>#$%
		
		 Mode 1:
		 1234567890_+<\b aka backspace>
		 QWERTYUIOP{}
		 ASDFGHJKL^&
		 ZXCVBNM
		 ,@~<\a aka space>()%
		*/
		
		num_txt_modes = 2;
		num_rows = 5;
		grid_gap = 10;
		num_per_row[0] = 13;
		pos_for_row[0] = 40;
		num_per_row[1] = 12;
		pos_for_row[1] = 60;
		num_per_row[2] = 11;
		pos_for_row[2] = 100;
		num_per_row[3] = 7;
		pos_for_row[3] = 120;
		num_per_row[4] = 7;
		pos_for_row[4] = 160;
	}
	
	// Wait for any A or Left/Right presses to finish
	while ((padsButtonsHeld() & (BUTTON_A|BUTTON_LEFT|BUTTON_RIGHT))){ VIDEO_WaitVSync (); }
	uiDrawObj_t *container = NULL;
	while(1) {
		// Double box for extra darkness
		uiDrawObj_t *newPanel = DrawEmptyBox(20,60, getVideoMode()->fbWidth-20, 440);
		DrawAddChild(newPanel, DrawEmptyBox(20,60, getVideoMode()->fbWidth-20, 440));
		sprintf(txtbuffer, "%s - Please enter a value", label);
		DrawAddChild(newPanel, DrawStyledLabel(25, 74, txtbuffer, GetTextScaleToFitInWidth(txtbuffer, getVideoMode()->fbWidth-50), ALIGN_LEFT, defaultColor));

		// Draw the text entry box (TODO: mask chars if the mode says to do so)
		DrawAddChild(newPanel, DrawEmptyBox(40, 100, getVideoMode()->fbWidth-40, 140));
		DrawAddChild(newPanel, DrawStyledLabelWithCaret(320, 120, text, GetTextScaleToFitInWidth(text, getVideoMode()->fbWidth-90), ALIGN_CENTER, defaultColor, caret));
		DrawAddChild(newPanel, DrawHintLabel(320, 160, "L/R  Cursor    START  Accept    B  Discard", 0.75f, ALIGN_CENTER, defaultColor));

		// Alphanumeric has a little "mode" hint at the bottom (upper/lower case set switching)
		if(mode & ENTRYMODE_ALPHA) {
			gridText = cur_txt_mode == 0 ? 
				(mode & ENTRYMODE_FILE ? txt_mode_file_chars_lower : txt_mode_chars_lower)
				: 
				(mode & ENTRYMODE_FILE ? txt_mode_file_chars_upper : txt_mode_chars_upper);
			sprintf(txtbuffer, "X  Change to [%s], current mode is [%s]",
													txt_modes_str[(cur_txt_mode + 1 >= num_txt_modes ? 0 : cur_txt_mode + 1)], txt_modes_str[cur_txt_mode]);
			DrawAddChild(newPanel, DrawHintLabel(25, 427, txtbuffer, 0.65f, ALIGN_LEFT, defaultColor));
		}
		
		// Draw the grid
		int y = 200, dx = 0, dy = 0, i = 0;
		int button_height = 30;
		for(dy = 0; dy < num_rows; dy++) {
			int x = pos_for_row[dy];
			for(dx = 0; dx < num_per_row[dy]; dx++) {
				// Space and Backspace are special, draw them in double the width with smaller fonts
				bool isSpecial = (gridText[i] == '\a' || gridText[i] == '\b');
				int button_width = isSpecial ? (60 + grid_gap) : 30;
				DrawAddChild(newPanel, DrawEmptyColouredBox(x, y, x+button_width, y+button_height, cur_col == dx && cur_row == dy ? (GXColor) {96,107,164,GUI_MSGBOX_ALPHA} : (GXColor) {0,0,0,GUI_MSGBOX_ALPHA}));
				float fontSize = isSpecial ? 0.65f : 1.0f;
				if(isSpecial)
					sprintf(txtbuffer, "%s", gridText[i] == '\a' ? "Space" : "Y  Back");
				else
					sprintf(txtbuffer, "%c", gridText[i]);
				GXColor keyColor = cur_col == dx && cur_row == dy ? defaultColor : deSelectedColor;
				if(gridText[i] == '\b')
					DrawAddChild(newPanel, DrawHintLabel(x+(button_width/2), y+(button_height/2), txtbuffer, fontSize, ALIGN_CENTER, keyColor));
				else
					DrawAddChild(newPanel, DrawStyledLabel(x+(button_width/2), y+(button_height/2), txtbuffer, fontSize, ALIGN_CENTER, keyColor));
				x += (grid_gap + button_width);
				i++;
			}
			y += (grid_gap + button_height);
		}
		
		container = DrawRepublish(container, newPanel);
		
		while (!(padsButtonsHeld() & (BUTTON_L|BUTTON_R|BUTTON_UP|BUTTON_DOWN|BUTTON_LEFT|BUTTON_RIGHT|BUTTON_B|BUTTON_A|BUTTON_X|BUTTON_Y|BUTTON_START)))
			{ VIDEO_WaitVSync (); }
		u32 btns = padsButtonsHeld();
		// Key nav
		if(btns & BUTTON_DOWN) {
			cur_row = (cur_row + 1 >= num_rows ? 0 : cur_row + 1);
		}
		if(btns & BUTTON_UP) {
			cur_row = (cur_row == 0 ? num_rows - 1 : cur_row - 1);
		}
		if(btns & BUTTON_LEFT) {
			cur_col = (cur_col == 0 ? num_per_row[cur_row] - 1 : cur_col - 1);
		}
		if(btns & BUTTON_RIGHT) {
			cur_col = (cur_col + 1 >= num_per_row[cur_row] ? 0 : cur_col + 1);
		}
		// If we went off the end due to a row that has less than another
		if(cur_col >= num_per_row[cur_row]) cur_col = num_per_row[cur_row] - 1;
		// Key press handling
		if((btns & BUTTON_A) || (btns & BUTTON_Y)) {
			int char_pos = cur_col;
			for(i = 0; i < cur_row; i++)
				char_pos += num_per_row[i];
			char pressed = gridText[char_pos];
			// Handle normal character presses
			if(pressed != '\b' && !(btns & BUTTON_Y)) {
				if(caret < size && strlen(text) < size) {
					//print_debug("Pressed [%c]\n", pressed);
					if(pressed == '\a')
						pressed = ' ';
					// Shuffle everything forward (don't want overwrite functionality)
					for(i = size-1; i > caret; i--) {
						text[i] = text[i-1];
					}
					text[caret] = pressed;
					caret++;
				}
			}
			// Handle deletes via Y button or "backspace" button
			else if((btns & BUTTON_Y) || (pressed == '\b')) {
				// Delete a character from the caret
				if(caret-1 >= 0) {
					for(i = caret-1; i < size; i++) {
						text[i] = text[i+1];
						text[i+1] = '\0';
					}
					text[size] = '\0';
					if(caret > 0)
						caret --;
				}
			}
		}
		// Mode switching if the set allows it (only alpha does for upper/lower)
		if(btns & BUTTON_X) {
			if(num_txt_modes > 0) {
				cur_txt_mode = (cur_txt_mode + 1 >= num_txt_modes ? 0 : cur_txt_mode + 1);
			}
		}
		if(btns & BUTTON_L) {
			if(caret > 0) caret--;
		}
		if(btns & BUTTON_R) {
			if(caret < strlen(text)) caret++;
		}
		if(btns & BUTTON_B) {
			break;
		}
		if(btns & BUTTON_START) {
			if(mode & (ENTRYMODE_ALPHA|ENTRYMODE_IP)) {
				strcpy(src, text);
			}
			else {
				// TODO fix this stuff at some point, we're checking based on text size rather than max val of the data type.
				u16 *src_int = (u16*)src;
				*src_int = (u16)atoi(text);
			}
			break;
		}
		while (padsButtonsHeld() & (BUTTON_L|BUTTON_R|BUTTON_UP|BUTTON_DOWN|BUTTON_LEFT|BUTTON_RIGHT|BUTTON_B|BUTTON_A|BUTTON_X|BUTTON_Y|BUTTON_START))
			{ VIDEO_WaitVSync (); }
	}
	if(text) free(text);
	DrawDispose(container);
}


static void videoDrawEvent(uiDrawObj_t *videoEvent) {
	//print_debug("Draw event: %08X (type %s)\n", (u32)videoEvent, typeStrings[videoEvent->type]);
	drawInit();
	switch(videoEvent->type) {
		case EV_TEXOBJ:
			_DrawTexObj(videoEvent);
			break;
		case EV_IMAGE:
			_DrawImage(videoEvent);
			break;
		case EV_BACKGROUND:
			_DrawBackground(videoEvent);
			break;
		case EV_MSGBOX:
			_DrawMessageBox(videoEvent);
			break;
		case EV_PROGRESS:
			_DrawProgressBar(videoEvent);
			break;
		case EV_SELECTABLEBUTTON:
			_DrawSelectableButton(videoEvent);
			break;
		case EV_EMPTYBOX:
			_DrawEmptyBox(videoEvent);
			break;
		case EV_TRANSPARENTBOX:
			_DrawTransparentBox(videoEvent);
			break;
		case EV_FILEBROWSERBUTTON:
			_DrawFileBrowserButton(videoEvent);
			break;
		case EV_VERTSCROLLBAR:
			_DrawVertScrollBar(videoEvent);
			break;
		case EV_STYLEDLABEL:
			_DrawStyledLabel(videoEvent);
			break;
		case EV_HOME:
			_DrawHome(videoEvent);
			break;
		case EV_DEVICESELECTOR:
			_DrawDeviceSelectorCard(videoEvent);
			break;
		case EV_TOOLTIP:
			_DrawTooltip(videoEvent);
			break;
		case EV_TITLEBAR:
			_DrawTitleBar(videoEvent);
			break;
		case EV_GAMEFLOW:
			_DrawGameflow(videoEvent);
			break;
		case EV_PRESENTATION:
			_DrawPresentation(videoEvent);
			break;
		case EV_CHEATS:
			_DrawCheats(videoEvent);
			break;
		case EV_SETTINGSFOCUS:
			_DrawSettingsFocus(videoEvent);
			break;
		default:
			break;
	}
	if(videoEvent->child != NULL) {
		videoDrawEvent(videoEvent->child);
	}
}

static void markDisposed(uiDrawObj_t *evt)
{
	if(evt && evt->child && !evt->child->disposed) {
		markDisposed(evt->child);
	}
	if(evt) {
		evt->disposed = true;
	}
}

static void copyDisplayFrame(void *framebuffer)
{
	/* Copy-clear obeys the EFB write masks. The 2D widget pipeline disables
	 * depth writes; leaving that mask in place preserves the previous cube
	 * depth and punches old silhouettes into its next animated position. */
	GX_SetZMode(GX_ENABLE, GX_ALWAYS, GX_TRUE);
	GX_SetColorUpdate(GX_ENABLE);
	GX_CopyDisp(framebuffer, GX_TRUE);
}

static void *videoUpdate(void *videoEventQueue) {
	GX_SetCurrentGXThread();
	
	//int frames = 0;
	//int framerate = 0;
	//u32 lasttime = gettick();
	while(video_thread == LWP_GetSelf()) {
		whichfb ^= 1;
		UIAnim_BeginFrame();
		/* One Menu Color per frame: every emitter recolors with it. */
		int menuColor = menuColorPreview >= 0 ? menuColorPreview : menuColorPinned;
		UIColor_Select(menuColor >= 0 ? menuColor : swissSettings.uiColor);
		UI_PERF_BEGIN(frameWorkStart);
		//frames++;
		LWP_MutexLock(_videomutex);
		UIScene_Update(UIAnim_Delta(), _CurrentMotionMode());
		/* Sample once before EV_BACKGROUND so the cube and later title bar read
		 * the exact same numeric civil-time frame. */
		_UpdateSystemInstrument();
		/* The Library emblem mirrors this frame's controller. PAD reads only
		 * copy the last post-retrace scan; no SI transfer happens here. */
		padInstrument = (indigoPadFrame_t) {
			true, padsStickX(), padsStickY(), padsSubStickX(), padsSubStickY(),
			padsButtonsHeld()
		};
		videoFrameSerial++;
		// Mark events recursively as disposed
		uiDrawObjQueue_t *videoEventQueueEntry = (uiDrawObjQueue_t*)videoEventQueue;
		while(videoEventQueueEntry != NULL) {
			uiDrawObj_t *videoEvent = videoEventQueueEntry->event;
			if(videoEvent->disposed) {
				markDisposed(videoEvent);
			}
			videoEventQueueEntry = videoEventQueueEntry->next;
		}
		// Free events
		videoEventQueueEntry = (uiDrawObjQueue_t*)videoEventQueue;
		while(videoEventQueueEntry != NULL) {
			uiDrawObj_t *videoEvent = videoEventQueueEntry->event;
			// Remove any video events marked for disposal
			if(videoEvent->disposed) {
				disposeEvent(videoEvent);
				videoEventQueueEntry = (uiDrawObjQueue_t*)videoEventQueue;
				continue;
			}
			videoEventQueueEntry = videoEventQueueEntry->next;
		}
		
		GXRModeObj *vmode = getVideoMode();
		if(vmode->field_rendering) {
			GX_SetViewportJitter(0.0f, 0.0f, vmode->fbWidth, vmode->efbHeight, 0.0f, 1.0f, VIDEO_GetNextField());
		}
		// Draw out every event
		videoEventQueueEntry = (uiDrawObjQueue_t*)videoEventQueue;
		while(videoEventQueueEntry != NULL) {
			uiDrawObj_t *videoEvent = videoEventQueueEntry->event;
			videoDrawEvent(videoEvent);
			videoEventQueueEntry = videoEventQueueEntry->next;
		}
		/* During the short boot reveal, veil the already-published legacy widgets
		 * and redraw the cube above them. Animations Off skips this pass entirely. */
		if(sceneRenderingEnabled && UIScene_Frame()->introProgress < 1.0f) {
			int icons[UI_HOME_FACE_COUNT];

			_HomeFaceIcons(icons);
			drawInit();
			IndigoBackground_DrawBootOverlay(UIAnim_Seconds(),
				!swissSettings.disableUIAnimations, UIScene_Frame(),
				&systemInstrument.clock, icons);
			drawInit();
		}
#if UI_PERF_CAPTURE
		else if(sceneRenderingEnabled) {
			_DrawPerfOverlay();
		}
#endif
		
		//Copy EFB->XFB
		u16 width = vmode->fbWidth;
		u16 height = GX_SetDispCopyYScale(getYScaleFactor(vmode->efbHeight, vmode->xfbHeight));
		copyDisplayFrame(xfb[whichfb]);
		GX_DrawDone();
		UI_PERF_END(UI_PERF_METRIC_FRAME_WORK, frameWorkStart);

		LWP_MutexUnlock(_videomutex);
		VIDEO_SetNextFramebuffer(xfb[whichfb]);
		VIDEO_ConfigurePan(0, 0, width, height);
		VIDEO_Flush();
		VIDEO_WaitForFlush();
	}
	return NULL;
}

void DrawAddChild(uiDrawObj_t *parent, uiDrawObj_t *child)
{
	LWP_MutexLock(_videomutex);
	//print_debug("Added a new child event %08X (type %s)\n", (u32)child, typeStrings[child->type]);
	uiDrawObj_t *current = parent;
    while (current->child != NULL) {
        current = current->child;
    }
	current->child = child;
	child->disposed = false;
	//print_debug("Add child %08X (type %s) to parent %08X (type %s)\n",
	//	(u32)child, typeStrings[child->type], (u32)parent, typeStrings[parent->type]);
	LWP_MutexUnlock(_videomutex);
}

uiDrawObj_t* DrawPublish(uiDrawObj_t *evt)
{
	LWP_MutexLock(_videomutex);
	uiDrawObj_t* event = addVideoEvent(evt);
	LWP_MutexUnlock(_videomutex);
	return event;
}

uiDrawObj_t* DrawRepublish(uiDrawObj_t *old, uiDrawObj_t *new)
{
	LWP_MutexLock(_videomutex);
	if (old) {
		old->disposed = true;
	}
	uiDrawObj_t* event = addVideoEvent(new);
	LWP_MutexUnlock(_videomutex);
	return event;
}

void DrawDispose(uiDrawObj_t *evt)
{
	LWP_MutexLock(_videomutex);
	evt->disposed = true;
	if(evt == menuColorPage) {
		menuColorPage = NULL;
		menuColorPinned = -1;
		menuColorPreview = -1;
	}
	LWP_MutexUnlock(_videomutex);
}

/* Settings changes a value when the button goes down but redraws its page
 * when it comes up, and its picker steps the live value through every color
 * while it builds the list. Pinning the color to the page keeps the screen
 * and the row's label in step. */
void DrawPinMenuColor(uiDrawObj_t *page, int color)
{
	LWP_MutexLock(_videomutex);
	menuColorPage = page;
	menuColorPinned = color;
	menuColorPreview = -1;
	LWP_MutexUnlock(_videomutex);
}

/* The Menu Color list shows the color it has focused. The page's next draw
 * (DrawPinMenuColor) ends the preview, so a pick never flashes the old color
 * between the list closing and the page showing the new value. */
void DrawPreviewMenuColor(int color)
{
	LWP_MutexLock(_videomutex);
	menuColorPreview = color;
	LWP_MutexUnlock(_videomutex);
}

void DrawInit(GXRModeObj *videoMode, bool black) {
	setVideoMode(videoMode);
	padsInit();
	init_font();
	init_textures();
	/* Every publish/update path enters this mutex, so it must exist before
	 * the first retained event becomes visible. */
	LWP_MutexInit(&_videomutex, false);
	if(!gameflowResetRegistered) {
		SYS_RegisterResetFunc(&gameflowResetInfo);
		gameflowResetRegistered = true;
	}
	UIAnim_Reset();
	memset(&systemInstrument, 0, sizeof(systemInstrument));
	systemInstrument.coreTemperature = -1;
	memcpy(systemInstrument.timeText, "--:--:--", 9u);
	videoFrameSerial = 0u;
	settingsFocusLastDrawFrame = 0u;
	UISettingsFocus_Reset(&settingsFocusState);
	UIPerf_Reset();
	UIScene_Reset();
	sceneRenderingEnabled = !black;
	uiDrawObj_t *container = DrawContainer();
	if(!black) {
		DrawAddChild(container, DrawImage(TEX_BACKDROP, 0, 0, 640, 480, 0, 0.0f, 1.0f, 0.0f, 1.0f, 0));
		DrawAddChild(container, DrawBackground());
		DrawAddChild(container, DrawTitleBar());
		buttonPanel = DrawHome();
		DrawAddChild(container, buttonPanel);
	}
	DrawPublish(container);
	LWP_CreateThread(&video_thread, videoUpdate, videoEventQueue, video_thread_stack, VIDEO_STACK_SIZE, VIDEO_PRIORITY);
}

void DrawLoadBackdrop(DEVICEHANDLER_INTERFACE *device) {
	file_handle *backdropFile = calloc(1, sizeof(file_handle));
	concat_path(backdropFile->name, device->initial->name, "swiss/backdrop.tpl");
	backdropFile->device = device;
	
	s32 id = 0;
	u32 fmt;
	u16 width, height;
	if(TPL_OpenTPLFromHandle(&backdropTPL, openFileStream(backdropFile)) >= 0) {
		time_t curtime;
		if(time(&curtime) != (time_t)-1) {
			struct tm *tm = localtime(&curtime);
			switch(backdropTPL.ntextures) {
				case 2:
					id = (tm->tm_mon + 2) % 12 / 6;
					break;
				case 3:
					id = (tm->tm_mon + 2) % 12 / 4;
					break;
				case 4:
					id = (tm->tm_mon + 1) % 12 / 3;
					break;
				case 6:
					id = (tm->tm_mon + 1) % 12 / 2;
					break;
				case 7:
					id = tm->tm_wday;
					break;
				case 12:
					id = tm->tm_mon;
					break;
				case 24:
					id = tm->tm_hour;
					break;
				case 30 ... 31:
					id = tm->tm_mday - 1;
					break;
				case 365 ... 366:
					id = tm->tm_yday;
					break;
				default:
					srand(curtime);
					id = rand();
					break;
			}
			id %= backdropTPL.ntextures;
		}
		if(TPL_GetTextureInfo(&backdropTPL, id, &fmt, &width, &height) >= 0) {
			switch(fmt) {
				case GX_TF_CI4:
				case GX_TF_CI8:
				case GX_TF_CI14:
					TPL_GetTextureCI(&backdropTPL, id, &backdropTexObj, &backdropTlutObj, fmt == GX_TF_CI14 ? GX_BIGTLUT0 : GX_TLUT0);
					GX_InitTexObjUserData(&backdropTexObj, &backdropTlutObj);
					break;
				default:
					TPL_GetTexture(&backdropTPL, id, &backdropTexObj);
					break;
			}
			GX_InitTexObjUserData(&backdropIndTexObj, NULL);
		}
		else {
			TPL_CloseTPLFile(&backdropTPL);
			free(backdropFile);
		}
	}
	else {
		TPL_CloseTPLFile(&backdropTPL);
		free(backdropFile);
	}
}

void DrawShutdown() {
	mutex_t mutex;
	lwp_t thread = video_thread;

	/* Cancel while the source device and shared mutex are still live. */
	DrawGameflowCancelPosters();
	if(gameflowResetRegistered) {
		SYS_UnregisterResetFunc(&gameflowResetInfo);
		gameflowResetRegistered = false;
	}
	video_thread = LWP_THREAD_NULL;
	if(thread != LWP_THREAD_NULL) {
		LWP_JoinThread(thread, NULL);
	}
	/* Cancellation above unpublished the pack while the mutex was live.
	 * With video readers joined, final arena disposal is intentionally
	 * lock-free and remains safe before the mutex is destroyed. */
	UIAssets_DisposeAfterVideoStop();
	mutex = _videomutex;
	_videomutex = LWP_MUTEX_NULL;
	if(mutex != LWP_MUTEX_NULL) {
		LWP_MutexDestroy(mutex);
	}
	GX_SetCurrentGXThread();
	unsetVideoMode();
}

void DrawVideoMode(GXRModeObj *videoMode)
{
	LWP_MutexLock(_videomutex);
	if(getVideoMode() != videoMode) {
		setVideoMode(videoMode);
	}
	else {
		updateVideoMode(videoMode);
	}
	LWP_MutexUnlock(_videomutex);
}
