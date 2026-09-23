#include <stddef.h>

#include "ui_gameflow.h"

#define UI_GAMEFLOW_CAROUSEL_RESPONSE 15.0f
#define UI_GAMEFLOW_DETAIL_RESPONSE 12.0f
#define UI_GAMEFLOW_LAUNCH_RESPONSE 16.0f
#define UI_GAMEFLOW_POSITION_EPSILON 0.0001f
#define UI_GAMEFLOW_VELOCITY_EPSILON 0.0005f

static uiMotionMode_t sanitizeMotionMode(uiMotionMode_t mode)
{
	switch(mode) {
		case UI_MOTION_FULL:
		case UI_MOTION_REDUCED:
		case UI_MOTION_OFF:
			return mode;
		default:
			return UI_MOTION_OFF;
	}
}

static uiGameflowMode_t sanitizeGameflowMode(uiGameflowMode_t mode)
{
	switch(mode) {
		case UI_GAMEFLOW_MODE_LIBRARY:
		case UI_GAMEFLOW_MODE_DETAIL:
		case UI_GAMEFLOW_MODE_LAUNCH:
			return mode;
		default:
			return UI_GAMEFLOW_MODE_LIBRARY;
	}
}

static uiGameflowDirection_t sanitizeDirection(uiGameflowDirection_t direction)
{
	if(direction == UI_GAMEFLOW_DIRECTION_PREVIOUS ||
		direction == UI_GAMEFLOW_DIRECTION_NEXT) {
		return direction;
	}
	return UI_GAMEFLOW_DIRECTION_NONE;
}

static uint32_t sanitizeCount(uint32_t itemCount)
{
	return itemCount > UI_GAMEFLOW_MAX_ITEMS ? UI_GAMEFLOW_MAX_ITEMS : itemCount;
}

static uint32_t sanitizeIndex(uint32_t selectedIndex, uint32_t itemCount)
{
	if(itemCount == 0u) {
		return 0u;
	}
	return selectedIndex < itemCount ? selectedIndex : itemCount - 1u;
}

static float clampProgress(float progress)
{
	if(progress <= 0.0f) {
		return 0.0f;
	}
	if(progress >= 1.0f) {
		return 1.0f;
	}
	return progress;
}

/* RFC-1982-style serial comparison. The exactly half-range case is unordered. */
static bool generationIsNewer(uint32_t candidate, uint32_t current)
{
	uint32_t distance = candidate - current;
	return distance != 0u && distance < 0x80000000u;
}

static int32_t ringDelta(uint32_t previous, uint32_t selected,
	uint32_t itemCount, uiGameflowDirection_t directionHint)
{
	uint32_t forward;
	int32_t backward;

	if(itemCount <= 1u || previous == selected) {
		return 0;
	}

	forward = selected >= previous ?
		selected - previous : itemCount - previous + selected;
	backward = (int32_t)forward - (int32_t)itemCount;

	if(directionHint == UI_GAMEFLOW_DIRECTION_NEXT) {
		return (int32_t)forward;
	}
	if(directionHint == UI_GAMEFLOW_DIRECTION_PREVIOUS) {
		return backward;
	}

	if(forward < (uint32_t)(-backward)) {
		return (int32_t)forward;
	}
	if(forward > (uint32_t)(-backward)) {
		return backward;
	}

	/* An even-sized ring has two equal arcs. Prefer the apparent index motion;
	 * callers can supply a hint when a two-item wrap must be disambiguated. */
	return selected > previous ? (int32_t)forward : backward;
}

static bool springSettled(const uiMotionSpring_t *spring)
{
	return UIMotion_SpringSettled(spring, UI_GAMEFLOW_POSITION_EPSILON,
		UI_GAMEFLOW_VELOCITY_EPSILON);
}

static float clampCarouselTravel(float travel)
{
	if(travel < -1.0f) {
		return -1.0f;
	}
	if(travel > 1.0f) {
		return 1.0f;
	}
	return travel;
}

static void retargetCarouselSpring(uiGameflowState_t *state, int32_t delta,
		uiMotionMode_t motionMode)
{
	uiMotionSpring_t *spring = &state->carouselSpring;
	float visualStep = delta > 0 ? 1.0f : -1.0f;
	float travel = spring->target - spring->value;

	/* Snapshot selection is latest-value state, not an animation queue. A new
	 * card begins at most one slot away; rapid same-direction input coalesces,
	 * while reversal subtracts a slot and preserves visible continuity. */
	travel = clampCarouselTravel(travel + visualStep);
	spring->target = 0.0f;
	spring->value = -travel;
	if(motionMode == UI_MOTION_REDUCED) {
		spring->velocity = 0.0f;
	}
	else if(motionMode == UI_MOTION_OFF) {
		UIMotion_SpringSnap(spring, 0.0f);
		state->direction = UI_GAMEFLOW_DIRECTION_NONE;
	}
}

