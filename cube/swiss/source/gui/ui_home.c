#include <stddef.h>
#include <limits.h>

#include "ui_home.h"

static const char *const faceLabels[UI_HOME_FACE_COUNT] = {
	"LIBRARY", "SOURCE", "SETTINGS", "SYSTEM"
};

static int positiveModulo(int value, int modulus)
{
	int result = value % modulus;
	return result < 0 ? result + modulus : result;
}

bool UIHome_IsFace(int face)
{
	return face >= UI_HOME_FACE_LIBRARY && face < UI_HOME_FACE_COUNT;
}

bool UIHome_IsSurface(int surface)
{
	return surface >= UI_HOME_SURFACE_RING &&
		surface < UI_HOME_SURFACE_COUNT;
}

uiHomeFace_t UIHome_FaceForTurn(int32_t turnOrdinal)
{
	return (uiHomeFace_t)positiveModulo((int)(turnOrdinal %
		(int32_t)UI_HOME_FACE_COUNT), UI_HOME_FACE_COUNT);
}

void UIHome_OrientationInit(uiHomeOrientation_t *orientation)
{
	if(orientation != NULL) {
		*orientation = (uiHomeOrientation_t) {{{1,0,0},{0,1,0},{0,0,1}}};
	}
}

bool UIHome_OrientationValid(const uiHomeOrientation_t *orientation)
{
	int row, col;
	int determinant;
	if(orientation == NULL) return false;
	for(row = 0; row < 3; ++row) {
		int rowCount = 0, colCount = 0;
		for(col = 0; col < 3; ++col) {
			int value = orientation->m[row][col];
			if(value < -1 || value > 1) return false;
			rowCount += value != 0;
			colCount += orientation->m[col][row] != 0;
		}
		if(rowCount != 1 || colCount != 1) return false;
	}
	determinant = orientation->m[0][0] * (orientation->m[1][1] *
		orientation->m[2][2] - orientation->m[1][2] * orientation->m[2][1]) -
		orientation->m[0][1] * (orientation->m[1][0] * orientation->m[2][2] -
		orientation->m[1][2] * orientation->m[2][0]) +
		orientation->m[0][2] * (orientation->m[1][0] * orientation->m[2][1] -
		orientation->m[1][1] * orientation->m[2][0]);
	return determinant == 1;
}

void UIHome_OrientationTurn(uiHomeOrientation_t *orientation,
	uiHomeTurnAxis_t axis, int direction)
{
	uiHomeOrientation_t before;
	int col, step = direction < 0 ? -1 : 1;
	if(!UIHome_OrientationValid(orientation) || direction == 0 ||
		(axis != UI_HOME_TURN_HORIZONTAL && axis != UI_HOME_TURN_VERTICAL)) return;
	before = *orientation;
	for(col = 0; col < 3; ++col) {
		/* Ry(-step*pi/2) or Rx(-step*pi/2), in screen coordinates. */
		if(axis == UI_HOME_TURN_HORIZONTAL) {
			orientation->m[0][col] = (int8_t)(-step * before.m[2][col]);
			orientation->m[2][col] = (int8_t)(step * before.m[0][col]);
		}
		else {
			orientation->m[1][col] = (int8_t)(step * before.m[2][col]);
			orientation->m[2][col] = (int8_t)(-step * before.m[1][col]);
		}
	}
}

void UIHome_OrientationMatrix(const uiHomeOrientation_t *orientation,
	float out[3][3])
{
	int row, col;
	bool valid = UIHome_OrientationValid(orientation);
	if(out == NULL) return;
	for(row = 0; row < 3; ++row) {
		for(col = 0; col < 3; ++col) {
			out[row][col] = valid ? (float)orientation->m[row][col] :
				(row == col ? 1.0f : 0.0f);
		}
	}
}

static void normalizeSelection(uiHomeState_t *state,
	uiHomeCapabilities_t capabilities)
{
	int rowCount = UIHome_RowCount(state->surface, capabilities);

	if(rowCount <= 0) {
		state->selection = 0;
	}
	else if(state->selection < 0 || state->selection >= rowCount) {
		state->selection = 0;
	}
}

void UIHome_Init(uiHomeState_t *state, uiHomeCapabilities_t capabilities)
{
	if(state == NULL) {
		return;
	}
	state->face = capabilities.hasSource ?
		UI_HOME_FACE_LIBRARY : UI_HOME_FACE_SOURCE;
	state->surface = UI_HOME_SURFACE_RING;
	state->selection = 0;
	state->turnOrdinal = (int32_t)state->face;
	state->revision = 1u;
	UIHome_OrientationInit(&state->orientation);
	state->turnAxis = UI_HOME_TURN_NONE;
	state->turnDirection = 0;
}

