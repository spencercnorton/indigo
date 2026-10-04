#include <math.h>
#include <stddef.h>
#include <string.h>

#include "ui_save_cubes.h"

/* The window's edges: a row fades out over half a cell past them. */
#define CUBES_FADE_TOP 112.0f
#define CUBES_FADE_BOTTOM 336.0f
#define CUBES_FADE_SPAN 28.0f
#define CUBES_GROW_RESPONSE 45.0f	/* a step grows the cube in about 0.1 s */
#define CUBES_SCROLL_RESPONSE 25.0f	/* a row in about 0.2 s */
#define CUBES_BOB_RATE 1.8f		/* radians a second: a bob every 3.5 s */
/* The IPL's selected-cube wobble, re-derived from its published numbers:
 * 35 and 70 units of a 65536-unit turn a step, 7 steps a 60 Hz frame. */
#define CUBES_WOBBLE_RATE 1.409345f
#define CUBES_WOBBLE_EASE 0.3f		/* seconds the wobble takes to grow in */
#define CUBES_WOBBLE_X 0.0335f		/* radians, at twice the rate */
#define CUBES_WOBBLE_YZ 0.0958f
#define CUBES_WOBBLE_COS 0.132406f	/* cos and sin of the Y turn's 1.438 lag */
#define CUBES_WOBBLE_SIN 0.991196f
#define CUBES_DRIFT_X (0.30f * UI_SAVE_CUBES_FACE)
#define CUBES_DRIFT_Y (0.15f * UI_SAVE_CUBES_FACE)
#define CUBES_FLOATING 0.002f		/* grown past this, a cube may overlap */
#define CUBES_PI 3.14159265f
#define CUBES_MIDDLE_X UI_SAVE_CUBES_VANISH_X	/* where the screen opens from */
#define CUBES_MIDDLE_Y UI_SAVE_CUBES_VANISH_Y
/* Opening (Full): the Home cube goes back into the distance, the graph paper
 * comes, each stack's cubes spiral out of the middle, one after another,
 * then the words come and the focused cube grows. A listing that comes
 * later than CUBES_ENTRY pops in instead; Reduced fades it all in. */
#define CUBES_HANDOVER 0.3f
#define CUBES_PAPER_IN 0.12f
#define CUBES_PAPER_TIME 0.33f
#define CUBES_SPIRAL_START 0.15f
#define CUBES_SPIRAL_STAGGER 0.006f
#define CUBES_SPIRAL_TIME 0.55f
#define CUBES_SPIRAL_TURN 1.8f		/* radians, unwound as it comes */
#define CUBES_ENTRY 1.2f
#define CUBES_CHROME_IN 0.95f
#define CUBES_CHROME_TIME 0.3f
#define CUBES_FOCUS_GROWS 1.1f
#define CUBES_POP_STAGGER 0.01f
#define CUBES_POP_TIME 0.25f
#define CUBES_FADE_TIME 0.25f
/* Leaving (Full): the words go, the cubes fall into the middle and fade,
 * and only once they are gone does the Home cube come back from the
 * distance, so the two never overlap. */
#define CUBES_LEAVE 0.45f
#define CUBES_LEAVE_REDUCED 0.2f
#define CUBES_LEAVE_CHROME 0.12f
#define CUBES_COLLAPSE 0.25f
#define CUBES_COLLAPSE_FADE 0.1f
#define CUBES_RETURN_START CUBES_COLLAPSE
/* A save gone or come: the cubes after it slide a cell, each a little
 * after the one before, as the IPL's ripple. */
#define CUBES_CASCADE_TIME 0.4f
#define CUBES_CASCADE_STAGGER 0.03f
/* Copy and Move: an arc dipping toward the info bar, growing toward the
 * viewer and swinging its face toward where it goes, then a hover until the
 * card has the save. */
#define CUBES_FLIGHT 0.6f
#define CUBES_ARC_DIP 110.0f
#define CUBES_ARC_FLOOR 400.0f
#define CUBES_ARC_GROWTH 0.5f
#define CUBES_ARC_RISE 60.0f
#define CUBES_ARC_YAW 0.6f		/* radians toward where it goes, mid-flight */
#define CUBES_ARC_TILT 0.35f		/* down into the dip, then up out of it */
#define CUBES_HOVER_EASE 0.25f
#define CUBES_HOVER_GROWTH 0.15f
#define CUBES_HOVER_PULSE 0.08f
#define CUBES_HOVER_TURN 0.15f
#define CUBES_HOVER_RATE 1.571f
#define CUBES_GLIDE 0.35f		/* Reduced: a straight line */
#define CUBES_LAND 0.2f
#define CUBES_BACK 0.45f
#define CUBES_SHAKE 0.3f
#define CUBES_SHAKE_PX 5.0f
#define CUBES_SHAKE_RATE 48.0f
/* Erase: the cube shrinks and turns, trembles until the card has done it,
 * then bursts into pale pieces that fly out, tumbling, slow, fall and fade.
 * The saves after it wait for the burst to open before they slide back,
 * and the one that takes its cell grows into focus only once the pieces
 * have mostly flown. */
#define CUBES_ERASE_SHRINK 0.1f
#define CUBES_ERASE_SCALE 0.9f
#define CUBES_ERASE_TURN 0.6f
#define CUBES_ERASE_BURST 0.5f
#define CUBES_ERASE_SETTLE 0.15f	/* the saves after it wait */
#define CUBES_ERASE_FOCUS 0.35f		/* the next save grows into focus */
#define CUBES_ERASE_FADE 0.2f		/* Reduced: it fades instead */
#define CUBES_ERASE_REGROW 0.15f
#define CUBES_BIT_REACH 95.0f		/* px the farthest piece flies */
#define CUBES_BIT_FALL 40.0f
#define CUBES_BIT_SIZE 0.16f		/* a piece's half, of the cube's face */
#define CUBES_BIT_SPIN 7.0f		/* radians a second */
#define CUBES_GHOST_PERIOD 0.9f
#define CUBES_MENU_OPEN 0.08f
#define CUBES_MENU_CLOSE 0.1f
#define CUBES_MENU_RESPONSE 25.0f
#define CUBES_MESSAGE_IN 0.1f
#define CUBES_MESSAGE_OUT 0.15f

/* Each cube's place in the bob: steps of the golden angle, so neighbours
 * across are 137 degrees apart and neighbours down nearly opposite. */
static const float bobPhase[16][2] = {
	{1.000000f, 0.000000f}, {-0.737369f, 0.675490f},
	{0.087426f, -0.996171f}, {0.608439f, 0.793601f},
	{-0.984713f, -0.174182f}, {0.843755f, -0.536728f},
	{-0.259604f, 0.965715f}, {-0.460907f, -0.887448f},
	{0.939321f, 0.343039f}, {-0.924346f, 0.381556f},
	{0.423846f, -0.905734f}, {0.299284f, 0.954164f},
	{-0.865211f, -0.501408f}, {0.976676f, -0.214719f},
	{-0.575129f, 0.818062f}, {-0.128511f, -0.991708f}
};

/* ------------------------------------------------------------------------
 * Faces.
 * --------------------------------------------------------------------- */

/* Corner i of the cube is x = i & 1, y = i & 2, far = i & 4 (x right, y
 * down, the front toward the viewer); 8 .. 11 are the icon's corners on the
 * front. Each face's corners run clockwise seen from outside. */
static const uint8_t cubesFace[6][4] = {
	{0, 1, 3, 2},	/* front */
	{4, 6, 7, 5},	/* back */
	{1, 5, 7, 3},	/* right */
	{4, 0, 2, 6},	/* left */
	{4, 5, 1, 0},	/* top */
	{2, 3, 7, 6}	/* bottom */
};
static const uint8_t cubesFaceRole[6] = {
	UI_SAVE_CUBES_ROLE_BODY, UI_SAVE_CUBES_ROLE_BODY,
	UI_SAVE_CUBES_ROLE_SIDE_EDGE, UI_SAVE_CUBES_ROLE_SIDE_EDGE,
	UI_SAVE_CUBES_ROLE_SIDE_TOP, UI_SAVE_CUBES_ROLE_SIDE_BOTTOM
};

