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

	/* The default sides, as a zeroed Home has. */
	memset(&value, 0, sizeof(value));
	value.hasSource = hasSource;
	value.hasRecent = hasRecent;
	value.hasApps = false;
	value.style = UI_HOME_CUBE_INFINITE;
	return value;
}

/* The same with an app on the source: the ring has five faces. */
static uiHomeCapabilities_t withApps(uiHomeCapabilities_t value)
{
	value.hasApps = true;
	return value;
}

/* The same with Setup > Console > Cube set to Classic. */
static uiHomeCapabilities_t classic(uiHomeCapabilities_t value)
{
	value.style = UI_HOME_CUBE_CLASSIC;
	return value;
}

/* A state's ring of four, or of five with Apps: the fields a Home with or
 * without apps publishes. */
static void setRing(uiHomeState_t *state, int ring)
{
	uiHomeState_t shaped;

	UIHome_Init(&shaped, ring == 5 ? withApps(capabilities(true, false)) :
		capabilities(true, false));
	state->faceCount = shaped.faceCount;
	memcpy(state->sides, shaped.sides, sizeof(state->sides));
	state->absent = shaped.absent;
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
	CHECK(actual->faceCount == expected->faceCount);
	CHECK(actual->turnAxis == expected->turnAxis);
	CHECK(actual->turnDirection == expected->turnDirection);
	CHECK(actual->style == expected->style);
	CHECK(memcmp(&actual->orientation, &expected->orientation,
		sizeof(actual->orientation)) == 0);
	CHECK(memcmp(actual->sides, expected->sides, sizeof(actual->sides)) == 0);
	CHECK(actual->absent == expected->absent);
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
	CHECK_TEXT(UIHome_FaceLabel(UI_HOME_FACE_APPS), "APPS");
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
	CHECK_TEXT(UIHome_PrimaryHint(UI_HOME_FACE_APPS, withApps(withSource)),
		"A  OPEN");
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

	UIHome_Init(&right, caps);
	UIHome_Init(&left, withApps(caps));
	for(index = -1; index <= 5; ++index) {
		CHECK(UIHome_RingFace(&right, (int32_t)index) == aroundZero[index + 1]);
	}
	CHECK(UIHome_RingFace(&right, INT32_MIN) == UI_HOME_FACE_LIBRARY);
	CHECK(UIHome_RingFace(&right, INT32_MAX) == UI_HOME_FACE_SYSTEM);
	/* With Apps the ring is five faces, Apps just before Library. */
	CHECK(UIHome_RingFace(&left, -1) == UI_HOME_FACE_APPS);
	CHECK(UIHome_RingFace(&left, 4) == UI_HOME_FACE_APPS);
	CHECK(UIHome_RingFace(&left, 5) == UI_HOME_FACE_LIBRARY);
	CHECK(UIHome_RingFace(&left, -6) == UI_HOME_FACE_APPS);
	CHECK(UIHome_RingFace(&left, INT32_MAX) == UI_HOME_FACE_SETTINGS);
	CHECK(UIHome_RingFace(&left, INT32_MIN) == UI_HOME_FACE_SETTINGS);
	CHECK(UIHome_RingFace(NULL, 3) == UI_HOME_FACE_LIBRARY);
	CHECK(UIHome_FaceCount(caps) == 4);
	CHECK(UIHome_FaceCount(withApps(caps)) == 5);
	/* The default cube: each face one turn further from Library, on the
	 * GameCube's sides. Apps keeps its side while it isn't there. */
	for(index = 0; index < (int)UI_HOME_FACE_COUNT; ++index) {
		static const int defaultSide[UI_HOME_FACE_COUNT] = {
			-1, UI_HOME_SIDE_UP, UI_HOME_SIDE_LEFT, UI_HOME_SIDE_RIGHT,
			UI_HOME_SIDE_DOWN
		};

		CHECK(UIHome_RingIndex(&left, (uiHomeFace_t)index) == index);
		CHECK(UIHome_RingIndex(&right, (uiHomeFace_t)index) ==
			(index == (int)UI_HOME_FACE_APPS ? -1 : index));
		CHECK(UIHome_FaceSide(&right, (uiHomeFace_t)index) ==
			defaultSide[index]);
	}
	CHECK(UIHome_LayoutValid(&right) && UIHome_LayoutValid(&left));
	CHECK(!UIHome_LayoutValid(NULL));

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
	for(index = 0; index < 4; ++index) {
		CHECK(UIHome_Apply(&right, UI_HOME_INPUT_RIGHT, caps) ==
			UI_HOME_EFFECT_NONE);
		CHECK(UIHome_Apply(&left, UI_HOME_INPUT_LEFT, caps) ==
			UI_HOME_EFFECT_NONE);
	}
	checkState(&right, UI_HOME_FACE_LIBRARY, UI_HOME_SURFACE_RING, 0,
		4, 5u);
	checkState(&left, UI_HOME_FACE_LIBRARY, UI_HOME_SURFACE_RING, 0,
		-4, 5u);

	/* Five turns either way round the ring with Apps. */
	caps = withApps(caps);
	UIHome_Init(&right, caps);
	UIHome_Init(&left, caps);
	CHECK(right.faceCount == 5);
	CHECK(UIHome_Apply(&left, UI_HOME_INPUT_LEFT, caps) ==
		UI_HOME_EFFECT_NONE);
	checkState(&left, UI_HOME_FACE_APPS, UI_HOME_SURFACE_RING, 0, -1, 2u);
	CHECK(UIHome_Apply(&right, UI_HOME_INPUT_RIGHT, caps) ==
		UI_HOME_EFFECT_NONE);
	checkState(&right, UI_HOME_FACE_SOURCE, UI_HOME_SURFACE_RING, 0, 1, 2u);
	for(index = 1; index < 5; ++index) {
		CHECK(UIHome_Apply(&right, UI_HOME_INPUT_RIGHT, caps) ==
			UI_HOME_EFFECT_NONE);
		CHECK(UIHome_Apply(&left, UI_HOME_INPUT_LEFT, caps) ==
			UI_HOME_EFFECT_NONE);
	}
	checkState(&right, UI_HOME_FACE_LIBRARY, UI_HOME_SURFACE_RING, 0,
		5, 6u);
	checkState(&left, UI_HOME_FACE_LIBRARY, UI_HOME_SURFACE_RING, 0,
		-5, 6u);
}

/* Apps comes and goes with the source's apps, publication-only like the
 * selection's repair: the cube keeps its place and the revision its count,
 * and standing on Apps as it goes lands on Library. */
