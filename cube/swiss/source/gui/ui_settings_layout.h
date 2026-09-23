#ifndef UI_SETTINGS_LAYOUT_H
#define UI_SETTINGS_LAYOUT_H

#include <stddef.h>

/*
 * ui_settings_layout -- pure presentation geometry for the Phase 4D
 * settings shell. Computes tabs, the visible row window, selection and
 * action-rail mapping, scroll treatment, and motion targets from
 * (page, option) alone. It owns NO Swiss settings, mutates NO option
 * values, and performs NO drawing or I/O, so it compiles and tests on
 * the host under strict C99 (buildtools/ui/tests/test_ui_settings_layout).
 *
 * The option-index model mirrors settings.h/show_settings() exactly and
 * is bound to the real enums by _Static_asserts in settings.c:
 *   options 0..rowCount-1          settable rows (page order = enum order)
 *   then [Back]  (absent on the first page)
 *   then [Next]  (absent on the last page)
 *   then Save & Exit
 *   then Discard & Exit            (index == settings_count_pp[page])
 *
 * All content rectangles stay inside the 640x480 title-safe area
 * x=32..608, y=34..438; boxes are placed so FrameBufferMagic's implicit
 * border expansion (3px for boxes <=30px tall, 10px otherwise) also
 * lands inside it.
 */

#define UI_SETLAYOUT_PAGE_COUNT 6
#define UI_SETLAYOUT_VISIBLE_ROWS 9
#define UI_SETLAYOUT_MAX_ACTIONS 4

/* Per-page settable-row counts (bound to settings.h in settings.c). */
#define UI_SETLAYOUT_ROWS_SYSTEM 20
#define UI_SETLAYOUT_ROWS_INTERFACE 14
#define UI_SETLAYOUT_ROWS_NETWORK 20
#define UI_SETLAYOUT_ROWS_GAME_GLOBAL 10
#define UI_SETLAYOUT_ROWS_GAME_DEFAULTS 22
#define UI_SETLAYOUT_ROWS_CURRENT_GAME 22

/* Safe-area bounds every computed rect must respect. */
#define UI_SETLAYOUT_SAFE_X0 32
#define UI_SETLAYOUT_SAFE_Y0 34
#define UI_SETLAYOUT_SAFE_X1 608
#define UI_SETLAYOUT_SAFE_Y1 438

/* Persistent titlebar exclusions. Settings content must remain clear of
 * both the Swiss mark and the live clock/temperature dial. */
#define UI_SETLAYOUT_TITLEBAR_LOGO_X0 30
#define UI_SETLAYOUT_TITLEBAR_LOGO_Y0 28
#define UI_SETLAYOUT_TITLEBAR_LOGO_X1 126
#define UI_SETLAYOUT_TITLEBAR_LOGO_Y1 60
#define UI_SETLAYOUT_TITLEBAR_DIAL_X0 530
#define UI_SETLAYOUT_TITLEBAR_DIAL_Y0 23
#define UI_SETLAYOUT_TITLEBAR_DIAL_X1 620
#define UI_SETLAYOUT_TITLEBAR_DIAL_Y1 63

/* Display values are bounded before font measurement or Draw* allocation.
 * The setting itself is never modified; only its one-frame presentation is
 * ellipsized. */
#define UI_SETLAYOUT_VALUE_SOURCE_LIMIT 1023u
#define UI_SETLAYOUT_VALUE_TEXT_MAX 24u
#define UI_SETLAYOUT_VALUE_BUFFER_SIZE 32u
#define UI_SETLAYOUT_LABEL_BUFFER_SIZE 96u
#define UI_SETLAYOUT_TEXT_BUFFER_SIZE 104u
#define UI_SETLAYOUT_ELLIPSIS_BYTE 0x85u
#define UI_SETLAYOUT_SCROLL_INSET 3

/* Settings text never shrinks below this native-grid scale. Wider strings
 * are ellipsized to the measured column before drawing. */
#define UI_SETLAYOUT_ROW_TEXT_FLOOR 0.60f

typedef struct {
	short x, y, w, h;
} uiSetLayoutRect_t;

typedef enum {
	UI_SETLAYOUT_ACTION_BACK = 0,
	UI_SETLAYOUT_ACTION_NEXT,
	UI_SETLAYOUT_ACTION_SAVE,
	UI_SETLAYOUT_ACTION_DISCARD
} uiSetLayoutActionKind_t;

/* Mirrors uiMotionMode_t values without dragging target headers in. */
typedef enum {
	UI_SETLAYOUT_MOTION_FULL = 0,
	UI_SETLAYOUT_MOTION_REDUCED,
	UI_SETLAYOUT_MOTION_OFF
} uiSetLayoutMotionMode_t;

typedef enum {
	UI_SETLAYOUT_ELLIPSIZE_TAIL = 0,
	UI_SETLAYOUT_ELLIPSIZE_MIDDLE
} uiSetLayoutEllipsizeMode_t;

typedef enum {
	UI_SETLAYOUT_TEXT_PLAIN = 0,
	UI_SETLAYOUT_TEXT_CYCLE,
	UI_SETLAYOUT_TEXT_EDITABLE
} uiSetLayoutTextKind_t;

typedef int (*uiSetLayoutTextMeasureFn)(const char *text);

