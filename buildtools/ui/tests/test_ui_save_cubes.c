#include "ui_save_cubes.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define FAR 1e9f

static uint8_t texels[UI_SAVES_ICON_FRAMES * UI_SAVES_ICON_BYTES];

static uiSaveCube_t cubeAt(float x, float y, float half, uiSaveCubesKind_t kind)
{
	uiSaveCube_t cube;

	memset(&cube, 0, sizeof(cube));
	cube.x = x;
	cube.y = y;
	cube.half = half;
	cube.kind = (uint8_t)kind;
	cube.alpha = 255;
	cube.icon = texels;
	return cube;
}

static float quadArea(const uiSaveCubesQuad_t *quad)
{
	float area = 0.0f;
	int i;

	for(i = 0; i < 4; i++) {
		int j = (i + 1) & 3;
		area += quad->x[i] * quad->y[j] - quad->x[j] * quad->y[i];
	}
	return 0.5f * area;
}

static int roles(const uiSaveCubesQuad_t *quads, int count, int role)
{
	int found = 0, i;

	for(i = 0; i < count; i++) {
		found += quads[i].role == role;
	}
	return found;
}

static bool near(float a, float b, float within)
{
	return fabsf(a - b) <= within;
}

/* Every corner of a cube's quads. */
static void bounds(const uiSaveCubesQuad_t *quads, int count, float box[4])
{
	int i, k;

	box[0] = box[1] = FAR;
	box[2] = box[3] = -FAR;
	for(i = 0; i < count; i++) {
		for(k = 0; k < 4; k++) {
			box[0] = fminf(box[0], quads[i].x[k]);
			box[1] = fminf(box[1], quads[i].y[k]);
			box[2] = fmaxf(box[2], quads[i].x[k]);
			box[3] = fmaxf(box[3], quads[i].y[k]);
		}
	}
}

static void testFaces(void)
{
	uiSaveCubesQuad_t quads[UI_SAVE_CUBES_QUADS];
	uiSaveCube_t cube = cubeAt(320.0f, 224.0f, 19.0f, UI_SAVE_CUBES_KIND_SAVE);
	float front[4], icon[4];
	int n, i;

	/* Square on to the eye: the front alone, as a rim, its inside and the
	 * icon, the face 38 px and the icon 32, a texel a pixel. */
	n = UISaveCubes_Faces(&cube, 0.0f, 640.0f, quads);
	assert(n == 6);
	assert(roles(quads, n, UI_SAVE_CUBES_ROLE_SIDE_EDGE) == 0);
	assert(roles(quads, n, UI_SAVE_CUBES_ROLE_SIDE_TOP) == 0);
	assert(roles(quads, n, UI_SAVE_CUBES_ROLE_SIDE_BOTTOM) == 0);
	assert(quads[n - 1].role == UI_SAVE_CUBES_ROLE_ICON);
	bounds(quads, n - 1, front);
	bounds(&quads[n - 1], 1, icon);
	assert(near(front[2] - front[0], 38.0f, 1e-3f) && near(front[3] - front[1], 38.0f, 1e-3f));
	assert(near(icon[2] - icon[0], 32.0f, 1e-3f) && near(icon[3] - icon[1], 32.0f, 1e-3f));
	assert(near(icon[0], 304.0f, 1e-3f) && near(icon[1], 208.0f, 1e-3f));
	/* The icon's corners run TL, TR, BR, BL for its texels. */
	assert(quads[n - 1].x[0] < quads[n - 1].x[1] && quads[n - 1].y[1] < quads[n - 1].y[2]);

	/* Left of and above the middle: the front, its right side and its
	 * bottom show, the sides toward the middle. */
	cube = cubeAt(92.0f, 140.0f, 19.0f, UI_SAVE_CUBES_KIND_SAVE);
	n = UISaveCubes_Faces(&cube, 0.0f, 640.0f, quads);
	assert(n == 8);
	assert(quads[0].role == UI_SAVE_CUBES_ROLE_SIDE_EDGE);
	assert(quads[1].role == UI_SAVE_CUBES_ROLE_SIDE_BOTTOM);
	assert(quads[0].x[0] > 92.0f + 18.9f);	/* it is the right side */
	assert(quads[1].y[0] > 140.0f + 18.9f);	/* and the bottom */
	assert(roles(quads, n, UI_SAVE_CUBES_ROLE_SIDE_TOP) == 0);
	bounds(quads, n - 1, front);
	bounds(&quads[n - 1], 1, icon);
	assert(icon[0] > front[0] && icon[2] < front[2] && icon[1] > front[1] && icon[3] < front[3]);
	/* At rest a cube's side reaches no more than 12 px past its face. */
	assert(front[2] - front[0] <= 38.0f + 12.0f && front[3] - front[1] <= 38.0f + 12.0f);

	/* Anywhere, turned any way: three faces at most, each wound clockwise
	 * with area, the icon inside the front. */
	for(i = 0; i < 4000; i++) {
		float m[9];
		float a = (float)i * 0.37f, b = (float)i * 0.23f;
		float ca = cosf(a), sa = sinf(a), cb = cosf(b), sb = sinf(b);
		int faces, k;

		m[0] = cb; m[1] = 0.0f; m[2] = sb;
		m[3] = sa * sb; m[4] = ca; m[5] = -sa * cb;
		m[6] = -ca * sb; m[7] = sa; m[8] = ca * cb;
		cube = cubeAt(20.0f + (float)(i % 41) * 15.0f, 20.0f + (float)(i % 29) * 15.0f,
			10.0f + (float)(i % 5) * 6.0f, (uiSaveCubesKind_t)(i % 3));
		cube.z = (float)(i % 7) * 30.0f;
		cube.turn = i % 4 ? m : NULL;
		n = UISaveCubes_Faces(&cube, -107.0f, 747.0f, quads);
		assert(n >= 1 && n <= UI_SAVE_CUBES_QUADS);
		/* A face is a quad, but a front with a rim is a rim of four, its
		 * inside, and a folder's shapes or an icon over that. */
		faces = n - roles(quads, n, UI_SAVE_CUBES_ROLE_ICON) -
			roles(quads, n, UI_SAVE_CUBES_ROLE_GLYPH) -
			4 * roles(quads, n, UI_SAVE_CUBES_ROLE_RIM_TOP);
		assert(faces >= 1 && faces <= 3);
		for(k = 0; k < n; k++) {
			assert(quadArea(&quads[k]) > 0.0f);
		}
	}

	/* A corner behind the eye, a clear cube, one off the stage: nothing. */
	cube = cubeAt(320.0f, 224.0f, 19.0f, UI_SAVE_CUBES_KIND_SAVE);
	cube.z = UI_SAVE_CUBES_EYE - 0.5f;
	assert(UISaveCubes_Faces(&cube, 0.0f, 640.0f, quads) == 0);
	{
		/* Its front clear of the eye, a turned corner past it. */
		const float half = 0.70710678f;
		const float turn[9] = {half, 0.0f, half, 0.0f, 1.0f, 0.0f, -half, 0.0f, half};

		cube.z = UI_SAVE_CUBES_EYE - 4.0f;
		cube.turn = turn;
		assert(UISaveCubes_Faces(&cube, 0.0f, 640.0f, quads) == 0);
		cube.turn = NULL;
	}
	cube.z = UI_SAVE_CUBES_EYE + 19.0f;
	assert(UISaveCubes_Faces(&cube, 0.0f, 640.0f, quads) == 0);
	cube = cubeAt(320.0f, 224.0f, 19.0f, UI_SAVE_CUBES_KIND_SAVE);
	cube.alpha = 0;
	assert(UISaveCubes_Faces(&cube, 0.0f, 640.0f, quads) == 0);
	cube = cubeAt(-60.0f, 224.0f, 19.0f, UI_SAVE_CUBES_KIND_SAVE);
	assert(UISaveCubes_Faces(&cube, 0.0f, 640.0f, quads) == 0);
	assert(UISaveCubes_Faces(&cube, -107.0f, 747.0f, quads) > 0);
	cube = cubeAt(320.0f, 540.0f, 19.0f, UI_SAVE_CUBES_KIND_SAVE);
	assert(UISaveCubes_Faces(&cube, 0.0f, 640.0f, quads) == 0);

	/* A free cell: a plain front. A folder: the rim, its inside and the
	 * folder's two shapes, never an icon. A save not read yet: no icon. */
	cube = cubeAt(320.0f, 224.0f, 13.68f, UI_SAVE_CUBES_KIND_EMPTY);
	n = UISaveCubes_Faces(&cube, 0.0f, 640.0f, quads);
	assert(n == 1 && quads[0].role == UI_SAVE_CUBES_ROLE_BODY);
	cube.kind = UI_SAVE_CUBES_KIND_FOLDER;
	n = UISaveCubes_Faces(&cube, 0.0f, 640.0f, quads);
	assert(n == 7 && roles(quads, n, UI_SAVE_CUBES_ROLE_GLYPH) == 2);
	assert(roles(quads, n, UI_SAVE_CUBES_ROLE_ICON) == 0);
	cube.kind = UI_SAVE_CUBES_KIND_SAVE;
	cube.icon = NULL;
	n = UISaveCubes_Faces(&cube, 0.0f, 640.0f, quads);
	assert(n == 5 && roles(quads, n, UI_SAVE_CUBES_ROLE_ICON) == 0);
}

