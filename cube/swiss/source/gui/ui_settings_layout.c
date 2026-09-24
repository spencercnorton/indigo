/*
 * ui_settings_layout.c -- pure presentation geometry for the Phase 4D
 * settings shell. Contract and coordinate rules: ui_settings_layout.h.
 * No Swiss headers, no drawing, no I/O, no allocation: everything is
 * computed into the caller's uiSetLayout_t, so the module is host-
 * testable under strict C99 and free of MEM1 cost beyond the caller's
 * single stack/struct instance.
 */

#include <string.h>

#include "ui_settings_layout.h"

/* Shared column geometry. */
#define TAB_STRIP_X0 44
#define TAB_STRIP_X1 596
#define TAB_STRIP_Y 72
#define TAB_CELL_H 20
#define TAB_TEXT_PAD 10

#define TITLE_X 52
#define TITLE_Y 108
#define TITLE_REGION_Y 97
#define TITLE_REGION_H 22
#define TITLE_PROGRESS_GAP 16
#define PROGRESS_X 588
#define PROGRESS_REGION_X 486
/* The game page has no tabs to page through; its hint holds two buttons. */
#define GAME_PROGRESS_REGION_X 438

#define SUBTITLE_Y 129
#define SUBTITLE_REGION_Y 122
#define SUBTITLE_REGION_H 15
#define SUBTITLE_HELP_GAP 16
#define HELP_X 512
#define HELP_REGION_W (PROGRESS_X - HELP_X)

#define ROWS_Y0 146
#define ROW_H 28
#define ROW_CAPSULE_H 20
#define ROW_X0 56
#define ROW_X1 574
#define ROW_LABEL_X 68
#define ROW_VALUE_X0 350
#define ROW_VALUE_X 558
#define ROW_VALUE_GAP 20
#define ROW_TAG_W 52
#define ROW_TAG_GAP 8

#define SCROLL_X 584
#define SCROLL_W 6
#define SCROLL_MIN_THUMB 18

#define RAIL_Y 411
#define RAIL_H 22
#define RAIL_X0 48
#define RAIL_X1 592
#define ACTION_W_SAVE 128
#define ACTION_W_DISCARD 148
#define ACTION_GAP 12

#define PANEL_X0 44
#define PANEL_Y0 74
#define PANEL_X1 596
#define PANEL_Y1 394

static const uiSetLayoutPage_t PAGES[UI_SETLAYOUT_PAGE_COUNT] = {
	{ "Quick", "Quick Settings",
	  "What you change between games.",
	  UI_SETLAYOUT_ROWS_QUICK, 0, 0 },
	{ "Game Defaults", "Game Defaults",
	  "What every game starts with, unless it has its own.",
	  UI_SETLAYOUT_ROWS_GAME_DEFAULTS, 1, 0 },
	{ "Setup", "Setup",
	  "Set once. Press A to open a section.",
	  UI_SETLAYOUT_ROWS_SETUP, UI_SETLAYOUT_TAB_SETUP, 0 },
	{ "Setup", "Display",
	  "Video mode, cable, and TV.",
	  UI_SETLAYOUT_ROWS_DISPLAY, UI_SETLAYOUT_TAB_SETUP, 0 },
	{ "Setup", "Console",
	  "Menu color, sound, language, and system.",
	  UI_SETLAYOUT_ROWS_CONSOLE, UI_SETLAYOUT_TAB_SETUP, 0 },
	{ "Setup", "Storage",
	  "Settings device, SD cards, and the disc drive.",
	  UI_SETLAYOUT_ROWS_STORAGE, UI_SETLAYOUT_TAB_SETUP, 0 },
	{ "Setup", "Network",
	  "Adapter, file servers, and RetroTINK-4K.",
	  UI_SETLAYOUT_ROWS_NETWORK, UI_SETLAYOUT_TAB_SETUP, 0 },
	{ "Setup", "Library",
	  "Browser views, the recent list, and the look.",
	  UI_SETLAYOUT_ROWS_LIBRARY, UI_SETLAYOUT_TAB_SETUP, 0 },
	{ "Setup", "Developer",
	  "USB Gecko, memory, and debugging.",
	  UI_SETLAYOUT_ROWS_DEVELOPER, UI_SETLAYOUT_TAB_SETUP, 0 },
	{ "Game", "Game Settings",
	  "Saved for this game only.",
	  UI_SETLAYOUT_ROWS_GAME, UI_SETLAYOUT_NO_TAB, 1 },
};