static float cubesArea(const float *x, const float *y, const uint8_t *corner)
{
	float area = 0.0f;
	int i;

	for(i = 0; i < 4; i++) {
		int a = corner[i], b = corner[(i + 1) & 3];
		area += x[a] * y[b] - x[b] * y[a];
	}
	return 0.5f * area;
}

static void cubesQuad(uiSaveCubesQuad_t *quad, const float *x, const float *y,
	const uint8_t *corner, uiSaveCubesRole_t role)
{
	int i;

	for(i = 0; i < 4; i++) {
		quad->x[i] = x[corner[i]];
		quad->y[i] = y[corner[i]];
	}
	quad->role = (uint8_t)role;
}

/* A rectangle u0..u1 across and v0..v1 down the icon's square, as it lies on
 * the projected front: exact at rest, within a pixel turned. */
static void cubesInset(uiSaveCubesQuad_t *quad, const float *x, const float *y,
	float u0, float v0, float u1, float v1)
{
	const float u[4] = {u0, u1, u1, u0};
	const float v[4] = {v0, v0, v1, v1};
	int i;

	for(i = 0; i < 4; i++) {
		float top = 1.0f - v[i];

		quad->x[i] = top * ((1.0f - u[i]) * x[8] + u[i] * x[9]) +
			v[i] * ((1.0f - u[i]) * x[10] + u[i] * x[11]);
		quad->y[i] = top * ((1.0f - u[i]) * y[8] + u[i] * y[9]) +
			v[i] * ((1.0f - u[i]) * y[10] + u[i] * y[11]);
	}
	quad->role = UI_SAVE_CUBES_ROLE_GLYPH;
}

int UISaveCubes_Faces(const uiSaveCube_t *cube, float left, float right,
	uiSaveCubesQuad_t out[UI_SAVE_CUBES_QUADS])
{
	static const uint8_t rims[4][4] = {
		{0, 1, 9, 8}, {1, 3, 11, 9}, {3, 2, 10, 11}, {2, 0, 8, 10}
	};
	static const uint8_t rimRole[4] = {
		UI_SAVE_CUBES_ROLE_RIM_TOP, UI_SAVE_CUBES_ROLE_RIM_RIGHT,
		UI_SAVE_CUBES_ROLE_RIM_BOTTOM, UI_SAVE_CUBES_ROLE_RIM_LEFT
	};
	static const uint8_t inside[4] = {8, 9, 11, 10};
	float x[12], y[12];
	float half, scale, middleX, middleY, reach;
	int i, n = 0;

	if(cube == NULL || cube->alpha == 0 || !(cube->half > 0.0f) ||
		!(UI_SAVE_CUBES_EYE - cube->z >= 1.0f)) {
		return 0;
	}
	half = cube->half;
	scale = UI_SAVE_CUBES_EYE / (UI_SAVE_CUBES_EYE - cube->z);
	middleX = UI_SAVE_CUBES_VANISH_X + (cube->x - UI_SAVE_CUBES_VANISH_X) * scale;
	middleY = UI_SAVE_CUBES_VANISH_Y + (cube->y - UI_SAVE_CUBES_VANISH_Y) * scale;
	reach = 1.6f * half * scale;
	if(middleX + reach < left || middleX - reach > right ||
		middleY + reach < 0.0f || middleY - reach > 480.0f) {
		return 0;
	}
	/* One divide a point: about its middle, turned, then pushed back so the
	 * front lies at the cube's z. */
	for(i = 0; i < 12; i++) {
		float edge = i < 8 ? half : half * (UI_SAVE_CUBES_ICON / UI_SAVE_CUBES_FACE);
		float q[3] = {(i & 1) ? edge : -edge, (i & 2) ? edge : -edge,
			(i & 4) ? -half : half};
		float p[3], depth;

		if(cube->turn != NULL) {
			const float *m = cube->turn;

			p[0] = m[0] * q[0] + m[1] * q[1] + m[2] * q[2];
			p[1] = m[3] * q[0] + m[4] * q[1] + m[5] * q[2];
			p[2] = m[6] * q[0] + m[7] * q[1] + m[8] * q[2];
		}
		else {
			memcpy(p, q, sizeof(p));
		}
		depth = UI_SAVE_CUBES_EYE - (p[2] + cube->z - half);
		if(depth < 1.0f) {
			return 0;
		}
		scale = UI_SAVE_CUBES_EYE / depth;
		x[i] = UI_SAVE_CUBES_VANISH_X + (p[0] + cube->x - UI_SAVE_CUBES_VANISH_X) * scale;
		y[i] = UI_SAVE_CUBES_VANISH_Y + (p[1] + cube->y - UI_SAVE_CUBES_VANISH_Y) * scale;
	}
	for(i = 1; i < 6; i++) {
		if(cubesArea(x, y, cubesFace[i]) > 0.5f) {
			cubesQuad(&out[n++], x, y, cubesFace[i], (uiSaveCubesRole_t)cubesFaceRole[i]);
		}
	}
	if(cubesArea(x, y, cubesFace[0]) <= 0.5f) {
		return n;
	}
	if(cube->kind == UI_SAVE_CUBES_KIND_EMPTY) {
		cubesQuad(&out[n++], x, y, cubesFace[0], UI_SAVE_CUBES_ROLE_BODY);
		return n;
	}
	for(i = 0; i < 4; i++) {
		cubesQuad(&out[n++], x, y, rims[i], (uiSaveCubesRole_t)rimRole[i]);
	}
	cubesQuad(&out[n++], x, y, inside, UI_SAVE_CUBES_ROLE_BODY);
	if(cube->kind == UI_SAVE_CUBES_KIND_FOLDER) {
		/* A folder: its tab, then its body. */
		cubesInset(&out[n++], x, y, 0.12f, 0.18f, 0.46f, 0.34f);
		cubesInset(&out[n++], x, y, 0.12f, 0.30f, 0.88f, 0.80f);
	}
	else if(cube->icon != NULL) {
		cubesQuad(&out[n++], x, y, inside, UI_SAVE_CUBES_ROLE_ICON);
	}
	return n;
}

void UISaveCubes_Colour(int shade, int role, uint8_t rgba[4])
{
	/* Each shade's body, then its rim, in Indigo's blue-violet: Menu Color
	 * Indigo comes out close to the IPL's blue, and no other color clips
	 * them (a deeper blue would lose its brightness turning green). */
	static const uint8_t palette[UI_SAVE_CUBES_SHADES][2][4] = {
		{{39, 46, 141, 235}, {78, 104, 236, 240}},
		{{196, 222, 255, 245}, {226, 236, 255, 250}},
		{{20, 22, 74, 160}, {20, 22, 74, 160}},
		{{150, 196, 250, 200}, {150, 196, 250, 200}},
		{{51, 73, 202, 240}, {51, 73, 202, 240}}
	};
	/* Per part: rim (1) or body (0), and its light in percent. */
	static const uint8_t light[UI_SAVE_CUBES_ROLES][2] = {
		{0, 100}, {1, 125}, {1, 100}, {1, 85}, {1, 70},
		{0, 115}, {0, 80}, {0, 60}, {1, 125}
	};
	const uint8_t *color;
	int i;

	if(shade < 0 || shade >= UI_SAVE_CUBES_SHADES) {
		shade = UI_SAVE_CUBES_SHADE_SAVE;
	}
	if(role < 0 || role >= UI_SAVE_CUBES_ROLES) {
		role = UI_SAVE_CUBES_ROLE_BODY;
	}
	color = palette[shade][light[role][0]];
	for(i = 0; i < 3; i++) {
		unsigned value = (unsigned)color[i] * light[role][1] / 100u;

		rgba[i] = (uint8_t)(value > 255u ? 255u : value);
	}
	rgba[3] = color[3];
}

/* ------------------------------------------------------------------------
 * Motion.
 * --------------------------------------------------------------------- */
