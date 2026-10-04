#ifndef UI_SAVE_CUBES_H
#define UI_SAVE_CUBES_H

#include <stdbool.h>
#include <stdint.h>

#include "ui_motion.h"
#include "ui_saves.h"

/* Memory Cards' screen (saves.c, drawn by FrameBufferMagic.c): two stacks of
 * save cubes side by side, as the IPL shows Slot A and Slot B, each a window
 * of four rows of four over graph paper. This half is pure: where each cube
 * sits, how it floats, which of its faces show and how they project, and
 * where the cursor goes. Stage px throughout; layouts stay in 0..640 in
 * both screen shapes. */

#define UI_SAVE_CUBES_STACKS 2
#define UI_SAVE_CUBES_COLUMNS 4
#define UI_SAVE_CUBES_ROWS 4		/* the window */
#define UI_SAVE_CUBES_MIN_CELLS 16	/* a card shows four rows at least */
/* A card's 127 saves and a free cell, or every entry a folder lists. */
#define UI_SAVE_CUBES_MAX_CELLS 256
/* The rows a stack draws: the window and one each side, seen while it
 * scrolls. */
#define UI_SAVE_CUBES_DRAWN_ROWS (UI_SAVE_CUBES_ROWS + 2)
#define UI_SAVE_CUBES_DRAWN (UI_SAVE_CUBES_DRAWN_ROWS * UI_SAVE_CUBES_COLUMNS)
/* Every cube a frame draws, both stacks. */
#define UI_SAVE_CUBES_FRAME (UI_SAVE_CUBES_STACKS * UI_SAVE_CUBES_DRAWN)

/* A cube's corners project through one vanishing point, the eye this far in
 * front of the stage (a 24 degree view): a cube's front face at rest lands
 * at its own size, and its sides show toward the middle. */
#define UI_SAVE_CUBES_VANISH_X 320.0f
#define UI_SAVE_CUBES_VANISH_Y 224.0f
#define UI_SAVE_CUBES_EYE 1130.0f
#define UI_SAVE_CUBES_PITCH 56.0f	/* cell to cell */
#define UI_SAVE_CUBES_FACE 38.0f	/* a save's cube */
#define UI_SAVE_CUBES_ICON 32.0f	/* its icon, a texel a pixel at rest */
#define UI_SAVE_CUBES_EMPTY 0.72f	/* a free cell's cube, to a save's */
#define UI_SAVE_CUBES_SELECTED 1.5f	/* the focused cube grows this much */
#define UI_SAVE_CUBES_LIFT 18.0f	/* and comes this far forward */
#define UI_SAVE_CUBES_BOB_PX 1.5f	/* every cube floats this much */
#define UI_SAVE_CUBES_STACK_X 176.0f	/* the left stack's middle */
#define UI_SAVE_CUBES_STACK_GAP 288.0f	/* to the right stack's */
#define UI_SAVE_CUBES_TOP_Y 140.0f	/* the window's first row */

/* The colors a face takes (UISaveCubes_Colour). */
typedef enum {
	UI_SAVE_CUBES_SHADE_SAVE = 0,
	UI_SAVE_CUBES_SHADE_SAVE_FOCUS,		/* icy pale */
	UI_SAVE_CUBES_SHADE_EMPTY,		/* navy, see-through */
	UI_SAVE_CUBES_SHADE_EMPTY_FOCUS,
	UI_SAVE_CUBES_SHADE_PLAIN,		/* a save with its icon gone */
	UI_SAVE_CUBES_SHADES
} uiSaveCubesShade_t;

typedef enum {
	UI_SAVE_CUBES_KIND_SAVE = 0,	/* a bevelled rim round its icon */
	UI_SAVE_CUBES_KIND_FOLDER,	/* the rim round a folder */
	UI_SAVE_CUBES_KIND_EMPTY	/* a plain front */
} uiSaveCubesKind_t;

/* What part of a cube a quad is, which says how it is lit. */
typedef enum {
	UI_SAVE_CUBES_ROLE_BODY = 0,	/* the front inside the rim, or a far face */
	UI_SAVE_CUBES_ROLE_RIM_TOP,
	UI_SAVE_CUBES_ROLE_RIM_LEFT,
	UI_SAVE_CUBES_ROLE_RIM_RIGHT,
	UI_SAVE_CUBES_ROLE_RIM_BOTTOM,
	UI_SAVE_CUBES_ROLE_SIDE_TOP,
	UI_SAVE_CUBES_ROLE_SIDE_EDGE,	/* left or right */
	UI_SAVE_CUBES_ROLE_SIDE_BOTTOM,
	UI_SAVE_CUBES_ROLE_GLYPH,	/* a folder's picture */
	UI_SAVE_CUBES_ROLES,
	UI_SAVE_CUBES_ROLE_ICON = UI_SAVE_CUBES_ROLES	/* the icon: its own colors */
} uiSaveCubesRole_t;

/* One cube as a frame draws it. */
typedef struct {
	float x, y;		/* its front face's middle at rest */
	float z;		/* lifted toward the viewer */
	float half;		/* half its face */
	const float *turn;	/* a 3x3 rotation about its middle, or NULL */
	const uint8_t *icon;	/* a 32x32 RGB5A3 frame, or NULL */
	uint8_t shade;		/* uiSaveCubesShade_t */
	uint8_t kind;		/* uiSaveCubesKind_t */
	uint8_t alpha;
} uiSaveCube_t;