static void testAppsFaceComesAndGoes(void)
{
	uiHomeCapabilities_t plain = capabilities(true, false);
	uiHomeCapabilities_t apps = withApps(plain);
	uiHomeState_t state;
	uiHomeState_t before;

	UIHome_Init(&state, apps);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_LEFT, apps) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_APPS, UI_HOME_SURFACE_RING, 0, -1, 2u);
	before = state;
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_ACTIVATE, apps) ==
		UI_HOME_EFFECT_OPEN_APPS);
	checkSameState(&state, &before);

	/* The apps go while Apps is in front: Library, and a ring of four. */
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_NONE, plain) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_LIBRARY, UI_HOME_SURFACE_RING, 0, 0, 2u);
	CHECK(state.faceCount == 4);
	CHECK(memcmp(&state.orientation, &before.orientation,
		sizeof(state.orientation)) == 0);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_LEFT, plain) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_SYSTEM, UI_HOME_SURFACE_RING, 0, -1, 3u);

	/* They come while System is in front: System stays, its ordinal now
	 * counts in a ring of five, and Left reaches Settings, Right Apps. */
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_NONE, apps) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_SYSTEM, UI_HOME_SURFACE_RING, 0, 3, 3u);
	CHECK(state.faceCount == 5);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_RIGHT, apps) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_APPS, UI_HOME_SURFACE_RING, 0, 4, 4u);

	/* A list open on another face keeps its row. */
	state = stateAt(UI_HOME_FACE_SOURCE, UI_HOME_SURFACE_SOURCE, 1, 1);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_NONE, apps) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_SOURCE, UI_HOME_SURFACE_SOURCE, 1, 1, 41u);
	CHECK(state.faceCount == 5);

	/* Apps in front of a ring of four is repaired to Library. */
	state = stateAt(UI_HOME_FACE_APPS, UI_HOME_SURFACE_RING, 0, 4);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_ACTIVATE, plain) ==
		UI_HOME_EFFECT_OPEN_LIBRARY);
	checkState(&state, UI_HOME_FACE_LIBRARY, UI_HOME_SURFACE_RING, 0, 0, 41u);
	CHECK(state.faceCount == 4);
}

