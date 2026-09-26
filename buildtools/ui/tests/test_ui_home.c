/*
 * Host test command (run from the repository root):
 * cc -std=c11 -Wall -Wextra -Werror -Wconversion -Wsign-conversion \
 *   -pedantic -Icube/swiss/source/gui \
 *   buildtools/ui/tests/test_ui_home.c cube/swiss/source/gui/ui_home.c \
 *   -o /tmp/test_ui_home && /tmp/test_ui_home
 *
 * In addition to authored transition examples, this suite carries an
 * independent contract oracle over every valid face, surface, input and
 * capability combination.  It also brute-forces all root input sequences up
 * to the first safe Restart depth.
 */

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_home.h"

static unsigned int checks;

#define CHECK(condition) do { \
	++checks; \
	if(!(condition)) { \
		fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, \
			#condition); \
		exit(EXIT_FAILURE); \
	} \
} while(0)

#define CHECK_TEXT(actual, expected) do { \
	const char *const checkedActual = (actual); \
	const char *const checkedExpected = (expected); \
	CHECK(checkedActual != NULL); \
	CHECK(checkedExpected != NULL); \
	CHECK(strcmp(checkedActual, checkedExpected) == 0); \
} while(0)

static uiHomeCapabilities_t capabilities(bool hasSource, bool hasRecent)
{
	uiHomeCapabilities_t value;

	value.hasSource = hasSource;
	value.hasRecent = hasRecent;
	return value;
}

static uiHomeState_t stateAt(uiHomeFace_t face, uiHomeSurface_t surface,
	int selection, int32_t turnOrdinal)
{
	uiHomeState_t state;

	UIHome_Init(&state, capabilities(true, false));
	state.face = face;
	state.surface = surface;
	state.selection = selection;
	state.turnOrdinal = turnOrdinal;
	state.revision = 41u;
	return state;
}

static void checkState(const uiHomeState_t *state, uiHomeFace_t face,
	uiHomeSurface_t surface, int selection, int32_t turnOrdinal,
	uint32_t revision)
{
	CHECK(state != NULL);
	CHECK(state->face == face);
	CHECK(state->surface == surface);
	CHECK(state->selection == selection);
	CHECK(state->turnOrdinal == turnOrdinal);
	CHECK(state->revision == revision);
}

static void checkSameState(const uiHomeState_t *actual,
	const uiHomeState_t *expected)
{
	CHECK(actual != NULL);
	CHECK(expected != NULL);
	CHECK(actual->face == expected->face);
	CHECK(actual->surface == expected->surface);
	CHECK(actual->selection == expected->selection);
	CHECK(actual->turnOrdinal == expected->turnOrdinal);
	CHECK(actual->revision == expected->revision);
	CHECK(actual->turnAxis == expected->turnAxis);
	CHECK(actual->turnDirection == expected->turnDirection);
	CHECK(memcmp(&actual->orientation, &expected->orientation,
		sizeof(actual->orientation)) == 0);
}

static void testValidationAndInitialization(void)
{
	uiHomeState_t state;
	uiHomeState_t before;
	uiHomeCapabilities_t withSource = capabilities(true, true);
	uiHomeCapabilities_t withoutSource = capabilities(false, false);
	int value;

	for(value = 0; value < (int)UI_HOME_FACE_COUNT; ++value) {
		CHECK(UIHome_IsFace(value));
	}
	CHECK(!UIHome_IsFace(-1));
	CHECK(!UIHome_IsFace((int)UI_HOME_FACE_COUNT));
	CHECK(!UIHome_IsFace(INT_MAX));

	for(value = 0; value < (int)UI_HOME_SURFACE_COUNT; ++value) {
		CHECK(UIHome_IsSurface(value));
	}
	CHECK(!UIHome_IsSurface(-1));
	CHECK(!UIHome_IsSurface((int)UI_HOME_SURFACE_COUNT));
	CHECK(!UIHome_IsSurface(INT_MAX));

	UIHome_Init(&state, withSource);
	checkState(&state, UI_HOME_FACE_LIBRARY, UI_HOME_SURFACE_RING, 0,
		0, 1u);
	UIHome_Init(&state, withoutSource);
	checkState(&state, UI_HOME_FACE_SOURCE, UI_HOME_SURFACE_RING, 0,
		1, 1u);
	UIHome_Init(NULL, withSource);
	CHECK(UIHome_Apply(NULL, UI_HOME_INPUT_ACTIVATE, withSource) ==
		UI_HOME_EFFECT_NONE);

	state = stateAt((uiHomeFace_t)-1, UI_HOME_SURFACE_RING, 0, 0);
	before = state;
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_RIGHT, withSource) ==
		UI_HOME_EFFECT_NONE);
	checkSameState(&state, &before);
	state = stateAt(UI_HOME_FACE_LIBRARY, (uiHomeSurface_t)-1, 0, 0);
	before = state;
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_RIGHT, withSource) ==
		UI_HOME_EFFECT_NONE);
	checkSameState(&state, &before);
	state = stateAt(UI_HOME_FACE_LIBRARY, UI_HOME_SURFACE_RING, 0, 0);
	before = state;
	CHECK(UIHome_Apply(&state, (uiHomeInput_t)99, withSource) ==
		UI_HOME_EFFECT_NONE);
	checkSameState(&state, &before);

	state = stateAt(UI_HOME_FACE_SOURCE, UI_HOME_SURFACE_SOURCE, 77, 1);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_NONE, withSource) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_SOURCE, UI_HOME_SURFACE_SOURCE, 0,
		1, 41u);
	state = stateAt(UI_HOME_FACE_SOURCE, UI_HOME_SURFACE_SOURCE, 1, 1);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_NONE, withoutSource) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_SOURCE, UI_HOME_SURFACE_SOURCE, 0,
		1, 41u);
}