typedef struct {
	float scale;
	int unscaledWidth;
	int renderedWidth;
	int ellipsized;
} uiSetLayoutTextFit_t;

typedef struct {
	const char *tabLabel;
	const char *title;
	const char *subtitle;
	int rowCount;
	int hasBack;
	int hasNext;
} uiSetLayoutPage_t;

typedef struct {
	int page;
	int option;

	/* Tab strip: cells sized by label length so every label fits. */
	int tabCount;
	int currentTab;
	uiSetLayoutRect_t tabCell[UI_SETLAYOUT_PAGE_COUNT];
	int tabLabelCenterX[UI_SETLAYOUT_PAGE_COUNT];
	int tabLabelY;

	/* Header. */
	int titleX, titleY;
	int titleMaxWidth;
	int subtitleX, subtitleY;
	int subtitleMaxWidth;
	int progressX, progressY;   /* right-aligned "n / 6" anchor */
	int progressMaxWidth;
	float pageProgress;         /* (page + 1) / 6 */
	uiSetLayoutRect_t titleRegion;
	uiSetLayoutRect_t subtitleRegion;
	uiSetLayoutRect_t progressRegion;

	/* Rows: a window of at most UI_SETLAYOUT_VISIBLE_ROWS around the
	 * selection; row i of the window shows settable row
	 * firstVisibleRow + i. */
	int rowCount;
	int firstVisibleRow;
	int visibleRowCount;
	int selectedRow;            /* absolute row index, -1 on the action rail */
	uiSetLayoutRect_t rowRect[UI_SETLAYOUT_VISIBLE_ROWS];
	int rowTextY[UI_SETLAYOUT_VISIBLE_ROWS];
	int rowLabelX;              /* left-aligned label anchor */
	int rowLabelMaxWidth;       /* fit boundary before the value gutter */
	int rowValueX0;             /* left edge of the reserved value column */
	int rowValueX;              /* right-aligned value anchor */
	int rowValueWidth;

	/* Slim scroll treatment (hidden when everything fits). */
	int scrollVisible;
	uiSetLayoutRect_t scrollTrack;
	uiSetLayoutRect_t scrollThumb;
	float scrollPercent;        /* thumb travel fraction, 0..1 */

	/* Persistent bottom action rail. */
	int actionCount;
	int actionKind[UI_SETLAYOUT_MAX_ACTIONS];
	uiSetLayoutRect_t actionRect[UI_SETLAYOUT_MAX_ACTIONS];
	int selectedAction;         /* -1 or index into actionKind/actionRect */

	/* Contextual help hint (only when the selected option has a tooltip). */
	int helpHintVisible;
	int helpHintX, helpHintY;
	int helpHintMaxWidth;
	uiSetLayoutRect_t helpHintRegion;

	/* Backing panel for the whole surface (border-expansion aware). */
	uiSetLayoutRect_t panel;

	/* Motion: the focus highlight's target center Y (rows or rail) and
	 * whether the consumer may animate toward it. Off must resolve on
	 * the next published frame. */
	float focusTargetY;
	int focusAnimate;           /* 0 under UI_SETLAYOUT_MOTION_OFF */
} uiSetLayout_t;

/* Static page descriptor (labels, row counts, nav availability). */
const uiSetLayoutPage_t *UISetLayout_PageDesc(int page);

/* Index of the Discard & Exit option == settings_count_pp[page]. */
int UISetLayout_DiscardIndex(int page);

/* Full layout for one published frame. motionMode uses the mirror enum;
 * hasTooltip reports whether the currently selected option has help. */
void UISetLayout_Compute(int page, int option, int hasTooltip,
                         int motionMode, uiSetLayout_t *out);

/* Copies at most UI_SETLAYOUT_VALUE_TEXT_MAX display bytes, replacing a
 * clipped tail or middle with the IPL ellipsis glyph. sourceLength is an
 * explicit available-byte count and is clamped to VALUE_SOURCE_LIMIT. */
size_t UISetLayout_EllipsizeValue(const char *source, size_t sourceLength,
	uiSetLayoutEllipsizeMode_t mode, char *out, size_t outCapacity);

/* General bounded ellipsizer used by the renderer's measured text-floor
 * loop. maxText is the requested display-byte budget and is clamped to the
 * destination capacity and source safety limit. */
size_t UISetLayout_EllipsizeText(const char *source, size_t sourceLength,
	size_t maxText, uiSetLayoutEllipsizeMode_t mode, char *out,
	size_t outCapacity);

/* Prepares one measured presentation copy. Decorations are retained while
 * only the source body is shortened, so selected cycle/text affordances never
 * disappear. A successful result is guaranteed to stay inside maxWidth at or
 * above floorScale. Invalid measurement or an impossible readable fit fails
 * closed with an empty output. */
int UISetLayout_PrepareText(const char *source, size_t sourceLength,
	size_t maxSourceBytes, uiSetLayoutEllipsizeMode_t mode,
	uiSetLayoutTextKind_t kind, int selected, int enabled, int maxWidth,
	float preferredScale, float floorScale, uiSetLayoutTextMeasureFn measure,
	char *out, size_t outCapacity, uiSetLayoutTextFit_t *fit);

#endif