/* Cubes at rest never overlap, bobbing either way, so they need no depth
 * and draw in one batch. Every cell of both stacks' six drawn rows. */
static void testFootprints(void)
{
	static float box[UI_SAVE_CUBES_FRAME * 2][4];
	uiSaveCubesQuad_t quads[UI_SAVE_CUBES_QUADS];
	int count = 0, s, cell, i, j;
	float bob;

	for(bob = -UI_SAVE_CUBES_BOB_PX; bob <= UI_SAVE_CUBES_BOB_PX; bob += UI_SAVE_CUBES_BOB_PX) {
		count = 0;
		for(s = 0; s < UI_SAVE_CUBES_STACKS; s++) {
			for(cell = 0; cell < UI_SAVE_CUBES_DRAWN; cell++) {
				int row = cell / 4 - 1;
				/* Neighbours bob opposite ways: the worst case. */
				float shift = (cell + row) % 2 ? bob : -bob;
				uiSaveCube_t cube = cubeAt(UI_SAVE_CUBES_STACK_X +
					(float)s * UI_SAVE_CUBES_STACK_GAP + ((float)(cell % 4) - 1.5f) *
					UI_SAVE_CUBES_PITCH, UI_SAVE_CUBES_TOP_Y + (float)row *
					UI_SAVE_CUBES_PITCH + shift, 0.5f * UI_SAVE_CUBES_FACE,
					UI_SAVE_CUBES_KIND_SAVE);
				int n = UISaveCubes_Faces(&cube, -107.0f, 747.0f, quads);

				assert(n > 0);
				bounds(quads, n, box[count++]);
			}
		}
		for(i = 0; i < count; i++) {
			for(j = i + 1; j < count; j++) {
				assert(box[i][2] <= box[j][0] || box[j][2] <= box[i][0] ||
					box[i][3] <= box[j][1] || box[j][3] <= box[i][1]);
			}
		}
	}
}

static void testColours(void)
{
	uint8_t a[4], b[4];

	UISaveCubes_Colour(UI_SAVE_CUBES_SHADE_SAVE, UI_SAVE_CUBES_ROLE_BODY, a);
	assert(a[0] == 39 && a[1] == 46 && a[2] == 141 && a[3] == 235);
	/* Lit from above: the top rim is brightest, the bottom side darkest. */
	UISaveCubes_Colour(UI_SAVE_CUBES_SHADE_SAVE, UI_SAVE_CUBES_ROLE_RIM_TOP, a);
	UISaveCubes_Colour(UI_SAVE_CUBES_SHADE_SAVE, UI_SAVE_CUBES_ROLE_RIM_BOTTOM, b);
	assert(a[2] > b[2] && a[2] == 255 && b[0] == 54);
	UISaveCubes_Colour(UI_SAVE_CUBES_SHADE_SAVE, UI_SAVE_CUBES_ROLE_SIDE_BOTTOM, b);
	assert(b[0] == 23 && b[2] == 84 && b[3] == 235);
	/* A pale cube's rim stays white, not wrapped round. */
	UISaveCubes_Colour(UI_SAVE_CUBES_SHADE_SAVE_FOCUS, UI_SAVE_CUBES_ROLE_RIM_TOP, a);
	assert(a[0] == 255 && a[1] == 255 && a[2] == 255);
	/* A free cell is see-through navy; an unknown shade or part draws as a
	 * save's body. */
	UISaveCubes_Colour(UI_SAVE_CUBES_SHADE_EMPTY, UI_SAVE_CUBES_ROLE_BODY, a);
	assert(a[3] == 160);
	UISaveCubes_Colour(99, -1, a);
	assert(a[0] == 39 && a[3] == 235);
}

/* ------------------------------------------------------------------------
 * Motion.
 * --------------------------------------------------------------------- */
static uiSavesArt_t art;
static uiSaveCubesGrid_t grid;
static uiSaveCubesMotion_t motion;
static uiSaveCube_t cubes[UI_SAVE_CUBES_OUT];

/* Both stacks: saves in the first cells of 40, the rest free. */
static void gridSet(int first0, int first1, int focusStack, int focusCell,
	uint32_t listing)
{
	int s, k;

	for(s = 0; s < UI_SAVE_CUBES_STACKS; s++) {
		uiSaveCubesStack_t *stack = &grid.stack[s];

		stack->cells = 40;
		stack->first = (int16_t)(s ? first1 : first0);
		stack->listing = listing + (uint32_t)s;
		for(k = 0; k < UI_SAVE_CUBES_DRAWN; k++) {
			int cell = (stack->first - 1 + k / 4) * 4 + k % 4;

			stack->cell[k].kind = cell < 30 ? UI_SAVE_CUBES_KIND_SAVE :
				UI_SAVE_CUBES_KIND_EMPTY;
			stack->cell[k].texels = cell < 30 ? texels : NULL;
			stack->cell[k].art = cell < 30 ? &art : NULL;
		}
	}
	grid.focusStack = (int8_t)focusStack;
	grid.focusCell = (int16_t)focusCell;
}

/* Long enough for the screen to have opened. */
#define OPENED 2.0f

static int frame(float dt, uiMotionMode_t mode, int *floating)
{
	int count = UISaveCubes_Frame(&motion, &grid, dt, mode, cubes, floating);

	assert(count >= 0 && count <= UI_SAVE_CUBES_OUT);
	assert(*floating >= 0 && *floating <= count);
	return count;
}

static void run(float seconds, float hz, uiMotionMode_t mode)
{
	int floating, i;

	for(i = 0; i < (int)(seconds * hz + 0.5f); i++) {
		frame(1.0f / hz, mode, &floating);
	}
}

/* Where cell of stack s rests, first being its window's first row. */
static float restX(int s, int cell)
{
	return UI_SAVE_CUBES_STACK_X + (float)s * UI_SAVE_CUBES_STACK_GAP +
		((float)(cell % 4) - 1.5f) * UI_SAVE_CUBES_PITCH;
}

static float restY(int cell, int first)
{
	return UI_SAVE_CUBES_TOP_Y + (float)(cell / 4 - first) * UI_SAVE_CUBES_PITCH;
}

static const uiSaveCube_t *find(int count, int s, int cell, int first)
{
	int i;

	for(i = 0; i < count; i++) {
		if(near(cubes[i].x, restX(s, cell), 12.0f) &&
			near(cubes[i].y, restY(cell, first), 12.0f)) {
			return &cubes[i];
		}
	}
	return NULL;
}

static void testGrow(float hz)
{
	const uiSaveCube_t *focus, *old;
	int floating, count;

	memset(&motion, 0, sizeof(motion));
	gridSet(0, 0, 0, 5, 10u);
	run(OPENED, hz, UI_MOTION_FULL);
	gridSet(0, 0, 0, 6, 10u);
	run(0.15f, hz, UI_MOTION_FULL);
	count = frame(0.0f, UI_MOTION_FULL, &floating);
	/* The new focus is 1.5x and forward, drawn last; the old one is back. */
	focus = &cubes[count - 1];
	assert(near(focus->half, 0.5f * UI_SAVE_CUBES_FACE * UI_SAVE_CUBES_SELECTED, 0.3f));
	assert(near(focus->z, UI_SAVE_CUBES_LIFT, 0.4f));
	assert(focus->shade == UI_SAVE_CUBES_SHADE_SAVE_FOCUS && focus->turn != NULL);
	old = find(count - 1, 0, 5, 0);
	assert(old != NULL && old->half < 0.5f * UI_SAVE_CUBES_FACE + 0.4f);
	assert(old->shade == UI_SAVE_CUBES_SHADE_SAVE);
}

