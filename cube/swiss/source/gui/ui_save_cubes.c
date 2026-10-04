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
	}
	motion->focusStack = grid->focusStack;
	motion->focusCell = grid->focusCell;
	motion->started = true;
}

/* Grown or growing cubes go last, nearest last; the rest keep their order.
 * Returns how many rest. An insertion sort in place: nearly all rest. */
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

int UISaveCubes_Frame(uiSaveCubesMotion_t *motion,
	const uiSaveCubesGrid_t *grid, float dt, uiMotionMode_t mode,
	uiSaveCube_t out[UI_SAVE_CUBES_FRAME], int *floating)
{
	bool floats[UI_SAVE_CUBES_FRAME];
	float bobSine = 0.0f, bobCosine = 0.0f, drift[2] = {0.0f, 0.0f};
	float wobble = 0.0f;
	int n = 0, s, k;

	if(!(dt > 0.0f)) {
		dt = 0.0f;
	}
	if(!motion->started) {
		cubesStart(motion, grid);
	}
	motion->seconds += dt;
	if(grid->focusStack != motion->focusStack ||
		grid->focusCell != motion->focusCell) {
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
	for(s = 0; s < UI_SAVE_CUBES_STACKS; s++) {
		const uiSaveCubesStack_t *stack = &grid->stack[s];
		bool snap = stack->listing != motion->listing[s];
		int cells = stack->cells > UI_SAVE_CUBES_MAX_CELLS ?
			UI_SAVE_CUBES_MAX_CELLS : stack->cells;
		float left = UI_SAVE_CUBES_STACK_X + (float)s * UI_SAVE_CUBES_STACK_GAP -
			1.5f * UI_SAVE_CUBES_PITCH;
		float top;

		/* Another listing (another card, folder or reload) starts still. */
		if(snap) {
			motion->listing[s] = stack->listing;
			UIMotion_SpringSnap(&motion->first[s], (float)stack->first);
			for(k = 0; k < UI_SAVE_CUBES_MAX_CELLS; k++) {
				UIMotion_SpringSnap(&motion->grow[s][k],
					s == grid->focusStack && k == grid->focusCell ? 1.0f : 0.0f);
			}
		}
		UIMotion_SpringRetarget(&motion->first[s], (float)stack->first, mode);
		top = UIMotion_SpringUpdate(&motion->first[s], dt, mode);
		for(k = 0; k < UI_SAVE_CUBES_DRAWN; k++) {
			int row = stack->first - 1 + k / UI_SAVE_CUBES_COLUMNS;
			int column = k % UI_SAVE_CUBES_COLUMNS;
			int cell = row * UI_SAVE_CUBES_COLUMNS + column;
			bool focused = s == grid->focusStack && cell == grid->focusCell;
			const uiSaveCubesCell_t *what = &stack->cell[k];
			uiSaveCube_t *cube = &out[n];
			uiMotionSpring_t *grow;
			float y, outside, fade, g;

			if(row < 0 || cell >= cells) {
				continue;
			}
			grow = &motion->grow[s][cell];
			UIMotion_SpringRetarget(grow, focused ? 1.0f : 0.0f, mode);
			g = UIMotion_SpringUpdate(grow, dt, mode);
			y = UI_SAVE_CUBES_TOP_Y + ((float)row - top) * UI_SAVE_CUBES_PITCH;
			outside = CUBES_FADE_TOP - y > y - CUBES_FADE_BOTTOM ?
				CUBES_FADE_TOP - y : y - CUBES_FADE_BOTTOM;
			fade = outside <= 0.0f ? 1.0f : 1.0f - outside / CUBES_FADE_SPAN;
			if(fade <= 0.0f) {
				continue;
			}
			memset(cube, 0, sizeof(*cube));
			cube->kind = what->kind;
			cube->shade = (uint8_t)(what->kind == UI_SAVE_CUBES_KIND_EMPTY ?
				(focused ? UI_SAVE_CUBES_SHADE_EMPTY_FOCUS : UI_SAVE_CUBES_SHADE_EMPTY) :
				(focused ? UI_SAVE_CUBES_SHADE_SAVE_FOCUS : UI_SAVE_CUBES_SHADE_SAVE));
			cube->half = 0.5f * UI_SAVE_CUBES_FACE *
				(what->kind == UI_SAVE_CUBES_KIND_EMPTY ? UI_SAVE_CUBES_EMPTY : 1.0f) *
				(1.0f + (UI_SAVE_CUBES_SELECTED - 1.0f) * g);
			cube->z = UI_SAVE_CUBES_LIFT * g;
			cube->x = left + (float)column * UI_SAVE_CUBES_PITCH;
			cube->y = y + UI_SAVE_CUBES_BOB_PX *
				(bobSine * bobPhase[(cell + 8 * s) & 15][0] +
				bobCosine * bobPhase[(cell + 8 * s) & 15][1]);
			if(focused && wobble > 0.0f) {
				cube->x += drift[0];
				cube->y += drift[1];
				cube->turn = motion->turn;
			}
			cube->icon = UISaveCubes_Icon(what, motion->seconds, mode);
			cube->alpha = (uint8_t)(255.0f * fade + 0.5f);
			floats[n++] = focused || g > CUBES_FLOATING;
		}
	}
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