static void testLabelsHintsAndRows(void)
{
	uiHomeCapabilities_t withSource = capabilities(true, true);
	uiHomeCapabilities_t withoutSource = capabilities(false, false);

	CHECK_TEXT(UIHome_FaceLabel(UI_HOME_FACE_LIBRARY), "LIBRARY");
	CHECK_TEXT(UIHome_FaceLabel(UI_HOME_FACE_SOURCE), "SOURCE");
	CHECK_TEXT(UIHome_FaceLabel(UI_HOME_FACE_SETTINGS), "SETTINGS");
	CHECK_TEXT(UIHome_FaceLabel(UI_HOME_FACE_SYSTEM), "SYSTEM");
	CHECK_TEXT(UIHome_FaceLabel((uiHomeFace_t)-1), "");
	CHECK_TEXT(UIHome_FaceLabel(UI_HOME_FACE_COUNT), "");

	CHECK_TEXT(UIHome_PrimaryHint(UI_HOME_FACE_LIBRARY, withSource),
		"A  OPEN");
	CHECK_TEXT(UIHome_PrimaryHint(UI_HOME_FACE_LIBRARY, withoutSource),
		"A  SELECT SOURCE");
	CHECK_TEXT(UIHome_PrimaryHint(UI_HOME_FACE_SOURCE, withSource),
		"A  ENTER");
	CHECK_TEXT(UIHome_PrimaryHint(UI_HOME_FACE_SOURCE, withoutSource),
		"A  ENTER");
	CHECK_TEXT(UIHome_PrimaryHint(UI_HOME_FACE_SETTINGS, withSource),
		"A  OPEN");
	CHECK_TEXT(UIHome_PrimaryHint(UI_HOME_FACE_SYSTEM, withSource),
		"A  ENTER");
	CHECK_TEXT(UIHome_PrimaryHint((uiHomeFace_t)-1, withSource), "");

	CHECK_TEXT(UIHome_SurfaceTitle(UI_HOME_SURFACE_RING), "HOME");
	CHECK_TEXT(UIHome_SurfaceTitle(UI_HOME_SURFACE_SOURCE), "SOURCE");
	CHECK_TEXT(UIHome_SurfaceTitle(UI_HOME_SURFACE_SYSTEM), "SYSTEM");
	CHECK_TEXT(UIHome_SurfaceTitle(UI_HOME_SURFACE_RESTART_CONFIRM),
		"RESTART INDIGO?");
	CHECK_TEXT(UIHome_SurfaceTitle((uiHomeSurface_t)-1), "HOME");

	CHECK(UIHome_RowCount(UI_HOME_SURFACE_RING, withSource) == 0);
	CHECK(UIHome_RowCount(UI_HOME_SURFACE_SOURCE, withSource) == 2);
	CHECK(UIHome_RowCount(UI_HOME_SURFACE_SOURCE, withoutSource) == 1);
	CHECK(UIHome_RowCount(UI_HOME_SURFACE_SYSTEM, withSource) == 3);
	CHECK(UIHome_RowCount(UI_HOME_SURFACE_SYSTEM, withoutSource) == 3);
	CHECK(UIHome_RowCount(UI_HOME_SURFACE_RESTART_CONFIRM, withSource) == 2);
	CHECK(UIHome_RowCount((uiHomeSurface_t)-1, withSource) == 0);

	CHECK(!UIHome_RowEnabled(UI_HOME_SURFACE_SOURCE, -1, withSource));
	CHECK(UIHome_RowEnabled(UI_HOME_SURFACE_SOURCE, 0, withSource));
	CHECK(UIHome_RowEnabled(UI_HOME_SURFACE_SOURCE, 1, withSource));
	CHECK(!UIHome_RowEnabled(UI_HOME_SURFACE_SOURCE, 2, withSource));
	CHECK(UIHome_RowEnabled(UI_HOME_SURFACE_SOURCE, 0, withoutSource));
	CHECK(!UIHome_RowEnabled(UI_HOME_SURFACE_SOURCE, 1, withoutSource));
	CHECK(UIHome_RowEnabled(UI_HOME_SURFACE_SYSTEM, 0, withoutSource));
	CHECK(UIHome_RowEnabled(UI_HOME_SURFACE_SYSTEM, 1, withoutSource));
	CHECK(UIHome_RowEnabled(UI_HOME_SURFACE_SYSTEM, 2, withoutSource));
	CHECK(!UIHome_RowEnabled(UI_HOME_SURFACE_SYSTEM, 3, withoutSource));
	CHECK(UIHome_RowEnabled(UI_HOME_SURFACE_RESTART_CONFIRM, 0,
		withoutSource));
	CHECK(UIHome_RowEnabled(UI_HOME_SURFACE_RESTART_CONFIRM, 1,
		withoutSource));
	CHECK(!UIHome_RowEnabled(UI_HOME_SURFACE_RING, 0, withSource));

	CHECK_TEXT(UIHome_RowLabel(UI_HOME_SURFACE_SOURCE, 0),
		"CHANGE SOURCE");
	CHECK_TEXT(UIHome_RowLabel(UI_HOME_SURFACE_SOURCE, 1),
		"REFRESH LIBRARY");
	CHECK_TEXT(UIHome_RowLabel(UI_HOME_SURFACE_SYSTEM, 0),
		"SYSTEM INFORMATION");
	CHECK_TEXT(UIHome_RowLabel(UI_HOME_SURFACE_SYSTEM, 1),
		"MEMORY CARDS");
	CHECK_TEXT(UIHome_RowLabel(UI_HOME_SURFACE_SYSTEM, 2),
		"RESTART INDIGO");
	CHECK_TEXT(UIHome_RowLabel(UI_HOME_SURFACE_SYSTEM, 3), "");
	CHECK_TEXT(UIHome_RowLabel(UI_HOME_SURFACE_SYSTEM, -1), "");
	CHECK_TEXT(UIHome_RowLabel(UI_HOME_SURFACE_RESTART_CONFIRM, 0),
		"CANCEL");
	CHECK_TEXT(UIHome_RowLabel(UI_HOME_SURFACE_RESTART_CONFIRM, 1),
		"RESTART");
	CHECK_TEXT(UIHome_RowLabel(UI_HOME_SURFACE_RING, 0), "");
	CHECK_TEXT(UIHome_RowLabel(UI_HOME_SURFACE_SOURCE, -1), "");
	CHECK_TEXT(UIHome_RowLabel(UI_HOME_SURFACE_SOURCE, 2), "");
}