const uiSetLayoutPage_t *UISetLayout_PageDesc(int page) {
	if (page < 0 || page >= UI_SETLAYOUT_PAGE_COUNT)
		return &PAGES[0];
	return &PAGES[page];
}

const char *UISetLayout_SettingsFileText(int state) {
	static const char *const text[UI_SETLAYOUT_SETTINGS_FILE_STATES] = {
		"Settings are saved in swiss/settings/global.ini.",
		"No swiss/settings/global.ini yet: using defaults.",
		"No device to save settings to.",
	};
	if (state < 0 || state >= UI_SETLAYOUT_SETTINGS_FILE_STATES)
		state = UI_SETLAYOUT_SETTINGS_FILE_NO_DEVICE;
	return text[state];
}

int UISetLayout_DiscardIndex(int page) {
	return UISetLayout_PageDesc(page)->rowCount + 1;
}

static int clampInt(int value, int lo, int hi) {
	if (value < lo)
		return lo;
	if (value > hi)
		return hi;
	return value;
}

size_t UISetLayout_EllipsizeText(const char *source, size_t sourceLength,
	size_t maxText, uiSetLayoutEllipsizeMode_t mode, char *out,
	size_t outCapacity) {
	size_t left;
	size_t right;

	if (out == NULL || outCapacity == 0u)
		return 0u;
	out[0] = '\0';
	if (source == NULL)
		return 0u;
	if (sourceLength > UI_SETLAYOUT_VALUE_SOURCE_LIMIT)
		sourceLength = UI_SETLAYOUT_VALUE_SOURCE_LIMIT;
	if (maxText > outCapacity - 1u)
		maxText = outCapacity - 1u;
	if (sourceLength <= maxText) {
		memcpy(out, source, sourceLength);
		out[sourceLength] = '\0';
		return sourceLength;
	}
	if (maxText == 0u)
		return 0u;
	if (maxText == 1u) {
		out[0] = (char)UI_SETLAYOUT_ELLIPSIS_BYTE;
		out[1] = '\0';
		return 1u;
	}
	if (mode == UI_SETLAYOUT_ELLIPSIZE_MIDDLE) {
		left = (maxText - 1u) / 2u;
		right = maxText - 1u - left;
		memcpy(out, source, left);
		out[left] = (char)UI_SETLAYOUT_ELLIPSIS_BYTE;
		memcpy(out + left + 1u, source + sourceLength - right, right);
	}
	else {
		left = maxText - 1u;
		memcpy(out, source, left);
		out[left] = (char)UI_SETLAYOUT_ELLIPSIS_BYTE;
	}
	out[maxText] = '\0';
	return maxText;
}

size_t UISetLayout_EllipsizeValue(const char *source, size_t sourceLength,
	uiSetLayoutEllipsizeMode_t mode, char *out, size_t outCapacity) {
	size_t maxText = outCapacity > 0u ? outCapacity - 1u : 0u;

	if (maxText > UI_SETLAYOUT_VALUE_TEXT_MAX)
		maxText = UI_SETLAYOUT_VALUE_TEXT_MAX;
	return UISetLayout_EllipsizeText(source, sourceLength, maxText, mode,
		out, outCapacity);
}