const uint8_t *UISaveCubes_Icon(const uiSaveCubesCell_t *cell, float seconds,
	uiMotionMode_t mode)
{
	int frame;

	if(cell == NULL || cell->texels == NULL || cell->art == NULL ||
		cell->kind != UI_SAVE_CUBES_KIND_SAVE) {
		return NULL;
	}
	frame = UISaves_ArtStep(cell->art, mode == UI_MOTION_OFF || !(seconds > 0.0f) ?
		0u : (uint32_t)(seconds * (float)UI_SAVES_ART_TICK_HZ));
	return frame >= 0 ? cell->texels + (size_t)frame * UI_SAVES_ICON_BYTES : NULL;
}

/* sin and cos of an angle under 0.1 radians, to a millionth, with no call. */
static void cubesSmallTurn(float angle, float *sine, float *cosine)
{
	float square = angle * angle;

	*sine = angle * (1.0f - square / 6.0f);
	*cosine = 1.0f - square * 0.5f * (1.0f - square / 12.0f);
}

/* The IPL's slow wobble, Z after Y after X, amount 0 .. 1 of it; drift is
 * where the cube wanders meanwhile. Two calls: the doubled rate's terms come
 * from the single rate's. */
static void cubesWobble(float seconds, float amount, float turn[9],
	float drift[2])
{
	float sine1 = sinf(CUBES_WOBBLE_RATE * seconds);
	float cosine1 = cosf(CUBES_WOBBLE_RATE * seconds);
	float sine2 = 2.0f * sine1 * cosine1;
	float cosine2 = 2.0f * cosine1 * cosine1 - 1.0f;
	float sx, cx, sy, cy, sz, cz;

	cubesSmallTurn(amount * CUBES_WOBBLE_X * cosine2, &sx, &cx);
	cubesSmallTurn(amount * CUBES_WOBBLE_YZ * (cosine1 * CUBES_WOBBLE_COS +
		sine1 * CUBES_WOBBLE_SIN), &sy, &cy);
	cubesSmallTurn(amount * CUBES_WOBBLE_YZ * cosine1, &sz, &cz);
	turn[0] = cz * cy;
	turn[1] = cz * sy * sx - sz * cx;
	turn[2] = cz * sy * cx + sz * sx;
	turn[3] = sz * cy;
	turn[4] = sz * sy * sx + cz * cx;
	turn[5] = sz * sy * cx - cz * sx;
	turn[6] = -sy;
	turn[7] = cy * sx;
	turn[8] = cy * cx;
	drift[0] = -amount * CUBES_DRIFT_X * cosine1;
	drift[1] = amount * CUBES_DRIFT_Y * sine2;
}

static float cubesClamp(float value)
{
	return value > 0.0f ? (value < 1.0f ? value : 1.0f) : 0.0f;
}

static float cubesLerp(float a, float b, float amount)
{
	return a + (b - a) * amount;
}

/* sin and cos of an angle within 2 radians, to a thousandth, with no call:
 * the opening's spiral turns every cube by its own angle, and a flight's
 * cube swings. */
static void cubesTurn(float angle, float *sine, float *cosine)
{
	float square = angle * angle;

	*sine = angle * (1.0f - square / 6.0f * (1.0f - square / 20.0f *
		(1.0f - square / 42.0f)));
	*cosine = 1.0f - square / 2.0f * (1.0f - square / 12.0f *
		(1.0f - square / 30.0f * (1.0f - square / 56.0f)));
}

/* A turn of angle about the up axis, from its sine and cosine. */
static void cubesYaw(float sine, float cosine, float turn[9])
{
	turn[0] = cosine; turn[1] = 0.0f; turn[2] = sine;
	turn[3] = 0.0f; turn[4] = 1.0f; turn[5] = 0.0f;
	turn[6] = -sine; turn[7] = 0.0f; turn[8] = cosine;
}

/* A tip of tilt about the across axis (up for more), then a turn of yaw
 * about the up axis, each within 2 radians. */
static void cubesSwing(float yaw, float tilt, float turn[9])
{
	float sy, cy, sx, cx;

	cubesTurn(yaw, &sy, &cy);
	cubesTurn(tilt, &sx, &cx);
	turn[0] = cy; turn[1] = sy * sx; turn[2] = sy * cx;
	turn[3] = 0.0f; turn[4] = cx; turn[5] = -sx;
	turn[6] = -sy; turn[7] = cy * sx; turn[8] = cy * cx;
}

void UISaveCubes_Where(int stack, int cell, float first, float *x, float *y)
{
	*x = UI_SAVE_CUBES_STACK_X + (float)stack * UI_SAVE_CUBES_STACK_GAP +
		((float)(cell % UI_SAVE_CUBES_COLUMNS) - 1.5f) * UI_SAVE_CUBES_PITCH;
	*y = UI_SAVE_CUBES_TOP_Y + ((float)(cell / UI_SAVE_CUBES_COLUMNS) - first) *
		UI_SAVE_CUBES_PITCH;
}

float UISaveCubes_OpSeconds(int kind, int phase, uiMotionMode_t mode)
{
	bool full = mode == UI_MOTION_FULL;

	if(mode == UI_MOTION_OFF || kind == UI_SAVE_CUBES_OP_NONE) {
		return 0.0f;
	}
	if(kind == UI_SAVE_CUBES_OP_ERASE) {
		return phase == UI_SAVE_CUBES_GO ? (full ? CUBES_ERASE_SHRINK : 0.0f) :
			phase == UI_SAVE_CUBES_LAND ? (full ? CUBES_ERASE_BURST : CUBES_ERASE_FADE) :
			(full ? CUBES_ERASE_REGROW : 0.0f);
	}
	return phase == UI_SAVE_CUBES_GO ? (full ? CUBES_FLIGHT : CUBES_GLIDE) :
		phase == UI_SAVE_CUBES_LAND ? CUBES_LAND :
		(full ? CUBES_BACK + CUBES_SHAKE : CUBES_GLIDE);
}

float UISaveCubes_LeaveSeconds(uiMotionMode_t mode)
{
	return mode == UI_MOTION_FULL ? CUBES_LEAVE :
		mode == UI_MOTION_REDUCED ? CUBES_LEAVE_REDUCED : 0.0f;
}

bool UISaveCubes_MessageHolds(float seconds, bool pressed)
{
	return !pressed && seconds < UI_SAVE_CUBES_MESSAGE;
}

void UISaveCubes_MenuBox(float cubeX, float cubeY, float width, int items,
	bool titled, uiSaveCubesBox_t *box)
{
	float title = titled ? UI_SAVE_CUBES_MENU_TITLE + 4.0f : 0.0f;
	float top = cubeY - 46.0f;

	box->width = width > UI_SAVE_CUBES_MENU_WIDTH ? width : UI_SAVE_CUBES_MENU_WIDTH;
	box->height = 16.0f + UI_SAVE_CUBES_MENU_PITCH * (float)(items > 0 ? items : 0);
	/* Past the middle, or where it would run off the stage (a wide one
	 * beside the right stack's first column), the box opens to the left. */
	box->x = cubeX > 400.0f || cubeX + 44.0f + box->width > 640.0f ?
		cubeX - 44.0f - box->width : cubeX + 44.0f;
	/* Inside the window, above the info bar. */
	if(top > 336.0f - title - box->height) {
		top = 336.0f - title - box->height;
	}
	if(top < 112.0f) {
		top = 112.0f;
	}
	box->titleY = top;
	box->y = top + title;
}

uiSaveCubesChange_t UISaveCubes_Change(const uint32_t *before, int beforeCount,
	const uint32_t *after, int afterCount, int *at)
{
	int same = 0, k;

	while(same < beforeCount && same < afterCount && before[same] == after[same]) {
		same++;
	}
	*at = same;
	if(afterCount == beforeCount) {
		return same == afterCount ? UI_SAVE_CUBES_SAME : UI_SAVE_CUBES_NEW;
	}
	if(afterCount == beforeCount - 1) {
		for(k = same; k < afterCount; k++) {
			if(after[k] != before[k + 1]) {
				return UI_SAVE_CUBES_NEW;
			}
		}
		return UI_SAVE_CUBES_CLOSED;
	}
	if(afterCount == beforeCount + 1) {
		for(k = same; k < beforeCount; k++) {
			if(after[k + 1] != before[k]) {
				return UI_SAVE_CUBES_NEW;
			}
		}
		return UI_SAVE_CUBES_OPENED;
	}
	return UI_SAVE_CUBES_NEW;
}