static void testMotion(void)
{
	const uiSaveCube_t *cube;
	float full = 0.0f, reduced = 0.0f, low = FAR, high = -FAR, other = FAR;
	int floating, count, i, k;

	testGrow(60.0f);
	testGrow(50.0f);

	/* Halfway through a step, both cubes float, nearest last. */
	memset(&motion, 0, sizeof(motion));
	gridSet(0, 0, 0, 5, 10u);
	run(OPENED, 60.0f, UI_MOTION_FULL);
	gridSet(0, 0, 0, 6, 10u);
	count = frame(1.0f / 30.0f, UI_MOTION_FULL, &floating);
	assert(count - floating == 2 && cubes[floating].z < cubes[count - 1].z);

	/* Motion Off: on the first frame the focus is grown, still and square,
	 * nothing bobs, and the icon holds its first frame. */
	memset(&motion, 0, sizeof(motion));
	gridSet(1, 0, 0, 9, 20u);
	frame(1.0f / 60.0f, UI_MOTION_OFF, &floating);
	gridSet(1, 0, 0, 10, 20u);
	for(k = 0; k < 120; k++) {
		count = frame(1.0f / 60.0f, UI_MOTION_OFF, &floating);
		cube = &cubes[count - 1];
		assert(cube->half == 0.5f * UI_SAVE_CUBES_FACE * UI_SAVE_CUBES_SELECTED);
		assert(cube->turn == NULL && cube->x == restX(0, 10) && cube->y == restY(10, 1));
		assert(cube->icon == texels);
		for(i = 0; i < floating; i++) {
			float rows = (cubes[i].y - UI_SAVE_CUBES_TOP_Y) / UI_SAVE_CUBES_PITCH;

			assert(rows == floorf(rows));
			assert(cubes[i].icon == NULL || cubes[i].icon == texels);
		}
	}

	/* The wobble: Reduced moves the focus no more than 0.35 of Full. */
	for(k = 0; k < 2; k++) {
		uiMotionMode_t mode = k ? UI_MOTION_REDUCED : UI_MOTION_FULL;
		float *most = k ? &reduced : &full;

		memset(&motion, 0, sizeof(motion));
		gridSet(0, 0, 1, 2, 30u);
		run(OPENED, 60.0f, mode);
		for(i = 0; i < 360; i++) {
			count = frame(1.0f / 60.0f, mode, &floating);
			cube = &cubes[count - 1];
			*most = fmaxf(*most, fabsf(cube->x - restX(1, 2)) + fabsf(cube->y - restY(2, 0)));
		}
	}
	assert(full > 5.0f && reduced > 0.0f && reduced <= 0.351f * full);

	/* Full: every other cube bobs, a pixel and a half, out of step with
	 * its neighbour. Reduced: none bob. */
	memset(&motion, 0, sizeof(motion));
	gridSet(0, 0, 1, 0, 40u);
	run(OPENED, 60.0f, UI_MOTION_FULL);
	for(i = 0; i < 240; i++) {
		count = frame(1.0f / 60.0f, UI_MOTION_FULL, &floating);
		cube = find(floating, 0, 1, 0);
		assert(cube != NULL);
		low = fminf(low, cube->y - restY(1, 0));
		high = fmaxf(high, cube->y - restY(1, 0));
		other = fminf(other, fabsf((cube->y - restY(1, 0)) -
			(find(floating, 0, 2, 0)->y - restY(2, 0))));
		assert(other < FAR);
	}
	assert(high > 1.3f && high <= UI_SAVE_CUBES_BOB_PX + 1e-3f && low < -1.3f);
	assert(other < 0.2f);	/* they cross */
	for(i = 0, high = 0.0f; i < 240; i++) {
		count = frame(1.0f / 60.0f, UI_MOTION_FULL, &floating);
		high = fmaxf(high, fabsf((find(floating, 0, 1, 0)->y - restY(1, 0)) -
			(find(floating, 0, 2, 0)->y - restY(2, 0))));
	}
	assert(high > 1.0f);	/* and part */
	for(i = 0; i < 120; i++) {
		count = frame(1.0f / 60.0f, UI_MOTION_REDUCED, &floating);
		cube = find(floating, 0, 1, 0);
		assert(cube->y == restY(1, 0));
	}

	/* Scrolling: rows outside the window fade, then settle out of sight. */
	memset(&motion, 0, sizeof(motion));
	gridSet(1, 0, 0, 16, 50u);
	run(1.0f, 60.0f, UI_MOTION_REDUCED);
	count = frame(0.0f, UI_MOTION_REDUCED, &floating);
	assert(count == 16 + 16);	/* four rows a stack: row 0 and row 5 are clear */
	for(i = 0; i < count; i++) {
		assert(cubes[i].alpha == 255);
	}
	gridSet(2, 0, 0, 20, 50u);
	count = frame(1.0f / 30.0f, UI_MOTION_REDUCED, &floating);
	for(i = 0, k = 0; i < count; i++) {
		k += cubes[i].alpha > 0 && cubes[i].alpha < 255;
	}
	assert(count > 32 && k >= 4);
	run(1.0f, 60.0f, UI_MOTION_REDUCED);
	count = frame(0.0f, UI_MOTION_REDUCED, &floating);
	assert(count == 32);
	cube = find(count, 0, 8, 2);
	assert(cube != NULL && cube->y == UI_SAVE_CUBES_TOP_Y);
	/* Another listing starts still: no scroll, the focus grown at once,
	 * every cube popping in from nothing where it rests. */
	gridSet(5, 0, 0, 22, 60u);
	count = frame(1.0f / 60.0f, UI_MOTION_FULL, &floating);
	assert(find(count, 0, 22, 5) == NULL && find(count, 0, 21, 5) == NULL);
	run(0.1f, 60.0f, UI_MOTION_FULL);
	count = frame(0.0f, UI_MOTION_FULL, &floating);
	cube = &cubes[count - 1];
	assert(near(cube->x, restX(0, 22), 12.0f) && cube->half > 0.0f &&
		cube->half < 0.3f * UI_SAVE_CUBES_FACE * UI_SAVE_CUBES_SELECTED);
	cube = find(floating, 0, 21, 5);
	assert(cube != NULL && near(cube->y, UI_SAVE_CUBES_TOP_Y, UI_SAVE_CUBES_BOB_PX + 1e-3f));
	assert(cube->half > 0.0f && cube->half < 0.3f * UI_SAVE_CUBES_FACE);
	run(0.6f, 60.0f, UI_MOTION_FULL);
	count = frame(0.0f, UI_MOTION_FULL, &floating);
	assert(cubes[count - 1].half == 0.5f * UI_SAVE_CUBES_FACE * UI_SAVE_CUBES_SELECTED);
	assert(find(floating, 0, 21, 5)->half == 0.5f * UI_SAVE_CUBES_FACE);

	/* No grid: nothing drawn for that stack. */
	memset(&motion, 0, sizeof(motion));
	gridSet(0, 0, 1, 0, 70u);
	grid.stack[0].cells = 0;
	count = frame(1.0f / 60.0f, UI_MOTION_FULL, &floating);
	for(i = 0; i < count; i++) {
		assert(cubes[i].x > UI_SAVE_CUBES_STACK_X + UI_SAVE_CUBES_STACK_GAP / 2.0f);
	}
}

static void testIcons(void)
{
	uiSaveCubesCell_t cell;

	memset(&art, 0, sizeof(art));
	art.steps = 2;
	art.stepFrame[0] = 0;
	art.stepFrame[1] = 1;
	art.stepHold[0] = 1;
	art.stepHold[1] = 2;
	art.period = 3;
	cell.texels = texels;
	cell.art = &art;
	cell.kind = UI_SAVE_CUBES_KIND_SAVE;
	/* 15 ticks a second: frame 0 for a tick, frame 1 for two, and round. */
	assert(UISaveCubes_Icon(&cell, 0.0f, UI_MOTION_FULL) == texels);
	assert(UISaveCubes_Icon(&cell, 1.5f / 15.0f, UI_MOTION_FULL) ==
		texels + UI_SAVES_ICON_BYTES);
	assert(UISaveCubes_Icon(&cell, 2.5f / 15.0f, UI_MOTION_REDUCED) ==
		texels + UI_SAVES_ICON_BYTES);
	assert(UISaveCubes_Icon(&cell, 3.5f / 15.0f, UI_MOTION_FULL) == texels);
	assert(UISaveCubes_Icon(&cell, 1.5f / 15.0f, UI_MOTION_OFF) == texels);
	art.stepFrame[1] = UI_SAVES_ART_BLANK;
	assert(UISaveCubes_Icon(&cell, 1.5f / 15.0f, UI_MOTION_FULL) == NULL);
	cell.kind = UI_SAVE_CUBES_KIND_FOLDER;
	assert(UISaveCubes_Icon(&cell, 0.0f, UI_MOTION_FULL) == NULL);
	cell.kind = UI_SAVE_CUBES_KIND_SAVE;
	cell.texels = NULL;
	assert(UISaveCubes_Icon(&cell, 0.0f, UI_MOTION_FULL) == NULL);
	art.stepFrame[1] = 1;
}

/* ------------------------------------------------------------------------
 * The cursor.
 * --------------------------------------------------------------------- */
static uiSaveCubesCursor_t cursorAt(int cells0, int first0, int cells1,
	int first1, int stack, int cell)
{
	uiSaveCubesCursor_t cursor;

	cursor.cells[0] = cells0;
	cursor.first[0] = first0;
	cursor.cells[1] = cells1;
	cursor.first[1] = first1;
	cursor.stack = stack;
	cursor.cell = cell;
	return cursor;
}

