#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_system_info.h"

static unsigned int checks;

#define CHECK(condition) do { \
	checks++; \
	if(!(condition)) { \
		fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
		exit(1); \
	} \
} while(0)

static int monoMeasure(const char *text)
{
	size_t length = text != NULL ? strlen(text) : 0u;
	return length > 100000u ? 1000000 : (int)length * 10;
}

static bool overlapsWithBorder(const uiSystemRect_t *a, int aBorder,
	const uiSystemRect_t *b, int bBorder)
{
	int ax0 = (int)a->x - aBorder;
	int ay0 = (int)a->y - aBorder;
	int ax1 = (int)a->x + (int)a->w + aBorder;
	int ay1 = (int)a->y + (int)a->h + aBorder;
	int bx0 = (int)b->x - bBorder;
	int by0 = (int)b->y - bBorder;
	int bx1 = (int)b->x + (int)b->w + bBorder;
	int by1 = (int)b->y + (int)b->h + bBorder;

	return ax0 < bx1 && ax1 > bx0 && ay0 < by1 && ay1 > by0;
}

static void testPagesAndLayout(void)
{
	int page;
	int other;
	uiSystemRect_t invalid = {0, 0, 0, 0};

	for(page = 0; page < UI_SYSTEM_PAGE_COUNT; page++) {
		const uiSystemPageDesc_t *desc = UISystem_PageDesc(page);
		uiSystemLayout_t layout;
		CHECK(desc != NULL);
		CHECK(desc->title != NULL && desc->title[0] != '\0');
		CHECK(desc->subtitle != NULL && desc->subtitle[0] != '\0');
		for(other = page + 1; other < UI_SYSTEM_PAGE_COUNT; other++) {
			CHECK(strcmp(desc->title, UISystem_PageDesc(other)->title) != 0);
		}
		UISystem_ComputeLayout(page, &layout);
		CHECK(layout.page == page);
		CHECK(UISystem_RectIsSafe(&layout.panel, 10));
		CHECK(UISystem_RectIsSafe(&layout.rail, 3));
		CHECK(UISystem_RectIsSafe(&layout.leftCard, 10));
		CHECK(UISystem_RectIsSafe(&layout.rightCard, 10));
		CHECK(UISystem_RectIsSafe(&layout.wideCard, 10));
		CHECK(!overlapsWithBorder(&layout.panel, 10, &layout.rail, 3));
		CHECK(!overlapsWithBorder(&layout.leftCard, 10,
			&layout.rightCard, 10));
		CHECK(layout.titleX >= UI_SYSTEM_SAFE_X0);
		CHECK(layout.progressX <= UI_SYSTEM_SAFE_X1);
		CHECK(layout.subtitleY > layout.titleY);
	}
	CHECK(UISystem_PageDesc(-1) == UISystem_PageDesc(0));
	CHECK(UISystem_PageDesc(999) ==
		UISystem_PageDesc(UI_SYSTEM_PAGE_COUNT - 1));
	CHECK(!UISystem_RectIsSafe(NULL, 0));
	CHECK(!UISystem_RectIsSafe(&invalid, 0));
	CHECK(!UISystem_RectIsSafe(&invalid, -1));
	UISystem_ComputeLayout(0, NULL);
}