static void cubesStart(uiSaveCubesMotion_t *motion,
	const uiSaveCubesGrid_t *grid)
{
	int s, cell;

	memset(motion, 0, sizeof(*motion));
	for(s = 0; s < UI_SAVE_CUBES_STACKS; s++) {
		for(cell = 0; cell < UI_SAVE_CUBES_MAX_CELLS; cell++) {
			UIMotion_SpringInit(&motion->grow[s][cell], 0.0f, CUBES_GROW_RESPONSE);
		}
		UIMotion_SpringInit(&motion->first[s], (float)grid->stack[s].first,
			CUBES_SCROLL_RESPONSE);
		motion->listing[s] = grid->stack[s].listing;
		motion->top[s] = (float)grid->stack[s].first;
	}
	UIMotion_SpringInit(&motion->menuBar, 0.0f, CUBES_MENU_RESPONSE);
	/* Long closed: no box or message fades out on the first frame. */
	motion->menuSeconds = -CUBES_MENU_CLOSE;
	motion->messageSeconds = -CUBES_MESSAGE_OUT;
	motion->focusStack = grid->focusStack;
	motion->focusCell = grid->focusCell;
	motion->growsFrom = CUBES_FOCUS_GROWS;
	motion->started = true;
}

/* Grown, growing and flying cubes go last, nearest last; the rest keep
 * their order. Returns how many rest. An insertion sort in place: nearly
 * all rest. */
static int cubesFloat(uiSaveCube_t *cubes, bool *floats, int count)
{
	int resting = 0, i, j;

	for(i = 1; i < count; i++) {
		uiSaveCube_t cube = cubes[i];
		bool up = floats[i];
		float key = up ? 1.0f + cube.z : 0.0f;

		for(j = i; j > 0 && (floats[j - 1] ? 1.0f + cubes[j - 1].z : 0.0f) > key; j--) {
			cubes[j] = cubes[j - 1];
			floats[j] = floats[j - 1];
		}
		cubes[j] = cube;
		floats[j] = up;
	}
	while(resting < count && !floats[resting]) {
		resting++;
	}
	return resting;
}

/* A quadratic arc from a to b, its middle pulled down toward the info bar
 * (no lower than the floor), at u of the way. */
static void cubesArc(float ax, float ay, float bx, float by, float u,
	float *x, float *y)
{
	float cx = 0.5f * (ax + bx);
	float cy = 0.5f * (ay + by) + CUBES_ARC_DIP;

	if(cy > CUBES_ARC_FLOOR) {
		cy = CUBES_ARC_FLOOR;
	}
	*x = (1.0f - u) * (1.0f - u) * ax + 2.0f * u * (1.0f - u) * cx + u * u * bx;
	*y = (1.0f - u) * (1.0f - u) * ay + 2.0f * u * (1.0f - u) * cy + u * u * by;
}

/* Where an operation's cube is tau seconds into phase, toCell the cell it
 * aims for; turn takes its turn. False when there is none to draw: with
 * Motion Off, a Copy's or Move's back home or a Reduced one done gliding,
 * or an erased cube's pieces (drawn on their own). */
static bool cubesOpPose(const uiSaveCubesMotion_t *motion,
	const uiSaveCubesOp_t *op, int phase, int toCell, float tau,
	uiMotionMode_t mode, uiSaveCubesPose_t *pose, float turn[9])
{
	int other = !op->from;
	const uiSaveCubesPose_t *from = &motion->opFrom;
	float homeX, homeY, toX, toY, u, sine = 0.0f, cosine = 1.0f;
	bool full = mode == UI_MOTION_FULL;

	if(mode == UI_MOTION_OFF || op->from < 0 || op->from >= UI_SAVE_CUBES_STACKS) {
		return false;
	}
	UISaveCubes_Where(op->from, op->fromCell, motion->top[op->from], &homeX, &homeY);
	if(toCell >= 0) {
		UISaveCubes_Where(other, toCell, motion->top[other], &toX, &toY);
	}
	else {
		toX = UI_SAVE_CUBES_STACK_X + (float)other * UI_SAVE_CUBES_STACK_GAP;
		toY = UI_SAVE_CUBES_HEADER_Y;
	}
	memset(pose, 0, sizeof(*pose));
	if(op->kind == UI_SAVE_CUBES_OP_ERASE) {
		if(phase == UI_SAVE_CUBES_GO) {
			/* Shrinking and turning, then trembling, a pixel either way. */
			static const float tremble[8] = {0.0f, 1.0f, -1.0f, 0.5f, -0.5f, 1.0f, 0.0f, -1.0f};
			u = full ? UIMotion_EaseOutCubic(tau / CUBES_ERASE_SHRINK) : 0.0f;
			pose->x = homeX + (full ? u * tremble[(int)(tau * 60.0f) & 7] : 0.0f);
			pose->y = homeY;
			pose->z = UI_SAVE_CUBES_LIFT;
			pose->scale = cubesLerp(UI_SAVE_CUBES_SELECTED, CUBES_ERASE_SCALE, u);
			pose->yaw = CUBES_ERASE_TURN * u;
		}
		else if(phase == UI_SAVE_CUBES_BACK && full && tau < CUBES_ERASE_REGROW) {
			u = UIMotion_EaseOutCubic(tau / CUBES_ERASE_REGROW);
			*pose = *from;
			pose->scale = cubesLerp(from->scale, UI_SAVE_CUBES_SELECTED, u);
			pose->yaw = from->yaw * (1.0f - u);
		}
		else {
			return false;
		}
		cubesSmallTurn(pose->yaw, &sine, &cosine);
		cubesYaw(sine, cosine, turn);
		return true;
	}
	if(phase == UI_SAVE_CUBES_GO) {
		if(!full) {
			u = UIMotion_Smoothstep(tau / CUBES_GLIDE);
			pose->x = cubesLerp(homeX, toX, u);
			pose->y = cubesLerp(homeY, toY, u);
			pose->z = UI_SAVE_CUBES_LIFT;
			pose->scale = cubesLerp(UI_SAVE_CUBES_SELECTED, 1.0f, u);
		}
		else if(tau < CUBES_FLIGHT) {
			float rise;

			u = UIMotion_Smoothstep(tau / CUBES_FLIGHT);
			rise = sinf(CUBES_PI * u);
			cubesArc(homeX, homeY, toX, toY, u, &pose->x, &pose->y);
			pose->z = UI_SAVE_CUBES_LIFT + CUBES_ARC_RISE * rise;
			pose->scale = cubesLerp(UI_SAVE_CUBES_SELECTED, 1.0f, u) +
				CUBES_ARC_GROWTH * rise;
			/* Its icon in sight all the way: it swings its face toward
			 * where it goes, most at the middle, tipped down into the
			 * dip and up out of it (the double angle's sine from the
			 * single's, one call more). */
			pose->yaw = (toX < homeX ? -CUBES_ARC_YAW : CUBES_ARC_YAW) * rise;
			pose->tilt = -2.0f * CUBES_ARC_TILT * rise * cosf(CUBES_PI * u);
		}
		else {
			/* Hovering over the cell until the card answers, easing into
			 * the IPL's pulse and sway. */
			float hover = tau - CUBES_FLIGHT;
			float ease = UIMotion_Smoothstep(hover / CUBES_HOVER_EASE);

			pose->x = toX;
			pose->y = toY;
			pose->z = UI_SAVE_CUBES_LIFT;
			pose->scale = 1.0f + ease * (CUBES_HOVER_GROWTH + CUBES_HOVER_PULSE *
				sinf(3.0f * CUBES_PI * hover));
			pose->yaw = ease * CUBES_HOVER_TURN * sinf(CUBES_HOVER_RATE * hover);
		}
	}
	else if(phase == UI_SAVE_CUBES_LAND) {
		u = UIMotion_EaseOutCubic(tau / CUBES_LAND);
		pose->x = cubesLerp(from->x, toX, u);
		pose->y = cubesLerp(from->y, toY, u);
		pose->z = from->z * (1.0f - u);
		/* Into a header it shrinks away; onto a cell it rests there. */
		pose->scale = toCell >= 0 ? cubesLerp(from->scale, 1.0f, u) :
			from->scale * (1.0f - u);
		pose->yaw = from->yaw * (1.0f - u);
		pose->tilt = from->tilt * (1.0f - u);
		if(pose->scale <= 0.0f) {
			return false;
		}
	}
	else {
		float time = full ? CUBES_BACK : CUBES_GLIDE;

		if(tau >= time) {
			return false;
		}
		u = UIMotion_Smoothstep(tau / time);
		if(full) {
			cubesArc(from->x, from->y, homeX, homeY, u, &pose->x, &pose->y);
		}
		else {
			pose->x = cubesLerp(from->x, homeX, u);
			pose->y = cubesLerp(from->y, homeY, u);
		}
		pose->z = cubesLerp(from->z, UI_SAVE_CUBES_LIFT, u);
		pose->scale = cubesLerp(from->scale, UI_SAVE_CUBES_SELECTED, u);
		pose->yaw = from->yaw * (1.0f - u);
		pose->tilt = from->tilt * (1.0f - u);
	}
	cubesSwing(pose->yaw, pose->tilt, turn);
	return true;
}