static void testCursor(void)
{
	uiSaveCubesCursor_t cursor;
	int i;

	assert(UISaveCubes_Cells(0) == 16);
	assert(UISaveCubes_Cells(15) == 16);
	assert(UISaveCubes_Cells(16) == 20);
	assert(UISaveCubes_Cells(27) == 28);
	assert(UISaveCubes_Cells(127) == 128);
	assert(UISaveCubes_Cells(256) == 256);
	assert(UISaveCubes_Cells(-3) == 16);

	/* The window moves only to keep the cursor in it, on the edge row. */
	assert(UISaveCubes_Window(0, 15, 28) == 0);
	assert(UISaveCubes_Window(0, 16, 28) == 1);
	assert(UISaveCubes_Window(3, 13, 28) == 3);
	assert(UISaveCubes_Window(3, 11, 28) == 2);
	assert(UISaveCubes_Window(5, 27, 28) == 3);
	assert(UISaveCubes_Window(2, 3, 16) == 0);

	/* Right from the left stack's fourth column: the right stack's first, at
	 * the same row on screen, whatever each has scrolled to. */
	cursor = cursorAt(28, 2, 40, 5, 0, 4 * 4 + 3);
	assert(UISaveCubes_Step(&cursor, UI_SAVE_CUBES_RIGHT));
	assert(cursor.stack == 1 && cursor.cell == 7 * 4 && cursor.first[1] == 5);
	assert(UISaveCubes_Step(&cursor, UI_SAVE_CUBES_LEFT));
	assert(cursor.stack == 0 && cursor.cell == 4 * 4 + 3 && cursor.first[0] == 2);
	/* Into a stack scrolled elsewhere: the same row on screen. */
	cursor = cursorAt(16, 0, 64, 12, 1, 15 * 4);
	assert(UISaveCubes_Step(&cursor, UI_SAVE_CUBES_LEFT));
	assert(cursor.stack == 0 && cursor.cell == 3 * 4 + 3);
	/* Within a stack: a column, or a row and the window with it. */
	cursor = cursorAt(28, 0, 16, 0, 0, 13);
	assert(UISaveCubes_Step(&cursor, UI_SAVE_CUBES_RIGHT) && cursor.cell == 14);
	assert(UISaveCubes_Step(&cursor, UI_SAVE_CUBES_DOWN) && cursor.cell == 18 &&
		cursor.first[0] == 1);
	assert(UISaveCubes_Step(&cursor, UI_SAVE_CUBES_DOWN) && cursor.cell == 22 &&
		cursor.first[0] == 2);
	assert(UISaveCubes_Step(&cursor, UI_SAVE_CUBES_UP) && cursor.cell == 18 &&
		cursor.first[0] == 2);
	/* Bumps at every edge, never a wrap. */
	assert(UISaveCubes_Step(&cursor, UI_SAVE_CUBES_DOWN));
	assert(UISaveCubes_Step(&cursor, UI_SAVE_CUBES_DOWN) && cursor.cell == 26);
	assert(!UISaveCubes_Step(&cursor, UI_SAVE_CUBES_DOWN) && cursor.cell == 26 &&
		cursor.first[0] == 3);
	cursor = cursorAt(28, 0, 16, 0, 0, 2);
	assert(!UISaveCubes_Step(&cursor, UI_SAVE_CUBES_UP) && cursor.cell == 2);
	cursor = cursorAt(28, 0, 16, 0, 0, 8);
	assert(!UISaveCubes_Step(&cursor, UI_SAVE_CUBES_LEFT) && cursor.stack == 0);
	cursor = cursorAt(28, 0, 16, 0, 1, 7);
	assert(!UISaveCubes_Step(&cursor, UI_SAVE_CUBES_RIGHT) && cursor.cell == 7);
	/* A stack with no grid is never stepped into. */
	cursor = cursorAt(0, 0, 16, 0, 1, 4);
	assert(!UISaveCubes_Step(&cursor, UI_SAVE_CUBES_LEFT) && cursor.stack == 1);
	cursor = cursorAt(16, 0, 16, 0, -1, 0);
	assert(!UISaveCubes_Step(&cursor, UI_SAVE_CUBES_DOWN));
	/* Down and up a long stack visit every row once. */
	cursor = cursorAt(128, 0, 16, 0, 0, 1);
	for(i = 0; UISaveCubes_Step(&cursor, UI_SAVE_CUBES_DOWN); i++) {
		assert(cursor.cell / 4 - cursor.first[0] == (i < 2 ? i + 1 : 3));
	}
	assert(i == 31 && cursor.cell == 125 && cursor.first[0] == 28);

	/* The cursor stays in its stack while that has a grid, else goes to
	 * the first that has one, else nowhere. */
	{
		int cells[2] = {16, 0};

		assert(UISaveCubes_Home(cells, 0) == 0);
		assert(UISaveCubes_Home(cells, 1) == 0);
		assert(UISaveCubes_Home(cells, -1) == 0);
		cells[0] = 0;
		cells[1] = 20;
		assert(UISaveCubes_Home(cells, 0) == 1 && UISaveCubes_Home(cells, -1) == 1);
		cells[0] = 16;
		assert(UISaveCubes_Home(cells, 1) == 1 && UISaveCubes_Home(cells, -1) == 0);
		cells[0] = cells[1] = 0;
		assert(UISaveCubes_Home(cells, 1) == -1);
	}

	/* L and R: each stack takes the place neither shows, A, B or the SD
	 * card, unless the press found a card that wasn't there. */
	assert(UISaveCubes_Swap(0, 1, false) == 2);
	assert(UISaveCubes_Swap(2, 1, false) == 0);
	assert(UISaveCubes_Swap(1, 0, false) == 2);
	assert(UISaveCubes_Swap(2, 0, false) == 1);
	assert(UISaveCubes_Swap(1, 2, false) == 0);
	assert(UISaveCubes_Swap(1, 0, true) == 1);
}

/* ------------------------------------------------------------------------
 * Opening and leaving.
 * --------------------------------------------------------------------- */
static float apart(const uiSaveCube_t *cube, float x, float y)
{
	return fabsf(cube->x - x) + fabsf(cube->y - y);
}

static void testOpening(void)
{
	const uiSaveCube_t *cube;
	float way, rest;
	int floating, count, i, k;

	/* Full: the Home cube goes back over 0.3 s; nothing shows until the
	 * cubes start out of the middle at 0.15 s; the paper comes from 0.12 s
	 * to 0.45; the words from 0.95 to 1.25; the focus grows from 1.1. */
	memset(&motion, 0, sizeof(motion));
	gridSet(0, 0, 0, 5, 200u);
	count = frame(1.0f / 60.0f, UI_MOTION_FULL, &floating);
	assert(count == 0 && motion.handover > 0.99f && motion.chrome == 0.0f &&
		motion.paper == 0.0f);
	run(0.1f, 60.0f, UI_MOTION_FULL);
	assert(frame(1.0f / 60.0f, UI_MOTION_FULL, &floating) == 0);
	run(0.2f, 60.0f, UI_MOTION_FULL);
	count = frame(1.0f / 60.0f, UI_MOTION_FULL, &floating);
	assert(count > 0 && motion.handover < 0.01f && motion.paper > 0.5f);
	/* On their way: drawn in toward the middle, and small. */
	for(i = 0, k = 0, way = 0.0f; i < count; i++) {
		cube = &cubes[i];
		assert(cube->half < 0.5f * UI_SAVE_CUBES_FACE);
		k += apart(cube, 320.0f, 224.0f) < 100.0f;
		way += apart(cube, 320.0f, 224.0f) / (float)count;
	}
	assert(k > 0);
	run(0.5f, 60.0f, UI_MOTION_FULL);	/* t = 0.87 */
	count = frame(1.0f / 60.0f, UI_MOTION_FULL, &floating);
	for(i = 0, rest = 0.0f; i < count; i++) {
		rest += apart(&cubes[i], 320.0f, 224.0f) / (float)count;
	}
	assert(way < 0.7f * rest);
	assert(motion.paper == 1.0f && motion.chrome == 0.0f);
	assert(find(count, 0, 0, 0) != NULL && find(count, 1, 15, 0) != NULL);
	assert(find(count, 0, 5, 0)->half == 0.5f * UI_SAVE_CUBES_FACE);
	run(0.3f, 60.0f, UI_MOTION_FULL);	/* t = 1.18 */
	count = frame(1.0f / 60.0f, UI_MOTION_FULL, &floating);
	assert(motion.chrome > 0.5f && motion.chrome < 1.0f);
	assert(cubes[count - 1].half > 0.5f * UI_SAVE_CUBES_FACE);
	run(0.2f, 60.0f, UI_MOTION_FULL);
	count = frame(1.0f / 60.0f, UI_MOTION_FULL, &floating);
	assert(motion.chrome == 1.0f && count == 32);
	for(i = 0; i < count; i++) {
		assert(cubes[i].alpha == 255);
	}

	/* A stack listed while it opens spirals in from then. */
	memset(&motion, 0, sizeof(motion));
	gridSet(0, 0, 0, 5, 210u);
	grid.stack[1].cells = 0;
	run(0.5f, 60.0f, UI_MOTION_FULL);
	grid.stack[1].cells = 40;
	grid.stack[1].listing = 300u;
	run(0.1f, 60.0f, UI_MOTION_FULL);
	count = frame(1.0f / 60.0f, UI_MOTION_FULL, &floating);
	for(i = 0; i < count; i++) {
		assert(cubes[i].x < UI_SAVE_CUBES_STACK_X + UI_SAVE_CUBES_STACK_GAP / 2.0f);
	}
	run(0.9f, 60.0f, UI_MOTION_FULL);
	count = frame(1.0f / 60.0f, UI_MOTION_FULL, &floating);
	assert(find(count, 1, 15, 0) != NULL && find(count, 1, 15, 0)->alpha == 255);

	/* Reduced: everything where it rests, fading in over 0.25 s; the Home
	 * cube gone at once. */
	memset(&motion, 0, sizeof(motion));
	gridSet(0, 0, 0, 5, 220u);
	run(0.1f, 60.0f, UI_MOTION_REDUCED);
	count = frame(0.0f, UI_MOTION_REDUCED, &floating);
	assert(count == 32 && motion.handover == 0.0f);
	assert(motion.chrome > 0.2f && motion.chrome < 0.6f);
	cube = find(count, 1, 15, 0);
	assert(cube != NULL && cube->x == restX(1, 15) && cube->alpha > 50 && cube->alpha < 160);
	run(0.2f, 60.0f, UI_MOTION_REDUCED);
	count = frame(0.0f, UI_MOTION_REDUCED, &floating);
	assert(find(count, 1, 15, 0)->alpha == 255 && motion.chrome == 1.0f);

	/* Off: all there on the first frame. */
	memset(&motion, 0, sizeof(motion));
	gridSet(0, 0, 0, 5, 230u);
	count = frame(1.0f / 60.0f, UI_MOTION_OFF, &floating);
	assert(count == 32 && motion.chrome == 1.0f && motion.paper == 1.0f &&
		motion.handover == 0.0f);
	assert(cubes[count - 1].half == 0.5f * UI_SAVE_CUBES_FACE * UI_SAVE_CUBES_SELECTED);
}

