#include <math.h>
#include <stddef.h>
#include <string.h>

#include "ui_cube_motif.h"

void UICubeMotif_Build(const uiHomeState_t *home, uiCubeMotifBasis_t *out)
{
	/* right/up/normal columns; vertical next comes from the bottom. */
	static const int8_t horizontal[4][3][3] = {
		{{1,0,0}, {0,1,0}, {0,0,1}},
		{{0,0,1}, {0,1,0}, {-1,0,0}},
		{{-1,0,0}, {0,1,0}, {0,0,-1}},
		{{0,0,-1}, {0,1,0}, {1,0,0}}
	};
	static const int8_t vertical[4][3][3] = {
		{{1,0,0}, {0,1,0}, {0,0,1}},
		{{1,0,0}, {0,0,-1}, {0,1,0}},
		{{1,0,0}, {0,-1,0}, {0,0,-1}},
		{{1,0,0}, {0,0,1}, {0,-1,0}}
	};
	if(out == NULL) return;
	if(home != NULL && (!UIHome_IsFace((int)home->face) ||
		home->turnAxis < UI_HOME_TURN_NONE || home->turnAxis > UI_HOME_TURN_VERTICAL ||
		!UIHome_OrientationValid(&home->orientation))) home = NULL;
	for(int face = 0; face < UI_HOME_FACE_COUNT; face++) {
		int relative = home != NULL ?
			(face - (int)home->face + UI_HOME_FACE_COUNT) % UI_HOME_FACE_COUNT : face;
		bool pitch = home != NULL && home->turnAxis == UI_HOME_TURN_VERTICAL;
		for(int row = 0; row < 3; row++) {
			for(int column = 0; column < 3; column++) {
				float value = 0.0f;
				for(int k = 0; k < 3; k++) {
					int inverse = home != NULL ? home->orientation.m[k][row] : (k == row);
					int component = pitch ? vertical[relative][k][column] :
						horizontal[relative][k][column];
					value += (float)(inverse * component);
				}
				out->face[face][row][column] = value;
			}
		}
	}
}

static bool sameBasis(const uiCubeMotifBasis_t *a, const uiCubeMotifBasis_t *b)
{
	/* Bases contain exact signed-permutation entries, never interpolated. */
	for(int face = 0; face < UI_HOME_FACE_COUNT; face++)
		for(int row = 0; row < 3; row++)
			for(int column = 0; column < 3; column++)
				if(a->face[face][row][column] != b->face[face][row][column]) return false;
	return true;
}

void UICubeMotif_Reset(uiCubeMotifState_t *state)
{
	if(state == NULL) return;
	UICubeMotif_Build(NULL, &state->basis);
	state->pending = state->basis;
	state->alpha = 1.0f;
	state->changing = false;
}

void UICubeMotif_Request(uiCubeMotifState_t *state,
	const uiHomeState_t *home, uiMotionMode_t mode)
{
	if(state == NULL) return;
	UICubeMotif_Build(home, &state->pending);
	state->changing = !sameBasis(&state->basis, &state->pending);
	if(mode == UI_MOTION_OFF) {
		state->basis = state->pending;
		state->alpha = 1.0f;
		state->changing = false;
	}
}

void UICubeMotif_Update(uiCubeMotifState_t *state,
	float deltaSeconds, uiMotionMode_t mode)
{
	if(state == NULL) return;
	if(mode == UI_MOTION_OFF) {
		state->basis = state->pending;
		state->alpha = 1.0f;
		state->changing = false;
		return;
	}
	if(!isfinite(deltaSeconds) || deltaSeconds <= 0.0f) return;
	if(deltaSeconds > 0.05f) deltaSeconds = 0.05f;
	/* Keep the currently drawn symbols attached to their physical faces until
	 * invisible. A rapid direction change only replaces one pending map; it
	 * cannot reset opacity, teleport a visible glyph, or grow a render queue. */
	if(state->changing) {
		float fadeTime = state->alpha * 0.070f;
		if(deltaSeconds < fadeTime) {
			state->alpha -= deltaSeconds / 0.070f;
			return;
		}
		deltaSeconds -= fadeTime;
		state->alpha = 0.0f;
		state->basis = state->pending;
		state->changing = false;
	}
	state->alpha += deltaSeconds / 0.100f;
	if(state->alpha > 1.0f) state->alpha = 1.0f;
}
