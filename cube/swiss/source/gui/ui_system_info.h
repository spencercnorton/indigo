#ifndef UI_SYSTEM_INFO_H
#define UI_SYSTEM_INFO_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Pure presentation policy for Swiss's native 640x480 System Information
 * surface. This module owns no hardware probing, drawing, allocation, input,
 * or global state. The menu thread captures runtime facts, then uses these
 * helpers to publish bounded text and geometry to the retained draw tree.
 */

#define UI_SYSTEM_PAGE_COUNT 6
#define UI_SYSTEM_TEXT_SCALE_FLOOR 0.60f
#define UI_SYSTEM_ELLIPSIS_BYTE 0x85u

#define UI_SYSTEM_SAFE_X0 32
#define UI_SYSTEM_SAFE_Y0 34
#define UI_SYSTEM_SAFE_X1 608
#define UI_SYSTEM_SAFE_Y1 438

typedef enum {
	UI_SYSTEM_PAGE_OVERVIEW = 0,
	UI_SYSTEM_PAGE_CONSOLE,
	UI_SYSTEM_PAGE_CONNECTIONS,
	UI_SYSTEM_PAGE_IO,
	UI_SYSTEM_PAGE_ABOUT,
	UI_SYSTEM_PAGE_CREDITS
} uiSystemPage_t;

typedef enum {
	UI_SYSTEM_REGION_NTSC_J = 0,
	UI_SYSTEM_REGION_NTSC_U,
	UI_SYSTEM_REGION_PAL,
	UI_SYSTEM_REGION_MPAL,
	UI_SYSTEM_REGION_UNKNOWN
} uiSystemRegion_t;

typedef struct {
	short x;
	short y;
	short w;
	short h;
} uiSystemRect_t;

typedef struct {
	const char *title;
	const char *subtitle;
} uiSystemPageDesc_t;

typedef struct {
	int page;
	uiSystemRect_t panel;
	uiSystemRect_t rail;
	uiSystemRect_t leftCard;
	uiSystemRect_t rightCard;
	uiSystemRect_t wideCard;
	int titleX;
	int titleY;
	int subtitleX;
	int subtitleY;
	int progressX;
	int progressY;
} uiSystemLayout_t;

typedef int (*uiSystemTextMeasureFn)(const char *text);

const uiSystemPageDesc_t *UISystem_PageDesc(int page);
void UISystem_ComputeLayout(int page, uiSystemLayout_t *out);

/* Includes the implicit DrawEmpty* border expansion in the safe-area check. */
bool UISystem_RectIsSafe(const uiSystemRect_t *rect, int borderExpansion);

/* Copies and fits display-only text. It never modifies source data and never
 * returns a scale below the native-grid readability floor. */
float UISystem_CopyFitted(char *destination, size_t capacity,
	const char *source, int maxWidth, float maxScale,
	uiSystemTextMeasureFn measure, bool *ellipsized);

bool UISystem_FormatClock(char *out, size_t capacity, int hour, int minute,
	bool available);
bool UISystem_FormatDate(char *out, size_t capacity, int weekday, int month,
	int day, int year, bool available);
bool UISystem_FormatTemperature(char *out, size_t capacity,
	int degreesCelsius, bool available);
bool UISystem_FormatCalibration(char *out, size_t capacity, int offsetCelsius);
void UISystem_FormatPageStatus(char *out, size_t capacity, int page);

const char *UISystem_SourceHealth(bool configured, bool available);
const char *UISystem_RegionName(uiSystemRegion_t region);

#endif