static void testLeaving(void)
{
	int floating, count, i;

	assert(UISaveCubes_LeaveSeconds(UI_MOTION_FULL) == 0.45f);
	assert(UISaveCubes_LeaveSeconds(UI_MOTION_REDUCED) == 0.2f);
	assert(UISaveCubes_LeaveSeconds(UI_MOTION_OFF) == 0.0f);

	/* Full: the words gone in 0.12 s, the cubes in the middle and clear by
	 * 0.3, the Home cube back from 0.15 to 0.45. */
	memset(&motion, 0, sizeof(motion));
	gridSet(0, 0, 0, 5, 240u);
	run(OPENED, 60.0f, UI_MOTION_FULL);
	grid.leaving = 1;
	run(0.06f, 60.0f, UI_MOTION_FULL);
	count = frame(0.0f, UI_MOTION_FULL, &floating);
	assert(motion.chrome > 0.3f && motion.chrome < 0.7f && motion.handover == 0.0f);
	for(i = 0; i < count; i++) {
		assert(cubes[i].alpha > 0);
	}
	run(0.25f, 60.0f, UI_MOTION_FULL);
	count = frame(0.0f, UI_MOTION_FULL, &floating);
	assert(count > 0 && motion.chrome == 0.0f);
	assert(motion.handover > 0.3f && motion.handover < 1.0f);
	for(i = 0; i < count; i++) {
		assert(cubes[i].alpha == 0 && near(cubes[i].x, 320.0f, 0.01f) &&
			near(cubes[i].y, 224.0f, 0.01f));
	}
	run(0.15f, 60.0f, UI_MOTION_FULL);
	frame(0.0f, UI_MOTION_FULL, &floating);
	assert(motion.handover == 1.0f && motion.paper < 0.01f);
	grid.leaving = 0;

	/* Reduced: they fade where they are; no Home cube. */
	memset(&motion, 0, sizeof(motion));
	gridSet(0, 0, 0, 5, 250u);
	run(OPENED, 60.0f, UI_MOTION_REDUCED);
	grid.leaving = 1;
	run(0.1f, 60.0f, UI_MOTION_REDUCED);
	count = frame(0.0f, UI_MOTION_REDUCED, &floating);
	assert(find(count, 1, 15, 0)->alpha < 160 && find(count, 1, 15, 0)->alpha > 90);
	run(0.11f, 60.0f, UI_MOTION_REDUCED);
	count = frame(0.0f, UI_MOTION_REDUCED, &floating);
	for(i = 0; i < count; i++) {
		assert(cubes[i].alpha == 0);
	}
	assert(find(count, 1, 15, 0) != NULL && motion.handover == 0.0f);
	grid.leaving = 0;
}

/* ------------------------------------------------------------------------
 * Copy, Move and Erase.
 * --------------------------------------------------------------------- */
/* Stack s: cells, its window at first, the first saves of them saves. */
static void stackSet(int s, int cells, int first, int saves, uint32_t listing,
	uint8_t change, int at)
{
	uiSaveCubesStack_t *stack = &grid.stack[s];
	int k;

	stack->cells = (int16_t)cells;
	stack->first = (int16_t)first;
	stack->listing = listing;
	stack->change = change;
	stack->changeAt = (int16_t)at;
	for(k = 0; k < UI_SAVE_CUBES_DRAWN; k++) {
		int cell = (first - 1 + k / 4) * 4 + k % 4;

		stack->cell[k].kind = cell < saves ? UI_SAVE_CUBES_KIND_SAVE :
			UI_SAVE_CUBES_KIND_EMPTY;
		stack->cell[k].texels = cell < saves ? texels : NULL;
		stack->cell[k].art = cell < saves ? &art : NULL;
	}
}

/* The left stack's 30 saves, the right's 10 of 16 cells, the focus on the
 * left's cell 5, opened. */
static void opStart(uiMotionMode_t mode, uint32_t listing)
{
	memset(&motion, 0, sizeof(motion));
	memset(&grid, 0, sizeof(grid));
	stackSet(0, 32, 0, 30, listing, UI_SAVE_CUBES_NEW, 0);
	stackSet(1, 16, 0, 10, listing + 1u, UI_SAVE_CUBES_NEW, 0);
	grid.focusStack = 0;
	grid.focusCell = 5;
	run(OPENED, 60.0f, mode);
}

static void opSet(uint8_t kind, uint8_t phase, uint16_t serial, int fromCell,
	int toCell)
{
	uiSaveCubesOp_t *op = &grid.op;

	op->kind = kind;
	op->phase = phase;
	op->serial = serial;
	op->from = 0;
	op->fromCell = (int16_t)fromCell;
	op->toCell = (int16_t)toCell;
	op->cube.texels = texels;
	op->cube.art = &art;
	op->cube.kind = UI_SAVE_CUBES_KIND_SAVE;
}

/* The operation's own cube: flying, or being erased. */
static const uiSaveCube_t *opCube(int count)
{
	int i;

	for(i = 0; i < count; i++) {
		if(cubes[i].turn == motion.opTurn) {
			return &cubes[i];
		}
	}
	return NULL;
}

static int bits(int count)
{
	int found = 0, i;

	for(i = 0; i < count; i++) {
		found += cubes[i].shade == UI_SAVE_CUBES_SHADE_PLAIN &&
			cubes[i].kind == UI_SAVE_CUBES_KIND_EMPTY;
	}
	return found;
}