static int composeMeasuredText(const char *prefix, const char *body,
	const char *suffix, char *out, size_t outCapacity) {
	size_t prefixLength = strlen(prefix);
	size_t bodyLength = strlen(body);
	size_t suffixLength = strlen(suffix);
	size_t total = prefixLength + bodyLength + suffixLength;

	if (out == NULL || outCapacity == 0u || total >= outCapacity)
		return 0;
	memcpy(out, prefix, prefixLength);
	memcpy(out + prefixLength, body, bodyLength);
	memcpy(out + prefixLength + bodyLength, suffix, suffixLength);
	out[total] = '\0';
	return 1;
}

int UISetLayout_PrepareText(const char *source, size_t sourceLength,
	size_t maxSourceBytes, uiSetLayoutEllipsizeMode_t mode,
	uiSetLayoutTextKind_t kind, int selected, int enabled, int maxWidth,
	float preferredScale, float floorScale, uiSetLayoutTextMeasureFn measure,
	char *out, size_t outCapacity, uiSetLayoutTextFit_t *fit) {
	char body[UI_SETLAYOUT_TEXT_BUFFER_SIZE];
	const char *prefix = "";
	const char *suffix = "";
	size_t prefixLength;
	size_t suffixLength;
	size_t bodyCapacity;
	size_t bodyLimit;
	int width;
	float scale;
	int shortened = 0;

	if (out != NULL && outCapacity > 0u)
		out[0] = '\0';
	if (fit != NULL)
		memset(fit, 0, sizeof(*fit));
	if (source == NULL || out == NULL || outCapacity == 0u || fit == NULL ||
		measure == NULL || maxWidth <= 0 || preferredScale <= 0.0f ||
		floorScale <= 0.0f || preferredScale < floorScale ||
		kind < UI_SETLAYOUT_TEXT_PLAIN || kind > UI_SETLAYOUT_TEXT_EDITABLE)
		return 0;
	if (sourceLength > UI_SETLAYOUT_VALUE_SOURCE_LIMIT)
		sourceLength = UI_SETLAYOUT_VALUE_SOURCE_LIMIT;
	if (maxSourceBytes > UI_SETLAYOUT_VALUE_SOURCE_LIMIT)
		maxSourceBytes = UI_SETLAYOUT_VALUE_SOURCE_LIMIT;
	if (selected && enabled && kind == UI_SETLAYOUT_TEXT_CYCLE) {
		prefix = "< ";
		suffix = " >";
	}
	else if (selected && enabled && kind == UI_SETLAYOUT_TEXT_EDITABLE) {
		suffix = "\205";
	}
	prefixLength = strlen(prefix);
	suffixLength = strlen(suffix);
	if (prefixLength + suffixLength >= outCapacity)
		return 0;
	bodyCapacity = outCapacity - prefixLength - suffixLength;
	if (bodyCapacity > sizeof(body))
		bodyCapacity = sizeof(body);
	if (bodyCapacity == 0u)
		return 0;
	bodyLimit = sourceLength;
	if (bodyLimit > maxSourceBytes)
		bodyLimit = maxSourceBytes;
	if (bodyLimit >= bodyCapacity)
		bodyLimit = bodyCapacity - 1u;
	if (bodyLimit < sourceLength)
		shortened = 1;

	for (;;) {
		UISetLayout_EllipsizeText(source, sourceLength, bodyLimit, mode,
			body, bodyCapacity);
		if (!composeMeasuredText(prefix, body, suffix, out, outCapacity)) {
			out[0] = '\0';
			return 0;
		}
		width = measure(out);
		if (width < 0) {
			out[0] = '\0';
			return 0;
		}
		scale = width > 0 ? (float)maxWidth / (float)width : preferredScale;
		if (scale >= floorScale)
			break;
		if (bodyLimit == 0u) {
			out[0] = '\0';
			return 0;
		}
		bodyLimit--;
		shortened = 1;
	}
	if (scale > preferredScale)
		scale = preferredScale;
	fit->scale = scale;
	fit->unscaledWidth = width;
	fit->renderedWidth = (int)((float)width * scale + 0.999f);
	if (fit->renderedWidth > maxWidth)
		fit->renderedWidth = maxWidth;
	fit->ellipsized = shortened || bodyLimit < sourceLength;
	return 1;
}