static void testEveryRingFaceAndInput(void)
{
	int faceIndex;
	int inputIndex;
	int ring;

	for(ring = 0; ring < 2; ++ring) {
	uiHomeCapabilities_t caps = ring ? withApps(capabilities(true, true)) :
		capabilities(true, true);
	int faceCount = UIHome_FaceCount(caps);

	for(faceIndex = 0; faceIndex < faceCount; ++faceIndex) {
		uiHomeFace_t face = (uiHomeFace_t)faceIndex;
		for(inputIndex = (int)UI_HOME_INPUT_NONE;
			inputIndex <= (int)UI_HOME_INPUT_SETTINGS; ++inputIndex) {
			uiHomeInput_t input = (uiHomeInput_t)inputIndex;
			uiHomeState_t state = stateAt(face, UI_HOME_SURFACE_RING, 0,
				(int32_t)faceIndex);
			uiHomeState_t before;
			uiHomeEffect_t effect;

			setRing(&state, faceCount);
			before = state;
			effect = UIHome_Apply(&state, input, caps);
			switch(input) {
				case UI_HOME_INPUT_LEFT:
				case UI_HOME_INPUT_UP:
					CHECK(effect == UI_HOME_EFFECT_NONE);
					checkState(&state,
						UIHome_RingFace(&before, (int32_t)faceIndex - 1),
						UI_HOME_SURFACE_RING, 0,
						(int32_t)faceIndex - 1, 42u);
					break;
				case UI_HOME_INPUT_RIGHT:
				case UI_HOME_INPUT_DOWN:
					CHECK(effect == UI_HOME_EFFECT_NONE);
					checkState(&state,
						UIHome_RingFace(&before, (int32_t)faceIndex + 1),
						UI_HOME_SURFACE_RING, 0,
						(int32_t)faceIndex + 1, 42u);
					break;
				case UI_HOME_INPUT_ACTIVATE:
					if(face == UI_HOME_FACE_APPS) {
						CHECK(effect == UI_HOME_EFFECT_OPEN_APPS);
						checkSameState(&state, &before);
					}
					else if(face == UI_HOME_FACE_LIBRARY) {
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
				case UI_HOME_INPUT_SETTINGS:
					CHECK(effect == UI_HOME_EFFECT_OPEN_SETTINGS);
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
			uiHomeState_t before;
			uiHomeCapabilities_t noRecent = capabilities(true, false);

			noRecent.hasApps = caps.hasApps;
			setRing(&state, faceCount);
			before = state;
			CHECK(UIHome_Apply(&state, UI_HOME_INPUT_RECENT, noRecent) ==
				UI_HOME_EFFECT_NONE);
			checkSameState(&state, &before);
		}
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
		inputIndex <= (int)UI_HOME_INPUT_SETTINGS; ++inputIndex) {
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
			case UI_HOME_INPUT_SETTINGS:
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
		inputIndex <= (int)UI_HOME_INPUT_SETTINGS; ++inputIndex) {
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
			case UI_HOME_INPUT_SETTINGS:
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
		inputIndex <= (int)UI_HOME_INPUT_SETTINGS; ++inputIndex) {
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
			case UI_HOME_INPUT_SETTINGS:
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
		CHECK(state.face == UIHome_RingFace(&state, state.turnOrdinal));
	}
	checkState(&state, UI_HOME_FACE_LIBRARY, UI_HOME_SURFACE_RING, 0,
		40000, 40001u);

	for(index = 0; index < 80003; ++index) {
		CHECK(UIHome_Apply(&state, UI_HOME_INPUT_LEFT, caps) ==
			UI_HOME_EFFECT_NONE);
		CHECK(state.face == UIHome_RingFace(&state, state.turnOrdinal));
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

/* Classic's cube, written out: each face's orientation is the one that has
 * its own side in front, a quarter turn from Library's. Library's side is
 * the front, Source's the top (+y), Settings' the left (-x), System's the
 * right (+x) and Apps' the bottom (-y). */
static const int8_t oracleClassicPoses[UI_HOME_FACE_COUNT][3][3] = {
	{{1,0,0},{0,1,0},{0,0,1}},
	{{1,0,0},{0,0,-1},{0,1,0}},
	{{0,0,1},{0,1,0},{-1,0,0}},
	{{0,0,-1},{0,1,0},{1,0,0}},
	{{1,0,0},{0,0,1},{0,-1,0}}
};

static bool oracleIsClassicPose(const uiHomeState_t *state)
{
	return memcmp(&state->orientation, oracleClassicPoses[state->face],
		sizeof(state->orientation)) == 0;
}

/* Classic: every face from every Classic face and input, in rings of four
 * and five. Library is the way between the faces, as on the GameCube; there
 * is no way round, and B turns back to Library. */
static void testClassicEveryFaceAndInput(void)
{
	/* Where Left, Right, Up, Down and B turn from each face; -1 refuses. */
	static const int turns[UI_HOME_FACE_COUNT][5] = {
		{ UI_HOME_FACE_SETTINGS, UI_HOME_FACE_SYSTEM, UI_HOME_FACE_SOURCE,
			UI_HOME_FACE_APPS, -1 },
		{ -1, -1, -1, UI_HOME_FACE_LIBRARY, UI_HOME_FACE_LIBRARY },
		{ -1, UI_HOME_FACE_LIBRARY, -1, -1, UI_HOME_FACE_LIBRARY },
		{ UI_HOME_FACE_LIBRARY, -1, -1, -1, UI_HOME_FACE_LIBRARY },
		{ -1, -1, UI_HOME_FACE_LIBRARY, -1, UI_HOME_FACE_LIBRARY }
	};
	/* The way back B turns: the direction opposite the face's side. */
	static const uiHomeInput_t backTurns[UI_HOME_FACE_COUNT] = {
		UI_HOME_INPUT_NONE, UI_HOME_INPUT_DOWN, UI_HOME_INPUT_RIGHT,
		UI_HOME_INPUT_LEFT, UI_HOME_INPUT_UP
	};
	int ring;

	for(ring = 4; ring <= 5; ++ring) {
	uiHomeCapabilities_t caps = classic(capabilities(true, true));
	int faceIndex;
	int inputIndex;

	caps.hasApps = ring == 5;
	for(faceIndex = 0; faceIndex < ring; ++faceIndex) {
		for(inputIndex = (int)UI_HOME_INPUT_NONE;
			inputIndex <= (int)UI_HOME_INPUT_RECENT; ++inputIndex) {
			uiHomeFace_t face = (uiHomeFace_t)faceIndex;
			uiHomeInput_t input = (uiHomeInput_t)inputIndex;
			uiHomeState_t state = stateAt(face, UI_HOME_SURFACE_RING, 0,
				(int32_t)faceIndex);
			uiHomeState_t before;
			uiHomeEffect_t effect;
			int column = input == UI_HOME_INPUT_BACK ? 4 : inputIndex - 1;
			int target = column >= 0 && column <= 4 ?
				turns[faceIndex][column] : -1;

			setRing(&state, ring);
			state.style = UI_HOME_CUBE_CLASSIC;
			memcpy(&state.orientation, oracleClassicPoses[faceIndex],
				sizeof(state.orientation));
			/* Without Apps, Down from Library has nowhere to go. */
			if(target == (int)UI_HOME_FACE_APPS && ring == 4) {
				target = -1;
			}
			before = state;
			effect = UIHome_Apply(&state, input, caps);
			if(input == UI_HOME_INPUT_ACTIVATE) {
				static const uiHomeEffect_t opens[UI_HOME_FACE_COUNT] = {
					UI_HOME_EFFECT_OPEN_LIBRARY, UI_HOME_EFFECT_NONE,
					UI_HOME_EFFECT_OPEN_SETTINGS, UI_HOME_EFFECT_NONE,
					UI_HOME_EFFECT_OPEN_APPS
				};

				CHECK(effect == opens[faceIndex]);
				if(face == UI_HOME_FACE_SOURCE || face == UI_HOME_FACE_SYSTEM) {
					checkState(&state, face, face == UI_HOME_FACE_SOURCE ?
						UI_HOME_SURFACE_SOURCE : UI_HOME_SURFACE_SYSTEM, 0,
						(int32_t)faceIndex, 42u);
					CHECK(oracleIsClassicPose(&state));
				}
				else {
					checkSameState(&state, &before);
				}
				continue;
			}
			if(input == UI_HOME_INPUT_RECENT) {
				CHECK(effect == UI_HOME_EFFECT_OPEN_RECENT);
				checkSameState(&state, &before);
				continue;
			}
			CHECK(effect == UI_HOME_EFFECT_NONE);
			if(target < 0) {
				/* Refused: nothing changes, not even the revision. */
				checkSameState(&state, &before);
				continue;
			}
			{
				uiHomeInput_t turn = input == UI_HOME_INPUT_BACK ?
					backTurns[faceIndex] : input;

				checkState(&state, (uiHomeFace_t)target,
					UI_HOME_SURFACE_RING, 0, (int32_t)target, 42u);
				CHECK(oracleIsClassicPose(&state));
				CHECK(state.turnAxis == (turn == UI_HOME_INPUT_UP ||
					turn == UI_HOME_INPUT_DOWN ? UI_HOME_TURN_VERTICAL :
					UI_HOME_TURN_HORIZONTAL));
				CHECK(state.turnDirection == (turn == UI_HOME_INPUT_LEFT ||
					turn == UI_HOME_INPUT_UP ? -1 : 1));
				CHECK(state.style == UI_HOME_CUBE_CLASSIC);
				CHECK(state.faceCount == ring);
			}
		}
		{
			/* Start with nothing recent does nothing on any face. */
			uiHomeState_t state = stateAt((uiHomeFace_t)faceIndex,
				UI_HOME_SURFACE_RING, 0, (int32_t)faceIndex);
			uiHomeState_t before;
			uiHomeCapabilities_t noRecent = caps;

			noRecent.hasRecent = false;
			setRing(&state, ring);
			state.style = UI_HOME_CUBE_CLASSIC;
			memcpy(&state.orientation, oracleClassicPoses[faceIndex],
				sizeof(state.orientation));
			before = state;
			CHECK(UIHome_Apply(&state, UI_HOME_INPUT_RECENT, noRecent) ==
				UI_HOME_EFFECT_NONE);
			checkSameState(&state, &before);
		}
	}
	}
}

/* Classic walked as a person would: no wrap-around, B home, Library first
 * with or without a source, and the style taking hold as Settings closes. */
static void testClassicWalks(void)
{
	static const struct {
		uiHomeInput_t input;
		uiHomeFace_t face;
		uint32_t revision;
	} walk[] = {
		{ UI_HOME_INPUT_LEFT, UI_HOME_FACE_SETTINGS, 2u },
		{ UI_HOME_INPUT_LEFT, UI_HOME_FACE_SETTINGS, 2u },
		{ UI_HOME_INPUT_RIGHT, UI_HOME_FACE_LIBRARY, 3u },
		/* Settings to System is Right, Right. */
		{ UI_HOME_INPUT_RIGHT, UI_HOME_FACE_SYSTEM, 4u },
		{ UI_HOME_INPUT_RIGHT, UI_HOME_FACE_SYSTEM, 4u },
		{ UI_HOME_INPUT_UP, UI_HOME_FACE_SYSTEM, 4u },
		{ UI_HOME_INPUT_DOWN, UI_HOME_FACE_SYSTEM, 4u },
		{ UI_HOME_INPUT_BACK, UI_HOME_FACE_LIBRARY, 5u },
		{ UI_HOME_INPUT_BACK, UI_HOME_FACE_LIBRARY, 5u },
		{ UI_HOME_INPUT_UP, UI_HOME_FACE_SOURCE, 6u },
		{ UI_HOME_INPUT_UP, UI_HOME_FACE_SOURCE, 6u },
		{ UI_HOME_INPUT_DOWN, UI_HOME_FACE_LIBRARY, 7u },
		{ UI_HOME_INPUT_DOWN, UI_HOME_FACE_APPS, 8u },
		{ UI_HOME_INPUT_DOWN, UI_HOME_FACE_APPS, 8u },
		{ UI_HOME_INPUT_LEFT, UI_HOME_FACE_APPS, 8u },
		{ UI_HOME_INPUT_UP, UI_HOME_FACE_LIBRARY, 9u }
	};
	uiHomeCapabilities_t caps = classic(withApps(capabilities(true, false)));
	uiHomeCapabilities_t infinite = withApps(capabilities(true, false));
	uiHomeState_t state;
	uiHomeState_t before;
	unsigned index;

	UIHome_Init(&state, caps);
	checkState(&state, UI_HOME_FACE_LIBRARY, UI_HOME_SURFACE_RING, 0, 0, 1u);
	CHECK(state.style == UI_HOME_CUBE_CLASSIC && oracleIsClassicPose(&state));
	for(index = 0; index < sizeof(walk) / sizeof(walk[0]); ++index) {
		CHECK(UIHome_Apply(&state, walk[index].input, caps) ==
			UI_HOME_EFFECT_NONE);
		checkState(&state, walk[index].face, UI_HOME_SURFACE_RING, 0,
			(int32_t)walk[index].face, walk[index].revision);
		CHECK(oracleIsClassicPose(&state));
	}

	/* No source: Classic still starts on Library, and A there turns up to
	 * Source with its list open, one revision. B closes the list, and B
	 * again turns back down to Library. */
	UIHome_Init(&state, classic(capabilities(false, false)));
	checkState(&state, UI_HOME_FACE_LIBRARY, UI_HOME_SURFACE_RING, 0, 0, 1u);
	CHECK(oracleIsClassicPose(&state));
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_ACTIVATE,
		classic(capabilities(false, false))) == UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_SOURCE, UI_HOME_SURFACE_SOURCE, 0, 1, 2u);
	CHECK(oracleIsClassicPose(&state));
	CHECK(state.turnAxis == UI_HOME_TURN_VERTICAL && state.turnDirection == -1);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_BACK,
		classic(capabilities(false, false))) == UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_SOURCE, UI_HOME_SURFACE_RING, 0, 1, 3u);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_BACK,
		classic(capabilities(false, false))) == UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_LIBRARY, UI_HOME_SURFACE_RING, 0, 0, 4u);
	CHECK(oracleIsClassicPose(&state));

	/* Cube changed in Settings takes hold when Settings closes, on the face
	 * it was opened from: published, not an input, so the revision stays.
	 * Infinite reached Settings two turns round, a half turn from Library;
	 * Classic shows Settings' own side, the left. */
	UIHome_Init(&state, infinite);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_RIGHT, infinite) ==
		UI_HOME_EFFECT_NONE);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_RIGHT, infinite) ==
		UI_HOME_EFFECT_NONE);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_ACTIVATE, infinite) ==
		UI_HOME_EFFECT_OPEN_SETTINGS);
	CHECK(!oracleIsClassicPose(&state));
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_NONE, caps) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_SETTINGS, UI_HOME_SURFACE_RING, 0, 2, 3u);
	CHECK(state.style == UI_HOME_CUBE_CLASSIC && oracleIsClassicPose(&state));
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_RIGHT, caps) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_LIBRARY, UI_HOME_SURFACE_RING, 0, 0, 4u);
	CHECK(oracleIsClassicPose(&state));
	/* And back to Infinite: the cube stays as it is, and turns round again. */
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_LEFT, caps) ==
		UI_HOME_EFFECT_NONE);
	before = state;
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_NONE, infinite) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_SETTINGS, UI_HOME_SURFACE_RING, 0, 2, 5u);
	CHECK(state.style == UI_HOME_CUBE_INFINITE);
	CHECK(memcmp(&state.orientation, &before.orientation,
		sizeof(state.orientation)) == 0);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_LEFT, infinite) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_SOURCE, UI_HOME_SURFACE_RING, 0, 1, 6u);
	/* A style that is neither is Infinite. */
	before = state;
	infinite.style = UI_HOME_CUBE_COUNT;
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_NONE, infinite) ==
		UI_HOME_EFFECT_NONE);
	checkSameState(&state, &before);

	/* Apps going while it is in front: Library, and its side in front. */
	UIHome_Init(&state, caps);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_DOWN, caps) ==
		UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_APPS, UI_HOME_SURFACE_RING, 0, 4, 2u);
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_NONE,
		classic(capabilities(true, false))) == UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_LIBRARY, UI_HOME_SURFACE_RING, 0, 0, 2u);
	CHECK(state.faceCount == 4 && oracleIsClassicPose(&state));
	CHECK(UIHome_Apply(&state, UI_HOME_INPUT_DOWN,
		classic(capabilities(true, false))) == UI_HOME_EFFECT_NONE);
	checkState(&state, UI_HOME_FACE_LIBRARY, UI_HOME_SURFACE_RING, 0, 0, 2u);
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