static void testFlight(void)
{
	const uiSaveCube_t *cube;
	float fromX = restX(0, 5), fromY = restY(5, 0);
	float toX = restX(1, 10), toY = restY(10, 0);
	float realX = restX(1, 3), realY = restY(3, 0), most = 0.0f;
	int floating, count, i;

	assert(UISaveCubes_OpSeconds(UI_SAVE_CUBES_OP_COPY, UI_SAVE_CUBES_GO, UI_MOTION_FULL) == 0.6f);
	assert(UISaveCubes_OpSeconds(UI_SAVE_CUBES_OP_MOVE, UI_SAVE_CUBES_LAND, UI_MOTION_REDUCED) == 0.2f);
	assert(UISaveCubes_OpSeconds(UI_SAVE_CUBES_OP_COPY, UI_SAVE_CUBES_BACK, UI_MOTION_FULL) == 0.75f);
	assert(UISaveCubes_OpSeconds(UI_SAVE_CUBES_OP_COPY, UI_SAVE_CUBES_GO, UI_MOTION_REDUCED) == 0.35f);
	assert(UISaveCubes_OpSeconds(UI_SAVE_CUBES_OP_ERASE, UI_SAVE_CUBES_LAND, UI_MOTION_FULL) == 0.4f);
	assert(UISaveCubes_OpSeconds(UI_SAVE_CUBES_OP_ERASE, UI_SAVE_CUBES_LAND, UI_MOTION_REDUCED) == 0.2f);
	assert(UISaveCubes_OpSeconds(UI_SAVE_CUBES_OP_COPY, UI_SAVE_CUBES_LAND, UI_MOTION_OFF) == 0.0f);
	assert(UISaveCubes_OpSeconds(UI_SAVE_CUBES_OP_NONE, UI_SAVE_CUBES_GO, UI_MOTION_FULL) == 0.0f);

	/* GO: the copy leaves the focused cube as it is, 1.5x and forward. */
	opStart(UI_MOTION_FULL, 400u);
	opSet(UI_SAVE_CUBES_OP_COPY, UI_SAVE_CUBES_GO, 1, 5, 10);
	count = frame(1.0f / 60.0f, UI_MOTION_FULL, &floating);
	cube = opCube(count);
	assert(cube != NULL && cube == &cubes[count - 1] && cube->icon != NULL);
	assert(near(cube->x, fromX, 0.5f) && near(cube->y, fromY, 0.5f));
	assert(near(cube->half, 0.5f * UI_SAVE_CUBES_FACE * UI_SAVE_CUBES_SELECTED, 0.2f));
	assert(near(cube->z, UI_SAVE_CUBES_LIFT, 0.5f));
	assert(find(count, 0, 5, 0) != NULL && find(count, 0, 5, 0) != cube);
	/* Halfway it dips toward the info bar and comes toward the viewer. */
	run(0.29f, 60.0f, UI_MOTION_FULL);
	count = frame(0.0f, UI_MOTION_FULL, &floating);
	cube = opCube(count);
	assert(cube->y > 0.5f * (fromY + toY) + 40.0f && cube->z > 70.0f);
	assert(cube->half > 0.5f * UI_SAVE_CUBES_FACE * 1.7f);
	assert(cube->x > fromX && cube->x < toX);
	/* Then it hovers on the cell for as long as the card takes, lifted:
	 * it never lands before LAND. */
	for(i = 0; i < 300; i++) {
		count = frame(1.0f / 60.0f, UI_MOTION_FULL, &floating);
		cube = opCube(count);
		assert(cube != NULL && cube->z >= UI_SAVE_CUBES_LIFT - 1e-3f);
		if(i > 30) {
			assert(near(cube->x, toX, 1e-3f) && near(cube->y, toY, 1e-3f));
			assert(cube->half >= 0.5f * UI_SAVE_CUBES_FACE - 1e-3f &&
				cube->half <= 0.5f * UI_SAVE_CUBES_FACE * 1.24f);
		}
	}
	/* LAND: written and read back, it lands on its cell 0.2 s later, the
	 * cell's own cube waiting for it, never sooner. */
	stackSet(1, 16, 0, 11, 402u, UI_SAVE_CUBES_OPENED, 10);
	opSet(UI_SAVE_CUBES_OP_COPY, UI_SAVE_CUBES_LAND, 1, 5, 10);
	for(i = 0; i <= 12; i++) {	/* 0 .. 0.2 s */
		count = frame(i ? 1.0f / 60.0f : 0.0f, UI_MOTION_FULL, &floating);
		cube = opCube(count);
		assert(cube != NULL && find(count, 1, 10, 0) == cube);
		for(int k = 0; k < count; k++) {
			assert(&cubes[k] == cube || apart(&cubes[k], toX, toY) > 20.0f);
		}
		if(i < 12) {
			assert(cube->z > 0.01f);
		}
	}
	assert(apart(cube, toX, toY) < 1e-3f && cube->z < 1e-3f);
	assert(near(cube->half, 0.5f * UI_SAVE_CUBES_FACE, 1e-3f));
	/* Then the cell's own cube takes over, where the flight left off and
	 * at the size it landed: it doesn't shrink and grow again. */
	grid.op.kind = UI_SAVE_CUBES_OP_NONE;
	count = frame(1.0f / 60.0f, UI_MOTION_FULL, &floating);
	assert(opCube(count) == NULL);
	cube = find(count, 1, 10, 0);
	assert(cube != NULL && near(cube->x, toX, 1e-3f) && cube->kind == UI_SAVE_CUBES_KIND_SAVE);
	assert(near(cube->half, 0.5f * UI_SAVE_CUBES_FACE, 1e-3f));
	/* The card put the next one in a free place at cell 3, not the 11 it
	 * aimed at: it lands there, and the saves after it go on a cell. */
	opSet(UI_SAVE_CUBES_OP_COPY, UI_SAVE_CUBES_GO, 11, 5, 11);
	run(1.0f, 60.0f, UI_MOTION_FULL);
	stackSet(1, 16, 0, 12, 404u, UI_SAVE_CUBES_OPENED, 3);
	opSet(UI_SAVE_CUBES_OP_COPY, UI_SAVE_CUBES_LAND, 11, 5, 3);
	count = frame(1.0f / 60.0f, UI_MOTION_FULL, &floating);
	cube = opCube(count);
	assert(near(cube->x, restX(1, 11), 4.0f));
	cube = find(count, 1, 3, 0);	/* cell 4's save, a cell back still */
	assert(cube != NULL && cube->turn != motion.opTurn);
	run(0.1f, 60.0f, UI_MOTION_FULL);
	count = frame(0.0f, UI_MOTION_FULL, &floating);
	assert(apart(opCube(count), realX, realY) > 1.0f);
	run(0.1f, 60.0f, UI_MOTION_FULL);
	count = frame(0.0f, UI_MOTION_FULL, &floating);
	cube = opCube(count);
	assert(apart(cube, realX, realY) < 1e-3f && cube->z < 1e-3f);
	run(0.6f, 60.0f, UI_MOTION_FULL);
	count = frame(0.0f, UI_MOTION_FULL, &floating);
	assert(find(count, 1, 4, 0)->x == restX(1, 4) && find(count, 1, 11, 0)->x == restX(1, 11));

	/* BACK: it goes home along the arc in 0.45 s, then the save shakes
	 * for 0.3 s and is still. (The focus elsewhere, so the save doesn't
	 * wobble.) */
	opStart(UI_MOTION_FULL, 410u);
	grid.focusCell = 6;
	run(0.5f, 60.0f, UI_MOTION_FULL);
	opSet(UI_SAVE_CUBES_OP_MOVE, UI_SAVE_CUBES_GO, 2, 5, 10);
	run(1.0f, 60.0f, UI_MOTION_FULL);
	count = frame(1.0f / 60.0f, UI_MOTION_FULL, &floating);
	/* A Move's save has left its cell. */
	cube = find(count, 0, 5, 0);
	assert(cube != NULL && cube->kind == UI_SAVE_CUBES_KIND_EMPTY && cube->icon == NULL);
	opSet(UI_SAVE_CUBES_OP_MOVE, UI_SAVE_CUBES_BACK, 2, 5, 10);
	run(0.45f, 60.0f, UI_MOTION_FULL);	/* 0.433 s since it was seen */
	count = frame(0.0f, UI_MOTION_FULL, &floating);
	cube = opCube(count);
	assert(cube != NULL && apart(cube, fromX, fromY) < 6.0f);
	assert(find(count, 0, 5, 0)->kind == UI_SAVE_CUBES_KIND_EMPTY);
	run(0.03f, 60.0f, UI_MOTION_FULL);
	count = frame(0.0f, UI_MOTION_FULL, &floating);
	assert(opCube(count) == NULL && find(count, 0, 5, 0)->kind == UI_SAVE_CUBES_KIND_SAVE);
	for(i = 0; i < 18; i++) {
		count = frame(1.0f / 60.0f, UI_MOTION_FULL, &floating);
		most = fmaxf(most, fabsf(find(count, 0, 5, 0)->x - fromX));
	}
	assert(most > 2.0f && most <= 5.0f);
	run(0.1f, 60.0f, UI_MOTION_FULL);
	count = frame(0.0f, UI_MOTION_FULL, &floating);
	assert(find(count, 0, 5, 0)->x == fromX);

	/* Bound for a folder the stack doesn't show: into its header, where
	 * it shrinks away. */
	opStart(UI_MOTION_FULL, 420u);
	opSet(UI_SAVE_CUBES_OP_COPY, UI_SAVE_CUBES_GO, 3, 5, -1);
	run(1.0f, 60.0f, UI_MOTION_FULL);
	count = frame(0.0f, UI_MOTION_FULL, &floating);
	cube = opCube(count);
	assert(near(cube->x, UI_SAVE_CUBES_STACK_X + UI_SAVE_CUBES_STACK_GAP, 1e-3f) &&
		near(cube->y, UI_SAVE_CUBES_HEADER_Y, 1e-3f));
	opSet(UI_SAVE_CUBES_OP_COPY, UI_SAVE_CUBES_LAND, 3, 5, -1);
	run(0.1f, 60.0f, UI_MOTION_FULL);
	count = frame(0.0f, UI_MOTION_FULL, &floating);
	assert(opCube(count)->half < 0.4f * UI_SAVE_CUBES_FACE);
	run(0.12f, 60.0f, UI_MOTION_FULL);
	count = frame(0.0f, UI_MOTION_FULL, &floating);
	assert(opCube(count) == NULL);

	/* Reduced: a straight 0.35 s glide, unturned and no bigger. */
	opStart(UI_MOTION_REDUCED, 430u);
	opSet(UI_SAVE_CUBES_OP_COPY, UI_SAVE_CUBES_GO, 4, 5, 10);
	frame(1.0f / 60.0f, UI_MOTION_REDUCED, &floating);
	for(i = 0; i < 30; i++) {
		float u;

		count = frame(1.0f / 60.0f, UI_MOTION_REDUCED, &floating);
		cube = opCube(count);
		u = (cube->x - fromX) / (toX - fromX);
		assert(u >= 0.0f && u <= 1.0f + 1e-4f);
		assert(near(cube->y, fromY + u * (toY - fromY), 0.01f));
		assert(cube->turn[2] == 0.0f && cube->half <= 0.5f * UI_SAVE_CUBES_FACE * 1.5f);
	}
	assert(near(cube->x, toX, 1e-3f));

	/* Off: no flight at all; the lists just change. */
	opStart(UI_MOTION_OFF, 440u);
	opSet(UI_SAVE_CUBES_OP_COPY, UI_SAVE_CUBES_GO, 5, 5, 10);
	for(i = 0; i < 60; i++) {
		count = frame(1.0f / 60.0f, UI_MOTION_OFF, &floating);
		assert(opCube(count) == NULL);
	}
	stackSet(1, 16, 0, 11, 442u, UI_SAVE_CUBES_OPENED, 10);
	opSet(UI_SAVE_CUBES_OP_COPY, UI_SAVE_CUBES_LAND, 5, 5, 10);
	count = frame(1.0f / 60.0f, UI_MOTION_OFF, &floating);
	assert(opCube(count) == NULL && find(count, 1, 10, 0)->kind == UI_SAVE_CUBES_KIND_SAVE);
	assert(find(count, 1, 11, 0)->x == restX(1, 11));
}