static void testFaceMappingAndSignedTurns(void)
{
	static const uiHomeFace_t aroundZero[] = {
		UI_HOME_FACE_SYSTEM,
		UI_HOME_FACE_LIBRARY,
		UI_HOME_FACE_SOURCE,
		UI_HOME_FACE_SETTINGS,
		UI_HOME_FACE_SYSTEM,
		UI_HOME_FACE_LIBRARY,
		UI_HOME_FACE_SOURCE
	};
	uiHomeCapabilities_t caps = capabilities(true, false);
	uiHomeState_t right;
	uiHomeState_t left;
	int index;

	for(index = -1; index <= 5; ++index) {
		CHECK(UIHome_FaceForTurn((int32_t)index) == aroundZero[index + 1]);
	}
	CHECK(UIHome_FaceForTurn(INT32_MIN) == UI_HOME_FACE_LIBRARY);
	CHECK(UIHome_FaceForTurn(INT32_MAX) == UI_HOME_FACE_SYSTEM);

	UIHome_Init(&right, caps);
	CHECK(UIHome_Apply(&right, UI_HOME_INPUT_RIGHT, caps) ==
		UI_HOME_EFFECT_NONE);
	CHECK(UIHome_Apply(&right, UI_HOME_INPUT_RIGHT, caps) ==
		UI_HOME_EFFECT_NONE);
	checkState(&right, UI_HOME_FACE_SETTINGS, UI_HOME_SURFACE_RING, 0,
		2, 3u);

	UIHome_Init(&left, caps);
	CHECK(UIHome_Apply(&left, UI_HOME_INPUT_LEFT, caps) ==
		UI_HOME_EFFECT_NONE);
	CHECK(UIHome_Apply(&left, UI_HOME_INPUT_LEFT, caps) ==
		UI_HOME_EFFECT_NONE);
	checkState(&left, UI_HOME_FACE_SETTINGS, UI_HOME_SURFACE_RING, 0,
		-2, 3u);
	CHECK(right.face == left.face);
	CHECK(right.turnOrdinal != left.turnOrdinal);

	UIHome_Init(&right, caps);
	UIHome_Init(&left, caps);
	for(index = 0; index < (int)UI_HOME_FACE_COUNT; ++index) {
		CHECK(UIHome_Apply(&right, UI_HOME_INPUT_RIGHT, caps) ==
			UI_HOME_EFFECT_NONE);
		CHECK(UIHome_Apply(&left, UI_HOME_INPUT_LEFT, caps) ==
			UI_HOME_EFFECT_NONE);
	}
	checkState(&right, UI_HOME_FACE_LIBRARY, UI_HOME_SURFACE_RING, 0,
		4, 5u);
	checkState(&left, UI_HOME_FACE_LIBRARY, UI_HOME_SURFACE_RING, 0,
		-4, 5u);
}

static void testEveryRingFaceAndInput(void)
{
	uiHomeCapabilities_t caps = capabilities(true, true);
	int faceIndex;
	int inputIndex;

	for(faceIndex = 0; faceIndex < (int)UI_HOME_FACE_COUNT; ++faceIndex) {
		uiHomeFace_t face = (uiHomeFace_t)faceIndex;
		for(inputIndex = (int)UI_HOME_INPUT_NONE;
			inputIndex <= (int)UI_HOME_INPUT_RECENT; ++inputIndex) {
			uiHomeInput_t input = (uiHomeInput_t)inputIndex;
			uiHomeState_t state = stateAt(face, UI_HOME_SURFACE_RING, 0,
				(int32_t)faceIndex);
			uiHomeState_t before = state;
			uiHomeEffect_t effect = UIHome_Apply(&state, input, caps);

			switch(input) {
				case UI_HOME_INPUT_LEFT:
				case UI_HOME_INPUT_UP:
					CHECK(effect == UI_HOME_EFFECT_NONE);
					checkState(&state,
						UIHome_FaceForTurn((int32_t)faceIndex - 1),
						UI_HOME_SURFACE_RING, 0,
						(int32_t)faceIndex - 1, 42u);
					break;
				case UI_HOME_INPUT_RIGHT:
				case UI_HOME_INPUT_DOWN:
					CHECK(effect == UI_HOME_EFFECT_NONE);
					checkState(&state,
						UIHome_FaceForTurn((int32_t)faceIndex + 1),
						UI_HOME_SURFACE_RING, 0,
						(int32_t)faceIndex + 1, 42u);
					break;
				case UI_HOME_INPUT_ACTIVATE:
					if(face == UI_HOME_FACE_LIBRARY) {
						CHECK(effect == UI_HOME_EFFECT_OPEN_LIBRARY);
						checkSameState(&state, &before);
					}
					else if(face == UI_HOME_FACE_SOURCE) {
						CHECK(effect == UI_HOME_EFFECT_NONE);
						checkState(&state, face,
							UI_HOME_SURFACE_SOURCE, 0,
							(int32_t)faceIndex, 42u);
					}
					else if(face == UI_HOME_FACE_SETTINGS) {
						CHECK(effect == UI_HOME_EFFECT_OPEN_SETTINGS);
						checkSameState(&state, &before);
					}
					else {
						CHECK(effect == UI_HOME_EFFECT_NONE);
						checkState(&state, face,
							UI_HOME_SURFACE_SYSTEM, 0,
							(int32_t)faceIndex, 42u);
					}
					break;
				case UI_HOME_INPUT_RECENT:
					CHECK(effect == UI_HOME_EFFECT_OPEN_RECENT);
					checkSameState(&state, &before);
					break;
				case UI_HOME_INPUT_NONE:
				case UI_HOME_INPUT_BACK:
					CHECK(effect == UI_HOME_EFFECT_NONE);
					checkSameState(&state, &before);
					break;
			}
		}

		{
			uiHomeState_t state = stateAt(face, UI_HOME_SURFACE_RING, 0,
				(int32_t)faceIndex);
			uiHomeState_t before = state;
			CHECK(UIHome_Apply(&state, UI_HOME_INPUT_RECENT,
				capabilities(true, false)) == UI_HOME_EFFECT_NONE);
			checkSameState(&state, &before);
		}
	}
}

static void testNoSourceRedirect(void)
{
	uiHomeCapabilities_t caps = capabilities(false, false);
	uiHomeState_t state = stateAt(UI_HOME_FACE_LIBRARY,
		UI_HOME_SURFACE_RING, 0, 0);

	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_ACTIVATE, caps) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_SOURCE, UI_HOME_SURFACE_SOURCE, 0,
		1, 42u);
}

