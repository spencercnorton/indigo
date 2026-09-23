#ifndef UI_MENU_INPUT_H
#define UI_MENU_INPUT_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Pure, controller-agnostic policy for turning four raw menu sticks into one
 * discrete intent stream. Callers provide one sample per physical channel;
 * this module deliberately never combines axes or channels by magnitude.
 */

#define UI_MENU_INPUT_CHANNEL_COUNT 4
#define UI_MENU_INPUT_NO_OWNER (-1)

#define UI_MENU_INPUT_ENGAGE 32
#define UI_MENU_INPUT_RELEASE 20
#define UI_MENU_INPUT_INITIAL_REPEAT_US 320000u
#define UI_MENU_INPUT_REPEAT_US 120000u
#define UI_MENU_INPUT_MAX_ELAPSED_US 50000u
#define UI_MENU_INPUT_AXIS_MASK 3u
#define UI_MENU_INPUT_REPEAT 4u

typedef enum {
	UI_MENU_INPUT_NONE = 0,
	UI_MENU_INPUT_LEFT,
	UI_MENU_INPUT_RIGHT,
	UI_MENU_INPUT_UP,
	UI_MENU_INPUT_DOWN
} uiMenuInputDirection_t;

typedef enum {
	UI_MENU_INPUT_AXIS_NONE = 0,
	UI_MENU_INPUT_AXIS_HORIZONTAL = 1,
	UI_MENU_INPUT_AXIS_VERTICAL = 2,
	UI_MENU_INPUT_AXIS_BOTH = 3
} uiMenuInputAxis_t;

typedef struct {
	int x;
	int y;
	bool valid;
} uiMenuInputSample_t;

typedef struct {
	bool channelArmed[UI_MENU_INPUT_CHANNEL_COUNT];
	int owner;
	uiMenuInputDirection_t direction;
	uint32_t heldMicroseconds;
	uint32_t policy;
	bool repeated;
} uiMenuInputState_t;

void UIMenuInput_Init(uiMenuInputState_t *state);

/*
 * Returns at most one direction per call. A channel must first be observed
 * inside the release deadzone before it can acquire ownership, so a secondary
 * controller already drifting when a menu opens cannot hijack that menu.
 * `inhibited` gives digital input precedence, restarts analog repeat timing,
 * and disarms every deflected channel until that channel returns to neutral.
 * `policy` combines one UI_MENU_INPUT_AXIS_* value with optional
 * UI_MENU_INPUT_REPEAT. Without the repeat flag, one gesture emits exactly one
 * intent until full neutral. Changing policy releases ownership and requires
 * neutral observation before another gesture can begin.
 */
uiMenuInputDirection_t UIMenuInput_Update(uiMenuInputState_t *state,
	const uiMenuInputSample_t samples[UI_MENU_INPUT_CHANNEL_COUNT],
	uint32_t elapsedMicroseconds, uint32_t policy, bool inhibited);

typedef struct {
	uint32_t previous;
} uiMenuActionState_t;

/* Quarantine buttons already held when entering or returning from a modal.
 * Each action rearms independently on release: an unrelated held trigger
 * must never prevent Back or another fresh action. Modifiers accompany a
 * fresh action but cannot trigger one. A held cancel button owns the sample. */
void UIMenuAction_Init(uiMenuActionState_t *state, uint32_t held);
uint32_t UIMenuAction_Update(uiMenuActionState_t *state, uint32_t held,
	uint32_t actionMask, uint32_t modifierMask, uint32_t cancelMask);

#endif