/* The ring the capabilities make: Apps is a fifth face while there are apps.
 * A ring of another size, or a face outside it, is set to it: Library when
 * the face in front is gone, the ordinal restarted at the face. So is a
 * style other than the capabilities' (anything but Classic is Infinite),
 * and in Classic the cube then shows the face in front's own side. */
static void oracleReconcileRing(uiHomeState_t *state,
	uiHomeCapabilities_t caps)
{
	int ring = caps.hasApps ? 5 : 4;
	uiHomeCubeStyle_t style = caps.style == UI_HOME_CUBE_CLASSIC ?
		UI_HOME_CUBE_CLASSIC : UI_HOME_CUBE_INFINITE;

	if(state->faceCount != ring || (int)state->face >= ring ||
			state->style != style) {
		if((int)state->face >= ring) {
			state->face = UI_HOME_FACE_LIBRARY;
			state->surface = UI_HOME_SURFACE_RING;
			state->selection = 0;
		}
		state->turnOrdinal = (int32_t)state->face;
		state->faceCount = ring;
		state->style = style;
		if(style == UI_HOME_CUBE_CLASSIC) {
			memcpy(&state->orientation, oracleClassicPoses[state->face],
				sizeof(state->orientation));
		}
		/* Source, Settings, System and Apps up, left, right and down;
		 * Apps there only with apps. */
		state->sides[0] = UI_HOME_FACE_SOURCE;
		state->sides[1] = UI_HOME_FACE_SETTINGS;
		state->sides[2] = UI_HOME_FACE_SYSTEM;
		state->sides[3] = UI_HOME_FACE_APPS;
		state->absent = caps.hasApps ? 0u : (uint8_t)(1u << UI_HOME_FACE_APPS);
	}
}

/* Classic as the GameCube's menu: from Library each direction reaches the
 * face on its side, when the ring has it; from that face only the way back,
 * or B, reaches Library. turn is the direction the cube turns. */
static bool oracleClassicMove(const uiHomeState_t *state, uiHomeInput_t input,
	uiHomeFace_t *target, uiHomeInput_t *turn)
{
	static const struct {
		uiHomeFace_t face;
		uiHomeInput_t out;
		uiHomeInput_t back;
	} sides[4] = {
		{ UI_HOME_FACE_SOURCE, UI_HOME_INPUT_UP, UI_HOME_INPUT_DOWN },
		{ UI_HOME_FACE_SETTINGS, UI_HOME_INPUT_LEFT, UI_HOME_INPUT_RIGHT },
		{ UI_HOME_FACE_SYSTEM, UI_HOME_INPUT_RIGHT, UI_HOME_INPUT_LEFT },
		{ UI_HOME_FACE_APPS, UI_HOME_INPUT_DOWN, UI_HOME_INPUT_UP }
	};
	int index;

	for(index = 0; index < 4; ++index) {
		if(state->face == UI_HOME_FACE_LIBRARY && input == sides[index].out &&
				(int)sides[index].face < state->faceCount) {
			*target = sides[index].face;
			*turn = input;
			return true;
		}
		if(state->face == sides[index].face &&
				(input == sides[index].back || input == UI_HOME_INPUT_BACK)) {
			*target = UI_HOME_FACE_LIBRARY;
			*turn = sides[index].back;
			return true;
		}
	}
	return false;
}