/* How far along from a to b a cube is, 0 .. 1, or -1 when none is on the
 * way between them. */
static float along(int count, float ax, float ay, float bx, float by)
{
	float length = sqrtf((bx - ax) * (bx - ax) + (by - ay) * (by - ay));
	int i;

	for(i = 0; i < count; i++) {
		float u = ((cubes[i].x - ax) * (bx - ax) + (cubes[i].y - ay) * (by - ay)) /
			(length * length);
		float off = fabsf((cubes[i].x - ax) * (by - ay) - (cubes[i].y - ay) * (bx - ax)) /
			length;

		if(u > 0.01f && u < 0.99f && off < 2.0f) {
			return u;
		}
	}
	return -1.0f;
}

static void testErase(void)
{
	const uiSaveCube_t *cube;
	float sixWas, sevenWas;
	int floating, count, i;
	unsigned last = 256u;

	/* GO: the save loses its icon, shrinks to 0.9 and turns, and stays so
	 * for as long as the card takes, in one piece. */
	opStart(UI_MOTION_FULL, 500u);
	opSet(UI_SAVE_CUBES_OP_ERASE, UI_SAVE_CUBES_GO, 6, 5, 0);
	for(i = 0; i < 120; i++) {
		count = frame(1.0f / 60.0f, UI_MOTION_FULL, &floating);
		assert(bits(count) == 0);
	}
	cube = opCube(count);
	assert(cube != NULL && cube->shade == UI_SAVE_CUBES_SHADE_PLAIN && cube->icon == NULL);
	assert(near(cube->half, 0.5f * UI_SAVE_CUBES_FACE * 0.9f, 1e-3f));
	assert(near(cube->turn[2], sinf(0.6f), 2e-3f) && near(cube->x, restX(0, 5), 1.01f));
	/* LAND: gone, in 8 pieces that scatter, fall and fade for 0.4 s; the
	 * saves after it come back a cell, each a little after the one before,
	 * and the free cell left at the end grows. */
	stackSet(0, 32, 0, 29, 502u, UI_SAVE_CUBES_CLOSED, 5);
	opSet(UI_SAVE_CUBES_OP_ERASE, UI_SAVE_CUBES_LAND, 6, 5, 0);
	count = frame(1.0f / 60.0f, UI_MOTION_FULL, &floating);
	assert(bits(count) == 8 && opCube(count) == NULL);
	/* Every save after it starts a cell on, so none is at the erased
	 * one's cell but its pieces. */
	for(i = 0; i < count; i++) {
		assert((cubes[i].kind == UI_SAVE_CUBES_KIND_EMPTY &&
			cubes[i].shade == UI_SAVE_CUBES_SHADE_PLAIN) ||
			apart(&cubes[i], restX(0, 5), restY(5, 0)) > 12.0f);
	}
	for(i = 0; i < 5; i++) {
		count = frame(1.0f / 60.0f, UI_MOTION_FULL, &floating);
		assert(bits(count) == 8);
		for(int k = 0; k < count; k++) {
			if(cubes[k].shade == UI_SAVE_CUBES_SHADE_PLAIN &&
				cubes[k].kind == UI_SAVE_CUBES_KIND_EMPTY) {
				assert(cubes[k].alpha < last);
				last = cubes[k].alpha;
				break;
			}
		}
	}
	/* 0.1 s in, cell 6's save (from cell 7) is further along than cell 7's
	 * (from cell 8, across the row's end), which started a little later. */
	sixWas = along(count, restX(0, 7), restY(7, 0), restX(0, 6), restY(6, 0));
	sevenWas = along(count, restX(0, 8), restY(8, 0), restX(0, 7), restY(7, 0));
	assert(sevenWas > 0.05f && sixWas > sevenWas + 0.05f);
	run(0.29f, 60.0f, UI_MOTION_FULL);
	count = frame(0.0f, UI_MOTION_FULL, &floating);
	assert(bits(count) == 8);
	run(0.04f, 60.0f, UI_MOTION_FULL);
	count = frame(0.0f, UI_MOTION_FULL, &floating);
	assert(bits(count) == 0);
	run(1.0f, 60.0f, UI_MOTION_FULL);
	count = frame(0.0f, UI_MOTION_FULL, &floating);
	assert(find(count, 0, 7, 0)->x == restX(0, 7));
	cube = find(count, 0, 29, 0);
	assert(cube == NULL || cube->kind == UI_SAVE_CUBES_KIND_EMPTY);

	/* The cell a save left at the end grows from nothing. */
	opStart(UI_MOTION_FULL, 510u);
	stackSet(0, 32, 0, 13, 512u, UI_SAVE_CUBES_NEW, 0);
	run(OPENED, 60.0f, UI_MOTION_FULL);
	stackSet(0, 32, 0, 12, 514u, UI_SAVE_CUBES_CLOSED, 5);
	run(0.4f, 60.0f, UI_MOTION_FULL);
	count = frame(0.0f, UI_MOTION_FULL, &floating);
	cube = find(count, 0, 12, 0);
	assert(cube != NULL && cube->kind == UI_SAVE_CUBES_KIND_EMPTY && cube->half > 1.0f &&
		cube->half < 0.5f * UI_SAVE_CUBES_FACE * UI_SAVE_CUBES_EMPTY - 1.0f);
	assert(find(count, 0, 13, 0)->half == 0.5f * UI_SAVE_CUBES_FACE * UI_SAVE_CUBES_EMPTY);

	/* BACK: it grows back with its icon in 0.15 s. */
	opStart(UI_MOTION_FULL, 520u);
	opSet(UI_SAVE_CUBES_OP_ERASE, UI_SAVE_CUBES_GO, 7, 5, 0);
	run(0.5f, 60.0f, UI_MOTION_FULL);
	opSet(UI_SAVE_CUBES_OP_ERASE, UI_SAVE_CUBES_BACK, 7, 5, 0);
	run(0.05f, 60.0f, UI_MOTION_FULL);
	count = frame(0.0f, UI_MOTION_FULL, &floating);
	cube = opCube(count);
	assert(cube != NULL && cube->icon != NULL && bits(count) == 0);
	assert(cube->half > 0.5f * UI_SAVE_CUBES_FACE * 0.9f &&
		cube->half < 0.5f * UI_SAVE_CUBES_FACE * UI_SAVE_CUBES_SELECTED);
	run(0.13f, 60.0f, UI_MOTION_FULL);
	count = frame(0.0f, UI_MOTION_FULL, &floating);
	assert(opCube(count) == NULL);
	assert(cubes[count - 1].half == 0.5f * UI_SAVE_CUBES_FACE * UI_SAVE_CUBES_SELECTED &&
		cubes[count - 1].icon != NULL);

	/* Reduced: no pieces; the plain cube fades out in 0.2 s. */
	opStart(UI_MOTION_REDUCED, 530u);
	opSet(UI_SAVE_CUBES_OP_ERASE, UI_SAVE_CUBES_GO, 8, 5, 0);
	run(0.3f, 60.0f, UI_MOTION_REDUCED);
	stackSet(0, 32, 0, 29, 532u, UI_SAVE_CUBES_CLOSED, 5);
	opSet(UI_SAVE_CUBES_OP_ERASE, UI_SAVE_CUBES_LAND, 8, 5, 0);
	run(0.1f, 60.0f, UI_MOTION_REDUCED);
	count = frame(0.0f, UI_MOTION_REDUCED, &floating);
	assert(bits(count) == 0);
	cube = &cubes[count - 1];
	assert(cube->shade == UI_SAVE_CUBES_SHADE_PLAIN && cube->alpha > 60 && cube->alpha < 200);
	run(0.13f, 60.0f, UI_MOTION_REDUCED);
	count = frame(0.0f, UI_MOTION_REDUCED, &floating);
	for(i = 0; i < count; i++) {
		assert(cubes[i].shade != UI_SAVE_CUBES_SHADE_PLAIN);
	}

	/* Off: plain while the card works, then simply gone. */
	opStart(UI_MOTION_OFF, 540u);
	opSet(UI_SAVE_CUBES_OP_ERASE, UI_SAVE_CUBES_GO, 9, 5, 0);
	count = frame(1.0f / 60.0f, UI_MOTION_OFF, &floating);
	cube = &cubes[count - 1];
	assert(cube->shade == UI_SAVE_CUBES_SHADE_PLAIN &&
		cube->half == 0.5f * UI_SAVE_CUBES_FACE * UI_SAVE_CUBES_SELECTED);
	stackSet(0, 32, 0, 29, 542u, UI_SAVE_CUBES_CLOSED, 5);
	opSet(UI_SAVE_CUBES_OP_ERASE, UI_SAVE_CUBES_LAND, 9, 5, 0);
	count = frame(1.0f / 60.0f, UI_MOTION_OFF, &floating);
	assert(bits(count) == 0 && find(count, 0, 6, 0)->x == restX(0, 6));
}