static void testSourceSurface(void)
{
	uiHomeCapabilities_t caps = capabilities(true, true);
	uiHomeState_t state;
	uiHomeState_t before;
	uiHomeEffect_t effect;
	int inputIndex;

	for(inputIndex = (int)UI_HOME_INPUT_NONE;
		inputIndex <= (int)UI_HOME_INPUT_RECENT; ++inputIndex) {
		uiHomeInput_t input = (uiHomeInput_t)inputIndex;
		state = stateAt(UI_HOME_FACE_SOURCE, UI_HOME_SURFACE_SOURCE, 0, 1);
		before = state;
		effect = UIHome_Apply(&state, input, caps);
		switch(input) {
			case UI_HOME_INPUT_UP:
			case UI_HOME_INPUT_DOWN:
				CHECK(effect == UI_HOME_EFFECT_NONE);
				checkState(&state, UI_HOME_FACE_SOURCE,
					UI_HOME_SURFACE_SOURCE, 1, 1, 42u);
				break;
			case UI_HOME_INPUT_ACTIVATE:
				CHECK(effect == UI_HOME_EFFECT_CHANGE_SOURCE);
				checkSameState(&state, &before);
				break;
			case UI_HOME_INPUT_BACK:
				CHECK(effect == UI_HOME_EFFECT_NONE);
				checkState(&state, UI_HOME_FACE_SOURCE,
					UI_HOME_SURFACE_RING, 0, 1, 42u);
				break;
			case UI_HOME_INPUT_NONE:
			case UI_HOME_INPUT_LEFT:
			case UI_HOME_INPUT_RIGHT:
			case UI_HOME_INPUT_RECENT:
				CHECK(effect == UI_HOME_EFFECT_NONE);
				checkSameState(&state, &before);
				break;
		}
	}

	state = stateAt(UI_HOME_FACE_SOURCE, UI_HOME_SURFACE_SOURCE, 1, 1);
	before = state;
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_ACTIVATE, caps) ==
		UI_HOME_EFFECT_REFRESH);
	checkSameState(&state, &before);
	state = stateAt(UI_HOME_FACE_SOURCE, UI_HOME_SURFACE_SOURCE, 1, 1);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_UP, caps) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_SOURCE, UI_HOME_SURFACE_SOURCE, 0,
		1, 42u);
	state = stateAt(UI_HOME_FACE_SOURCE, UI_HOME_SURFACE_SOURCE, 1, 1);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_DOWN, caps) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_SOURCE, UI_HOME_SURFACE_SOURCE, 0,
		1, 42u);

	caps = capabilities(false, true);
	for(inputIndex = (int)UI_HOME_INPUT_LEFT;
		inputIndex <= (int)UI_HOME_INPUT_DOWN; ++inputIndex) {
		state = stateAt(UI_HOME_FACE_SOURCE, UI_HOME_SURFACE_SOURCE, 0, 1);
		before = state;
		CHECK(UIHome_Apply(&state, (uiHomeInput_t)inputIndex, caps) ==
			UI_HOME_EFFECT_NONE);
		checkSameState(&state, &before);
	}
	state = stateAt(UI_HOME_FACE_SOURCE, UI_HOME_SURFACE_SOURCE, 1, 1);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_ACTIVATE, caps) ==
		UI_HOME_EFFECT_CHANGE_SOURCE);
	checkState(&state, UI_HOME_FACE_SOURCE, UI_HOME_SURFACE_SOURCE, 0,
		1, 41u);
}

static void testSystemSurface(void)
{
	uiHomeCapabilities_t caps = capabilities(true, true);
	uiHomeState_t state;
	uiHomeState_t before;
	uiHomeEffect_t effect;
	int inputIndex;

	for(inputIndex = (int)UI_HOME_INPUT_NONE;
		inputIndex <= (int)UI_HOME_INPUT_RECENT; ++inputIndex) {
		uiHomeInput_t input = (uiHomeInput_t)inputIndex;
		state = stateAt(UI_HOME_FACE_SYSTEM, UI_HOME_SURFACE_SYSTEM, 0, 3);
		before = state;
		effect = UIHome_Apply(&state, input, caps);
		switch(input) {
			case UI_HOME_INPUT_UP:
				CHECK(effect == UI_HOME_EFFECT_NONE);
				checkState(&state, UI_HOME_FACE_SYSTEM,
					UI_HOME_SURFACE_SYSTEM, 2, 3, 42u);
				break;
			case UI_HOME_INPUT_DOWN:
				CHECK(effect == UI_HOME_EFFECT_NONE);
				checkState(&state, UI_HOME_FACE_SYSTEM,
					UI_HOME_SURFACE_SYSTEM, 1, 3, 42u);
				break;
			case UI_HOME_INPUT_ACTIVATE:
				CHECK(effect == UI_HOME_EFFECT_OPEN_INFO);
				checkSameState(&state, &before);
				break;
			case UI_HOME_INPUT_BACK:
				CHECK(effect == UI_HOME_EFFECT_NONE);
				checkState(&state, UI_HOME_FACE_SYSTEM,
					UI_HOME_SURFACE_RING, 0, 3, 42u);
				break;
			case UI_HOME_INPUT_NONE:
			case UI_HOME_INPUT_LEFT:
			case UI_HOME_INPUT_RIGHT:
			case UI_HOME_INPUT_RECENT:
				CHECK(effect == UI_HOME_EFFECT_NONE);
				checkSameState(&state, &before);
				break;
		}
	}

	/* Memory Cards opens its screen and keeps the row. */
	state = stateAt(UI_HOME_FACE_SYSTEM, UI_HOME_SURFACE_SYSTEM, 1, 3);
	before = state;
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_ACTIVATE, caps) ==
		UI_HOME_EFFECT_OPEN_SAVES);
	checkSameState(&state, &before);
	state = stateAt(UI_HOME_FACE_SYSTEM, UI_HOME_SURFACE_SYSTEM, 2, 3);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_ACTIVATE, caps) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_SYSTEM,
		UI_HOME_SURFACE_RESTART_CONFIRM, 0, 3, 42u);
	state = stateAt(UI_HOME_FACE_SYSTEM, UI_HOME_SURFACE_SYSTEM, 1, 3);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_UP, caps) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_SYSTEM, UI_HOME_SURFACE_SYSTEM, 0,
		3, 42u);
	state = stateAt(UI_HOME_FACE_SYSTEM, UI_HOME_SURFACE_SYSTEM, 1, 3);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_DOWN, caps) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_SYSTEM, UI_HOME_SURFACE_SYSTEM, 2,
		3, 42u);
	state = stateAt(UI_HOME_FACE_SYSTEM, UI_HOME_SURFACE_SYSTEM, 2, 3);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_DOWN, caps) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_SYSTEM, UI_HOME_SURFACE_SYSTEM, 0,
		3, 42u);
}

