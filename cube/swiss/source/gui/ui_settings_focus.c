#include <math.h>
#include <string.h>

#include "ui_settings_focus.h"

#define SETTINGS_FOCUS_POSITION_RESPONSE 18.0f
#define SETTINGS_FOCUS_SIZE_RESPONSE 22.0f
#define SETTINGS_FOCUS_MIN_WIDTH 16.0f
#define SETTINGS_FOCUS_MIN_HEIGHT 16.0f
#define SETTINGS_FOCUS_MAX_HEIGHT 64.0f

static float clampFloat(float value, float lo, float hi)
{
	if(value < lo) {
		return lo;
	}
	if(value > hi) {
		return hi;
	}
	return value;
}

static uiMotionMode_t sanitizeMode(uiMotionMode_t mode)
{
	if(mode < UI_MOTION_FULL || mode > UI_MOTION_OFF) {
		return UI_MOTION_FULL;
	}
	return mode;
}

static void targetComponents(const uiSetLayoutRect_t *target,
	float *centerX, float *centerY, float *width, float *height)
{
	float x;
	float y;
	float maxWidth = (float)(UI_SETLAYOUT_SAFE_X1 - UI_SETLAYOUT_SAFE_X0);
	float maxHeight = (float)(UI_SETLAYOUT_SAFE_Y1 - UI_SETLAYOUT_SAFE_Y0);

	if(target == NULL) {
		x = (float)UI_SETLAYOUT_SAFE_X0;
		y = (float)UI_SETLAYOUT_SAFE_Y0;
		*width = SETTINGS_FOCUS_MIN_WIDTH;
		*height = SETTINGS_FOCUS_MIN_HEIGHT;
	}
	else {
		x = (float)target->x;
		y = (float)target->y;
		*width = clampFloat((float)target->w,
			SETTINGS_FOCUS_MIN_WIDTH, maxWidth);
		*height = clampFloat((float)target->h,
			SETTINGS_FOCUS_MIN_HEIGHT, SETTINGS_FOCUS_MAX_HEIGHT);
	}
	if(*height > maxHeight) {
		*height = maxHeight;
	}
	x = clampFloat(x, (float)UI_SETLAYOUT_SAFE_X0,
		(float)UI_SETLAYOUT_SAFE_X1 - *width);
	y = clampFloat(y, (float)UI_SETLAYOUT_SAFE_Y0,
		(float)UI_SETLAYOUT_SAFE_Y1 - *height);
	*centerX = x + *width * 0.5f;
	*centerY = y + *height * 0.5f;
}

static void boundSpring(uiMotionSpring_t *spring, float lo, float hi)
{
	float bounded = clampFloat(spring->value, lo, hi);
	if(bounded != spring->value) {
		spring->value = bounded;
		spring->velocity = 0.0f;
	}
}

void UISettingsFocus_Reset(uiSettingsFocusState_t *state)
{
	if(state != NULL) {
		memset(state, 0, sizeof(*state));
	}
}

void UISettingsFocus_Init(uiSettingsFocusState_t *state,
	const uiSetLayoutRect_t *target)
{
	float centerX;
	float centerY;
	float width;
	float height;

	if(state == NULL) {
		return;
	}
	targetComponents(target, &centerX, &centerY, &width, &height);
	UIMotion_SpringInit(&state->centerX, centerX,
		SETTINGS_FOCUS_POSITION_RESPONSE);
	UIMotion_SpringInit(&state->centerY, centerY,
		SETTINGS_FOCUS_POSITION_RESPONSE);
	UIMotion_SpringInit(&state->width, width,
		SETTINGS_FOCUS_SIZE_RESPONSE);
	UIMotion_SpringInit(&state->height, height,
		SETTINGS_FOCUS_SIZE_RESPONSE);
	state->initialized = true;
}

void UISettingsFocus_Retarget(uiSettingsFocusState_t *state,
	const uiSetLayoutRect_t *target, uiMotionMode_t mode)
{
	float centerX;
	float centerY;
	float width;
	float height;

	if(state == NULL) {
		return;
	}
	if(!state->initialized) {
		UISettingsFocus_Init(state, target);
		return;
	}
	mode = sanitizeMode(mode);
	targetComponents(target, &centerX, &centerY, &width, &height);
	UIMotion_SpringRetarget(&state->centerX, centerX, mode);
	UIMotion_SpringRetarget(&state->centerY, centerY, mode);
	UIMotion_SpringRetarget(&state->width, width, mode);
	UIMotion_SpringRetarget(&state->height, height, mode);
}

void UISettingsFocus_Update(uiSettingsFocusState_t *state,
	float deltaSeconds, uiMotionMode_t mode, uiSettingsFocusFrame_t *out)
{
	float halfWidth;
	float halfHeight;
	float maxWidth = (float)(UI_SETLAYOUT_SAFE_X1 - UI_SETLAYOUT_SAFE_X0);

	if(state == NULL || out == NULL) {
		return;
	}
	if(!state->initialized) {
		UISettingsFocus_Init(state, NULL);
	}
	if(!isfinite(deltaSeconds)) {
		deltaSeconds = 0.0f;
	}
	mode = sanitizeMode(mode);
	UIMotion_SpringUpdate(&state->width, deltaSeconds, mode);
	UIMotion_SpringUpdate(&state->height, deltaSeconds, mode);
	boundSpring(&state->width, SETTINGS_FOCUS_MIN_WIDTH, maxWidth);
	boundSpring(&state->height, SETTINGS_FOCUS_MIN_HEIGHT,
		SETTINGS_FOCUS_MAX_HEIGHT);
	halfWidth = state->width.value * 0.5f;
	halfHeight = state->height.value * 0.5f;
	UIMotion_SpringUpdate(&state->centerX, deltaSeconds, mode);
	UIMotion_SpringUpdate(&state->centerY, deltaSeconds, mode);
	boundSpring(&state->centerX,
		(float)UI_SETLAYOUT_SAFE_X0 + halfWidth,
		(float)UI_SETLAYOUT_SAFE_X1 - halfWidth);
	boundSpring(&state->centerY,
		(float)UI_SETLAYOUT_SAFE_Y0 + halfHeight,
		(float)UI_SETLAYOUT_SAFE_Y1 - halfHeight);
	out->x = state->centerX.value - halfWidth;
	out->y = state->centerY.value - halfHeight;
	out->w = state->width.value;
	out->h = state->height.value;
}

bool UISettingsFocus_IsContinuous(uint32_t currentFrame,
	uint32_t lastDrawFrame, uint32_t maximumGap)
{
	return (uint32_t)(currentFrame - lastDrawFrame) <= maximumGap;
}
