#ifndef UI_SETTINGS_FOCUS_H
#define UI_SETTINGS_FOCUS_H

/* Pure retained focus motion for the Settings page. The page's one event
 * owns one state, retargets it from each snapshot's focus rectangle, and
 * updates it exactly once per rendered frame under FrameBufferMagic's video
 * mutex. */

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

#endif