static void testRestartConfirmation(void)
{
	uiHomeCapabilities_t caps = capabilities(true, true);
	uiHomeState_t state;
	uiHomeState_t before;
	uiHomeEffect_t effect;
	int inputIndex;

	for(inputIndex = (int)UI_HOME_INPUT_NONE;
		inputIndex <= (int)UI_HOME_INPUT_RECENT; ++inputIndex) {
		uiHomeInput_t input = (uiHomeInput_t)inputIndex;
		state = stateAt(UI_HOME_FACE_SYSTEM,
			UI_HOME_SURFACE_RESTART_CONFIRM, 0, 3);
		before = state;
		effect = UIHome_Apply(&state, input, caps);
		switch(input) {
			case UI_HOME_INPUT_LEFT:
			case UI_HOME_INPUT_RIGHT:
			case UI_HOME_INPUT_UP:
			case UI_HOME_INPUT_DOWN:
				CHECK(effect == UI_HOME_EFFECT_NONE);
				checkState(&state, UI_HOME_FACE_SYSTEM,
					UI_HOME_SURFACE_RESTART_CONFIRM, 1, 3, 42u);
				break;
			case UI_HOME_INPUT_ACTIVATE:
				CHECK(effect == UI_HOME_EFFECT_NONE);
				checkState(&state, UI_HOME_FACE_SYSTEM,
					UI_HOME_SURFACE_SYSTEM, 2, 3, 42u);
				break;
			case UI_HOME_INPUT_BACK:
				CHECK(effect == UI_HOME_EFFECT_NONE);
				checkState(&state, UI_HOME_FACE_SYSTEM,
					UI_HOME_SURFACE_SYSTEM, 2, 3, 42u);
				break;
			case UI_HOME_INPUT_NONE:
			case UI_HOME_INPUT_RECENT:
				CHECK(effect == UI_HOME_EFFECT_NONE);
				checkSameState(&state, &before);
				break;
		}
	}

	/* The A press that opens confirmation cannot also confirm Restart. */
	state = stateAt(UI_HOME_FACE_SYSTEM, UI_HOME_SURFACE_SYSTEM, 2, 3);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_ACTIVATE, caps) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_SYSTEM,
		UI_HOME_SURFACE_RESTART_CONFIRM, 0, 3, 42u);

	/* A fresh press on the safe default cancels instead of restarting. */
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_ACTIVATE, caps) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_SYSTEM, UI_HOME_SURFACE_SYSTEM, 2,
		3, 43u);

	/* Re-enter, deliberately select Restart, then require another fresh A. */
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_ACTIVATE, caps) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_SYSTEM,
		UI_HOME_SURFACE_RESTART_CONFIRM, 0, 3, 44u);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_RIGHT, caps) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_SYSTEM,
		UI_HOME_SURFACE_RESTART_CONFIRM, 1, 3, 45u);
	before = state;
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_ACTIVATE, caps) ==
		UI_HOME_EFFECT_RESTART);
	checkSameState(&state, &before);

	state = stateAt(UI_HOME_FACE_SYSTEM,
		UI_HOME_SURFACE_RESTART_CONFIRM, 1, 3);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_BACK, caps) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_SYSTEM, UI_HOME_SURFACE_SYSTEM, 2,
		3, 42u);
}

static void testLongRunOrdinalAndRevision(void)
{
	uiHomeCapabilities_t caps = capabilities(true, false);
	uiHomeState_t state;
	int index;

	UIHome_Init(&state, caps);
	for(index = 0; index < 40000; ++index) {
		CHECK(UIHome_Apply(&state, UI_HOME_INPUT_RIGHT, caps) ==
			UI_HOME_EFFECT_NONE);
		CHECK(state.face == UIHome_FaceForTurn(state.turnOrdinal));
	}
	checkState(&state, UI_HOME_FACE_LIBRARY, UI_HOME_SURFACE_RING, 0,
		40000, 40001u);

	for(index = 0; index < 80003; ++index) {
		CHECK(UIHome_Apply(&state, UI_HOME_INPUT_LEFT, caps) ==
			UI_HOME_EFFECT_NONE);
		CHECK(state.face == UIHome_FaceForTurn(state.turnOrdinal));
	}
	checkState(&state, UI_HOME_FACE_SOURCE, UI_HOME_SURFACE_RING, 0,
		-40003, 120004u);

	state = stateAt(UI_HOME_FACE_SYSTEM, UI_HOME_SURFACE_RING, 0,
		INT32_MAX);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_LEFT, caps) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_SETTINGS, UI_HOME_SURFACE_RING, 0,
		INT32_MAX - 1, 42u);
	state = stateAt(UI_HOME_FACE_LIBRARY, UI_HOME_SURFACE_RING, 0,
		INT32_MIN);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_RIGHT, caps) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_SOURCE, UI_HOME_SURFACE_RING, 0,
		INT32_MIN + 1, 42u);

	state = stateAt(UI_HOME_FACE_LIBRARY, UI_HOME_SURFACE_RING, 0, 0);
	state.revision = UINT32_MAX;
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_RIGHT, caps) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_SOURCE, UI_HOME_SURFACE_RING, 0,
		1, 0u);
}

/* ------------------------------------------------------------------------- */
/* Independent reducer oracle.                                               */
/* ------------------------------------------------------------------------- */

static int oraclePositiveModulo(int value, int modulus)
{
	int result = value % modulus;

	return result < 0 ? result + modulus : result;
}

static int oracleRowCount(uiHomeSurface_t surface,
	uiHomeCapabilities_t caps)
{
	if(surface == UI_HOME_SURFACE_SOURCE) {
		return caps.hasSource ? 2 : 1;
	}
	if(surface == UI_HOME_SURFACE_SYSTEM) {
		return 3;
	}
	if(surface == UI_HOME_SURFACE_RESTART_CONFIRM) {
		return 2;
	}
	return 0;
}

/* Capability reconciliation is intentionally publication-only in the current
 * API: it repairs a stale selection but does not create an input revision.
 * homePublish() publishes the reconciled snapshot unconditionally. */
static void oracleNormalizeSelection(uiHomeState_t *state,
	uiHomeCapabilities_t caps)
{
	int rowCount = oracleRowCount(state->surface, caps);

	if(rowCount <= 0 || state->selection < 0 ||
			state->selection >= rowCount) {
		state->selection = 0;
	}
}

static void oracleEnterSurface(uiHomeState_t *state,
	uiHomeSurface_t surface, int selection)
{
	state->surface = surface;
	state->selection = selection;
	state->revision++;
}

static void oracleMoveFace(uiHomeState_t *state, uiHomeTurnAxis_t axis, int direction)
{
	int32_t step = direction < 0 ? INT32_C(-1) : INT32_C(1);

	/* Independent full matrix multiplication oracle. */
	int rotation[3][3] = {{1,0,0},{0,1,0},{0,0,1}};
	uiHomeOrientation_t before = state->orientation;
	if(axis == UI_HOME_TURN_HORIZONTAL) {
		rotation[0][0] = rotation[2][2] = 0;
		rotation[0][2] = -direction; rotation[2][0] = direction;
	}
	else {
		rotation[1][1] = rotation[2][2] = 0;
		rotation[1][2] = direction; rotation[2][1] = -direction;
	}
	for(int r = 0; r < 3; ++r) for(int c = 0; c < 3; ++c) {
		int sum = 0;
		for(int k = 0; k < 3; ++k) sum += rotation[r][k] * before.m[k][c];
		state->orientation.m[r][c] = (int8_t)sum;
	}
	state->turnAxis = axis;
	state->turnDirection = direction;
	if((state->turnOrdinal == INT32_MAX && step > 0) ||
		(state->turnOrdinal == INT32_MIN && step < 0))
		state->turnOrdinal %= 4;
	state->turnOrdinal += step;
	state->face = (uiHomeFace_t)oraclePositiveModulo(
		(int)(state->turnOrdinal % (int32_t)UI_HOME_FACE_COUNT),
		(int)UI_HOME_FACE_COUNT);
	state->surface = UI_HOME_SURFACE_RING;
	state->selection = 0;
	state->revision++;
}

static void oracleMoveRow(uiHomeState_t *state, int direction,
	uiHomeCapabilities_t caps)
{
	int rowCount = oracleRowCount(state->surface, caps);
	int next;

	if(rowCount <= 1) {
		state->selection = 0;
		return;
	}
	next = oraclePositiveModulo(state->selection +
		(direction < 0 ? -1 : 1), rowCount);
	if(next != state->selection) {
		state->selection = next;
		state->revision++;
	}
}