static void testFormatting(void)
{
	char text[96];

	CHECK(UISystem_FormatClock(text, sizeof(text), 0, 0, true));
	CHECK(strcmp(text, "00:00") == 0);
	CHECK(UISystem_FormatClock(text, sizeof(text), 23, 59, true));
	CHECK(strcmp(text, "23:59") == 0);
	CHECK(!UISystem_FormatClock(text, sizeof(text), 24, 0, true));
	CHECK(strcmp(text, "TIME UNAVAILABLE") == 0);
	CHECK(!UISystem_FormatClock(text, sizeof(text), 10, 30, false));
	CHECK(!UISystem_FormatClock(NULL, 0u, 10, 30, true));

	CHECK(UISystem_FormatDate(text, sizeof(text), 1, 6, 13, 2026, true));
	CHECK(strcmp(text, "MONDAY  JULY 13, 2026") == 0);
	CHECK(!UISystem_FormatDate(text, sizeof(text), 7, 0, 1, 2026, true));
	CHECK(strcmp(text, "DATE UNAVAILABLE") == 0);
	CHECK(!UISystem_FormatDate(text, sizeof(text), 0, 0, 1, 1999, true));
	CHECK(!UISystem_FormatDate(text, sizeof(text), 0, 0, 1, 2026, false));

	CHECK(UISystem_FormatTemperature(text, sizeof(text), 42, true));
	CHECK(strcmp(text, "42\260C") == 0);
	CHECK(UISystem_FormatTemperature(text, sizeof(text), -20, true));
	CHECK(strcmp(text, "-20\260C") == 0);
	CHECK(!UISystem_FormatTemperature(text, sizeof(text), -1, false));
	CHECK(strcmp(text, "UNAVAILABLE") == 0);
	CHECK(!UISystem_FormatTemperature(text, sizeof(text), 126, true));

	CHECK(UISystem_FormatCalibration(text, sizeof(text), 0));
	CHECK(strcmp(text, "+0\260C OFFSET") == 0);
	CHECK(UISystem_FormatCalibration(text, sizeof(text), -80));
	CHECK(strcmp(text, "-80\260C OFFSET") == 0);
	CHECK(!UISystem_FormatCalibration(text, sizeof(text), 81));
	CHECK(strcmp(text, "INVALID OFFSET") == 0);

	UISystem_FormatPageStatus(text, sizeof(text), 0);
	CHECK(strcmp(text, "L/R  PAGE 1 OF 6    B  BACK") == 0);
	UISystem_FormatPageStatus(text, sizeof(text), 999);
	CHECK(strcmp(text, "L/R  PAGE 6 OF 6    B  BACK") == 0);
	UISystem_FormatPageStatus(NULL, 0u, 0);

	CHECK(strcmp(UISystem_SourceHealth(false, false), "NOT MOUNTED") == 0);
	CHECK(strcmp(UISystem_SourceHealth(false, true), "NOT MOUNTED") == 0);
	CHECK(strcmp(UISystem_SourceHealth(true, false), "UNAVAILABLE") == 0);
	CHECK(strcmp(UISystem_SourceHealth(true, true), "READY") == 0);
	CHECK(strcmp(UISystem_RegionName(UI_SYSTEM_REGION_NTSC_J),
		"NTSC-J / JAPAN") == 0);
	CHECK(strcmp(UISystem_RegionName(UI_SYSTEM_REGION_NTSC_U),
		"NTSC-U / AMERICAS") == 0);
	CHECK(strcmp(UISystem_RegionName(UI_SYSTEM_REGION_PAL),
		"PAL / EUROPE") == 0);
	CHECK(strcmp(UISystem_RegionName(UI_SYSTEM_REGION_MPAL),
		"MPAL / BRAZIL") == 0);
	CHECK(strcmp(UISystem_RegionName((uiSystemRegion_t)999),
		"REGION UNKNOWN") == 0);
}

static void testTextFit(void)
{
	char text[64];
	char longText[512];
	bool ellipsized;
	float scale;
	size_t index;

	for(index = 0u; index + 1u < sizeof(longText); index++) {
		longText[index] = (char)('A' + (index % 26u));
	}
	longText[sizeof(longText) - 1u] = '\0';

	scale = UISystem_CopyFitted(text, sizeof(text), "1234567890", 80, 0.90f,
		monoMeasure, &ellipsized);
	CHECK(scale > 0.79f && scale < 0.81f);
	CHECK(!ellipsized);
	CHECK(strcmp(text, "1234567890") == 0);

	scale = UISystem_CopyFitted(text, sizeof(text), longText, 120, 0.90f,
		monoMeasure, &ellipsized);
	CHECK(scale == UI_SYSTEM_TEXT_SCALE_FLOOR);
	CHECK(ellipsized);
	CHECK(text[0] != '\0');
	CHECK((unsigned char)text[strlen(text) - 1u] == UI_SYSTEM_ELLIPSIS_BYTE);
	CHECK((float)monoMeasure(text) * scale <= 120.0f);

	scale = UISystem_CopyFitted(text, sizeof(text), "SHORT", 500, 0.40f,
		monoMeasure, &ellipsized);
	CHECK(scale == UI_SYSTEM_TEXT_SCALE_FLOOR);
	CHECK(!ellipsized);

	CHECK(UISystem_CopyFitted(text, sizeof(text), NULL, 100, 1.0f,
		monoMeasure, NULL) == UI_SYSTEM_TEXT_SCALE_FLOOR);
	CHECK(text[0] == '\0');
	CHECK(UISystem_CopyFitted(NULL, 0u, "TEXT", 100, 1.0f,
		monoMeasure, NULL) == UI_SYSTEM_TEXT_SCALE_FLOOR);
	CHECK(UISystem_CopyFitted(text, sizeof(text), "TEXT", 0, 1.0f,
		monoMeasure, NULL) == UI_SYSTEM_TEXT_SCALE_FLOOR);
	CHECK(text[0] == '\0');
	CHECK(UISystem_CopyFitted(text, 1u, longText, 1, 1.0f,
		monoMeasure, &ellipsized) == UI_SYSTEM_TEXT_SCALE_FLOOR);
	CHECK(text[0] == '\0');
}

int main(void)
{
	testPagesAndLayout();
	testFormatting();
	testTextFit();
	printf("ui_system_info: %u checks passed\n", checks);
	return 0;
}