static void moveFace(uiHomeState_t *state, uiHomeTurnAxis_t axis, int direction)
{
	int step = direction < 0 ? -1 : 1;
	/* Keep the semantic counter bounded without signed overflow. Orientation
	 * is absolute and does not depend on the counter's accumulated magnitude. */
	if((state->turnOrdinal == INT32_MAX && step > 0) ||
		(state->turnOrdinal == INT32_MIN && step < 0)) {
		state->turnOrdinal %= (int32_t)UI_HOME_FACE_COUNT;
	}
	state->turnOrdinal += (int32_t)step;
	UIHome_OrientationTurn(&state->orientation, axis, step);
	state->turnAxis = axis;
	state->turnDirection = step;
	state->face = UIHome_FaceForTurn(state->turnOrdinal);
	state->surface = UI_HOME_SURFACE_RING;
	state->selection = 0;
	state->revision++;
}

static void moveRow(uiHomeState_t *state, int direction,
	uiHomeCapabilities_t capabilities)
{
	int rowCount = UIHome_RowCount(state->surface, capabilities);
	int next;

	if(rowCount <= 1) {
		state->selection = 0;
		return;
	}
	next = positiveModulo(state->selection + (direction < 0 ? -1 : 1),
		rowCount);
	if(next != state->selection) {
		state->selection = next;
		state->revision++;
	}
}

static void enterSurface(uiHomeState_t *state, uiHomeSurface_t surface,
	int selection)
{
	state->surface = surface;
	state->selection = selection;
	state->revision++;
}

static uiHomeEffect_t applyRing(uiHomeState_t *state, uiHomeInput_t input,
	uiHomeCapabilities_t capabilities)
{
	if(input == UI_HOME_INPUT_LEFT || input == UI_HOME_INPUT_UP) {
		moveFace(state, input == UI_HOME_INPUT_UP ?
			UI_HOME_TURN_VERTICAL : UI_HOME_TURN_HORIZONTAL, -1);
	}
	else if(input == UI_HOME_INPUT_RIGHT || input == UI_HOME_INPUT_DOWN) {
		moveFace(state, input == UI_HOME_INPUT_DOWN ?
			UI_HOME_TURN_VERTICAL : UI_HOME_TURN_HORIZONTAL, 1);
	}
	else if(input == UI_HOME_INPUT_RECENT) {
		return capabilities.hasRecent ?
			UI_HOME_EFFECT_OPEN_RECENT : UI_HOME_EFFECT_NONE;
	}
	else if(input == UI_HOME_INPUT_ACTIVATE) {
		switch(state->face) {
			case UI_HOME_FACE_LIBRARY:
				if(capabilities.hasSource) {
					return UI_HOME_EFFECT_OPEN_LIBRARY;
				}
				moveFace(state, UI_HOME_TURN_HORIZONTAL, 1);
				/* Keep one visible state revision per accepted input. */
				state->surface = UI_HOME_SURFACE_SOURCE;
				break;
			case UI_HOME_FACE_SOURCE:
				enterSurface(state, UI_HOME_SURFACE_SOURCE, 0);
				break;
			case UI_HOME_FACE_SETTINGS:
				return UI_HOME_EFFECT_OPEN_SETTINGS;
			case UI_HOME_FACE_SYSTEM:
				enterSurface(state, UI_HOME_SURFACE_SYSTEM, 0);
				break;
			default:
				break;
		}
	}
	return UI_HOME_EFFECT_NONE;
}

static uiHomeEffect_t applySource(uiHomeState_t *state,
	uiHomeInput_t input, uiHomeCapabilities_t capabilities)
{
	if(input == UI_HOME_INPUT_UP) {
		moveRow(state, -1, capabilities);
	}
	else if(input == UI_HOME_INPUT_DOWN) {
		moveRow(state, 1, capabilities);
	}
	else if(input == UI_HOME_INPUT_BACK) {
		enterSurface(state, UI_HOME_SURFACE_RING, 0);
	}
	else if(input == UI_HOME_INPUT_ACTIVATE) {
		if(state->selection == 0) {
			return UI_HOME_EFFECT_CHANGE_SOURCE;
		}
		if(state->selection == 1 && capabilities.hasSource) {
			return UI_HOME_EFFECT_REFRESH;
		}
	}
	return UI_HOME_EFFECT_NONE;
}

static uiHomeEffect_t applySystem(uiHomeState_t *state,
	uiHomeInput_t input, uiHomeCapabilities_t capabilities)
{
	if(input == UI_HOME_INPUT_UP) {
		moveRow(state, -1, capabilities);
	}
	else if(input == UI_HOME_INPUT_DOWN) {
		moveRow(state, 1, capabilities);
	}
	else if(input == UI_HOME_INPUT_BACK) {
		enterSurface(state, UI_HOME_SURFACE_RING, 0);
	}
	else if(input == UI_HOME_INPUT_ACTIVATE) {
		if(state->selection == 0) {
			return UI_HOME_EFFECT_OPEN_INFO;
		}
		if(state->selection == 1) {
			/* Cancellation is always the initially selected confirmation. */
			enterSurface(state, UI_HOME_SURFACE_RESTART_CONFIRM, 0);
		}
	}
	return UI_HOME_EFFECT_NONE;
}