/* Takes in the operation grid shows: a new phase starts now, from where its
 * cube was as the last one ended, or where a flight hovers if that wasn't
 * seen. */
static void cubesOpTrack(uiSaveCubesMotion_t *motion,
	const uiSaveCubesGrid_t *grid, uiMotionMode_t mode)
{
	const uiSaveCubesOp_t *op = &grid->op;
	bool seen = op->serial == motion->opSerial && op->kind == motion->opKind;
	float scratch[9];

	if(op->kind == UI_SAVE_CUBES_OP_NONE) {
		motion->opKind = UI_SAVE_CUBES_OP_NONE;
		return;
	}
	if(seen && op->phase == motion->opPhase) {
		return;
	}
	if(op->phase != UI_SAVE_CUBES_GO) {
		if(!seen) {
			/* Its going never showed: from its end, over the cell. */
			motion->opTo = op->toCell;
		}
		if(!cubesOpPose(motion, op, seen ? motion->opPhase : UI_SAVE_CUBES_GO,
			motion->opTo, seen ? motion->seconds - motion->opSeconds :
			(op->kind == UI_SAVE_CUBES_OP_ERASE ? CUBES_ERASE_SHRINK : CUBES_FLIGHT),
			mode, &motion->opFrom, scratch)) {
			memset(&motion->opFrom, 0, sizeof(motion->opFrom));
		}
	}
	else {
		motion->opTo = op->toCell;
	}
	/* Bursting: the save that takes its cell comes into focus once the
	 * pieces have mostly flown, from its own size, not the erased one's. */
	if(op->kind == UI_SAVE_CUBES_OP_ERASE && op->phase == UI_SAVE_CUBES_LAND &&
		mode == UI_MOTION_FULL && op->from >= 0 && op->from < UI_SAVE_CUBES_STACKS &&
		op->fromCell >= 0 && op->fromCell < UI_SAVE_CUBES_MAX_CELLS) {
		motion->growsFrom = motion->seconds + CUBES_ERASE_FOCUS;
		UIMotion_SpringSnap(&motion->grow[op->from][op->fromCell], 0.0f);
	}
	motion->opSerial = op->serial;
	motion->opKind = op->kind;
	motion->opPhase = op->phase;
	motion->opSeconds = motion->seconds;
}

/* The box beside the focus, a message and the leaving: from when each
 * changed. */
static void cubesOverlays(uiSaveCubesMotion_t *motion,
	const uiSaveCubesGrid_t *grid, float dt, uiMotionMode_t mode)
{
	float since;

	if(grid->menu && (!motion->menuOpen || grid->menuSerial != motion->menuSerial)) {
		motion->menuOpen = true;
		motion->menuSerial = grid->menuSerial;
		motion->menuSeconds = motion->seconds;
		UIMotion_SpringSnap(&motion->menuBar, (float)grid->menuFocus);
	}
	else if(!grid->menu && motion->menuOpen) {
		motion->menuOpen = false;
		motion->menuSeconds = motion->seconds;
	}
	UIMotion_SpringRetarget(&motion->menuBar, (float)grid->menuFocus, mode);
	motion->menuItem = UIMotion_SpringUpdate(&motion->menuBar, dt, mode);
	since = motion->seconds - motion->menuSeconds;
	motion->menuScale = 1.0f;
	if(mode == UI_MOTION_OFF) {
		motion->menuAlpha = motion->menuOpen ? 1.0f : 0.0f;
	}
	else if(motion->menuOpen) {
		motion->menuAlpha = cubesClamp(since / CUBES_MENU_OPEN);
		motion->menuScale = 0.92f + 0.08f * motion->menuAlpha;
	}
	else {
		motion->menuAlpha = 1.0f - cubesClamp(since / CUBES_MENU_CLOSE);
	}
	if((grid->message != 0) != motion->messageShown) {
		motion->messageShown = grid->message != 0;
		motion->messageSeconds = motion->seconds;
	}
	since = motion->seconds - motion->messageSeconds;
	motion->messageAlpha = mode == UI_MOTION_OFF ? (motion->messageShown ? 1.0f : 0.0f) :
		motion->messageShown ? cubesClamp(since / CUBES_MESSAGE_IN) :
		1.0f - cubesClamp(since / CUBES_MESSAGE_OUT);
	if(grid->leaving && !motion->leaving) {
		motion->leaving = true;
		motion->leaveSeconds = motion->seconds;
	}
}

/* The words, the graph paper and the Home cube, opening and leaving. */
static void cubesStage(uiSaveCubesMotion_t *motion, uiMotionMode_t mode)
{
	float t = motion->seconds, leave = motion->seconds - motion->leaveSeconds;

	if(mode == UI_MOTION_OFF) {
		motion->chrome = motion->paper = 1.0f;
		motion->handover = 0.0f;
	}
	else if(mode == UI_MOTION_REDUCED) {
		motion->chrome = motion->paper = cubesClamp(t / CUBES_FADE_TIME);
		motion->handover = 0.0f;
	}
	else {
		float gone = cubesClamp(t / CUBES_HANDOVER);

		motion->chrome = UIMotion_Smoothstep((t - CUBES_CHROME_IN) / CUBES_CHROME_TIME);
		motion->paper = cubesClamp((t - CUBES_PAPER_IN) / CUBES_PAPER_TIME);
		motion->handover = 1.0f - gone * gone * gone;
	}
	if(!motion->leaving || mode == UI_MOTION_OFF) {
		return;
	}
	if(mode == UI_MOTION_REDUCED) {
		motion->chrome *= 1.0f - cubesClamp(leave / CUBES_LEAVE_REDUCED);
		motion->paper *= 1.0f - cubesClamp(leave / CUBES_LEAVE_REDUCED);
		return;
	}
	motion->chrome *= 1.0f - cubesClamp(leave / CUBES_LEAVE_CHROME);
	motion->paper *= 1.0f - UIMotion_Smoothstep(leave / CUBES_LEAVE);
	leave = UIMotion_EaseOutCubic((leave - CUBES_RETURN_START) /
		(CUBES_LEAVE - CUBES_RETURN_START));
	if(leave > motion->handover) {
		motion->handover = leave;
	}
}

/* How a cell's cube comes in with its listing, k its place in the stack's
 * drawn rows: spiralling out of the middle while the screen opens, else
 * popping in; Reduced fades it in. Moves (x, y) and scales scale and alpha. */