static void oracleClassicTurn(uiHomeState_t *state, uiHomeFace_t target,
	uiHomeInput_t turn)
{
	memcpy(&state->orientation, oracleClassicPoses[target],
		sizeof(state->orientation));
	state->turnAxis = turn == UI_HOME_INPUT_UP || turn == UI_HOME_INPUT_DOWN ?
		UI_HOME_TURN_VERTICAL : UI_HOME_TURN_HORIZONTAL;
	state->turnDirection = turn == UI_HOME_INPUT_LEFT ||
		turn == UI_HOME_INPUT_UP ? -1 : 1;
	state->face = target;
	state->turnOrdinal = (int32_t)target;
	state->surface = UI_HOME_SURFACE_RING;
	state->selection = 0;
	state->revision++;
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
		state->turnOrdinal %= state->faceCount;
	state->turnOrdinal += step;
	state->face = (uiHomeFace_t)oraclePositiveModulo(
		(int)(state->turnOrdinal % (int32_t)state->faceCount),
		state->faceCount);
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
	oracleReconcileRing(state, caps);
	oracleNormalizeSelection(state, caps);

	if(state->surface == UI_HOME_SURFACE_RING &&
			state->style == UI_HOME_CUBE_CLASSIC) {
		uiHomeFace_t target;
		uiHomeInput_t turn;

		if(input == UI_HOME_INPUT_ACTIVATE &&
				state->face == UI_HOME_FACE_LIBRARY && !caps.hasSource) {
			oracleClassicTurn(state, UI_HOME_FACE_SOURCE, UI_HOME_INPUT_UP);
			state->surface = UI_HOME_SURFACE_SOURCE;
			return UI_HOME_EFFECT_NONE;
		}
		if(oracleClassicMove(state, input, &target, &turn)) {
			oracleClassicTurn(state, target, turn);
			return UI_HOME_EFFECT_NONE;
		}
		/* Every other turn, and B, is refused; A and Start are the ring's. */
		if(input != UI_HOME_INPUT_ACTIVATE && input != UI_HOME_INPUT_RECENT) {
			return UI_HOME_EFFECT_NONE;
		}
	}
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
			else if(state->face == UI_HOME_FACE_APPS) {
				return UI_HOME_EFFECT_OPEN_APPS;
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
	CHECK((int)actualEffect <= (int)UI_HOME_EFFECT_OPEN_APPS);
	CHECK((int)actual.face < actual.faceCount);
	CHECK(UIHome_LayoutValid(&actual));
	CHECK(UIHome_RingFace(&actual, actual.turnOrdinal) == actual.face);
	/* Classic's cube always shows the face in front's own side. */
	if(actual.style == UI_HOME_CUBE_CLASSIC) {
		CHECK(actual.turnOrdinal == (int32_t)actual.face);
		CHECK(oracleIsClassicPose(&actual));
	}
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
				for(capsMask = 0; capsMask < 64; ++capsMask) {
					uiHomeCapabilities_t caps = capabilities(
						(capsMask & 1) != 0, (capsMask & 2) != 0);
					/* The state's ring, and the capabilities' ring, each
					 * four or five faces; the state's style and the
					 * capabilities', each Infinite or Classic. */
					int stateRing = (capsMask & 8) != 0 ? 5 : 4;
					bool stateClassic = (capsMask & 32) != 0;
					int rowCount = oracleRowCount(
						(uiHomeSurface_t)surfaceIndex, caps);
					int selectionCount = rowCount > 0 ? rowCount : 1;
					int selection;

					caps.hasApps = (capsMask & 4) != 0;
					if((capsMask & 16) != 0) {
						caps = classic(caps);
					}
					if(faceIndex >= stateRing) {
						continue;
					}
					for(selection = 0; selection < selectionCount;
							++selection) {
						for(cycleIndex = 0; cycleIndex <
								(int)(sizeof(ordinalCycles) /
								sizeof(ordinalCycles[0])); ++cycleIndex) {
							int32_t ordinal = (int32_t)faceIndex +
								(int32_t)(ordinalCycles[cycleIndex] *
								stateRing);
							uiHomeState_t state = stateAt(
								(uiHomeFace_t)faceIndex,
								(uiHomeSurface_t)surfaceIndex,
								selection, ordinal);

							setRing(&state, stateRing);
							/* A Classic state is always on its face's
							 * own side, its ordinal the face. */
							if(stateClassic) {
								if(ordinalCycles[cycleIndex] != 0) {
									continue;
								}
								state.style = UI_HOME_CUBE_CLASSIC;
								memcpy(&state.orientation,
									oracleClassicPoses[faceIndex],
									sizeof(state.orientation));
							}
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
	static const int invalidInputs[] = { -1, 9, 99, INT_MAX };
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
			for(capsMask = 0; capsMask < 8; ++capsMask) {
				caps = capabilities((capsMask & 1) != 0,
					(capsMask & 2) != 0);
				caps.hasApps = (capsMask & 4) != 0;
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

/* ------------------------------------------------------------------------- */
/* Faces on other sides, and sides with none: an oracle of its own.          */
/* ------------------------------------------------------------------------- */

#define NO_FACE UI_HOME_FACE_LIBRARY

/* Up, Left, Right and Down, as Settings may name them. */
static const uint8_t layoutFixtures[][UI_HOME_SIDE_COUNT] = {
	{ UI_HOME_FACE_SOURCE, UI_HOME_FACE_SETTINGS, UI_HOME_FACE_SYSTEM,
		UI_HOME_FACE_APPS },
	/* The issue's cube from today's faces: Settings down, Apps left. */
	{ UI_HOME_FACE_SOURCE, UI_HOME_FACE_APPS, UI_HOME_FACE_SYSTEM,
		UI_HOME_FACE_SETTINGS },
	{ NO_FACE, UI_HOME_FACE_SETTINGS, UI_HOME_FACE_SYSTEM, UI_HOME_FACE_APPS },
	{ UI_HOME_FACE_SOURCE, UI_HOME_FACE_SETTINGS, NO_FACE, UI_HOME_FACE_APPS },
	{ NO_FACE, UI_HOME_FACE_SETTINGS, NO_FACE, UI_HOME_FACE_APPS },
	{ NO_FACE, NO_FACE, NO_FACE, UI_HOME_FACE_SETTINGS },
	{ NO_FACE, NO_FACE, NO_FACE, NO_FACE },
	{ UI_HOME_FACE_APPS, UI_HOME_FACE_SOURCE, UI_HOME_FACE_SETTINGS,
		UI_HOME_FACE_SYSTEM },
	/* Source last, a turn left of Library. */
	{ UI_HOME_FACE_SETTINGS, UI_HOME_FACE_SYSTEM, UI_HOME_FACE_APPS,
		UI_HOME_FACE_SOURCE },
	/* Source two turns from Library either way once Apps is there. */
	{ UI_HOME_FACE_SETTINGS, UI_HOME_FACE_SOURCE, UI_HOME_FACE_SYSTEM,
		UI_HOME_FACE_APPS },
	/* A face named twice keeps its first side. */
	{ UI_HOME_FACE_SETTINGS, UI_HOME_FACE_SETTINGS, UI_HOME_FACE_SYSTEM,
		UI_HOME_FACE_SYSTEM },
	/* Values that aren't faces are none. */
	{ 7, 200, UI_HOME_FACE_COUNT, UI_HOME_FACE_SOURCE }
};

typedef struct {
	/* Each face's side, -1 for none; the ring, Library first. */
	int side[UI_HOME_FACE_COUNT];
	uiHomeFace_t ring[UI_HOME_RING_MAX];
	int count;
	uint8_t sides[UI_HOME_SIDE_COUNT];
	uint8_t absent;
} oracleLayout_t;

static oracleLayout_t oracleLayoutOf(const uint8_t wanted[UI_HOME_SIDE_COUNT],
	bool hasApps)
{
	oracleLayout_t layout;
	int side;
	int face;

	memset(&layout, 0, sizeof(layout));
	for(face = 0; face < (int)UI_HOME_FACE_COUNT; ++face) {
		layout.side[face] = -1;
	}
	layout.ring[0] = UI_HOME_FACE_LIBRARY;
	layout.count = 1;
	for(side = 0; side < UI_HOME_SIDE_COUNT; ++side) {
		face = wanted[side];
		layout.sides[side] = UI_HOME_FACE_LIBRARY;
		if(face < 1 || face >= (int)UI_HOME_FACE_COUNT ||
				layout.side[face] >= 0) {
			continue;
		}
		layout.side[face] = side;
		layout.sides[side] = (uint8_t)face;
		if(face == (int)UI_HOME_FACE_APPS && !hasApps) {
			layout.absent = (uint8_t)(1u << UI_HOME_FACE_APPS);
			continue;
		}
		layout.ring[layout.count++] = (uiHomeFace_t)face;
	}
	return layout;
}

static int oraclePlace(const oracleLayout_t *layout, uiHomeFace_t face)
{
	int place;

	for(place = 0; place < layout->count; ++place) {
		if(layout->ring[place] == face) {
			return place;
		}
	}
	return -1;
}

static uiHomeCapabilities_t layoutCaps(const uint8_t wanted[UI_HOME_SIDE_COUNT],
	bool hasSource, bool hasApps, bool isClassic)
{
	uiHomeCapabilities_t caps = capabilities(hasSource, true);

	caps.hasApps = hasApps;
	caps.customSides = true;
	memcpy(caps.sides, wanted, sizeof(caps.sides));
	return isClassic ? classic(caps) : caps;
}

/* Classic's pose for a side: the default face on that side has it. */
static const int8_t (*oracleSidePose(int side))[3]
{
	static const uiHomeFace_t onSide[UI_HOME_SIDE_COUNT] = {
		UI_HOME_FACE_SOURCE, UI_HOME_FACE_SETTINGS, UI_HOME_FACE_SYSTEM,
		UI_HOME_FACE_APPS
	};

	return oracleClassicPoses[side < 0 ? UI_HOME_FACE_LIBRARY : onSide[side]];
}

static uiHomeEffect_t oracleFaceActivate(uiHomeFace_t face)
{
	switch(face) {
		case UI_HOME_FACE_LIBRARY: return UI_HOME_EFFECT_OPEN_LIBRARY;
		case UI_HOME_FACE_SETTINGS: return UI_HOME_EFFECT_OPEN_SETTINGS;
		case UI_HOME_FACE_APPS: return UI_HOME_EFFECT_OPEN_APPS;
		default: return UI_HOME_EFFECT_NONE;
	}
}

static void testLayoutsInit(const oracleLayout_t *layout, uiHomeCapabilities_t caps)
{
	uiHomeState_t state;
	int face;
	int32_t ordinal;
	int sourcePlace = oraclePlace(layout, UI_HOME_FACE_SOURCE);

	UIHome_Init(&state, caps);
	CHECK(state.faceCount == layout->count);
	CHECK(UIHome_FaceCount(caps) == layout->count);
	CHECK(memcmp(state.sides, layout->sides, sizeof(state.sides)) == 0);
	CHECK(state.absent == layout->absent);
	CHECK(UIHome_LayoutValid(&state));
	CHECK(state.surface == UI_HOME_SURFACE_RING && state.selection == 0);
	CHECK(state.revision == 1u && state.turnAxis == UI_HOME_TURN_NONE);
	/* Infinite with no source starts on Source, when the ring has it. */
	if(!caps.hasSource && caps.style != UI_HOME_CUBE_CLASSIC &&
			sourcePlace > 0) {
		CHECK(state.face == UI_HOME_FACE_SOURCE);
		CHECK(state.turnOrdinal == sourcePlace);
	}
	else {
		CHECK(state.face == UI_HOME_FACE_LIBRARY && state.turnOrdinal == 0);
	}
	for(ordinal = -2 * layout->count; ordinal <= 2 * layout->count; ++ordinal) {
		CHECK(UIHome_RingFace(&state, ordinal) ==
			layout->ring[oraclePositiveModulo((int)ordinal, layout->count)]);
	}
	for(face = 0; face < (int)UI_HOME_FACE_COUNT; ++face) {
		CHECK(UIHome_RingIndex(&state, (uiHomeFace_t)face) ==
			oraclePlace(layout, (uiHomeFace_t)face));
		CHECK(UIHome_FaceSide(&state, (uiHomeFace_t)face) ==
			layout->side[face]);
	}
}

/* Infinite: Left and Up one face back round the ring, Right and Down one on;
 * Library alone doesn't turn. A on Library with no source turns to Source
 * a turn away, or opens the picker. */
static void testLayoutRing(const oracleLayout_t *layout, uiHomeCapabilities_t caps)
{
	int place;
	int inputIndex;

	for(place = 0; place < layout->count; ++place) {
		for(inputIndex = (int)UI_HOME_INPUT_NONE;
				inputIndex <= (int)UI_HOME_INPUT_RECENT; ++inputIndex) {
			uiHomeInput_t input = (uiHomeInput_t)inputIndex;
			uiHomeFace_t face = layout->ring[place];
			uiHomeState_t state;
			uiHomeState_t expected;
			uiHomeEffect_t effect;
			uiHomeEffect_t expectedEffect = UI_HOME_EFFECT_NONE;
			int step = 0;

			UIHome_Init(&state, caps);
			state.face = face;
			state.turnOrdinal = (int32_t)place;
			state.revision = 41u;
			expected = state;
			if(input >= UI_HOME_INPUT_LEFT && input <= UI_HOME_INPUT_DOWN) {
				step = layout->count < 2 ? 0 :
					input == UI_HOME_INPUT_LEFT || input == UI_HOME_INPUT_UP ? -1 : 1;
			}
			else if(input == UI_HOME_INPUT_ACTIVATE) {
				int sourcePlace = oraclePlace(layout, UI_HOME_FACE_SOURCE);

				if(face == UI_HOME_FACE_LIBRARY && !caps.hasSource) {
					if(sourcePlace == 1) step = 1;
					else if(sourcePlace > 1 && sourcePlace == layout->count - 1)
						step = -1;
					else expectedEffect = UI_HOME_EFFECT_CHANGE_SOURCE;
				}
				else if(face == UI_HOME_FACE_SOURCE || face == UI_HOME_FACE_SYSTEM) {
					expected.surface = face == UI_HOME_FACE_SOURCE ?
						UI_HOME_SURFACE_SOURCE : UI_HOME_SURFACE_SYSTEM;
					expected.revision++;
				}
				else {
					expectedEffect = oracleFaceActivate(face);
				}
			}
			else if(input == UI_HOME_INPUT_RECENT) {
				expectedEffect = UI_HOME_EFFECT_OPEN_RECENT;
			}
			if(step != 0) {
				expected.turnOrdinal += step;
				expected.face = layout->ring[oraclePositiveModulo(
					place + step, layout->count)];
				expected.turnAxis = input == UI_HOME_INPUT_UP ||
					input == UI_HOME_INPUT_DOWN ? UI_HOME_TURN_VERTICAL :
					UI_HOME_TURN_HORIZONTAL;
				expected.turnDirection = step;
				UIHome_OrientationTurn(&expected.orientation,
					expected.turnAxis, step);
				expected.revision++;
				if(input == UI_HOME_INPUT_ACTIVATE) {
					expected.surface = UI_HOME_SURFACE_SOURCE;
				}
			}
			effect = UIHome_Apply(&state, input, caps);
			CHECK(effect == expectedEffect);
			checkSameState(&state, &expected);
		}
	}
}

/* Classic: Library turns to the face on each side there is one on; that face
 * turns back the opposite way, or with B. */
static void testLayoutClassic(const oracleLayout_t *layout,
	uiHomeCapabilities_t caps)
{
	static const uiHomeInput_t out[UI_HOME_SIDE_COUNT] = {
		UI_HOME_INPUT_UP, UI_HOME_INPUT_LEFT, UI_HOME_INPUT_RIGHT,
		UI_HOME_INPUT_DOWN
	};
	static const uiHomeInput_t back[UI_HOME_SIDE_COUNT] = {
		UI_HOME_INPUT_DOWN, UI_HOME_INPUT_RIGHT, UI_HOME_INPUT_LEFT,
		UI_HOME_INPUT_UP
	};
	int place;
	int inputIndex;

	for(place = 0; place < layout->count; ++place) {
		for(inputIndex = (int)UI_HOME_INPUT_NONE;
				inputIndex <= (int)UI_HOME_INPUT_RECENT; ++inputIndex) {
			uiHomeInput_t input = (uiHomeInput_t)inputIndex;
			uiHomeFace_t face = layout->ring[place];
			int side = layout->side[face];
			uiHomeState_t state;
			uiHomeState_t expected;
			uiHomeEffect_t effect;
			uiHomeEffect_t expectedEffect = UI_HOME_EFFECT_NONE;
			uiHomeFace_t target = face;
			uiHomeInput_t turn = UI_HOME_INPUT_NONE;
			int index;

			UIHome_Init(&state, caps);
			state.face = face;
			state.turnOrdinal = (int32_t)place;
			memcpy(&state.orientation, oracleSidePose(side),
				sizeof(state.orientation));
			state.revision = 41u;
			expected = state;
			if(face == UI_HOME_FACE_LIBRARY) {
				for(index = 0; index < UI_HOME_SIDE_COUNT; ++index) {
					uiHomeFace_t there = (uiHomeFace_t)layout->sides[index];

					if(input == out[index] && oraclePlace(layout, there) > 0) {
						target = there;
						turn = input;
					}
				}
			}
			else if(input == back[side] || input == UI_HOME_INPUT_BACK) {
				target = UI_HOME_FACE_LIBRARY;
				turn = back[side];
			}
			if(input == UI_HOME_INPUT_ACTIVATE) {
				if(face == UI_HOME_FACE_LIBRARY && !caps.hasSource) {
					if(oraclePlace(layout, UI_HOME_FACE_SOURCE) > 0) {
						target = UI_HOME_FACE_SOURCE;
						turn = out[layout->side[UI_HOME_FACE_SOURCE]];
					}
					else {
						expectedEffect = UI_HOME_EFFECT_CHANGE_SOURCE;
					}
				}
				else if(face == UI_HOME_FACE_SOURCE || face == UI_HOME_FACE_SYSTEM) {
					expected.surface = face == UI_HOME_FACE_SOURCE ?
						UI_HOME_SURFACE_SOURCE : UI_HOME_SURFACE_SYSTEM;
					expected.revision++;
				}
				else {
					expectedEffect = oracleFaceActivate(face);
				}
			}
			else if(input == UI_HOME_INPUT_RECENT) {
				expectedEffect = UI_HOME_EFFECT_OPEN_RECENT;
			}
			if(turn != UI_HOME_INPUT_NONE) {
				expected.face = target;
				expected.turnOrdinal = (int32_t)oraclePlace(layout, target);
				memcpy(&expected.orientation,
					oracleSidePose(target == UI_HOME_FACE_LIBRARY ? -1 :
					layout->side[target]), sizeof(expected.orientation));
				expected.turnAxis = turn == UI_HOME_INPUT_UP ||
					turn == UI_HOME_INPUT_DOWN ? UI_HOME_TURN_VERTICAL :
					UI_HOME_TURN_HORIZONTAL;
				expected.turnDirection = turn == UI_HOME_INPUT_LEFT ||
					turn == UI_HOME_INPUT_UP ? -1 : 1;
				expected.surface = input == UI_HOME_INPUT_ACTIVATE ?
					UI_HOME_SURFACE_SOURCE : UI_HOME_SURFACE_RING;
				expected.selection = 0;
				expected.revision++;
			}
			effect = UIHome_Apply(&state, input, caps);
			CHECK(effect == expectedEffect);
			checkSameState(&state, &expected);
		}
	}
}

/* A new layout from Settings: the face in front stays where it is still in
 * the ring, its ordinal its new place and, in Classic, its new side in front;
 * otherwise Library. Published, not an input: the revision stays. */
static void testLayoutChanges(const oracleLayout_t *layouts, int layoutCount,
	bool hasApps, bool isClassic)
{
	int from;
	int to;
	int place;

	for(from = 0; from < layoutCount; ++from) {
		for(to = 0; to < layoutCount; ++to) {
			const oracleLayout_t *before = &layouts[from];
			const oracleLayout_t *after = &layouts[to];
			uiHomeCapabilities_t fromCaps = layoutCaps(layoutFixtures[from],
				true, hasApps, isClassic);
			uiHomeCapabilities_t toCaps = layoutCaps(layoutFixtures[to],
				true, hasApps, isClassic);

			for(place = 0; place < before->count; ++place) {
				uiHomeFace_t face = before->ring[place];
				int newPlace = oraclePlace(after, face);
				uiHomeState_t state;
				uiHomeState_t expected;

				UIHome_Init(&state, fromCaps);
				state.face = face;
				state.turnOrdinal = (int32_t)place;
				if(isClassic) {
					memcpy(&state.orientation,
						oracleSidePose(before->side[face]),
						sizeof(state.orientation));
				}
				state.revision = 41u;
				expected = state;
				memcpy(expected.sides, after->sides, sizeof(expected.sides));
				expected.absent = after->absent;
				expected.faceCount = after->count;
				if(newPlace < 0) {
					expected.face = UI_HOME_FACE_LIBRARY;
					newPlace = 0;
				}
				/* Nothing changed: nothing moves, the ordinal included. */
				if(from != to || newPlace != place) {
					expected.turnOrdinal = (int32_t)newPlace;
				}
				if(isClassic) {
					memcpy(&expected.orientation,
						oracleSidePose(after->side[expected.face]),
						sizeof(expected.orientation));
				}
				CHECK(UIHome_Apply(&state, UI_HOME_INPUT_NONE, toCaps) ==
					UI_HOME_EFFECT_NONE);
				checkSameState(&state, &expected);
				CHECK(UIHome_LayoutValid(&state));
			}
		}
	}
}

static void testLayouts(void)
{
	enum { FIXTURES = (int)(sizeof(layoutFixtures) / sizeof(layoutFixtures[0])) };
	oracleLayout_t layouts[FIXTURES];
	int fixture;
	int mask;

	for(mask = 0; mask < 8; ++mask) {
		bool hasSource = (mask & 1) != 0;
		bool hasApps = (mask & 2) != 0;
		bool isClassic = (mask & 4) != 0;

		for(fixture = 0; fixture < FIXTURES; ++fixture) {
			uiHomeCapabilities_t caps = layoutCaps(layoutFixtures[fixture],
				hasSource, hasApps, isClassic);

			layouts[fixture] = oracleLayoutOf(layoutFixtures[fixture], hasApps);
			testLayoutsInit(&layouts[fixture], caps);
			if(isClassic) {
				testLayoutClassic(&layouts[fixture], caps);
			}
			else {
				testLayoutRing(&layouts[fixture], caps);
			}
		}
		if(hasSource) {
			testLayoutChanges(layouts, FIXTURES, hasApps, isClassic);
		}
	}
	/* The fixtures' rings, written out where it matters. */
	CHECK(oracleLayoutOf(layoutFixtures[1], true).ring[4] ==
		UI_HOME_FACE_SETTINGS);
	CHECK(oracleLayoutOf(layoutFixtures[6], true).count == 1);
	CHECK(oracleLayoutOf(layoutFixtures[10], true).count == 3);
	CHECK(oracleLayoutOf(layoutFixtures[11], true).count == 2);
	/* The default sides are the capabilities' default. */
	{
		uiHomeState_t custom;
		uiHomeState_t plain;

		UIHome_Init(&custom, layoutCaps(layoutFixtures[0], true, true, false));
		UIHome_Init(&plain, withApps(capabilities(true, true)));
		checkSameState(&custom, &plain);
	}
	/* Malformed sides and absent faces are not a layout. */
	{
		uiHomeState_t state;

		UIHome_Init(&state, withApps(capabilities(true, true)));
		state.sides[1] = UI_HOME_FACE_SOURCE;
		CHECK(!UIHome_LayoutValid(&state));
		UIHome_Init(&state, withApps(capabilities(true, true)));
		state.sides[2] = UI_HOME_FACE_COUNT;
		CHECK(!UIHome_LayoutValid(&state));
		UIHome_Init(&state, withApps(capabilities(true, true)));
		state.absent = (uint8_t)(1u << UI_HOME_FACE_APPS);
		CHECK(!UIHome_LayoutValid(&state));
		state.faceCount = 4;
		CHECK(UIHome_LayoutValid(&state));
		state.sides[3] = UI_HOME_FACE_LIBRARY;
		CHECK(!UIHome_LayoutValid(&state));
	}
}

/* Y opens Settings from the ring in either cube, whatever face is in front
 * and wherever Settings is, or with Settings on no side; the lists and the
 * Restart question keep it. Nothing turns, nothing counts as an input. */
static void testSettingsInput(void)
{
	static const uint8_t noSettings[UI_HOME_SIDE_COUNT] = {
		UI_HOME_FACE_SOURCE, NO_FACE, UI_HOME_FACE_SYSTEM, UI_HOME_FACE_APPS
	};
	int mask;

	for(mask = 0; mask < 8; ++mask) {
		uiHomeCapabilities_t caps = (mask & 4) != 0 ?
			layoutCaps(noSettings, (mask & 1) != 0, true, (mask & 2) != 0) :
			withApps(capabilities((mask & 1) != 0, false));
		uiHomeState_t state;
		int place;

		if((mask & 2) != 0) {
			caps = classic(caps);
		}
		UIHome_Init(&state, caps);
		for(place = 0; place < state.faceCount; ++place) {
			uiHomeState_t at = state;
			uiHomeState_t before;

			at.face = UIHome_RingFace(&state, place);
			at.turnOrdinal = (int32_t)place;
			if((mask & 2) == 0) {
				before = at;
				CHECK(UIHome_Apply(&at, UI_HOME_INPUT_SETTINGS, caps) ==
					UI_HOME_EFFECT_OPEN_SETTINGS);
				checkSameState(&at, &before);
			}
			else if(at.face == UI_HOME_FACE_LIBRARY) {
				before = at;
				CHECK(UIHome_Apply(&at, UI_HOME_INPUT_SETTINGS, caps) ==
					UI_HOME_EFFECT_OPEN_SETTINGS);
				checkSameState(&at, &before);
			}
		}
	}
	{
		uiHomeCapabilities_t caps = capabilities(true, true);
		static const uiHomeSurface_t lists[] = {
			UI_HOME_SURFACE_SOURCE, UI_HOME_SURFACE_SYSTEM,
			UI_HOME_SURFACE_RESTART_CONFIRM
		};
		unsigned index;

		for(index = 0; index < sizeof(lists) / sizeof(lists[0]); ++index) {
			uiHomeState_t state = stateAt(
				lists[index] == UI_HOME_SURFACE_SOURCE ? UI_HOME_FACE_SOURCE :
				UI_HOME_FACE_SYSTEM, lists[index], 1,
				lists[index] == UI_HOME_SURFACE_SOURCE ? 1 : 3);
			uiHomeState_t before = state;

			CHECK(UIHome_Apply(&state, UI_HOME_INPUT_SETTINGS, caps) ==
				UI_HOME_EFFECT_NONE);
			checkSameState(&state, &before);
		}
	}
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
	/* With Apps between Library and System, six samples never restart. */
	UIHome_Init(&state, withApps(caps));
	restartReachableCount = 0u;
	bruteForceRestart(state, withApps(caps), sequence, 0);
	CHECK(restartReachableCount == 0u);

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
	testAppsFaceComesAndGoes();
	testEveryRingFaceAndInput();
	testNoSourceRedirect();
	testClassicEveryFaceAndInput();
	testClassicWalks();
	testSourceSurface();
	testSystemSurface();
	testRestartConfirmation();
	testLongRunOrdinalAndRevision();
	testExhaustiveReducerOracle();
	testInvalidInputAndStateOracle();
	testRestartReachabilityOracle();
	testLayouts();
	testSettingsInput();
	printf("ui_home: %u checks passed\n", checks);
	return EXIT_SUCCESS;
}