/*
 * Tab cells: width follows label length, and the leftover strip width is
 * spread as even gaps. The tabs are the first UI_SETLAYOUT_TAB_COUNT
 * pages. Integer math only -- byte-identical on host and target.
 */
static void computeTabs(uiSetLayout_t *out) {
	int textW[UI_SETLAYOUT_TAB_COUNT];
	int total = 0;
	int i, x, gap, remainder;

	for (i = 0; i < UI_SETLAYOUT_TAB_COUNT; i++) {
		/* IPL font at the tab size averages ~6px per character. */
		textW[i] = (int)strlen(PAGES[i].tabLabel) * 6 + TAB_TEXT_PAD * 2;
		total += textW[i];
	}
	gap = (TAB_STRIP_X1 - TAB_STRIP_X0 - total) / (UI_SETLAYOUT_TAB_COUNT - 1);
	if (gap < 2)
		gap = 2;
	remainder = (TAB_STRIP_X1 - TAB_STRIP_X0) -
	            (total + gap * (UI_SETLAYOUT_TAB_COUNT - 1));
	x = TAB_STRIP_X0 + (remainder > 0 ? remainder / 2 : 0);
	for (i = 0; i < UI_SETLAYOUT_TAB_COUNT; i++) {
		out->tabCell[i].x = (short)x;
		out->tabCell[i].y = TAB_STRIP_Y;
		out->tabCell[i].w = (short)textW[i];
		out->tabCell[i].h = TAB_CELL_H;
		out->tabLabelCenterX[i] = x + textW[i] / 2;
		x += textW[i] + gap;
	}
	/* drawString anchors text on its vertical CENTER. */
	out->tabLabelY = TAB_STRIP_Y + TAB_CELL_H / 2;
	out->tabCount = UI_SETLAYOUT_TAB_COUNT;
}

static void computeRows(const uiSetLayoutPage_t *desc, uiSetLayout_t *out) {
	int effRow, maxFirst, i;

	out->rowCount = desc->rowCount;
	out->visibleRowCount = desc->rowCount < UI_SETLAYOUT_VISIBLE_ROWS ?
	                       desc->rowCount : UI_SETLAYOUT_VISIBLE_ROWS;
	out->selectedRow = out->option < desc->rowCount ? out->option : -1;

	/* Center the window on the selection; the action rail keeps the last
	 * window so the list doesn't jump when focus moves to the rail. */
	effRow = out->selectedRow >= 0 ? out->selectedRow : desc->rowCount - 1;
	maxFirst = desc->rowCount - out->visibleRowCount;
	if (maxFirst < 0)
		maxFirst = 0;
	out->firstVisibleRow =
		clampInt(effRow - UI_SETLAYOUT_VISIBLE_ROWS / 2, 0, maxFirst);

	for (i = 0; i < out->visibleRowCount; i++) {
		out->rowRect[i].x = ROW_X0;
		out->rowRect[i].y = (short)(ROWS_Y0 + i * ROW_H);
		out->rowRect[i].w = ROW_X1 - ROW_X0;
		out->rowRect[i].h = ROW_CAPSULE_H;
		out->rowTextY[i] = out->rowRect[i].y +
			out->rowRect[i].h / 2;
	}
	out->rowLabelX = ROW_LABEL_X;
	out->rowLabelMaxWidth = ROW_VALUE_X0 - ROW_LABEL_X - ROW_VALUE_GAP;
	out->rowValueX0 = ROW_VALUE_X0;
	out->rowValueX = ROW_VALUE_X;
	out->rowTagX = ROW_VALUE_X;
	out->rowTagWidth = 0;
	/* Tagged pages give the tag the right end of the value column. */
	if (desc->hasTags) {
		out->rowTagWidth = ROW_TAG_W;
		out->rowValueX = ROW_VALUE_X - ROW_TAG_W - ROW_TAG_GAP;
	}
	out->rowValueWidth = out->rowValueX - ROW_VALUE_X0;
}