static void cubesArrive(const uiSaveCubesMotion_t *motion, int s, int k,
	uiMotionMode_t mode, float *x, float *y, float *scale, float *alpha)
{
	float since = motion->seconds - motion->changed[s];
	float e, sine, cosine, dx, dy;

	if(mode == UI_MOTION_OFF || motion->change[s] != UI_SAVE_CUBES_NEW) {
		return;
	}
	if(mode == UI_MOTION_REDUCED) {
		*alpha *= cubesClamp(since / CUBES_FADE_TIME);
		return;
	}
	if(motion->changed[s] >= CUBES_ENTRY) {
		*scale *= UIMotion_EaseOutCubic((since - CUBES_POP_STAGGER * (float)k) /
			CUBES_POP_TIME);
		return;
	}
	e = UIMotion_EaseOutCubic((since - CUBES_SPIRAL_START -
		CUBES_SPIRAL_STAGGER * (float)k) / CUBES_SPIRAL_TIME);
	if(e >= 1.0f) {
		return;
	}
	cubesTurn(CUBES_SPIRAL_TURN * (1.0f - e), &sine, &cosine);
	dx = (*x - CUBES_MIDDLE_X) * e;
	dy = (*y - CUBES_MIDDLE_Y) * e;
	*x = CUBES_MIDDLE_X + cosine * dx - sine * dy;
	*y = CUBES_MIDDLE_Y + sine * dx + cosine * dy;
	*scale *= 0.3f + 0.7f * e;
	*alpha *= cubesClamp(4.0f * e);
}

/* The cubes besides the cells: a copied or moved save's cube flying, an
 * erased one's pieces (Reduced: the cube fading), and the ghost where a
 * Copy or Move would land. All float. Returns how many cubes out holds. */
static int cubesExtras(uiSaveCubesMotion_t *motion,
	const uiSaveCubesGrid_t *grid, uiMotionMode_t mode, float opTau,
	uiSaveCube_t *out, bool *floats, int n)
{
	/* The pieces' ways out, round the compass, every other one less far, so
	 * they burst rather than ring. */
	static const float bitWay[UI_SAVE_CUBES_BITS][2] = {
		{1.0f, 0.0f}, {0.52f, -0.52f}, {0.0f, -1.0f}, {-0.52f, -0.52f},
		{-1.0f, 0.0f}, {-0.52f, 0.52f}, {0.0f, 1.0f}, {0.52f, 0.52f}
	};
	const uiSaveCubesOp_t *op = &grid->op;
	const uiSaveCubesPose_t *from = &motion->opFrom;
	uiSaveCubesPose_t pose;
	uiSaveCube_t *cube;
	int k;

	if((op->kind == UI_SAVE_CUBES_OP_COPY || op->kind == UI_SAVE_CUBES_OP_MOVE) &&
		cubesOpPose(motion, op, op->phase, op->phase == UI_SAVE_CUBES_GO ?
		motion->opTo : op->toCell, opTau, mode, &pose, motion->opTurn)) {
		cube = &out[n];
		memset(cube, 0, sizeof(*cube));
		cube->x = pose.x;
		cube->y = pose.y;
		cube->z = pose.z;
		cube->half = 0.5f * UI_SAVE_CUBES_FACE * pose.scale;
		cube->turn = motion->opTurn;
		cube->kind = UI_SAVE_CUBES_KIND_SAVE;
		cube->shade = UI_SAVE_CUBES_SHADE_SAVE;
		cube->icon = UISaveCubes_Icon(&op->cube, motion->seconds, mode);
		cube->alpha = 255;
		floats[n++] = true;
	}
	else if(op->kind == UI_SAVE_CUBES_OP_ERASE && op->phase == UI_SAVE_CUBES_LAND) {
		float size = UI_SAVE_CUBES_FACE * from->scale;

		if(mode == UI_MOTION_REDUCED && opTau < CUBES_ERASE_FADE) {
			cube = &out[n];
			memset(cube, 0, sizeof(*cube));
			cube->x = from->x;
			cube->y = from->y;
			cube->z = from->z;
			cube->half = 0.5f * size;
			cube->kind = UI_SAVE_CUBES_KIND_SAVE;
			cube->shade = UI_SAVE_CUBES_SHADE_PLAIN;
			cube->alpha = (uint8_t)(255.0f * (1.0f - opTau / CUBES_ERASE_FADE));
			floats[n++] = true;
		}
		else if(mode == UI_MOTION_FULL && opTau < CUBES_ERASE_BURST) {
			/* Thrown out fast and slowing, falling a little, the pale of
			 * the cube they were, bright until they fade at the end. */
			float u = opTau / CUBES_ERASE_BURST;
			float flung = 1.0f - (1.0f - u) * (1.0f - u);

			cubesYaw(sinf(CUBES_ERASE_TURN + CUBES_BIT_SPIN * opTau),
				cosf(CUBES_ERASE_TURN + CUBES_BIT_SPIN * opTau), motion->bitTurn);
			for(k = 0; k < UI_SAVE_CUBES_BITS; k++) {
				cube = &out[n];
				memset(cube, 0, sizeof(*cube));
				cube->x = from->x + bitWay[k][0] * CUBES_BIT_REACH * flung;
				cube->y = from->y + bitWay[k][1] * CUBES_BIT_REACH * flung +
					CUBES_BIT_FALL * u * u;
				cube->z = from->z;
				cube->half = CUBES_BIT_SIZE * size * (1.0f - 0.3f * u);
				cube->turn = motion->bitTurn;
				cube->kind = UI_SAVE_CUBES_KIND_SAVE;
				cube->shade = UI_SAVE_CUBES_SHADE_SAVE_FOCUS;
				cube->alpha = (uint8_t)(255.0f * (1.0f - u * u) + 0.5f);
				floats[n++] = true;
			}
		}
	}
	if(grid->ghost && grid->ghostCell >= 0 && grid->focusStack >= 0 &&
		grid->focusStack < UI_SAVE_CUBES_STACKS) {
		int s = !grid->focusStack;
		const uiSaveCubesStack_t *stack = &grid->stack[s];

		k = grid->ghostCell - (stack->first - 1) * UI_SAVE_CUBES_COLUMNS;
		if(k >= 0 && k < UI_SAVE_CUBES_DRAWN && grid->ghostCell < stack->cells) {
			/* Between half and eight tenths, about Reduced's steady 0.65. */
			float pulse = mode == UI_MOTION_FULL ? 0.65f + 0.15f *
				sinf(2.0f * CUBES_PI * motion->seconds / CUBES_GHOST_PERIOD) : 0.65f;

			cube = &out[n];
			memset(cube, 0, sizeof(*cube));
			UISaveCubes_Where(s, grid->ghostCell, motion->top[s], &cube->x, &cube->y);
			cube->half = 0.5f * UI_SAVE_CUBES_FACE;
			cube->kind = UI_SAVE_CUBES_KIND_SAVE;
			cube->shade = UI_SAVE_CUBES_SHADE_EMPTY_FOCUS;
			cube->alpha = (uint8_t)(255.0f * pulse + 0.5f);
			floats[n++] = true;
		}
	}
	return n;
}

/* Leaving (Full): every cube falls into the middle and fades at the end;
 * Reduced fades them where they are. */
static void cubesLeave(const uiSaveCubesMotion_t *motion, uiMotionMode_t mode,
	uiSaveCube_t *out, int n)
{
	float leave = motion->seconds - motion->leaveSeconds, keep = 1.0f, alpha;
	int i;

	if(!motion->leaving || mode == UI_MOTION_OFF) {
		return;
	}
	if(mode == UI_MOTION_REDUCED) {
		alpha = 1.0f - cubesClamp(leave / CUBES_LEAVE_REDUCED);
	}
	else {
		keep = 1.0f - UIMotion_Smoothstep(leave / CUBES_COLLAPSE);
		alpha = 1.0f - cubesClamp((leave - (CUBES_COLLAPSE - CUBES_COLLAPSE_FADE)) /
			CUBES_COLLAPSE_FADE);
	}
	for(i = 0; i < n; i++) {
		out[i].x = CUBES_MIDDLE_X + (out[i].x - CUBES_MIDDLE_X) * keep;
		out[i].y = CUBES_MIDDLE_Y + (out[i].y - CUBES_MIDDLE_Y) * keep;
		out[i].alpha = (uint8_t)((float)out[i].alpha * alpha + 0.5f);
	}
}

