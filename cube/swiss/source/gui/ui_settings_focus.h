#ifndef UI_SETTINGS_FOCUS_H
#define UI_SETTINGS_FOCUS_H

/* Pure retained focus motion for the settings shell. The video event owns
 * one state, retargets it from immutable layout rectangles, and updates it
 * exactly once per rendered frame under FrameBufferMagic's video mutex. */

#include <stdbool.h>
#include <stdint.h>

#include "ui_motion.h"
#include "ui_settings_layout.h"

typedef struct {
	float x;
	float y;
	float w;
	float h;
} uiSettingsFocusFrame_t;

typedef struct {
	uiMotionSpring_t centerX;
	uiMotionSpring_t centerY;
	uiMotionSpring_t width;
	uiMotionSpring_t height;
	bool initialized;
} uiSettingsFocusState_t;

void UISettingsFocus_Reset(uiSettingsFocusState_t *state);
void UISettingsFocus_Init(uiSettingsFocusState_t *state,
	const uiSetLayoutRect_t *target);
void UISettingsFocus_Retarget(uiSettingsFocusState_t *state,
	const uiSetLayoutRect_t *target, uiMotionMode_t mode);
void UISettingsFocus_Update(uiSettingsFocusState_t *state,
	float deltaSeconds, uiMotionMode_t mode, uiSettingsFocusFrame_t *out);

/* Unsigned subtraction keeps the replacement window correct across the
 * video-frame serial's UINT32_MAX -> 0 wrap. */
bool UISettingsFocus_IsContinuous(uint32_t currentFrame,
	uint32_t lastDrawFrame, uint32_t maximumGap);

#endif