static void computeScroll(uiSetLayout_t *out) {
	int trackH = out->visibleRowCount * ROW_H - (ROW_H - ROW_CAPSULE_H);
	int innerH = trackH - UI_SETLAYOUT_SCROLL_INSET * 2;
	int thumbH, thumbTravel, thumbY;

	out->scrollVisible = out->rowCount > UI_SETLAYOUT_VISIBLE_ROWS;
	out->scrollTrack.x = SCROLL_X;
	out->scrollTrack.y = ROWS_Y0;
	out->scrollTrack.w = SCROLL_W;
	out->scrollTrack.h = (short)trackH;
	if (!out->scrollVisible) {
		out->scrollThumb = out->scrollTrack;
		return;
	}
	thumbH = innerH * out->visibleRowCount / out->rowCount;
	if (thumbH < SCROLL_MIN_THUMB)
		thumbH = SCROLL_MIN_THUMB;
	if (thumbH > innerH)
		thumbH = innerH;
	thumbTravel = innerH - thumbH;
	thumbY = ROWS_Y0 + UI_SETLAYOUT_SCROLL_INSET;
	if (out->rowCount - out->visibleRowCount > 0) {
		thumbY += thumbTravel * out->firstVisibleRow /
		          (out->rowCount - out->visibleRowCount);
	}
	out->scrollThumb.x = SCROLL_X;
	out->scrollThumb.y = (short)thumbY;
	out->scrollThumb.w = SCROLL_W;
	out->scrollThumb.h = (short)thumbH;
	/* DrawVertScrollBar reserves three pixels at both track ends. Derive
	 * the published percent from that exact inset travel model. */
	if (thumbTravel > 0) {
		int offset = thumbY - ROWS_Y0 - UI_SETLAYOUT_SCROLL_INSET;
		if (offset <= 0)
			out->scrollPercent = 0.0f;
		else if (offset >= thumbTravel)
			out->scrollPercent = 1.0f;
		else
			/* The renderer reconstructs an integer pixel with truncation.
			 * A sub-pixel positive bias prevents binary float round-down. */
			out->scrollPercent = ((float)offset + 0.01f) /
				(float)thumbTravel;
	}
}

/*
 * Action rail: Save & Exit (option rowCount) and Discard & Exit (option
 * rowCount + 1), right-aligned so the confirming pair reads as one group.
 */
static void computeActions(const uiSetLayoutPage_t *desc, uiSetLayout_t *out) {
	static const short widths[UI_SETLAYOUT_MAX_ACTIONS] = {
		ACTION_W_SAVE, ACTION_W_DISCARD
	};
	int x = RAIL_X1;
	int i;

	out->actionKind[0] = UI_SETLAYOUT_ACTION_SAVE;
	out->actionKind[1] = UI_SETLAYOUT_ACTION_DISCARD;
	out->actionCount = UI_SETLAYOUT_MAX_ACTIONS;
	for (i = UI_SETLAYOUT_MAX_ACTIONS - 1; i >= 0; i--) {
		x -= widths[i];
		out->actionRect[i].x = (short)x;
		out->actionRect[i].y = RAIL_Y;
		out->actionRect[i].w = widths[i];
		out->actionRect[i].h = RAIL_H;
		x -= ACTION_GAP;
	}
	out->selectedAction = out->option >= desc->rowCount ?
		out->option - desc->rowCount : -1;
}