int UISaveCubes_Frame(uiSaveCubesMotion_t *motion,
	const uiSaveCubesGrid_t *grid, float dt, uiMotionMode_t mode,
	uiSaveCube_t out[UI_SAVE_CUBES_OUT], int *floating)
{
	bool floats[UI_SAVE_CUBES_OUT];
	const uiSaveCubesOp_t *op = &grid->op;
	float bobSine = 0.0f, bobCosine = 0.0f, drift[2] = {0.0f, 0.0f};
	float wobble = 0.0f, opTau;
	bool grows;
	int n = 0, s, k;

	if(!(dt > 0.0f)) {
		dt = 0.0f;
	}
	if(!motion->started) {
		cubesStart(motion, grid);
	}
	motion->seconds += dt;
	cubesOpTrack(motion, grid, mode);
	/* While the screen opens the focused cube waits to grow, and while an
	 * erased one bursts. */
	grows = mode != UI_MOTION_FULL || motion->seconds >= motion->growsFrom;
	if(grid->focusStack != motion->focusStack ||
		grid->focusCell != motion->focusCell || !grows) {
		motion->focusStack = grid->focusStack;
		motion->focusCell = grid->focusCell;
		motion->focusSeconds = 0.0f;
	}
	else {
		motion->focusSeconds += dt;
	}
	if(mode == UI_MOTION_FULL) {
		bobSine = sinf(CUBES_BOB_RATE * motion->seconds);
		bobCosine = cosf(CUBES_BOB_RATE * motion->seconds);
	}
	if(grid->focusStack >= 0) {
		wobble = UIMotion_Amplitude(1.0f, mode) *
			UIMotion_Smoothstep(motion->focusSeconds / CUBES_WOBBLE_EASE);
	}
	if(wobble > 0.0f) {
		cubesWobble(motion->focusSeconds, wobble, motion->turn, drift);
	}
	cubesOverlays(motion, grid, dt, mode);
	cubesStage(motion, mode);
	opTau = motion->seconds - motion->opSeconds;
	for(s = 0; s < UI_SAVE_CUBES_STACKS; s++) {
		const uiSaveCubesStack_t *stack = &grid->stack[s];
		int cells = stack->cells > UI_SAVE_CUBES_MAX_CELLS ?
			UI_SAVE_CUBES_MAX_CELLS : stack->cells;
		bool sliding;
		float since;

		if(stack->listing != motion->listing[s]) {
			motion->listing[s] = stack->listing;
			motion->change[s] = stack->change;
			motion->changeAt[s] = stack->changeAt;
			motion->changed[s] = motion->seconds;
			/* The saves after an erased one wait for its burst to open. */
			if(stack->change == UI_SAVE_CUBES_CLOSED && mode == UI_MOTION_FULL &&
				op->kind == UI_SAVE_CUBES_OP_ERASE && op->phase == UI_SAVE_CUBES_LAND &&
				op->from == s) {
				motion->changed[s] += CUBES_ERASE_SETTLE;
			}
			/* Another card or folder starts still; the same one read
			 * again keeps moving as it was. */
			if(stack->change == UI_SAVE_CUBES_NEW) {
				UIMotion_SpringSnap(&motion->first[s], (float)stack->first);
				for(k = 0; k < UI_SAVE_CUBES_MAX_CELLS; k++) {
					UIMotion_SpringSnap(&motion->grow[s][k], grows &&
						s == grid->focusStack && k == grid->focusCell ? 1.0f : 0.0f);
				}
			}
		}
		UIMotion_SpringRetarget(&motion->first[s], (float)stack->first, mode);
		motion->top[s] = UIMotion_SpringUpdate(&motion->first[s], dt, mode);
		since = motion->seconds - motion->changed[s];
		sliding = mode != UI_MOTION_OFF && (motion->change[s] == UI_SAVE_CUBES_CLOSED ||
			motion->change[s] == UI_SAVE_CUBES_OPENED);
		for(k = 0; k < UI_SAVE_CUBES_DRAWN; k++) {
			int row = stack->first - 1 + k / UI_SAVE_CUBES_COLUMNS;
			int column = k % UI_SAVE_CUBES_COLUMNS;
			int cell = row * UI_SAVE_CUBES_COLUMNS + column;
			bool focused = s == grid->focusStack && cell == grid->focusCell;
			bool mine = op->kind != UI_SAVE_CUBES_OP_NONE && s == op->from &&
				cell == op->fromCell;
			const uiSaveCubesCell_t *what = &stack->cell[k];
			uiSaveCube_t *cube = &out[n];
			uiMotionSpring_t *grow;
			uiSaveCubesPose_t pose;
			float x, y, outside, fade, g, scale = 1.0f, alpha = 1.0f;
			uint8_t kind = what->kind;

			if(row < 0 || cell >= cells) {
				continue;
			}
			grow = &motion->grow[s][cell];
			UIMotion_SpringRetarget(grow, focused && grows ? 1.0f : 0.0f, mode);
			g = UIMotion_SpringUpdate(grow, dt, mode);
			/* The ghost stands in this cell; a landing cube is on its way
			 * into this one. */
			if((grid->ghost && grid->focusStack >= 0 && s == !grid->focusStack &&
				cell == grid->ghostCell) || (mode != UI_MOTION_OFF &&
				op->phase == UI_SAVE_CUBES_LAND && op->toCell >= 0 &&
				(op->kind == UI_SAVE_CUBES_OP_COPY || op->kind == UI_SAVE_CUBES_OP_MOVE) &&
				s == !op->from && cell == op->toCell)) {
				continue;
			}
			UISaveCubes_Where(s, cell, motion->top[s], &x, &y);
			if(sliding && motion->changeAt[s] >= 0) {
				int at = motion->changeAt[s];
				int firstDrawn = (stack->first - 1) * UI_SAVE_CUBES_COLUMNS;
				int was = -1;
				float slide;

				if(motion->change[s] == UI_SAVE_CUBES_CLOSED && cell >= at) {
					was = kind != UI_SAVE_CUBES_KIND_EMPTY ? cell + 1 : -1;
				}
				else if(motion->change[s] == UI_SAVE_CUBES_OPENED && cell > at) {
					was = kind != UI_SAVE_CUBES_KIND_EMPTY ? cell - 1 : -1;
				}
				slide = UIMotion_EaseOutCubic((since - CUBES_CASCADE_STAGGER *
					(float)(cell - (at > firstDrawn ? at : firstDrawn))) /
					CUBES_CASCADE_TIME);
				if(was >= 0 && slide < 1.0f) {
					float wasX, wasY;

					UISaveCubes_Where(s, was, motion->top[s], &wasX, &wasY);
					x = cubesLerp(wasX, x, slide);
					y = cubesLerp(wasY, y, slide);
				}
				/* The cell a save left at the end grows from nothing. The
				 * one that came doesn't: its cube landed there at its size
				 * and hands over to it. */
				else if(motion->change[s] == UI_SAVE_CUBES_CLOSED && cell >= at &&
					kind == UI_SAVE_CUBES_KIND_EMPTY && k > 0 &&
					stack->cell[k - 1].kind != UI_SAVE_CUBES_KIND_EMPTY) {
					scale *= slide;
				}
			}
			cubesArrive(motion, s, k, mode, &x, &y, &scale, &alpha);
			outside = CUBES_FADE_TOP - y > y - CUBES_FADE_BOTTOM ?
				CUBES_FADE_TOP - y : y - CUBES_FADE_BOTTOM;
			fade = outside <= 0.0f ? 1.0f : 1.0f - outside / CUBES_FADE_SPAN;
			fade *= alpha;
			if(fade <= 0.0f || scale <= 0.0f) {
				continue;
			}
			/* A Move's save has left its cell while it flies. */
			if(mine && op->kind == UI_SAVE_CUBES_OP_MOVE && (op->phase == UI_SAVE_CUBES_GO ||
				(op->phase == UI_SAVE_CUBES_BACK && opTau <
				(mode == UI_MOTION_FULL ? CUBES_BACK : CUBES_GLIDE)))) {
				kind = UI_SAVE_CUBES_KIND_EMPTY;
			}
			memset(cube, 0, sizeof(*cube));
			cube->kind = kind;
			cube->shade = (uint8_t)(kind == UI_SAVE_CUBES_KIND_EMPTY ?
				(focused ? UI_SAVE_CUBES_SHADE_EMPTY_FOCUS : UI_SAVE_CUBES_SHADE_EMPTY) :
				(focused ? UI_SAVE_CUBES_SHADE_SAVE_FOCUS : UI_SAVE_CUBES_SHADE_SAVE));
			cube->half = 0.5f * UI_SAVE_CUBES_FACE * scale *
				(kind == UI_SAVE_CUBES_KIND_EMPTY ? UI_SAVE_CUBES_EMPTY : 1.0f) *
				(1.0f + (UI_SAVE_CUBES_SELECTED - 1.0f) * g);
			cube->z = UI_SAVE_CUBES_LIFT * g;
			cube->x = x;
			cube->y = y + UI_SAVE_CUBES_BOB_PX *
				(bobSine * bobPhase[(cell + 8 * s) & 15][0] +
				bobCosine * bobPhase[(cell + 8 * s) & 15][1]);
			if(focused && wobble > 0.0f) {
				cube->x += drift[0];
				cube->y += drift[1];
				cube->turn = motion->turn;
			}
			cube->icon = kind == what->kind ? UISaveCubes_Icon(what, motion->seconds, mode) :
				NULL;
			if(mine && op->kind == UI_SAVE_CUBES_OP_ERASE &&
				cubesOpPose(motion, op, op->phase, motion->opTo, opTau, mode, &pose,
				motion->opTurn)) {
				/* The save being erased: plain, shrunk and turned, or
				 * growing back with its icon when that failed. */
				cube->half = 0.5f * UI_SAVE_CUBES_FACE * pose.scale;
				cube->x = pose.x;
				cube->y = pose.y;
				cube->turn = motion->opTurn;
				if(op->phase == UI_SAVE_CUBES_GO) {
					cube->shade = UI_SAVE_CUBES_SHADE_PLAIN;
					cube->icon = NULL;
				}
			}
			else if(mine && op->kind == UI_SAVE_CUBES_OP_ERASE &&
				op->phase == UI_SAVE_CUBES_GO) {
				cube->shade = UI_SAVE_CUBES_SHADE_PLAIN;
				cube->icon = NULL;
			}
			/* Back home from a failed Copy or Move, the save shakes. */
			if(mine && op->phase == UI_SAVE_CUBES_BACK && mode == UI_MOTION_FULL &&
				op->kind != UI_SAVE_CUBES_OP_ERASE && opTau >= CUBES_BACK &&
				opTau < CUBES_BACK + CUBES_SHAKE) {
				float shake = opTau - CUBES_BACK;

				cube->x += CUBES_SHAKE_PX * sinf(CUBES_SHAKE_RATE * shake) *
					(1.0f - shake / CUBES_SHAKE);
			}
			cube->alpha = (uint8_t)(255.0f * fade + 0.5f);
			floats[n++] = focused || g > CUBES_FLOATING || mine;
		}
	}
	n = cubesExtras(motion, grid, mode, opTau, out, floats, n);
	cubesLeave(motion, mode, out, n);
	*floating = cubesFloat(out, floats, n);
	return n;
}

