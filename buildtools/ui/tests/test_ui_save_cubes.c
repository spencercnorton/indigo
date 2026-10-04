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
static uiSaveCube_t cubes[UI_SAVE_CUBES_FRAME];

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

static int frame(float dt, uiMotionMode_t mode, int *floating)
{
	int count = UISaveCubes_Frame(&motion, &grid, dt, mode, cubes, floating);

	assert(count >= 0 && count <= UI_SAVE_CUBES_FRAME);
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
	run(1.0f, hz, UI_MOTION_FULL);
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
	run(1.0f, 60.0f, UI_MOTION_FULL);
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
	/* Another listing starts still: no scroll, no grow. */
	gridSet(5, 0, 0, 22, 60u);
	count = frame(1.0f / 60.0f, UI_MOTION_FULL, &floating);
	cube = &cubes[count - 1];
	assert(cube->half == 0.5f * UI_SAVE_CUBES_FACE * UI_SAVE_CUBES_SELECTED);
	cube = find(floating, 0, 21, 5);
	assert(cube != NULL && near(cube->y, UI_SAVE_CUBES_TOP_Y, UI_SAVE_CUBES_BOB_PX + 1e-3f));

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

int main(void)
{
	testFaces();
	testFootprints();
	testColours();
	testIcons();
	testMotion();
	testCursor();
	puts("save cubes: faces, footprints, colours, icons, motion and the cursor passed");
	return 0;
}