static void refreshFrame(uiGameflowState_t *state)
{
	uiGameflowFrame_t *frame = &state->frame;

	frame->mode = state->mode;
	frame->direction = state->direction;
	frame->generation = state->generation;
	frame->itemCount = state->itemCount;
	frame->selectedIndex = state->selectedIndex;
	frame->previousIndex = state->previousIndex;
	frame->focusIndex = state->selectionPinned ?
		state->pinnedIndex : state->selectedIndex;
	frame->carouselPosition = state->carouselSpring.value;
	frame->carouselTarget = state->carouselSpring.target;
	frame->carouselTravel = state->carouselSpring.target -
		state->carouselSpring.value;
	frame->detailProgress = clampProgress(state->detailSpring.value);
	frame->launchProgress = clampProgress(state->launchSpring.value);
	frame->hasSnapshot = state->hasSnapshot;
	frame->selectionPinned = state->selectionPinned;
	frame->transitioning = !springSettled(&state->carouselSpring) ||
		!springSettled(&state->detailSpring) ||
		!springSettled(&state->launchSpring);
}

static void retargetModeSprings(uiGameflowState_t *state,
	uiMotionMode_t motionMode)
{
	float detailTarget = state->mode == UI_GAMEFLOW_MODE_LIBRARY ? 0.0f : 1.0f;
	float launchTarget = state->mode == UI_GAMEFLOW_MODE_LAUNCH ? 1.0f : 0.0f;

	UIMotion_SpringRetarget(&state->detailSpring, detailTarget, motionMode);
	UIMotion_SpringRetarget(&state->launchSpring, launchTarget, motionMode);
}

void UIGameflow_Init(uiGameflowState_t *state)
{
	if(state == NULL) {
		return;
	}

	*state = (uiGameflowState_t){0};
	state->mode = UI_GAMEFLOW_MODE_LIBRARY;
	state->direction = UI_GAMEFLOW_DIRECTION_NONE;
	UIMotion_SpringInit(&state->carouselSpring, 0.0f,
		UI_GAMEFLOW_CAROUSEL_RESPONSE);
	UIMotion_SpringInit(&state->detailSpring, 0.0f,
		UI_GAMEFLOW_DETAIL_RESPONSE);
	UIMotion_SpringInit(&state->launchSpring, 0.0f,
		UI_GAMEFLOW_LAUNCH_RESPONSE);
	refreshFrame(state);
}

bool UIGameflow_ApplySnapshot(uiGameflowState_t *state,
	const uiGameflowSelectionSnapshot_t *snapshot, uiMotionMode_t motionMode)
{
	uiGameflowDirection_t directionHint;
	bool hadSnapshot;
	uint32_t oldCount;
	uint32_t oldSelection;
	uint32_t newSelection;
	uint32_t newCount;
	int32_t delta;

	if(state == NULL || snapshot == NULL) {
		return false;
	}
	if(state->hasSnapshot &&
		!generationIsNewer(snapshot->generation, state->generation)) {
		return false;
	}

	motionMode = sanitizeMotionMode(motionMode);
	newCount = sanitizeCount(snapshot->itemCount);
	directionHint = sanitizeDirection(snapshot->directionHint);
	hadSnapshot = state->hasSnapshot;
	oldCount = sanitizeCount(state->itemCount);
	state->mode = sanitizeGameflowMode(state->mode);
	if(state->mode == UI_GAMEFLOW_MODE_LIBRARY) {
		state->selectionPinned = false;
	}

	state->generation = snapshot->generation;
	state->hasSnapshot = true;
	state->itemCount = newCount;

	if(newCount == 0u) {
		state->selectedIndex = 0u;
		state->previousIndex = 0u;
		state->pinnedIndex = 0u;
		state->direction = UI_GAMEFLOW_DIRECTION_NONE;
		state->selectionPinned = false;
		state->mode = UI_GAMEFLOW_MODE_LIBRARY;
		UIMotion_SpringRetarget(&state->carouselSpring, 0.0f, motionMode);
		retargetModeSprings(state, motionMode);
		refreshFrame(state);
		return true;
	}

	newSelection = sanitizeIndex(snapshot->selectedIndex, newCount);
	if(!hadSnapshot || oldCount == 0u) {
		state->selectedIndex = newSelection;
		state->previousIndex = newSelection;
		state->pinnedIndex = newSelection;
		state->direction = UI_GAMEFLOW_DIRECTION_NONE;
		UIMotion_SpringSnap(&state->carouselSpring, 0.0f);
		retargetModeSprings(state, motionMode);
		refreshFrame(state);
		return true;
	}

	oldSelection = sanitizeIndex(state->selectedIndex, newCount);
	if(state->selectionPinned) {
		/* Library navigation is frozen through Detail and Launch. A newer
		 * snapshot still advances the generation and may shrink the set. */
		newSelection = sanitizeIndex(state->pinnedIndex, newCount);
		state->pinnedIndex = newSelection;
	}

	delta = ringDelta(oldSelection, newSelection, newCount, directionHint);
	state->previousIndex = oldSelection;
	state->selectedIndex = newSelection;
	if(delta > 0) {
		state->direction = UI_GAMEFLOW_DIRECTION_NEXT;
	}
	else if(delta < 0) {
		state->direction = UI_GAMEFLOW_DIRECTION_PREVIOUS;
	}
	else if(springSettled(&state->carouselSpring)) {
		state->direction = UI_GAMEFLOW_DIRECTION_NONE;
	}
	if(delta != 0) {
		if(snapshot->snapTransition) {
			UIMotion_SpringSnap(&state->carouselSpring, 0.0f);
			state->direction = UI_GAMEFLOW_DIRECTION_NONE;
		}
		else {
			retargetCarouselSpring(state, delta, motionMode);
		}
		if(springSettled(&state->carouselSpring)) {
			state->direction = UI_GAMEFLOW_DIRECTION_NONE;
		}
	}
	else if(motionMode == UI_MOTION_OFF) {
		UIMotion_SpringSnap(&state->carouselSpring,
			state->carouselSpring.target);
		state->direction = UI_GAMEFLOW_DIRECTION_NONE;
	}
	retargetModeSprings(state, motionMode);
	refreshFrame(state);
	return true;
}