/* This oracle is written from the interaction contract rather than sharing
 * reducer helpers.  It predicts both the complete next state and exact effect
 * for one discrete input sample. */
static uiHomeEffect_t oracleApply(uiHomeState_t *state, uiHomeInput_t input,
	uiHomeCapabilities_t caps)
{
	if(state == NULL || !UIHome_IsFace((int)state->face) ||
			!UIHome_IsSurface((int)state->surface)) {
		return UI_HOME_EFFECT_NONE;
	}
	oracleNormalizeSelection(state, caps);

	if(state->surface == UI_HOME_SURFACE_RING) {
		if(input == UI_HOME_INPUT_LEFT || input == UI_HOME_INPUT_UP) {
			oracleMoveFace(state, input == UI_HOME_INPUT_UP ?
				UI_HOME_TURN_VERTICAL : UI_HOME_TURN_HORIZONTAL, -1);
		}
		else if(input == UI_HOME_INPUT_RIGHT || input == UI_HOME_INPUT_DOWN) {
			oracleMoveFace(state, input == UI_HOME_INPUT_DOWN ?
				UI_HOME_TURN_VERTICAL : UI_HOME_TURN_HORIZONTAL, 1);
		}
		else if(input == UI_HOME_INPUT_RECENT) {
			return caps.hasRecent ? UI_HOME_EFFECT_OPEN_RECENT :
				UI_HOME_EFFECT_NONE;
		}
		else if(input == UI_HOME_INPUT_ACTIVATE) {
			if(state->face == UI_HOME_FACE_LIBRARY) {
				if(caps.hasSource) {
					return UI_HOME_EFFECT_OPEN_LIBRARY;
				}
				oracleMoveFace(state, UI_HOME_TURN_HORIZONTAL, 1);
				state->surface = UI_HOME_SURFACE_SOURCE;
			}
			else if(state->face == UI_HOME_FACE_SOURCE) {
				oracleEnterSurface(state, UI_HOME_SURFACE_SOURCE, 0);
			}
			else if(state->face == UI_HOME_FACE_SETTINGS) {
				return UI_HOME_EFFECT_OPEN_SETTINGS;
			}
			else if(state->face == UI_HOME_FACE_SYSTEM) {
				oracleEnterSurface(state, UI_HOME_SURFACE_SYSTEM, 0);
			}
		}
		return UI_HOME_EFFECT_NONE;
	}

	if(state->surface == UI_HOME_SURFACE_SOURCE) {
		if(input == UI_HOME_INPUT_UP) {
			oracleMoveRow(state, -1, caps);
		}
		else if(input == UI_HOME_INPUT_DOWN) {
			oracleMoveRow(state, 1, caps);
		}
		else if(input == UI_HOME_INPUT_BACK) {
			oracleEnterSurface(state, UI_HOME_SURFACE_RING, 0);
		}
		else if(input == UI_HOME_INPUT_ACTIVATE) {
			if(state->selection == 0) {
				return UI_HOME_EFFECT_CHANGE_SOURCE;
			}
			if(state->selection == 1 && caps.hasSource) {
				return UI_HOME_EFFECT_REFRESH;
			}
		}
		return UI_HOME_EFFECT_NONE;
	}

	if(state->surface == UI_HOME_SURFACE_SYSTEM) {
		if(input == UI_HOME_INPUT_UP) {
			oracleMoveRow(state, -1, caps);
		}
		else if(input == UI_HOME_INPUT_DOWN) {
			oracleMoveRow(state, 1, caps);
		}
		else if(input == UI_HOME_INPUT_BACK) {
			oracleEnterSurface(state, UI_HOME_SURFACE_RING, 0);
		}
		else if(input == UI_HOME_INPUT_ACTIVATE) {
			if(state->selection == 0) {
				return UI_HOME_EFFECT_OPEN_INFO;
			}
			if(state->selection == 1) {
				return UI_HOME_EFFECT_OPEN_SAVES;
			}
			if(state->selection == 2) {
				oracleEnterSurface(state,
					UI_HOME_SURFACE_RESTART_CONFIRM, 0);
			}
		}
		return UI_HOME_EFFECT_NONE;
	}

	if(input == UI_HOME_INPUT_LEFT || input == UI_HOME_INPUT_UP) {
		oracleMoveRow(state, -1, caps);
	}
	else if(input == UI_HOME_INPUT_RIGHT || input == UI_HOME_INPUT_DOWN) {
		oracleMoveRow(state, 1, caps);
	}
	else if(input == UI_HOME_INPUT_BACK) {
		oracleEnterSurface(state, UI_HOME_SURFACE_SYSTEM, 2);
	}
	else if(input == UI_HOME_INPUT_ACTIVATE) {
		if(state->selection == 0) {
			oracleEnterSurface(state, UI_HOME_SURFACE_SYSTEM, 2);
		}
		else if(state->selection == 1) {
			return UI_HOME_EFFECT_RESTART;
		}
	}
	return UI_HOME_EFFECT_NONE;
}

static void checkOracleTransition(uiHomeState_t before, uiHomeInput_t input,
	uiHomeCapabilities_t caps)
{
	uiHomeState_t expected = before;
	uiHomeState_t actual = before;
	uiHomeEffect_t expectedEffect = oracleApply(&expected, input, caps);
	uiHomeEffect_t actualEffect = UIHome_Apply(&actual, input, caps);

	CHECK(actualEffect == expectedEffect);
	checkSameState(&actual, &expected);
	CHECK((int)actualEffect >= (int)UI_HOME_EFFECT_NONE);
	CHECK((int)actualEffect <= (int)UI_HOME_EFFECT_OPEN_SAVES);
	if(actualEffect == UI_HOME_EFFECT_RESTART) {
		CHECK(before.surface == UI_HOME_SURFACE_RESTART_CONFIRM);
		CHECK(input == UI_HOME_INPUT_ACTIVATE);
		CHECK(expected.selection == 1);
	}
}