void UISetLayout_Compute(int page, int option, int hasTooltip,
                         int motionMode, uiSetLayout_t *out) {
	const uiSetLayoutPage_t *desc;
	int discard;
	int progressLeft;

	memset(out, 0, sizeof(*out));
	page = clampInt(page, 0, UI_SETLAYOUT_PAGE_COUNT - 1);
	desc = UISetLayout_PageDesc(page);
	discard = UISetLayout_DiscardIndex(page);
	out->page = page;
	out->option = clampInt(option, 0, discard);
	out->currentTab = desc->tab;

	if (desc->tab != UI_SETLAYOUT_NO_TAB)
		computeTabs(out);

	progressLeft = desc->tab == UI_SETLAYOUT_NO_TAB ? GAME_PROGRESS_REGION_X :
		PROGRESS_REGION_X;
	out->titleRegion.x = TITLE_X;
	out->titleRegion.y = TITLE_REGION_Y;
	out->titleRegion.w = progressLeft - TITLE_PROGRESS_GAP - TITLE_X;
	out->titleRegion.h = TITLE_REGION_H;
	out->titleX = out->titleRegion.x;
	out->titleY = TITLE_Y;
	out->titleMaxWidth = out->titleRegion.w;
	out->progressRegion.x = progressLeft;
	out->progressRegion.y = TITLE_REGION_Y;
	out->progressRegion.w = PROGRESS_X - progressLeft;
	out->progressRegion.h = TITLE_REGION_H;
	out->progressX = PROGRESS_X;
	out->progressY = TITLE_Y;
	out->progressMaxWidth = out->progressRegion.w;
	out->helpHintRegion.x = HELP_X;
	out->helpHintRegion.y = SUBTITLE_REGION_Y;
	out->helpHintRegion.w = HELP_REGION_W;
	out->helpHintRegion.h = SUBTITLE_REGION_H;
	out->helpHintVisible = hasTooltip ? 1 : 0;
	out->helpHintX = out->helpHintRegion.x;
	out->helpHintY = SUBTITLE_Y;
	out->helpHintMaxWidth = out->helpHintRegion.w;
	out->subtitleRegion.x = TITLE_X;
	out->subtitleRegion.y = SUBTITLE_REGION_Y;
	out->subtitleRegion.w = (out->helpHintVisible ? HELP_X - SUBTITLE_HELP_GAP :
		PROGRESS_X) - TITLE_X;
	out->subtitleRegion.h = SUBTITLE_REGION_H;
	out->subtitleX = out->subtitleRegion.x;
	out->subtitleY = SUBTITLE_Y;
	out->subtitleMaxWidth = out->subtitleRegion.w;
	out->pageProgress = desc->tab == UI_SETLAYOUT_NO_TAB ? 0.0f :
		(float)(desc->tab + 1) / (float)UI_SETLAYOUT_TAB_COUNT;

	computeRows(desc, out);
	computeScroll(out);
	computeActions(desc, out);

	out->panel.x = PANEL_X0;
	out->panel.y = PANEL_Y0;
	out->panel.w = PANEL_X1 - PANEL_X0;
	out->panel.h = PANEL_Y1 - PANEL_Y0;

	/* Motion: focus target is the selected capsule's center; Off snaps
	 * (focusAnimate 0) and must resolve on the next published frame.
	 * Reduced keeps eased focus travel but consumers drop secondary
	 * effects; Full may animate freely. */
	if (out->selectedRow >= 0) {
		int windowRow = out->selectedRow - out->firstVisibleRow;
		out->focusTargetY = (float)(ROWS_Y0 + windowRow * ROW_H +
		                            ROW_CAPSULE_H / 2);
	} else if (out->selectedAction >= 0) {
		out->focusTargetY = (float)(RAIL_Y + RAIL_H / 2);
	} else {
		out->focusTargetY = (float)ROWS_Y0;
	}
	out->focusAnimate = motionMode != UI_SETLAYOUT_MOTION_OFF;
}