void UIGameflow_SetMode(uiGameflowState_t *state, uiGameflowMode_t mode,
	uiMotionMode_t motionMode)
{
	if(state == NULL) {
		return;
	}

	motionMode = sanitizeMotionMode(motionMode);
	mode = sanitizeGameflowMode(mode);
	state->itemCount = sanitizeCount(state->itemCount);
	state->selectedIndex = sanitizeIndex(state->selectedIndex, state->itemCount);
	state->previousIndex = sanitizeIndex(state->previousIndex, state->itemCount);

	if(!state->hasSnapshot || state->itemCount == 0u) {
		mode = UI_GAMEFLOW_MODE_LIBRARY;
	}

	if(mode == UI_GAMEFLOW_MODE_LIBRARY) {
		if(state->selectionPinned) {
			state->selectedIndex = sanitizeIndex(state->pinnedIndex,
				state->itemCount);
		}
		state->selectionPinned = false;
	} else {
		if(!state->selectionPinned) {
			state->pinnedIndex = state->selectedIndex;
			state->selectionPinned = true;
		}
		state->pinnedIndex = sanitizeIndex(state->pinnedIndex,
			state->itemCount);
		state->selectedIndex = state->pinnedIndex;
	}

	state->mode = mode;
	retargetModeSprings(state, motionMode);
	if(motionMode == UI_MOTION_OFF) {
		UIMotion_SpringSnap(&state->carouselSpring,
			state->carouselSpring.target);
		state->direction = UI_GAMEFLOW_DIRECTION_NONE;
	}
	refreshFrame(state);
}

void UIGameflow_Update(uiGameflowState_t *state, float deltaSeconds,
	uiMotionMode_t motionMode)
{
	uiGameflowMode_t sanitizedMode;

	if(state == NULL) {
		return;
	}

	motionMode = sanitizeMotionMode(motionMode);
	if(!(deltaSeconds >= 0.0f)) {
		deltaSeconds = 0.0f;
	}

	state->itemCount = sanitizeCount(state->itemCount);
	state->selectedIndex = sanitizeIndex(state->selectedIndex, state->itemCount);
	state->previousIndex = sanitizeIndex(state->previousIndex, state->itemCount);
	state->pinnedIndex = sanitizeIndex(state->pinnedIndex, state->itemCount);
	sanitizedMode = sanitizeGameflowMode(state->mode);
	if(!state->hasSnapshot || state->itemCount == 0u) {
		sanitizedMode = UI_GAMEFLOW_MODE_LIBRARY;
	}
	if(sanitizedMode != state->mode ||
		(sanitizedMode == UI_GAMEFLOW_MODE_LIBRARY && state->selectionPinned) ||
		(sanitizedMode != UI_GAMEFLOW_MODE_LIBRARY &&
		!state->selectionPinned)) {
		UIGameflow_SetMode(state, sanitizedMode, motionMode);
	}

	UIMotion_SpringUpdate(&state->carouselSpring, deltaSeconds, motionMode);
	UIMotion_SpringUpdate(&state->detailSpring, deltaSeconds, motionMode);
	UIMotion_SpringUpdate(&state->launchSpring, deltaSeconds, motionMode);
	if(springSettled(&state->carouselSpring)) {
		state->direction = UI_GAMEFLOW_DIRECTION_NONE;
	}
	refreshFrame(state);
}

const uiGameflowFrame_t *UIGameflow_Frame(const uiGameflowState_t *state)
{
	return state != NULL ? &state->frame : NULL;
}

bool UIGameflow_IsGenerationCurrent(const uiGameflowState_t *state,
	uint32_t generation)
{
	return state != NULL && state->hasSnapshot &&
		state->generation == generation;
}