static void testExhaustiveReducerOracle(void)
{
	static const int ordinalCycles[] = { -3, 0, 4 };
	int faceIndex;
	int surfaceIndex;
	int inputIndex;
	int capsMask;
	int cycleIndex;

	for(faceIndex = 0; faceIndex < (int)UI_HOME_FACE_COUNT; ++faceIndex) {
		for(surfaceIndex = 0; surfaceIndex <
				(int)UI_HOME_SURFACE_COUNT; ++surfaceIndex) {
			for(inputIndex = (int)UI_HOME_INPUT_NONE;
					inputIndex <= (int)UI_HOME_INPUT_RECENT; ++inputIndex) {
				for(capsMask = 0; capsMask < 4; ++capsMask) {
					uiHomeCapabilities_t caps = capabilities(
						(capsMask & 1) != 0, (capsMask & 2) != 0);
					int rowCount = oracleRowCount(
						(uiHomeSurface_t)surfaceIndex, caps);
					int selectionCount = rowCount > 0 ? rowCount : 1;
					int selection;

					for(selection = 0; selection < selectionCount;
							++selection) {
						for(cycleIndex = 0; cycleIndex <
								(int)(sizeof(ordinalCycles) /
								sizeof(ordinalCycles[0])); ++cycleIndex) {
							int32_t ordinal = (int32_t)faceIndex +
								(int32_t)(ordinalCycles[cycleIndex] *
								(int)UI_HOME_FACE_COUNT);
							uiHomeState_t state = stateAt(
								(uiHomeFace_t)faceIndex,
								(uiHomeSurface_t)surfaceIndex,
								selection, ordinal);

							checkOracleTransition(state,
								(uiHomeInput_t)inputIndex, caps);
						}
					}
				}
			}
		}
	}
}

static void testInvalidInputAndStateOracle(void)
{
	static const int invalidInputs[] = { -1, 8, 99, INT_MAX };
	static const int invalidFaces[] = { -1, UI_HOME_FACE_COUNT, INT_MAX };
	static const int invalidSurfaces[] = {
		-1, UI_HOME_SURFACE_COUNT, INT_MAX
	};
	static const int invalidSelections[] = { -7, 2, 77, INT_MIN, INT_MAX };
	uiHomeCapabilities_t caps;
	uiHomeState_t state;
	uiHomeState_t before;
	int faceIndex;
	int surfaceIndex;
	int capsMask;
	int inputIndex;
	int valueIndex;

	/* Unknown input codes are inert after the documented capability/selection
	 * reconciliation, including a source disappearing under Refresh. */
	for(faceIndex = 0; faceIndex < (int)UI_HOME_FACE_COUNT; ++faceIndex) {
		for(surfaceIndex = 0; surfaceIndex <
				(int)UI_HOME_SURFACE_COUNT; ++surfaceIndex) {
			for(capsMask = 0; capsMask < 4; ++capsMask) {
				caps = capabilities((capsMask & 1) != 0,
					(capsMask & 2) != 0);
				for(valueIndex = 0; valueIndex <
						(int)(sizeof(invalidInputs) /
						sizeof(invalidInputs[0])); ++valueIndex) {
					state = stateAt((uiHomeFace_t)faceIndex,
						(uiHomeSurface_t)surfaceIndex, 1,
						(int32_t)faceIndex);
					checkOracleTransition(state,
						(uiHomeInput_t)invalidInputs[valueIndex], caps);
				}
			}
		}
	}

	/* Invalid face or surface values fail closed before any normalization. */
	for(valueIndex = 0; valueIndex < (int)(sizeof(invalidFaces) /
			sizeof(invalidFaces[0])); ++valueIndex) {
		for(surfaceIndex = 0; surfaceIndex <
				(int)UI_HOME_SURFACE_COUNT; ++surfaceIndex) {
			for(inputIndex = (int)UI_HOME_INPUT_NONE;
					inputIndex <= (int)UI_HOME_INPUT_RECENT; ++inputIndex) {
				state = stateAt((uiHomeFace_t)invalidFaces[valueIndex],
					(uiHomeSurface_t)surfaceIndex, 77, 0);
				before = state;
				CHECK(UIHome_Apply(&state, (uiHomeInput_t)inputIndex,
					capabilities(true, true)) == UI_HOME_EFFECT_NONE);
				checkSameState(&state, &before);
			}
		}
	}
	for(valueIndex = 0; valueIndex < (int)(sizeof(invalidSurfaces) /
			sizeof(invalidSurfaces[0])); ++valueIndex) {
		for(faceIndex = 0; faceIndex < (int)UI_HOME_FACE_COUNT; ++faceIndex) {
			for(inputIndex = (int)UI_HOME_INPUT_NONE;
					inputIndex <= (int)UI_HOME_INPUT_RECENT; ++inputIndex) {
				state = stateAt((uiHomeFace_t)faceIndex,
					(uiHomeSurface_t)invalidSurfaces[valueIndex], 77,
					(int32_t)faceIndex);
				before = state;
				CHECK(UIHome_Apply(&state, (uiHomeInput_t)inputIndex,
					capabilities(true, true)) == UI_HOME_EFFECT_NONE);
				checkSameState(&state, &before);
			}
		}
	}

	/* Selection corruption is normalized to each surface's safe row before
	 * the input is interpreted; the independent oracle checks every action. */
	for(surfaceIndex = 0; surfaceIndex <
			(int)UI_HOME_SURFACE_COUNT; ++surfaceIndex) {
		for(capsMask = 0; capsMask < 4; ++capsMask) {
			caps = capabilities((capsMask & 1) != 0,
				(capsMask & 2) != 0);
			for(valueIndex = 0; valueIndex <
					(int)(sizeof(invalidSelections) /
					sizeof(invalidSelections[0])); ++valueIndex) {
				for(inputIndex = (int)UI_HOME_INPUT_NONE;
						inputIndex <= (int)UI_HOME_INPUT_RECENT;
						++inputIndex) {
					state = stateAt(UI_HOME_FACE_SYSTEM,
						(uiHomeSurface_t)surfaceIndex,
						invalidSelections[valueIndex], 3);
					checkOracleTransition(state,
						(uiHomeInput_t)inputIndex, caps);
				}
			}
		}
	}

	/* Make the no-revision capability reconciliation contract explicit. */
	state = stateAt(UI_HOME_FACE_SOURCE, UI_HOME_SURFACE_SOURCE, 1, 1);
	CHECK(UIHome_Apply(&state, (uiHomeInput_t)99,
		capabilities(false, true)) == UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_SOURCE, UI_HOME_SURFACE_SOURCE, 0, 1,
		41u);
}

enum { RESTART_BRUTE_DEPTH = 6 };

static unsigned int restartReachableCount;

static bool isRestartChoiceInput(uiHomeInput_t input)
{
	return input == UI_HOME_INPUT_LEFT || input == UI_HOME_INPUT_RIGHT ||
		input == UI_HOME_INPUT_UP || input == UI_HOME_INPUT_DOWN;
}