static void testChanges(void)
{
	static const uint32_t before[5] = {11u, 22u, 33u, 44u, 55u};
	static const uint32_t gone[4] = {11u, 22u, 44u, 55u};
	static const uint32_t came[6] = {11u, 22u, 33u, 99u, 44u, 55u};
	static const uint32_t other[5] = {11u, 22u, 34u, 44u, 55u};
	static const uint32_t swapped[6] = {11u, 22u, 99u, 34u, 44u, 55u};
	const uiSaveCube_t *cube;
	float half;
	int at = -1, floating, count;

	assert(UISaveCubes_Change(before, 5, before, 5, &at) == UI_SAVE_CUBES_SAME && at == 5);
	assert(UISaveCubes_Change(before, 5, gone, 4, &at) == UI_SAVE_CUBES_CLOSED && at == 2);
	assert(UISaveCubes_Change(before, 5, came, 6, &at) == UI_SAVE_CUBES_OPENED && at == 3);
	assert(UISaveCubes_Change(before, 5, before, 4, &at) == UI_SAVE_CUBES_CLOSED && at == 4);
	assert(UISaveCubes_Change(before, 5, other, 5, &at) == UI_SAVE_CUBES_NEW);
	assert(UISaveCubes_Change(came, 6, before, 4, &at) == UI_SAVE_CUBES_NEW);
	assert(UISaveCubes_Change(gone, 4, before, 5, &at) == UI_SAVE_CUBES_OPENED && at == 2);
	assert(UISaveCubes_Change(came, 6, other, 5, &at) == UI_SAVE_CUBES_NEW);
	assert(UISaveCubes_Change(before, 5, swapped, 6, &at) == UI_SAVE_CUBES_NEW);
	assert(UISaveCubes_Change(NULL, 0, before, 1, &at) == UI_SAVE_CUBES_OPENED && at == 0);
	assert(UISaveCubes_Change(NULL, 0, NULL, 0, &at) == UI_SAVE_CUBES_SAME);

	/* Read again with nothing moved: the stack carries on as it was, the
	 * focus mid-grow, nothing popping in. */
	opStart(UI_MOTION_FULL, 600u);
	grid.focusCell = 6;
	run(0.05f, 60.0f, UI_MOTION_FULL);
	count = frame(0.0f, UI_MOTION_FULL, &floating);
	half = cubes[count - 1].half;
	assert(half > 0.5f * UI_SAVE_CUBES_FACE && half < 0.5f * UI_SAVE_CUBES_FACE * 1.45f);
	stackSet(0, 32, 0, 30, 602u, UI_SAVE_CUBES_SAME, 0);
	count = frame(1.0f / 60.0f, UI_MOTION_FULL, &floating);
	assert(cubes[count - 1].half > half &&
		cubes[count - 1].half < 0.5f * UI_SAVE_CUBES_FACE * UI_SAVE_CUBES_SELECTED - 0.5f);
	cube = find(count, 0, 1, 0);
	assert(cube->half == 0.5f * UI_SAVE_CUBES_FACE && cube->alpha == 255);
}

static void testOverlays(void)
{
	uiSaveCubesBox_t box;
	const uiSaveCube_t *cube;
	int floating, count, i;
	float low = FAR, high = -FAR;

	/* A message stays 2 s, or until A or B. */
	assert(UISaveCubes_MessageHolds(0.0f, false));
	assert(UISaveCubes_MessageHolds(1.99f, false));
	assert(!UISaveCubes_MessageHolds(2.0f, false));
	assert(!UISaveCubes_MessageHolds(0.5f, true));

	/* The box opens in 0.08 s from 0.92x, closes in 0.1 s; its bar slides
	 * to the focus, and another box starts with it there. */
	opStart(UI_MOTION_FULL, 700u);
	assert(motion.menuAlpha == 0.0f && motion.messageAlpha == 0.0f);
	grid.menu = 1;
	grid.menuSerial = 1;
	grid.menuFocus = 0;
	frame(1.0f / 60.0f, UI_MOTION_FULL, &floating);
	assert(motion.menuAlpha == 0.0f && motion.menuScale == 0.92f);
	run(0.04f, 60.0f, UI_MOTION_FULL);
	frame(0.0f, UI_MOTION_FULL, &floating);
	assert(motion.menuAlpha > 0.3f && motion.menuAlpha < 0.7f);
	run(0.05f, 60.0f, UI_MOTION_FULL);
	frame(0.0f, UI_MOTION_FULL, &floating);
	assert(motion.menuAlpha == 1.0f && motion.menuScale == 1.0f && motion.menuItem == 0.0f);
	grid.menuFocus = 2;
	run(0.1f, 60.0f, UI_MOTION_FULL);
	frame(0.0f, UI_MOTION_FULL, &floating);
	assert(motion.menuItem > 1.0f && motion.menuItem < 2.0f);
	run(0.5f, 60.0f, UI_MOTION_FULL);
	frame(0.0f, UI_MOTION_FULL, &floating);
	assert(near(motion.menuItem, 2.0f, 0.01f));
	grid.menuSerial = 2;
	grid.menuFocus = 1;
	frame(1.0f / 60.0f, UI_MOTION_FULL, &floating);
	assert(motion.menuItem == 1.0f && motion.menuAlpha == 0.0f);
	run(0.2f, 60.0f, UI_MOTION_FULL);
	grid.menu = 0;
	run(0.05f, 60.0f, UI_MOTION_FULL);
	frame(0.0f, UI_MOTION_FULL, &floating);
	assert(motion.menuAlpha > 0.3f && motion.menuAlpha < 0.7f);
	run(0.08f, 60.0f, UI_MOTION_FULL);
	frame(0.0f, UI_MOTION_FULL, &floating);
	assert(motion.menuAlpha == 0.0f);
	/* A message in over 0.1 s, out over 0.15 s. */
	grid.message = 1;
	run(0.05f, 60.0f, UI_MOTION_FULL);
	frame(0.0f, UI_MOTION_FULL, &floating);
	assert(motion.messageAlpha > 0.3f && motion.messageAlpha < 0.7f);
	run(2.0f, 60.0f, UI_MOTION_FULL);
	assert(motion.messageAlpha == 1.0f);
	grid.message = 0;
	run(0.1f, 60.0f, UI_MOTION_FULL);
	frame(0.0f, UI_MOTION_FULL, &floating);
	assert(motion.messageAlpha > 0.2f && motion.messageAlpha < 0.5f);
	run(0.08f, 60.0f, UI_MOTION_FULL);
	assert(motion.messageAlpha == 0.0f);
	/* Off: at once. */
	grid.menu = grid.message = 1;
	frame(1.0f / 60.0f, UI_MOTION_OFF, &floating);
	assert(motion.menuAlpha == 1.0f && motion.messageAlpha == 1.0f);
	grid.menu = grid.message = 0;
	frame(1.0f / 60.0f, UI_MOTION_OFF, &floating);
	assert(motion.menuAlpha == 0.0f && motion.messageAlpha == 0.0f);

	/* The ghost: pale, where the save would land, pulsing between half and
	 * eight tenths; the free cube there makes way. Reduced holds it. */
	grid.ghost = 1;
	grid.ghostCell = 10;
	for(i = 0; i < 60; i++) {
		count = frame(1.0f / 60.0f, UI_MOTION_FULL, &floating);
		cube = find(count, 1, 10, 0);
		assert(cube != NULL && cube->shade == UI_SAVE_CUBES_SHADE_EMPTY_FOCUS &&
			cube->kind == UI_SAVE_CUBES_KIND_SAVE && cube->icon == NULL);
		assert(cube->half == 0.5f * UI_SAVE_CUBES_FACE && cube->x == restX(1, 10));
		low = fminf(low, (float)cube->alpha);
		high = fmaxf(high, (float)cube->alpha);
	}
	assert(low < 135.0f && low >= 127.0f && high > 195.0f && high <= 205.0f);
	count = frame(1.0f / 60.0f, UI_MOTION_REDUCED, &floating);
	assert(find(count, 1, 10, 0)->alpha == 166);
	grid.ghost = 0;
	count = frame(1.0f / 60.0f, UI_MOTION_FULL, &floating);
	assert(find(count, 1, 10, 0)->kind == UI_SAVE_CUBES_KIND_EMPTY);

	/* The box beside its cube: to the right, or past the middle to the
	 * left; inside the window, clear of the info bar. */
	UISaveCubes_MenuBox(148.0f, 196.0f, 0.0f, 3, false, &box);
	assert(box.x == 192.0f && box.y == 150.0f && box.width == 112.0f && box.height == 88.0f);
	UISaveCubes_MenuBox(436.0f, 196.0f, 140.0f, 3, false, &box);
	assert(box.x == 436.0f - 44.0f - 140.0f && box.width == 140.0f);
	UISaveCubes_MenuBox(260.0f, 140.0f, 0.0f, 2, true, &box);
	assert(box.titleY == 112.0f && box.y == 146.0f && box.x == 304.0f);
	UISaveCubes_MenuBox(548.0f, 308.0f, 0.0f, 3, true, &box);
	assert(box.y + box.height == 336.0f && box.titleY == box.y - 34.0f);
	assert(box.x + box.width <= 640.0f);
}

int main(void)
{
	testFaces();
	testFootprints();
	testColours();
	testIcons();
	testMotion();
	testOpening();
	testLeaving();
	testFlight();
	testErase();
	testChanges();
	testOverlays();
	testCursor();
	puts("save cubes: faces, footprints, colours, icons, motion, opening, leaving, "
		"copy, move, erase, menus and the cursor passed");
	return 0;
}
