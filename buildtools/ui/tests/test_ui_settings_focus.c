#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "ui_settings_focus.h"

static int checks;

#define CHECK(condition) do { \
	checks++; \
	if(!(condition)) { \
		fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, \
			#condition); \
		exit(EXIT_FAILURE); \
	} \
} while(0)

static uiSetLayoutRect_t rect(short x, short y, short w, short h)
{
	uiSetLayoutRect_t result = {x, y, w, h};
	return result;
}

static void checkBounded(const uiSettingsFocusFrame_t *frame)
{
	CHECK(isfinite(frame->x));
	CHECK(isfinite(frame->y));
	CHECK(isfinite(frame->w));
	CHECK(isfinite(frame->h));
	CHECK(frame->x >= (float)UI_SETLAYOUT_SAFE_X0);
	CHECK(frame->y >= (float)UI_SETLAYOUT_SAFE_Y0);
	CHECK(frame->x + frame->w <= (float)UI_SETLAYOUT_SAFE_X1 + 0.001f);
	CHECK(frame->y + frame->h <= (float)UI_SETLAYOUT_SAFE_Y1 + 0.001f);
}

static void testFullRapidReversal(void)
{
	uiSettingsFocusState_t state;
	uiSettingsFocusFrame_t frame;
	uiSetLayoutRect_t top = rect(49, 129, 526, 30);
	uiSetLayoutRect_t bottom = rect(49, 353, 526, 30);
	float beforeReverse;
	int i;

	UISettingsFocus_Reset(&state);
	UISettingsFocus_Init(&state, &top);
	UISettingsFocus_Update(&state, 0.0f, UI_MOTION_FULL, &frame);
	CHECK(frame.y == 129.0f);
	UISettingsFocus_Retarget(&state, &bottom, UI_MOTION_FULL);
	for(i = 0; i < 5; ++i) {
		UISettingsFocus_Update(&state, 1.0f / 60.0f,
			UI_MOTION_FULL, &frame);
		checkBounded(&frame);
	}
	CHECK(frame.y > 129.0f && frame.y < 353.0f);
	beforeReverse = frame.y;
	UISettingsFocus_Retarget(&state, &top, UI_MOTION_FULL);
	UISettingsFocus_Update(&state, 0.0f, UI_MOTION_FULL, &frame);
	CHECK(frame.y == beforeReverse); /* a zero-time retarget never teleports */
	for(i = 0; i < 720; ++i) {
		const uiSetLayoutRect_t *target = (i & 1) ? &top : &bottom;
		UISettingsFocus_Retarget(&state, target, UI_MOTION_FULL);
		UISettingsFocus_Update(&state, 1.0f / 120.0f,
			UI_MOTION_FULL, &frame);
		checkBounded(&frame);
	}
}

static void testReducedAndOffContracts(void)
{
	uiSettingsFocusState_t state;
	uiSettingsFocusFrame_t frame;
	uiSetLayoutRect_t row = rect(49, 129, 526, 30);
	uiSetLayoutRect_t action = rect(441, 403, 154, 32);
	int i;

	UISettingsFocus_Init(&state, &row);
	UISettingsFocus_Retarget(&state, &action, UI_MOTION_FULL);
	UISettingsFocus_Update(&state, 1.0f / 60.0f, UI_MOTION_FULL, &frame);
	CHECK(state.centerY.velocity != 0.0f);
	UISettingsFocus_Retarget(&state, &row, UI_MOTION_REDUCED);
	CHECK(state.centerY.velocity == 0.0f);
	for(i = 0; i < 120; ++i)
		UISettingsFocus_Update(&state, 1.0f / 60.0f,
			UI_MOTION_REDUCED, &frame);
	CHECK(fabsf(frame.y - 129.0f) < 0.01f);

	UISettingsFocus_Retarget(&state, &action, UI_MOTION_OFF);
	UISettingsFocus_Update(&state, 1.0f / 60.0f, UI_MOTION_OFF, &frame);
	CHECK(frame.x == 441.0f);
	CHECK(frame.y == 403.0f);
	CHECK(frame.w == 154.0f);
	CHECK(frame.h == 32.0f);
	CHECK(state.centerX.velocity == 0.0f);
	CHECK(state.centerY.velocity == 0.0f);
	checkBounded(&frame);
}

static void testPersistedMotionPreferenceCompatibility(void)
{
	uiMotionMode_t mode;

	/* Configurations written before Reduced existed retain their exact
	 * behavior: enabled means Full and disabled means Off. */
	CHECK(UIMotion_ModeFromFlags(0, 0) == UI_MOTION_FULL);
	CHECK(UIMotion_ModeFromFlags(1, 0) == UI_MOTION_OFF);
	CHECK(UIMotion_ModeFromFlags(0, 1) == UI_MOTION_REDUCED);
	CHECK(UIMotion_ModeFromFlags(1, 1) == UI_MOTION_OFF);

	mode = UI_MOTION_FULL;
	mode = UIMotion_CycleMode(mode, 1);
	CHECK(mode == UI_MOTION_REDUCED);
	mode = UIMotion_CycleMode(mode, 1);
	CHECK(mode == UI_MOTION_OFF);
	mode = UIMotion_CycleMode(mode, 1);
	CHECK(mode == UI_MOTION_FULL);
	mode = UIMotion_CycleMode(mode, -1);
	CHECK(mode == UI_MOTION_OFF);
	mode = UIMotion_CycleMode(mode, -1);
	CHECK(mode == UI_MOTION_REDUCED);
	CHECK(UIMotion_CycleMode(mode, 0) == UI_MOTION_REDUCED);
	CHECK(UIMotion_CycleMode((uiMotionMode_t)99, 0) == UI_MOTION_FULL);
}

static void testBoundsAndDeltaSanitization(void)
{
	uiSettingsFocusState_t state;
	uiSettingsFocusFrame_t frame;
	uiSetLayoutRect_t invalid = rect(-32000, -32000, 32767, 32767);

	UISettingsFocus_Reset(&state);
	UISettingsFocus_Init(&state, &invalid);
	UISettingsFocus_Update(&state, -10.0f, (uiMotionMode_t)99, &frame);
	checkBounded(&frame);
	UISettingsFocus_Update(&state, 1000.0f, UI_MOTION_FULL, &frame);
	checkBounded(&frame);
	UISettingsFocus_Update(&state, NAN, UI_MOTION_FULL, &frame);
	checkBounded(&frame);
	UISettingsFocus_Update(&state, INFINITY, UI_MOTION_FULL, &frame);
	checkBounded(&frame);
}

int main(void)
{
	testFullRapidReversal();
	testReducedAndOffContracts();
	testPersistedMotionPreferenceCompatibility();
	testBoundsAndDeltaSanitization();
	printf("settings focus: %d checks passed\n", checks);
	return 0;
}