/* ------------------------------------------------------------------------
 * The cursor.
 * --------------------------------------------------------------------- */
int UISaveCubes_Cells(int saves)
{
	int cells = saves < 0 ? 1 : saves + 1;

	cells = (cells + UI_SAVE_CUBES_COLUMNS - 1) / UI_SAVE_CUBES_COLUMNS *
		UI_SAVE_CUBES_COLUMNS;
	return cells < UI_SAVE_CUBES_MIN_CELLS ? UI_SAVE_CUBES_MIN_CELLS :
		cells > UI_SAVE_CUBES_MAX_CELLS ? UI_SAVE_CUBES_MAX_CELLS : cells;
}

int UISaveCubes_Landing(int saves)
{
	saves = saves < 0 ? 0 : saves;
	return saves < UISaveCubes_Cells(saves) ? saves : -1;
}

int UISaveCubes_Window(int first, int cell, int cells)
{
	int row = cell / UI_SAVE_CUBES_COLUMNS;
	int last = cells / UI_SAVE_CUBES_COLUMNS - UI_SAVE_CUBES_ROWS;

	if(row < first) {
		first = row;
	}
	if(row >= first + UI_SAVE_CUBES_ROWS) {
		first = row - UI_SAVE_CUBES_ROWS + 1;
	}
	if(first > last) {
		first = last;
	}
	return first < 0 ? 0 : first;
}

bool UISaveCubes_Step(uiSaveCubesCursor_t *cursor, uiSaveCubesStep_t step)
{
	int s, cell, column, row;

	if(cursor == NULL || cursor->stack < 0 || cursor->stack >= UI_SAVE_CUBES_STACKS ||
		cursor->cells[cursor->stack] <= 0) {
		return false;
	}
	s = cursor->stack;
	cell = cursor->cell;
	column = cell % UI_SAVE_CUBES_COLUMNS;
	row = cell / UI_SAVE_CUBES_COLUMNS;
	switch(step) {
		case UI_SAVE_CUBES_UP:
			if(row == 0) {
				return false;
			}
			cell -= UI_SAVE_CUBES_COLUMNS;
			break;
		case UI_SAVE_CUBES_DOWN:
			if(cell + UI_SAVE_CUBES_COLUMNS >= cursor->cells[s]) {
				return false;
			}
			cell += UI_SAVE_CUBES_COLUMNS;
			break;
		case UI_SAVE_CUBES_LEFT:
		case UI_SAVE_CUBES_RIGHT: {
			int way = step == UI_SAVE_CUBES_RIGHT ? 1 : -1;
			int rows;

			if(column + way >= 0 && column + way < UI_SAVE_CUBES_COLUMNS) {
				cell += way;
				break;
			}
			/* Off the edge into the other stack, at the same row on screen,
			 * as Right from Slot A's fourth column reaches Slot B's first. */
			if(s + way < 0 || s + way >= UI_SAVE_CUBES_STACKS ||
				cursor->cells[s + way] <= 0) {
				return false;
			}
			row = cursor->first[s + way] + row - cursor->first[s];
			s += way;
			rows = cursor->cells[s] / UI_SAVE_CUBES_COLUMNS;
			row = row < 0 ? 0 : row >= rows ? rows - 1 : row;
			cell = row * UI_SAVE_CUBES_COLUMNS + (way > 0 ? 0 : UI_SAVE_CUBES_COLUMNS - 1);
			if(cell >= cursor->cells[s]) {
				cell = cursor->cells[s] - 1;
			}
			break;
		}
		default:
			return false;
	}
	cursor->stack = s;
	cursor->cell = cell;
	cursor->first[s] = UISaveCubes_Window(cursor->first[s], cell, cursor->cells[s]);
	return true;
}

int UISaveCubes_Home(const int cells[UI_SAVE_CUBES_STACKS], int stack)
{
	int s;

	if(stack >= 0 && stack < UI_SAVE_CUBES_STACKS && cells[stack] > 0) {
		return stack;
	}
	for(s = 0; s < UI_SAVE_CUBES_STACKS; s++) {
		if(cells[s] > 0) {
			return s;
		}
	}
	return -1;
}

int UISaveCubes_Swap(int source, int other, bool found)
{
	int next;

	if(found) {
		return source;
	}
	next = (source + 1) % UI_SAVES_PLACE_CHOOSE;
	if(next == other) {
		next = (next + 1) % UI_SAVES_PLACE_CHOOSE;
	}
	return next;
}
