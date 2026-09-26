/*
 * Host test command (run from the repository root):
 * cc -std=c11 -Wall -Wextra -Werror -pedantic -Icube/swiss/source/gui \
 *   buildtools/ui/tests/test_gameflow_state.c \
 *   cube/swiss/source/gui/ui_gameflow.c cube/swiss/source/gui/ui_motion.c \
 *   -lm -o /tmp/test_gameflow_state && /tmp/test_gameflow_state
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "ui_gameflow.h"

#define CHECK(condition) do { \
	if(!(condition)) { \
		fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, \
			#condition); \
		exit(EXIT_FAILURE); \
	} \
} while(0)

static bool nearlyEqual(float a, float b)
{
	return fabsf(a - b) <= 0.0001f;
}

static uiGameflowSelectionSnapshot_t snapshot(uint32_t generation,
	uint32_t itemCount, uint32_t selectedIndex,
	uiGameflowDirection_t directionHint)
{
	uiGameflowSelectionSnapshot_t value;
	value.generation = generation;
	value.itemCount = itemCount;
	value.selectedIndex = selectedIndex;
	value.directionHint = directionHint;
	value.snapTransition = false;
	return value;
}

static void apply(uiGameflowState_t *state, uint32_t generation,
	uint32_t itemCount, uint32_t selectedIndex,
	uiGameflowDirection_t directionHint, uiMotionMode_t motionMode)
{
	uiGameflowSelectionSnapshot_t value = snapshot(generation, itemCount,
		selectedIndex, directionHint);
	CHECK(UIGameflow_ApplySnapshot(state, &value, motionMode));
}

static void testGenerationAndSanitization(void)
{
	uiGameflowSelectionSnapshot_t value;
	uiGameflowState_t state;
	const uiGameflowFrame_t *frame;

	UIGameflow_Init(&state);
	frame = UIGameflow_Frame(&state);
	CHECK(frame != NULL);
	CHECK(frame->mode == UI_GAMEFLOW_MODE_LIBRARY);
	CHECK(frame->itemCount == 0u);
	CHECK(!frame->hasSnapshot);
	CHECK(!UIGameflow_ApplySnapshot(&state, NULL, UI_MOTION_FULL));

	value = snapshot(10u, 5u, 99u, (uiGameflowDirection_t)77);
	CHECK(UIGameflow_ApplySnapshot(&state, &value, (uiMotionMode_t)77));
	frame = UIGameflow_Frame(&state);
	CHECK(frame->itemCount == 5u);
	CHECK(frame->selectedIndex == 4u);
	CHECK(frame->previousIndex == 4u);
	CHECK(frame->direction == UI_GAMEFLOW_DIRECTION_NONE);
	CHECK(nearlyEqual(frame->carouselPosition, 0.0f));
	CHECK(UIGameflow_IsGenerationCurrent(&state, 10u));

	value = snapshot(10u, 5u, 1u, UI_GAMEFLOW_DIRECTION_NEXT);
	CHECK(!UIGameflow_ApplySnapshot(&state, &value, UI_MOTION_FULL));
	value.generation = 9u;
	CHECK(!UIGameflow_ApplySnapshot(&state, &value, UI_MOTION_FULL));
	CHECK(UIGameflow_Frame(&state)->selectedIndex == 4u);

	value = snapshot(11u, UINT32_MAX, UINT32_MAX,
		UI_GAMEFLOW_DIRECTION_NONE);
	CHECK(UIGameflow_ApplySnapshot(&state, &value, UI_MOTION_OFF));
	CHECK(UIGameflow_Frame(&state)->itemCount == UI_GAMEFLOW_MAX_ITEMS);
	CHECK(UIGameflow_Frame(&state)->selectedIndex ==
		UI_GAMEFLOW_MAX_ITEMS - 1u);

	UIGameflow_SetMode(&state, (uiGameflowMode_t)88, (uiMotionMode_t)88);
	CHECK(UIGameflow_Frame(&state)->mode == UI_GAMEFLOW_MODE_LIBRARY);
	apply(&state, 12u, 0u, UINT32_MAX, UI_GAMEFLOW_DIRECTION_NEXT,
		UI_MOTION_OFF);
	frame = UIGameflow_Frame(&state);
	CHECK(frame->mode == UI_GAMEFLOW_MODE_LIBRARY);
	CHECK(frame->itemCount == 0u);
	CHECK(frame->selectedIndex == 0u);
	CHECK(nearlyEqual(frame->carouselPosition, 0.0f));

	UIGameflow_Init(&state);
	apply(&state, UINT32_MAX, 3u, 0u, UI_GAMEFLOW_DIRECTION_NONE,
		UI_MOTION_OFF);
	apply(&state, 0u, 3u, 1u, UI_GAMEFLOW_DIRECTION_NEXT,
		UI_MOTION_OFF);
	CHECK(UIGameflow_IsGenerationCurrent(&state, 0u));
	value = snapshot(0x80000000u, 3u, 2u, UI_GAMEFLOW_DIRECTION_NEXT);
	CHECK(!UIGameflow_ApplySnapshot(&state, &value, UI_MOTION_FULL));
}

static void testWrapAwareDirection(void)
{
	uiGameflowState_t state;
	const uiGameflowFrame_t *frame;

	UIGameflow_Init(&state);
	apply(&state, 1u, 5u, 4u, UI_GAMEFLOW_DIRECTION_NONE, UI_MOTION_OFF);
	apply(&state, 2u, 5u, 0u, UI_GAMEFLOW_DIRECTION_NONE, UI_MOTION_FULL);
	frame = UIGameflow_Frame(&state);
	CHECK(frame->previousIndex == 4u);
	CHECK(frame->selectedIndex == 0u);
	CHECK(frame->direction == UI_GAMEFLOW_DIRECTION_NEXT);
	CHECK(nearlyEqual(frame->carouselPosition, -1.0f));
	CHECK(nearlyEqual(frame->carouselTarget, 0.0f));
	CHECK(nearlyEqual(frame->carouselTravel, 1.0f));

	UIGameflow_Update(&state, 1.0f / 60.0f, UI_MOTION_FULL);
	CHECK(UIGameflow_Frame(&state)->carouselPosition > -1.0f);
	CHECK(UIGameflow_Frame(&state)->carouselPosition < 0.0f);
	apply(&state, 3u, 5u, 4u, UI_GAMEFLOW_DIRECTION_NONE, UI_MOTION_FULL);
	frame = UIGameflow_Frame(&state);
	CHECK(frame->direction == UI_GAMEFLOW_DIRECTION_PREVIOUS);
	CHECK(nearlyEqual(frame->carouselTarget, 0.0f));
	CHECK(frame->carouselPosition > 0.0f);
	CHECK(frame->carouselPosition < 1.0f);
	CHECK(frame->carouselTravel < 0.0f);

	UIGameflow_Init(&state);
	apply(&state, 1u, 2u, 0u, UI_GAMEFLOW_DIRECTION_NONE, UI_MOTION_OFF);
	apply(&state, 2u, 2u, 1u, UI_GAMEFLOW_DIRECTION_PREVIOUS,
		UI_MOTION_FULL);
	frame = UIGameflow_Frame(&state);
	CHECK(frame->direction == UI_GAMEFLOW_DIRECTION_PREVIOUS);
	CHECK(nearlyEqual(frame->carouselPosition, 1.0f));
	CHECK(nearlyEqual(frame->carouselTarget, 0.0f));
	CHECK(nearlyEqual(frame->carouselTravel, -1.0f));

	UIGameflow_Init(&state);
	apply(&state, 1u, 2u, 0u, UI_GAMEFLOW_DIRECTION_NONE, UI_MOTION_OFF);
	apply(&state, 2u, 2u, 1u, UI_GAMEFLOW_DIRECTION_NEXT,
		UI_MOTION_FULL);
	frame = UIGameflow_Frame(&state);
	CHECK(frame->direction == UI_GAMEFLOW_DIRECTION_NEXT);
	CHECK(nearlyEqual(frame->carouselPosition, -1.0f));
	CHECK(nearlyEqual(frame->carouselTarget, 0.0f));
	CHECK(nearlyEqual(frame->carouselTravel, 1.0f));
}

static void testMotionModesAndInterruption(void)
{
	uiGameflowState_t state;
	const uiGameflowFrame_t *frame;
	float interruptedPosition;
	float interruptedTravel;
	float interruptedVelocity;
	int i;

	UIGameflow_Init(&state);
	apply(&state, 1u, 6u, 0u, UI_GAMEFLOW_DIRECTION_NONE, UI_MOTION_OFF);
	apply(&state, 2u, 6u, 1u, UI_GAMEFLOW_DIRECTION_NEXT, UI_MOTION_FULL);
	UIGameflow_Update(&state, 1.0f / 60.0f, UI_MOTION_FULL);
	CHECK(state.carouselSpring.value > -1.0f);
	CHECK(state.carouselSpring.value < 0.0f);
	CHECK(state.carouselSpring.velocity > 0.0f);
	interruptedPosition = state.carouselSpring.value;
	interruptedTravel = UIGameflow_Frame(&state)->carouselTravel;
	interruptedVelocity = state.carouselSpring.velocity;

	apply(&state, 3u, 6u, 0u, UI_GAMEFLOW_DIRECTION_PREVIOUS,
		UI_MOTION_FULL);
	CHECK(nearlyEqual(UIGameflow_Frame(&state)->carouselTravel,
		interruptedTravel - 1.0f));
	CHECK(nearlyEqual(state.carouselSpring.velocity, interruptedVelocity));
	CHECK(nearlyEqual(state.carouselSpring.target, 0.0f));

	apply(&state, 4u, 6u, 1u, UI_GAMEFLOW_DIRECTION_NEXT, UI_MOTION_REDUCED);
	CHECK(nearlyEqual(state.carouselSpring.velocity, 0.0f));
	CHECK(nearlyEqual(state.carouselSpring.value, interruptedPosition));
	apply(&state, 5u, 6u, 2u, UI_GAMEFLOW_DIRECTION_NEXT, UI_MOTION_OFF);
	CHECK(nearlyEqual(state.carouselSpring.value, state.carouselSpring.target));
	CHECK(UIGameflow_Frame(&state)->direction ==
		UI_GAMEFLOW_DIRECTION_NONE);

	UIGameflow_SetMode(&state, UI_GAMEFLOW_MODE_DETAIL, UI_MOTION_REDUCED);
	UIGameflow_Update(&state, 1.0f / 60.0f, UI_MOTION_REDUCED);
	frame = UIGameflow_Frame(&state);
	CHECK(frame->detailProgress > 0.0f && frame->detailProgress < 1.0f);
	UIGameflow_SetMode(&state, UI_GAMEFLOW_MODE_LAUNCH, UI_MOTION_OFF);
	frame = UIGameflow_Frame(&state);
	CHECK(nearlyEqual(frame->detailProgress, 1.0f));
	CHECK(nearlyEqual(frame->launchProgress, 1.0f));

	/* NaN/negative deltas are inert; an invalid motion mode safely snaps. */
	UIGameflow_SetMode(&state, UI_GAMEFLOW_MODE_DETAIL, UI_MOTION_FULL);
	interruptedPosition = state.launchSpring.value;
	UIGameflow_Update(&state, NAN, UI_MOTION_FULL);
	CHECK(nearlyEqual(state.launchSpring.value, interruptedPosition));
	UIGameflow_Update(&state, -1.0f, UI_MOTION_FULL);
	CHECK(nearlyEqual(state.launchSpring.value, interruptedPosition));
	UIGameflow_Update(&state, 1.0f / 60.0f, (uiMotionMode_t)99);
	CHECK(nearlyEqual(state.launchSpring.value, state.launchSpring.target));

	UIGameflow_SetMode(&state, UI_GAMEFLOW_MODE_LIBRARY, UI_MOTION_FULL);
	for(i = 0; i < 240; ++i) {
		UIGameflow_Update(&state, 1.0f / 60.0f, UI_MOTION_FULL);
	}
	frame = UIGameflow_Frame(&state);
	CHECK(!frame->transitioning);
	CHECK(frame->direction == UI_GAMEFLOW_DIRECTION_NONE);
	CHECK(nearlyEqual(frame->detailProgress, 0.0f));
	CHECK(nearlyEqual(frame->launchProgress, 0.0f));
}

