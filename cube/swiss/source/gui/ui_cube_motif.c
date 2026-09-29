#include <math.h>
#include <stddef.h>
#include <string.h>

#include "ui_cube_motif.h"

/* A glyph leaves its side over 70 ms and arrives on its new one over 100. */
#define MOTIF_FADE_OUT_SECONDS 0.070f
#define MOTIF_FADE_IN_SECONDS 0.100f

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
	int count;

	if(out == NULL) return;
	if(home != NULL && (!UIHome_IsFace((int)home->face) ||
		(home->faceCount != UI_HOME_FACE_APPS &&
			home->faceCount != UI_HOME_FACE_COUNT) ||
		(int)home->face >= home->faceCount ||
		home->turnAxis < UI_HOME_TURN_NONE || home->turnAxis > UI_HOME_TURN_VERTICAL ||
		!UIHome_OrientationValid(&home->orientation))) home = NULL;
	count = home != NULL ? home->faceCount : UI_HOME_FACE_APPS;
	out->faceCount = count;
	for(int face = 0; face < UI_HOME_FACE_COUNT; face++) {
		/* The side of the band: 0 front, 1 next, 2 back and 3 previous. */
		int side = 2;
		bool shown = face < count;
		const uiHomeState_t *placed = home;
		bool pitch;

		if(shown) {
			int relative = home != NULL ?
				(face - (int)home->face + count) % count : face;

			if(relative == 0) side = 0;
			else if(relative == 1) side = 1;
			else if(relative == count - 1) side = 3;
			/* Five faces: the one two ahead has no side; the one two
			 * behind is at the back. */
			else if(count == UI_HOME_FACE_COUNT && relative == 2) shown = false;
		}
		/* A face with no side keeps one place, behind the authored front,
		 * however the cube turns: it never moves, and never draws. */
		if(!shown) placed = NULL;
		pitch = placed != NULL && placed->turnAxis == UI_HOME_TURN_VERTICAL;
		out->shown[face] = shown;
		for(int row = 0; row < 3; row++) {
			for(int column = 0; column < 3; column++) {
				float value = 0.0f;
				for(int k = 0; k < 3; k++) {
					int inverse = placed != NULL ? placed->orientation.m[k][row] : (k == row);
					int component = pitch ? vertical[side][k][column] :
						horizontal[side][k][column];
					value += (float)(inverse * component);
				}
				out->face[face][row][column] = value;
			}
		}
	}
}

/* Bases contain exact signed-permutation entries, never interpolated. */
static bool samePlace(const uiCubeMotifBasis_t *a, const uiCubeMotifBasis_t *b,
	int face)
{
	for(int row = 0; row < 3; row++)
		for(int column = 0; column < 3; column++)
			if(a->face[face][row][column] != b->face[face][row][column]) return false;
	return true;
}

static bool sameBasis(const uiCubeMotifBasis_t *a, const uiCubeMotifBasis_t *b)
{
	if(a->faceCount != b->faceCount) return false;
	for(int face = 0; face < UI_HOME_FACE_COUNT; face++)
		if(!samePlace(a, b, face) || a->shown[face] != b->shown[face]) return false;
	return true;
}

static void settle(uiCubeMotifState_t *state)
{
	state->basis = state->pending;
	for(int face = 0; face < UI_HOME_FACE_COUNT; face++)
		state->alpha[face] = state->basis.shown[face] ? 1.0f : 0.0f;
	state->changing = false;
}

void UICubeMotif_Reset(uiCubeMotifState_t *state)
{
	if(state == NULL) return;
	UICubeMotif_Build(NULL, &state->pending);
	settle(state);
}

void UICubeMotif_Request(uiCubeMotifState_t *state,
	const uiHomeState_t *home, uiMotionMode_t mode)
{
	if(state == NULL) return;
	UICubeMotif_Build(home, &state->pending);
	state->changing = !sameBasis(&state->basis, &state->pending);
	if(mode == UI_MOTION_OFF) settle(state);
}

void UICubeMotif_Update(uiCubeMotifState_t *state,
	float deltaSeconds, uiMotionMode_t mode)
{
	bool changing = false;

	if(state == NULL) return;
	if(mode == UI_MOTION_OFF) {
		settle(state);
		return;
	}
	if(!isfinite(deltaSeconds) || deltaSeconds <= 0.0f) return;
	if(deltaSeconds > 0.05f) deltaSeconds = 0.05f;
	/* Keep each drawn glyph attached to its physical side until it is
	 * invisible. A glyph whose side stays stays lit; a rapid direction
	 * change only replaces the one pending map, so it cannot reset an
	 * opacity, teleport a visible glyph, or grow a render queue. */
	state->basis.faceCount = state->pending.faceCount;
	for(int face = 0; face < UI_HOME_FACE_COUNT; face++) {
		float left = deltaSeconds;
		float target;

		if(!samePlace(&state->basis, &state->pending, face)) {
			float fadeTime = state->alpha[face] * MOTIF_FADE_OUT_SECONDS;

			if(left < fadeTime) {
				state->alpha[face] -= left / MOTIF_FADE_OUT_SECONDS;
				changing = true;
				continue;
			}
			left -= fadeTime;
			state->alpha[face] = 0.0f;
			memcpy(state->basis.face[face], state->pending.face[face],
				sizeof(state->basis.face[face]));
		}
		state->basis.shown[face] = state->pending.shown[face];
		target = state->basis.shown[face] ? 1.0f : 0.0f;
		if(state->alpha[face] < target) {
			state->alpha[face] += left / MOTIF_FADE_IN_SECONDS;
			if(state->alpha[face] > target) state->alpha[face] = target;
		}
		else if(state->alpha[face] > target) {
			state->alpha[face] -= left / MOTIF_FADE_OUT_SECONDS;
			if(state->alpha[face] < target) state->alpha[face] = target;
		}
		changing = changing || state->alpha[face] != target;
	}
	state->changing = changing;
}

bool UICubeMotif_Settled(const uiCubeMotifState_t *state)
{
	if(state == NULL) return true;
	if(state->changing || !sameBasis(&state->basis, &state->pending)) return false;
	for(int face = 0; face < UI_HOME_FACE_COUNT; face++)
		if(state->alpha[face] != (state->basis.shown[face] ? 1.0f : 0.0f)) return false;
	return true;
}