static uiHomeEffect_t applyRestartConfirm(uiHomeState_t *state,
	uiHomeInput_t input, uiHomeCapabilities_t capabilities)
{
	if(input == UI_HOME_INPUT_LEFT || input == UI_HOME_INPUT_UP) {
		moveRow(state, -1, capabilities);
	}
	else if(input == UI_HOME_INPUT_RIGHT || input == UI_HOME_INPUT_DOWN) {
		moveRow(state, 1, capabilities);
	}
	else if(input == UI_HOME_INPUT_BACK) {
		enterSurface(state, UI_HOME_SURFACE_SYSTEM, 1);
	}
	else if(input == UI_HOME_INPUT_ACTIVATE) {
		if(state->selection == 0) {
			enterSurface(state, UI_HOME_SURFACE_SYSTEM, 1);
		}
		else if(state->selection == 1) {
			return UI_HOME_EFFECT_RESTART;
		}
	}
	return UI_HOME_EFFECT_NONE;
}

uiHomeEffect_t UIHome_Apply(uiHomeState_t *state, uiHomeInput_t input,
	uiHomeCapabilities_t capabilities)
{
	if(state == NULL || !UIHome_IsFace((int)state->face) ||
		!UIHome_IsSurface((int)state->surface)) {
		return UI_HOME_EFFECT_NONE;
	}
	normalizeSelection(state, capabilities);
	switch(state->surface) {
		case UI_HOME_SURFACE_RING:
			return applyRing(state, input, capabilities);
		case UI_HOME_SURFACE_SOURCE:
			return applySource(state, input, capabilities);
		case UI_HOME_SURFACE_SYSTEM:
			return applySystem(state, input, capabilities);
		case UI_HOME_SURFACE_RESTART_CONFIRM:
			return applyRestartConfirm(state, input, capabilities);
		default:
			return UI_HOME_EFFECT_NONE;
	}
}

int UIHome_RowCount(uiHomeSurface_t surface,
	uiHomeCapabilities_t capabilities)
{
	switch(surface) {
		case UI_HOME_SURFACE_SOURCE:
			return capabilities.hasSource ? 2 : 1;
		case UI_HOME_SURFACE_SYSTEM:
		case UI_HOME_SURFACE_RESTART_CONFIRM:
			return 2;
		default:
			return 0;
	}
}

bool UIHome_RowEnabled(uiHomeSurface_t surface, int row,
	uiHomeCapabilities_t capabilities)
{
	return row >= 0 && row < UIHome_RowCount(surface, capabilities);
}

const char *UIHome_FaceLabel(uiHomeFace_t face)
{
	if(!UIHome_IsFace((int)face)) {
		return "";
	}
	return faceLabels[(int)face];
}

const char *UIHome_PrimaryHint(uiHomeFace_t face,
	uiHomeCapabilities_t capabilities)
{
	switch(face) {
		case UI_HOME_FACE_LIBRARY:
			return capabilities.hasSource ? "A  OPEN" : "A  SELECT SOURCE";
		case UI_HOME_FACE_SOURCE:
		case UI_HOME_FACE_SYSTEM:
			return "A  ENTER";
		case UI_HOME_FACE_SETTINGS:
			return "A  OPEN";
		default:
			return "";
	}
}

const char *UIHome_SurfaceTitle(uiHomeSurface_t surface)
{
	switch(surface) {
		case UI_HOME_SURFACE_SOURCE:
			return "SOURCE";
		case UI_HOME_SURFACE_SYSTEM:
			return "SYSTEM";
		case UI_HOME_SURFACE_RESTART_CONFIRM:
			return "RESTART INDIGO?";
		default:
			return "HOME";
	}
}

const char *UIHome_RowLabel(uiHomeSurface_t surface, int row)
{
	if(row < 0 || row >= 2) {
		return "";
	}
	switch(surface) {
		case UI_HOME_SURFACE_SOURCE:
			return row == 0 ? "CHANGE SOURCE" : "REFRESH LIBRARY";
		case UI_HOME_SURFACE_SYSTEM:
			return row == 0 ? "SYSTEM INFORMATION" : "RESTART INDIGO";
		case UI_HOME_SURFACE_RESTART_CONFIRM:
			return row == 0 ? "CANCEL" : "RESTART";
		default:
			return "";
	}
}