static void testPageJumpSnapsWithoutFalseNeighbor(void)
{
	uiGameflowState_t state;
	const uiGameflowFrame_t *frame;
	uiGameflowSelectionSnapshot_t value;

	UIGameflow_Init(&state);
	apply(&state, 1u, 20u, 0u, UI_GAMEFLOW_DIRECTION_NONE, UI_MOTION_OFF);
	value = snapshot(2u, 20u, 9u, UI_GAMEFLOW_DIRECTION_NEXT);
	value.snapTransition = true;
	CHECK(UIGameflow_ApplySnapshot(&state, &value, UI_MOTION_FULL));
	frame = UIGameflow_Frame(&state);
	CHECK(frame->selectedIndex == 9u);
	CHECK(frame->direction == UI_GAMEFLOW_DIRECTION_NONE);
	CHECK(nearlyEqual(frame->carouselPosition, 0.0f));
	CHECK(nearlyEqual(frame->carouselTravel, 0.0f));
}

static void testBoundedPhaseRefreshAndImmediateOff(void)
{
	uiGameflowState_t state;
	const uiGameflowFrame_t *frame;
	float travel;
	uint32_t selected = 0u;
	uint32_t generation = 1u;

	/* Off snapshots settle immediately regardless of logical ring distance. */
	UIGameflow_Init(&state);
	apply(&state, generation++, UI_GAMEFLOW_MAX_ITEMS, selected,
		UI_GAMEFLOW_DIRECTION_NONE, UI_MOTION_OFF);
	for(int i = 0; i < 300; ++i) {
		selected = selected == 0u ? UI_GAMEFLOW_MAX_ITEMS - 1u : selected - 1u;
		apply(&state, generation++, UI_GAMEFLOW_MAX_ITEMS, selected,
			UI_GAMEFLOW_DIRECTION_NEXT, UI_MOTION_OFF);
		frame = UIGameflow_Frame(&state);
		CHECK(nearlyEqual(frame->carouselTarget, 0.0f));
		CHECK(nearlyEqual(frame->carouselPosition, 0.0f));
		CHECK(frame->direction == UI_GAMEFLOW_DIRECTION_NONE);
	}

	/* Full-motion input is latest-value state rather than an unbounded queue.
	 * Hundreds of maximum arcs without a video update stay within one local
	 * slot; an opposite unit exactly cancels and a new unit remains visible. */
	UIGameflow_Init(&state);
	selected = 0u;
	generation = 1u;
	apply(&state, generation++, UI_GAMEFLOW_MAX_ITEMS, selected,
		UI_GAMEFLOW_DIRECTION_NONE, UI_MOTION_OFF);
	for(int i = 0; i < 300; ++i) {
		selected = selected == 0u ? UI_GAMEFLOW_MAX_ITEMS - 1u : selected - 1u;
		apply(&state, generation++, UI_GAMEFLOW_MAX_ITEMS, selected,
			UI_GAMEFLOW_DIRECTION_NEXT, UI_MOTION_FULL);
		frame = UIGameflow_Frame(&state);
		CHECK(frame->selectedIndex == selected);
		CHECK(isfinite(frame->carouselPosition));
		CHECK(fabsf(frame->carouselTravel) <= 1.0f);
		CHECK(nearlyEqual(frame->carouselTravel, 1.0f));
	}
	selected = (selected + 1u) % UI_GAMEFLOW_MAX_ITEMS;
	apply(&state, generation++, UI_GAMEFLOW_MAX_ITEMS, selected,
		UI_GAMEFLOW_DIRECTION_PREVIOUS, UI_MOTION_FULL);
	frame = UIGameflow_Frame(&state);
	CHECK(nearlyEqual(frame->carouselTravel, 0.0f));
	CHECK(frame->direction == UI_GAMEFLOW_DIRECTION_NONE);
	selected = selected == 0u ? UI_GAMEFLOW_MAX_ITEMS - 1u : selected - 1u;
	apply(&state, generation++, UI_GAMEFLOW_MAX_ITEMS, selected,
		UI_GAMEFLOW_DIRECTION_NEXT, UI_MOTION_FULL);
	frame = UIGameflow_Frame(&state);
	CHECK(nearlyEqual(frame->carouselTravel, 1.0f));
	CHECK(frame->direction == UI_GAMEFLOW_DIRECTION_NEXT);

	/* A newer metadata-only snapshot preserves the active directional leg. */
	UIGameflow_Init(&state);
	apply(&state, 1u, 5u, 0u, UI_GAMEFLOW_DIRECTION_NONE, UI_MOTION_OFF);
	apply(&state, 2u, 5u, 1u, UI_GAMEFLOW_DIRECTION_NEXT, UI_MOTION_FULL);
	UIGameflow_Update(&state, 1.0f / 60.0f, UI_MOTION_FULL);
	frame = UIGameflow_Frame(&state);
	CHECK(frame->direction == UI_GAMEFLOW_DIRECTION_NEXT);
	CHECK(frame->transitioning);
	travel = frame->carouselTravel;
	apply(&state, 3u, 5u, 1u, UI_GAMEFLOW_DIRECTION_NONE, UI_MOTION_FULL);
	frame = UIGameflow_Frame(&state);
	CHECK(frame->direction == UI_GAMEFLOW_DIRECTION_NEXT);
	CHECK(frame->transitioning);
	CHECK(nearlyEqual(frame->carouselTravel, travel));

	/* Public mutations that receive Off resolve immediately, without needing
	 * one later video-thread update to apply the accessibility preference. */
	UIGameflow_SetMode(&state, UI_GAMEFLOW_MODE_DETAIL, UI_MOTION_FULL);
	UIGameflow_Update(&state, 1.0f / 60.0f, UI_MOTION_FULL);
	CHECK(UIGameflow_Frame(&state)->detailProgress > 0.0f);
	CHECK(UIGameflow_Frame(&state)->detailProgress < 1.0f);
	apply(&state, 4u, 5u, 1u, UI_GAMEFLOW_DIRECTION_NONE, UI_MOTION_OFF);
	frame = UIGameflow_Frame(&state);
	CHECK(nearlyEqual(frame->carouselPosition, frame->carouselTarget));
	CHECK(nearlyEqual(frame->detailProgress, 1.0f));
	CHECK(nearlyEqual(frame->launchProgress, 0.0f));
	CHECK(!frame->transitioning);
	CHECK(frame->direction == UI_GAMEFLOW_DIRECTION_NONE);

	UIGameflow_SetMode(&state, UI_GAMEFLOW_MODE_LIBRARY, UI_MOTION_OFF);
	apply(&state, 5u, 5u, 2u, UI_GAMEFLOW_DIRECTION_NEXT, UI_MOTION_FULL);
	UIGameflow_Update(&state, 1.0f / 60.0f, UI_MOTION_FULL);
	CHECK(UIGameflow_Frame(&state)->transitioning);
	UIGameflow_SetMode(&state, UI_GAMEFLOW_MODE_LIBRARY, UI_MOTION_OFF);
	frame = UIGameflow_Frame(&state);
	CHECK(nearlyEqual(frame->carouselPosition, frame->carouselTarget));
	CHECK(!frame->transitioning);
	CHECK(frame->direction == UI_GAMEFLOW_DIRECTION_NONE);

	UIGameflow_SetMode(&state, UI_GAMEFLOW_MODE_DETAIL, UI_MOTION_OFF);
	apply(&state, 6u, 1u, 0u, UI_GAMEFLOW_DIRECTION_PREVIOUS,
		UI_MOTION_FULL);
	CHECK(UIGameflow_Frame(&state)->focusIndex == 0u);
	apply(&state, 7u, 0u, 0u, UI_GAMEFLOW_DIRECTION_NONE, UI_MOTION_OFF);
	frame = UIGameflow_Frame(&state);
	CHECK(frame->mode == UI_GAMEFLOW_MODE_LIBRARY);
	CHECK(!frame->selectionPinned);
	CHECK(!frame->transitioning);

	UIGameflow_Init(&state);
	apply(&state, 1u, 3u, 0u, UI_GAMEFLOW_DIRECTION_NONE, UI_MOTION_OFF);
	apply(&state, 2u, 3u, 1u, UI_GAMEFLOW_DIRECTION_NEXT, UI_MOTION_FULL);
	UIGameflow_Update(&state, INFINITY, UI_MOTION_FULL);
	CHECK(isfinite(UIGameflow_Frame(&state)->carouselPosition));
}

