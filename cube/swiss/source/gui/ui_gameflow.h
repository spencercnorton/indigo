#ifndef UI_GAMEFLOW_H
#define UI_GAMEFLOW_H

#include <stdbool.h>
#include <stdint.h>

#include "ui_motion.h"

/* Keeps carousel arithmetic exactly representable by a float while remaining
 * far above Swiss's practical library size. Oversized snapshots are clamped. */
#define UI_GAMEFLOW_MAX_ITEMS 65535u

typedef enum {
	UI_GAMEFLOW_MODE_LIBRARY = 0,
	UI_GAMEFLOW_MODE_DETAIL,
	UI_GAMEFLOW_MODE_LAUNCH
} uiGameflowMode_t;

typedef enum {
	UI_GAMEFLOW_DIRECTION_PREVIOUS = -1,
	UI_GAMEFLOW_DIRECTION_NONE = 0,
	UI_GAMEFLOW_DIRECTION_NEXT = 1
} uiGameflowDirection_t;

/*
 * Menu-thread selection snapshot. generation is a wrapping uint32_t serial;
 * equal, stale, and exactly half-range-ambiguous snapshots are rejected.
 * A NONE directionHint asks the state machine to choose the shortest ring arc.
 */
typedef struct {
	uint32_t generation;
	uint32_t itemCount;
	uint32_t selectedIndex;
	uiGameflowDirection_t directionHint;
	/* Page jumps may place the old card outside the retained render window.
	 * Snap that explicit transition instead of animating an unrelated card. */
	bool snapTransition;
} uiGameflowSelectionSnapshot_t;

/*
 * Read-only render values copied into one persistent EV_GAMEFLOW event.
 * carouselPosition/Target are deliberately rebased local spring coordinates;
 * renderers should consume carouselTravel for the selected card's signed
 * one-slot transition.
 */
typedef struct {
	uiGameflowMode_t mode;
	uiGameflowDirection_t direction;
	uint32_t generation;
	uint32_t itemCount;
	uint32_t selectedIndex;
	uint32_t previousIndex;
	uint32_t focusIndex;
	float carouselPosition;
	float carouselTarget;
	float carouselTravel;
	float detailProgress;
	float launchProgress;
	bool hasSnapshot;
	bool selectionPinned;
	bool transitioning;
} uiGameflowFrame_t;

/*
 * Fixed POD state: no heap ownership, I/O, locks, or external pointers. Embed
 * one instance in the retained event. The event wrapper must serialize every
 * state access (including Update, Frame, and IsGenerationCurrent) under the
 * same video mutex. A UIGameflow_Frame pointer must not be retained after that
 * mutex is released.
 */
typedef struct {
	uiGameflowMode_t mode;
	uiGameflowDirection_t direction;
	uint32_t generation;
	uint32_t itemCount;
	uint32_t selectedIndex;
	uint32_t previousIndex;
	uint32_t pinnedIndex;
	bool hasSnapshot;
	bool selectionPinned;
	uiMotionSpring_t carouselSpring;
	uiMotionSpring_t detailSpring;
	uiMotionSpring_t launchSpring;
	uiGameflowFrame_t frame;
} uiGameflowState_t;

void UIGameflow_Init(uiGameflowState_t *state);

/* Returns false without mutation when snapshot is null, equal, or stale. */
bool UIGameflow_ApplySnapshot(uiGameflowState_t *state,
	const uiGameflowSelectionSnapshot_t *snapshot, uiMotionMode_t motionMode);

/* Invalid modes safely select Library. Detail/Launch require a non-empty set. */
void UIGameflow_SetMode(uiGameflowState_t *state, uiGameflowMode_t mode,
	uiMotionMode_t motionMode);

/* Advance all retained motion on the video thread. Invalid motion snaps Off. */
void UIGameflow_Update(uiGameflowState_t *state, float deltaSeconds,
	uiMotionMode_t motionMode);

const uiGameflowFrame_t *UIGameflow_Frame(const uiGameflowState_t *state);
bool UIGameflow_IsGenerationCurrent(const uiGameflowState_t *state,
	uint32_t generation);

#endif