/* A projected quad, its corners clockwise on screen: TL, TR, BR, BL for the
 * icon's texels. */
typedef struct {
	float x[4], y[4];
	uint8_t role;		/* uiSaveCubesRole_t */
} uiSaveCubesQuad_t;

/* At most three faces show: two sides, then the front as a rim of four, its
 * inside and a folder's two shapes; then the icon. */
#define UI_SAVE_CUBES_QUADS 10

/* cube's quads, in the order they are drawn, the icon last: none when it is
 * clear, outside left .. right or 0 .. 480, or a corner comes within a pixel
 * of the eye. A face shows when it winds clockwise with more than half a
 * pixel of area: a box shows three at most and they never overlap, so no
 * depth is needed. */
int UISaveCubes_Faces(const uiSaveCube_t *cube, float left, float right,
	uiSaveCubesQuad_t out[UI_SAVE_CUBES_QUADS]);

/* A face's color before Menu Color turns it, as RGBA: the shade's body or
 * rim, lit for the part of the cube it is. */
void UISaveCubes_Colour(int shade, int role, uint8_t rgba[4]);

/* ------------------------------------------------------------------------
 * A frame of the screen: what the menu thread publishes, and the motion the
 * video thread keeps.
 * --------------------------------------------------------------------- */
typedef struct {
	const uint8_t *texels;		/* the save's 8 frames, or NULL: plain */
	const uiSavesArt_t *art;	/* which frame shows when, with texels */
	uint8_t kind;			/* uiSaveCubesKind_t */
} uiSaveCubesCell_t;

typedef struct {
	/* Rows first - 1 .. first + 4, four cells each. */
	uiSaveCubesCell_t cell[UI_SAVE_CUBES_DRAWN];
	int16_t cells;		/* 0: no grid (no card, or it can't be read) */
	int16_t first;		/* the window's first row */
	uint32_t listing;	/* another listing snaps the stack's motion */
} uiSaveCubesStack_t;

typedef struct {
	uiSaveCubesStack_t stack[UI_SAVE_CUBES_STACKS];
	int8_t focusStack;	/* -1: nothing focused */
	int16_t focusCell;
} uiSaveCubesGrid_t;

typedef struct {
	uiMotionSpring_t grow[UI_SAVE_CUBES_STACKS][UI_SAVE_CUBES_MAX_CELLS];
	uiMotionSpring_t first[UI_SAVE_CUBES_STACKS];
	uint32_t listing[UI_SAVE_CUBES_STACKS];
	float seconds;		/* the page's own clock */
	float focusSeconds;	/* since the focus came */
	int focusStack, focusCell;
	float turn[9];		/* the focused cube's wobble */
	bool started;
} uiSaveCubesMotion_t;

/* The frame the icon of cell shows seconds into the page: frame 0 with
 * Motion Off, or NULL for none. */
const uint8_t *UISaveCubes_Icon(const uiSaveCubesCell_t *cell, float seconds,
	uiMotionMode_t mode);

/* Moves motion dt seconds on and writes this frame's cubes to out: those at
 * rest first, then from *floating on those growing, shrinking or focused,
 * farthest first. Returns how many. The focused cube grows to 1.5x, comes
 * forward and wobbles; every cube bobs out of step with its neighbours (not
 * with Reduced motion); a row outside the window fades out; Motion Off snaps
 * it all still. */
int UISaveCubes_Frame(uiSaveCubesMotion_t *motion,
	const uiSaveCubesGrid_t *grid, float dt, uiMotionMode_t mode,
	uiSaveCube_t out[UI_SAVE_CUBES_FRAME], int *floating);

/* ------------------------------------------------------------------------
 * The cursor.
 * --------------------------------------------------------------------- */
typedef enum {
	UI_SAVE_CUBES_UP = 0,
	UI_SAVE_CUBES_DOWN,
	UI_SAVE_CUBES_LEFT,
	UI_SAVE_CUBES_RIGHT
} uiSaveCubesStep_t;

typedef struct {
	int cells[UI_SAVE_CUBES_STACKS];	/* 0: no grid */
	int first[UI_SAVE_CUBES_STACKS];
	int stack;	/* -1: none */
	int cell;
} uiSaveCubesCursor_t;

/* The cells a stack of saves (and folders) shows: every one and a free
 * cell, in whole rows, four rows at least, UI_SAVE_CUBES_MAX_CELLS at most. */
int UISaveCubes_Cells(int saves);

/* The window's first row that shows cell, moved as little as it can from
 * first, in a stack of cells. */
int UISaveCubes_Window(int first, int cell, int cells);

/* Moves the cursor one step: within a stack, scrolling the window with it,
 * or from an edge column into the other stack at the same row on screen.
 * False, with nothing changed, at an edge or toward a stack with no grid: a
 * bump, as the IPL's cursor never wraps. */
bool UISaveCubes_Step(uiSaveCubesCursor_t *cursor, uiSaveCubesStep_t step);

/* The stack the cursor belongs in: its own while that has a grid, else
 * the first that has one (the left, as the IPL opens on Slot A), else -1. */
int UISaveCubes_Home(const int cells[UI_SAVE_CUBES_STACKS], int stack);

/* L or R: what a stack showing source (0, 1 or 2: Slot A, Slot B, the SD
 * card) shows next, never other, what the other stack shows. found: the
 * press looked again at a slot that had no card and found one, so nothing
 * swaps. */
int UISaveCubes_Swap(int source, int other, bool found);

#endif