static void testPinnedDetailLaunchSelection(void)
{
	uiGameflowState_t state;
	const uiGameflowFrame_t *frame;

	UIGameflow_Init(&state);
	apply(&state, 1u, 5u, 2u, UI_GAMEFLOW_DIRECTION_NONE, UI_MOTION_OFF);
	UIGameflow_SetMode(&state, UI_GAMEFLOW_MODE_DETAIL, UI_MOTION_FULL);
	frame = UIGameflow_Frame(&state);
	CHECK(frame->mode == UI_GAMEFLOW_MODE_DETAIL);
	CHECK(frame->selectionPinned);
	CHECK(frame->focusIndex == 2u);

	/* A newer menu snapshot is generation-safe but cannot move the card while
	 * Detail/Launch owns it. */
	apply(&state, 2u, 5u, 4u, UI_GAMEFLOW_DIRECTION_NEXT, UI_MOTION_FULL);
	frame = UIGameflow_Frame(&state);
	CHECK(frame->selectedIndex == 2u);
	CHECK(frame->focusIndex == 2u);
	CHECK(UIGameflow_IsGenerationCurrent(&state, 2u));

	UIGameflow_SetMode(&state, UI_GAMEFLOW_MODE_LAUNCH, UI_MOTION_FULL);
	frame = UIGameflow_Frame(&state);
	CHECK(frame->mode == UI_GAMEFLOW_MODE_LAUNCH);
	CHECK(frame->focusIndex == 2u);
	UIGameflow_Update(&state, 1.0f / 60.0f, UI_MOTION_FULL);
	UIGameflow_SetMode(&state, UI_GAMEFLOW_MODE_DETAIL, UI_MOTION_FULL);
	CHECK(UIGameflow_Frame(&state)->focusIndex == 2u);
	UIGameflow_SetMode(&state, UI_GAMEFLOW_MODE_LIBRARY, UI_MOTION_FULL);
	frame = UIGameflow_Frame(&state);
	CHECK(frame->mode == UI_GAMEFLOW_MODE_LIBRARY);
	CHECK(!frame->selectionPinned);
	CHECK(frame->selectedIndex == 2u);

	/* If the backing set shrinks, the pin is clamped instead of dereferenced
	 * out of range, and that safe selection is what Library restores. */
	apply(&state, 3u, 5u, 4u, UI_GAMEFLOW_DIRECTION_NEXT, UI_MOTION_OFF);
	UIGameflow_SetMode(&state, UI_GAMEFLOW_MODE_DETAIL, UI_MOTION_OFF);
	apply(&state, 4u, 3u, 0u, UI_GAMEFLOW_DIRECTION_PREVIOUS, UI_MOTION_FULL);
	frame = UIGameflow_Frame(&state);
	CHECK(frame->selectedIndex == 2u);
	CHECK(frame->focusIndex == 2u);
	UIGameflow_SetMode(&state, UI_GAMEFLOW_MODE_LAUNCH, UI_MOTION_OFF);
	CHECK(UIGameflow_Frame(&state)->focusIndex == 2u);
	UIGameflow_SetMode(&state, UI_GAMEFLOW_MODE_LIBRARY, UI_MOTION_OFF);
	CHECK(UIGameflow_Frame(&state)->selectedIndex == 2u);
}