static void bruteForceRestart(uiHomeState_t state,
	uiHomeCapabilities_t caps, uiHomeInput_t sequence[RESTART_BRUTE_DEPTH],
	int depth)
{
	int inputIndex;

	if(depth >= RESTART_BRUTE_DEPTH) {
		return;
	}
	for(inputIndex = (int)UI_HOME_INPUT_NONE;
			inputIndex <= (int)UI_HOME_INPUT_RECENT; ++inputIndex) {
		uiHomeState_t before = state;
		uiHomeState_t next = state;
		uiHomeInput_t input = (uiHomeInput_t)inputIndex;
		uiHomeEffect_t effect;

		sequence[depth] = input;
		effect = UIHome_Apply(&next, input, caps);
		if(effect == UI_HOME_EFFECT_RESTART) {
			++restartReachableCount;
			CHECK(depth + 1 == RESTART_BRUTE_DEPTH);
			CHECK(before.face == UI_HOME_FACE_SYSTEM);
			CHECK(before.surface == UI_HOME_SURFACE_RESTART_CONFIRM);
			CHECK(before.selection == 1);
			CHECK(input == UI_HOME_INPUT_ACTIVATE);
			CHECK(sequence[0] == UI_HOME_INPUT_LEFT ||
				sequence[0] == UI_HOME_INPUT_UP);
			CHECK(sequence[1] == UI_HOME_INPUT_ACTIVATE);
			/* Restart is System's last row: Up wraps to it; Down reaches
			 * Memory Cards first, too far for six samples. */
			CHECK(sequence[2] == UI_HOME_INPUT_UP);
			CHECK(sequence[3] == UI_HOME_INPUT_ACTIVATE);
			CHECK(isRestartChoiceInput(sequence[4]));
			CHECK(sequence[5] == UI_HOME_INPUT_ACTIVATE);
		}
		else {
			CHECK(effect != UI_HOME_EFFECT_RESTART);
		}
		bruteForceRestart(next, caps, sequence, depth + 1);
	}
}

static void testRestartReachabilityOracle(void)
{
	static const uiHomeInput_t canonical[RESTART_BRUTE_DEPTH] = {
		UI_HOME_INPUT_LEFT,
		UI_HOME_INPUT_ACTIVATE,
		UI_HOME_INPUT_UP,
		UI_HOME_INPUT_ACTIVATE,
		UI_HOME_INPUT_RIGHT,
		UI_HOME_INPUT_ACTIVATE
	};
	uiHomeCapabilities_t caps = capabilities(true, true);
	uiHomeInput_t sequence[RESTART_BRUTE_DEPTH];
	uiHomeState_t state;
	uiHomeState_t before;
	uiHomeEffect_t effect = UI_HOME_EFFECT_NONE;
	int index;

	UIHome_Init(&state, caps);
	restartReachableCount = 0u;
	bruteForceRestart(state, caps, sequence, 0);
	CHECK(restartReachableCount == 8u);

	/* The canonical six distinct samples expose each safe stage.  No prefix
	 * emits Restart; the final, fresh A is the only destructive effect. */
	UIHome_Init(&state, caps);
	for(index = 0; index < RESTART_BRUTE_DEPTH; ++index) {
		before = state;
		effect = UIHome_Apply(&state, canonical[index], caps);
		if(index < RESTART_BRUTE_DEPTH - 1) {
			CHECK(effect == UI_HOME_EFFECT_NONE);
		}
		else {
			CHECK(effect == UI_HOME_EFFECT_RESTART);
			checkSameState(&state, &before);
		}
		if(index == 0) {
			checkState(&state, UI_HOME_FACE_SYSTEM, UI_HOME_SURFACE_RING,
				0, -1, 2u);
		}
		else if(index == 1) {
			checkState(&state, UI_HOME_FACE_SYSTEM, UI_HOME_SURFACE_SYSTEM,
				0, -1, 3u);
		}
		else if(index == 2) {
			checkState(&state, UI_HOME_FACE_SYSTEM, UI_HOME_SURFACE_SYSTEM,
				2, -1, 4u);
		}
		else if(index == 3) {
			checkState(&state, UI_HOME_FACE_SYSTEM,
				UI_HOME_SURFACE_RESTART_CONFIRM, 0, -1, 5u);
		}
		else if(index == 4) {
			checkState(&state, UI_HOME_FACE_SYSTEM,
				UI_HOME_SURFACE_RESTART_CONFIRM, 1, -1, 6u);
		}
	}
}

static void testOrientationGroup(void)
{
	uiHomeState_t states[24];
	unsigned count = 1;
	UIHome_Init(&states[0], capabilities(true, false));
	for(unsigned head = 0; head < count; ++head) {
		for(int command = UI_HOME_INPUT_LEFT; command <= UI_HOME_INPUT_DOWN; ++command) {
			uiHomeState_t actual = states[head], expected = states[head];
			oracleApply(&expected, (uiHomeInput_t)command, capabilities(true, false));
			UIHome_Apply(&actual, (uiHomeInput_t)command, capabilities(true, false));
			checkSameState(&actual, &expected);
			CHECK(UIHome_OrientationValid(&actual.orientation));
			unsigned found;
			for(found = 0; found < count; ++found)
				if(memcmp(&states[found].orientation, &actual.orientation,
					sizeof(actual.orientation)) == 0) break;
			if(found == count) {
				CHECK(count < 24);
				states[count++] = actual;
			}
			uiHomeState_t cycle = states[head];
			for(int turn = 0; turn < 4; ++turn)
				UIHome_Apply(&cycle, (uiHomeInput_t)command, capabilities(true, false));
			CHECK(memcmp(&cycle.orientation, &states[head].orientation,
				sizeof(cycle.orientation)) == 0);
		}
	}
	CHECK(count == 24);
	uiHomeState_t limits = states[0];
	limits.turnOrdinal = INT32_MAX;
	UIHome_Apply(&limits, UI_HOME_INPUT_RIGHT, capabilities(true, false));
	CHECK(limits.turnOrdinal == 4 && limits.face == UI_HOME_FACE_LIBRARY);
	limits.turnOrdinal = INT32_MIN;
	UIHome_Apply(&limits, UI_HOME_INPUT_UP, capabilities(true, false));
	CHECK(limits.turnOrdinal == -1 && limits.face == UI_HOME_FACE_SYSTEM);
	uiHomeOrientation_t bad = {{{-1,0,0},{0,1,0},{0,0,1}}};
	CHECK(!UIHome_OrientationValid(&bad)); /* Reflection is not a cube rotation. */
	bad = states[0].orientation; bad.m[0][0] = 2;
	CHECK(!UIHome_OrientationValid(&bad));
	CHECK(!UIHome_OrientationValid(NULL));
}

int main(void)
{
	testOrientationGroup();
	testValidationAndInitialization();
	testLabelsHintsAndRows();
	testFaceMappingAndSignedTurns();
	testEveryRingFaceAndInput();
	testNoSourceRedirect();
	testSourceSurface();
	testSystemSurface();
	testRestartConfirmation();
	testLongRunOrdinalAndRevision();
	testExhaustiveReducerOracle();
	testInvalidInputAndStateOracle();
	testRestartReachabilityOracle();
	printf("ui_home: %u checks passed\n", checks);
	return EXIT_SUCCESS;
}