static void testGridRowsAndHighlight(void)
{
	uiGameflowState_t state;
	const uiGameflowFrame_t *frame;
	uiGameflowSelectionSnapshot_t value;
	float column;
	int i;

	/* Twelve cards in rows of five. The first snapshot lands without
	 * motion, the highlight on the selection's column. */
	UIGameflow_Init(&state);
	UIGameflow_SetColumns(&state, 5u);
	apply(&state, 1u, 12u, 3u, UI_GAMEFLOW_DIRECTION_NONE, UI_MOTION_FULL);
	frame = UIGameflow_Frame(&state);
	CHECK(nearlyEqual(frame->columnPosition, 3.0f));
	CHECK(nearlyEqual(frame->carouselTravel, 0.0f));
	CHECK(!frame->transitioning);

	/* Along a row the rows stay and the highlight slides. */
	apply(&state, 2u, 12u, 4u, UI_GAMEFLOW_DIRECTION_NEXT, UI_MOTION_FULL);
	frame = UIGameflow_Frame(&state);
	CHECK(nearlyEqual(frame->carouselTravel, 0.0f));
	CHECK(nearlyEqual(frame->columnPosition, 3.0f));
	CHECK(frame->transitioning);
	UIGameflow_Update(&state, 1.0f / 60.0f, UI_MOTION_FULL);
	column = UIGameflow_Frame(&state)->columnPosition;
	CHECK(column > 3.0f && column < 4.0f);
	for(i = 0; i < 240; ++i) {
		UIGameflow_Update(&state, 1.0f / 60.0f, UI_MOTION_FULL);
	}
	frame = UIGameflow_Frame(&state);
	CHECK(nearlyEqual(frame->columnPosition, 4.0f));
	CHECK(!frame->transitioning);

	/* Right from the end of a row: one row down, the highlight to the
	 * row's start. */
	apply(&state, 3u, 12u, 5u, UI_GAMEFLOW_DIRECTION_NEXT, UI_MOTION_FULL);
	frame = UIGameflow_Frame(&state);
	CHECK(nearlyEqual(frame->carouselTravel, 1.0f));
	CHECK(frame->direction == UI_GAMEFLOW_DIRECTION_NEXT);
	CHECK(nearlyEqual(frame->columnPosition, 4.0f));
	CHECK(nearlyEqual(state.columnSpring.target, 0.0f));

	/* Down from the last row to the first is one more row down. */
	apply(&state, 4u, 12u, 10u, UI_GAMEFLOW_DIRECTION_NEXT, UI_MOTION_OFF);
	apply(&state, 5u, 12u, 0u, UI_GAMEFLOW_DIRECTION_NEXT, UI_MOTION_FULL);
	frame = UIGameflow_Frame(&state);
	CHECK(nearlyEqual(frame->carouselTravel, 1.0f));
	CHECK(nearlyEqual(frame->columnPosition, 0.0f));
	/* And up from the first row to the last is one row up. */
	apply(&state, 6u, 12u, 11u, UI_GAMEFLOW_DIRECTION_PREVIOUS,
		UI_MOTION_OFF);
	frame = UIGameflow_Frame(&state);
	CHECK(nearlyEqual(frame->carouselTravel, 0.0f));
	CHECK(nearlyEqual(frame->columnPosition, 1.0f));
	CHECK(!frame->transitioning);
	apply(&state, 7u, 12u, 1u, UI_GAMEFLOW_DIRECTION_NEXT, UI_MOTION_FULL);
	apply(&state, 8u, 12u, 11u, UI_GAMEFLOW_DIRECTION_PREVIOUS,
		UI_MOTION_FULL);
	frame = UIGameflow_Frame(&state);
	/* The second press reverses the first: no travel left. */
	CHECK(nearlyEqual(frame->carouselTravel, 0.0f));

	/* Two rows are resolved by the press's direction. */
	UIGameflow_Init(&state);
	UIGameflow_SetColumns(&state, 5u);
	apply(&state, 1u, 8u, 6u, UI_GAMEFLOW_DIRECTION_NONE, UI_MOTION_OFF);
	apply(&state, 2u, 8u, 1u, UI_GAMEFLOW_DIRECTION_PREVIOUS,
		UI_MOTION_FULL);
	CHECK(nearlyEqual(UIGameflow_Frame(&state)->carouselTravel, -1.0f));
	UIGameflow_Init(&state);
	UIGameflow_SetColumns(&state, 5u);
	apply(&state, 1u, 8u, 6u, UI_GAMEFLOW_DIRECTION_NONE, UI_MOTION_OFF);
	apply(&state, 2u, 8u, 1u, UI_GAMEFLOW_DIRECTION_NEXT, UI_MOTION_FULL);
	CHECK(nearlyEqual(UIGameflow_Frame(&state)->carouselTravel, 1.0f));

	/* Reduced drops the highlight's momentum; a page snaps everything. */
	UIGameflow_Init(&state);
	UIGameflow_SetColumns(&state, 5u);
	apply(&state, 1u, 40u, 0u, UI_GAMEFLOW_DIRECTION_NONE, UI_MOTION_OFF);
	apply(&state, 2u, 40u, 1u, UI_GAMEFLOW_DIRECTION_NEXT, UI_MOTION_FULL);
	UIGameflow_Update(&state, 1.0f / 60.0f, UI_MOTION_FULL);
	CHECK(state.columnSpring.velocity > 0.0f);
	apply(&state, 3u, 40u, 2u, UI_GAMEFLOW_DIRECTION_NEXT,
		UI_MOTION_REDUCED);
	CHECK(nearlyEqual(state.columnSpring.velocity, 0.0f));
	value = snapshot(4u, 40u, 17u, UI_GAMEFLOW_DIRECTION_NEXT);
	value.snapTransition = true;
	CHECK(UIGameflow_ApplySnapshot(&state, &value, UI_MOTION_FULL));
	frame = UIGameflow_Frame(&state);
	CHECK(nearlyEqual(frame->carouselTravel, 0.0f));
	CHECK(nearlyEqual(frame->columnPosition, 2.0f));
	CHECK(!frame->transitioning);
	/* Off settles at once. */
	apply(&state, 5u, 40u, 23u, UI_GAMEFLOW_DIRECTION_NEXT, UI_MOTION_OFF);
	frame = UIGameflow_Frame(&state);
	CHECK(nearlyEqual(frame->carouselTravel, 0.0f));
	CHECK(nearlyEqual(frame->columnPosition, 3.0f));
	CHECK(!frame->transitioning);
	/* Detail holds the card and its column; Library gives it back. */
	UIGameflow_SetMode(&state, UI_GAMEFLOW_MODE_DETAIL, UI_MOTION_OFF);
	apply(&state, 6u, 40u, 24u, UI_GAMEFLOW_DIRECTION_NEXT, UI_MOTION_OFF);
	frame = UIGameflow_Frame(&state);
	CHECK(frame->focusIndex == 23u);
	CHECK(nearlyEqual(frame->columnPosition, 3.0f));
	UIGameflow_SetMode(&state, UI_GAMEFLOW_MODE_LIBRARY, UI_MOTION_OFF);
	CHECK(UIGameflow_Frame(&state)->selectedIndex == 23u);

	/* Off stops a sliding highlight at once, even through a mode change. */
	UIGameflow_Init(&state);
	UIGameflow_SetColumns(&state, 5u);
	apply(&state, 1u, 40u, 0u, UI_GAMEFLOW_DIRECTION_NONE, UI_MOTION_OFF);
	apply(&state, 2u, 40u, 3u, UI_GAMEFLOW_DIRECTION_NEXT, UI_MOTION_FULL);
	CHECK(UIGameflow_Frame(&state)->transitioning);
	UIGameflow_SetMode(&state, UI_GAMEFLOW_MODE_DETAIL, UI_MOTION_OFF);
	frame = UIGameflow_Frame(&state);
	CHECK(nearlyEqual(frame->columnPosition, 3.0f));
	CHECK(!frame->transitioning);

	/* On a ring the highlight never moves. */
	UIGameflow_Init(&state);
	apply(&state, 1u, 12u, 3u, UI_GAMEFLOW_DIRECTION_NONE, UI_MOTION_OFF);
	apply(&state, 2u, 12u, 9u, UI_GAMEFLOW_DIRECTION_NEXT, UI_MOTION_FULL);
	CHECK(nearlyEqual(UIGameflow_Frame(&state)->columnPosition, 0.0f));
	CHECK(nearlyEqual(UIGameflow_Frame(&state)->carouselTravel, 1.0f));
	UIGameflow_SetColumns(NULL, 5u);
}

int main(void)
{
	testGenerationAndSanitization();
	testWrapAwareDirection();
	testMotionModesAndInterruption();
	testPageJumpSnapsWithoutFalseNeighbor();
	testBoundedPhaseRefreshAndImmediateOff();
	testPinnedDetailLaunchSelection();
	testGridRowsAndHighlight();
	puts("ui_gameflow state tests passed");
	return EXIT_SUCCESS;
}
