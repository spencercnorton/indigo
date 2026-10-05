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
	uint8_t folderColor;	/* prepared path identity; only folder rims/glyphs */
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

/* Only the silhouette gets coverage: a one-native-pixel strip outside each
 * exposed edge, sharing mitred corners. Fill faces and icon texels stay as
 * they are; a strip's first two vertices keep the face alpha, the last two
 * are clear. pixelWidth is one frame pixel across, in stage units. */
#define UI_SAVE_CUBES_COVERAGE 8
int UISaveCubes_Coverage(const uiSaveCubesQuad_t *faces, int count,
	float pixelWidth, uiSaveCubesQuad_t out[UI_SAVE_CUBES_COVERAGE]);

/* A face's color before Menu Color turns it, as RGBA: the shade's body or
 * rim, lit for the part of the cube it is. */
void UISaveCubes_Colour(int shade, int role, uint8_t rgba[4]);
/* Explicit folder identity bypasses Menu Color. False for saves, Default,
 * invalid colors and body faces, which retain the native cube shading. */
bool UISaveCubes_FolderColour(const uiSaveCube_t *cube, int role, uint8_t rgba[4]);

/* ------------------------------------------------------------------------
 * A frame of the screen: what the menu thread publishes, and the motion the
 * video thread keeps.
 * --------------------------------------------------------------------- */
typedef struct {
	const uint8_t *texels;		/* the save's 8 frames, or NULL: plain */
	const uiSavesArt_t *art;	/* which frame shows when, with texels */
	uint8_t kind;			/* uiSaveCubesKind_t */
	uint8_t folderColor;		/* path-keyed folder identity, never a save */
} uiSaveCubesCell_t;

/* How a stack's cells came to be what a new listing shows. */
typedef enum {
	UI_SAVE_CUBES_NEW = 0,		/* another card or folder: in from nothing */
	UI_SAVE_CUBES_SAME,		/* read again, nothing moved */
	UI_SAVE_CUBES_CLOSED,		/* the save at changeAt went: the rest come back one */
	UI_SAVE_CUBES_OPENED		/* one came at changeAt: the rest go on one */
} uiSaveCubesChange_t;

typedef struct {
	/* Rows first - 1 .. first + 4, four cells each. */
	uiSaveCubesCell_t cell[UI_SAVE_CUBES_DRAWN];
	int16_t cells;		/* 0: no grid (no card, or it can't be read) */
	int16_t first;		/* the window's first row */
	uint32_t listing;	/* another listing moves the stack as change says */
	uint8_t change;		/* uiSaveCubesChange_t */
	int16_t changeAt;	/* the cell it closed or opened at */
} uiSaveCubesStack_t;

/* Copy, Move and Erase, as saves.c runs them: it publishes each phase as
 * the card's work reaches it, and the video thread moves the cube from
 * when it sees it, while the menu thread waits on the card. */
typedef enum {
	UI_SAVE_CUBES_OP_NONE = 0,
	UI_SAVE_CUBES_OP_COPY,
	UI_SAVE_CUBES_OP_MOVE,
	UI_SAVE_CUBES_OP_ERASE
} uiSaveCubesOpKind_t;

typedef enum {
	UI_SAVE_CUBES_GO = 0,	/* reading and writing: it flies, then hovers */
	UI_SAVE_CUBES_LAND,	/* written and read back the same: it lands */
	UI_SAVE_CUBES_BACK	/* it failed and nothing changed: it goes back */
} uiSaveCubesPhase_t;

typedef struct {
	uint8_t kind;		/* uiSaveCubesOpKind_t */
	uint8_t phase;		/* uiSaveCubesPhase_t */
	uint16_t serial;	/* each operation its own */
	int8_t from;		/* the save's stack: Copy and Move go to the other */
	int16_t fromCell;
	int16_t toCell;		/* the other stack's cell it lands in, or -1: its
				 * header (a folder the stack doesn't show) */
	uiSaveCubesCell_t cube;	/* what flies: the save's art, or plain */
} uiSaveCubesOp_t;

typedef struct {
	uiSaveCubesStack_t stack[UI_SAVE_CUBES_STACKS];
	uiSaveCubesOp_t op;
	int8_t focusStack;	/* -1: nothing focused */
	int16_t focusCell;
	uint8_t ghost;		/* a ghost cube where a Copy or Move would land: */
	int16_t ghostCell;	/* this cell of the other stack */
	uint8_t menu;		/* a box is open beside the focused cube */
	uint8_t menuFocus;	/* its item focused */
	uint16_t menuSerial;	/* another box opens afresh */
	uint8_t message;	/* a message shows */
	uint8_t leaving;	/* B: the screen goes, the Home cube comes back */
} uiSaveCubesGrid_t;

/* Where a flying or erased cube is: scale is of a save's cube, yaw its turn
 * about the up axis, tilt its tip about the across axis. */
typedef struct {
	float x, y, z, scale, yaw, tilt;
} uiSaveCubesPose_t;

typedef struct {
	uiMotionSpring_t grow[UI_SAVE_CUBES_STACKS][UI_SAVE_CUBES_MAX_CELLS];
	uiMotionSpring_t first[UI_SAVE_CUBES_STACKS];
	uint32_t listing[UI_SAVE_CUBES_STACKS];
	float top[UI_SAVE_CUBES_STACKS];	/* the first row this frame, scrolling */
	uint8_t change[UI_SAVE_CUBES_STACKS];	/* how the listing came, and when */
	int16_t changeAt[UI_SAVE_CUBES_STACKS];
	float changed[UI_SAVE_CUBES_STACKS];
	float seconds;		/* the page's own clock */
	float focusSeconds;	/* since the focus came */
	float growsFrom;	/* the focused cube waits to grow until then */
	int focusStack, focusCell;
	float turn[9];		/* the focused cube's wobble */
	float opTurn[9];	/* a flying or erased cube's */
	float bitTurn[9];	/* an erased cube's pieces' */
	/* The operation's phase since when, the cell it aimed at going, and
	 * where its cube was when the phase changed. */
	uint16_t opSerial;
	uint8_t opKind, opPhase;
	int16_t opTo;
	float opSeconds;
	uiSaveCubesPose_t opFrom;
	uiMotionSpring_t menuBar;	/* the box's focus, in items */
	uint16_t menuSerial;
	bool menuOpen;
	float menuSeconds;
	bool messageShown;
	float messageSeconds;
	bool leaving;
	float leaveSeconds;
	bool started;
	/* What else the frame draws, worked out with the cubes: */
	float chrome;		/* headers, the info bar and the hints, 0 .. 1 */
	float paper;		/* the graph paper */
	float handover;		/* the Home cube: 1 where Home left it, 0 gone */
	float menuAlpha, menuScale, menuItem;
	float messageAlpha;
} uiSaveCubesMotion_t;

/* Besides the cells, a frame can draw a flying or erased cube and the ghost,
 * or an erased cube's pieces. */
#define UI_SAVE_CUBES_BITS 8
#define UI_SAVE_CUBES_OUT (UI_SAVE_CUBES_FRAME + 2 + UI_SAVE_CUBES_BITS)
/* Where a cube bound for a folder the stack doesn't show ends: its header. */
#define UI_SAVE_CUBES_HEADER_Y 74.0f
/* A message closes by itself after this many seconds. */
#define UI_SAVE_CUBES_MESSAGE 2.0f
/* Its words fit this many px at their 0.56, an ellipsis ending what doesn't:
 * its box, 48 px wider, stays inside the 4:3 stage as the info bar does. */
#define UI_SAVE_CUBES_MESSAGE_WIDTH 512

/* The frame the icon of cell shows seconds into the page: frame 0 with
 * Motion Off, or NULL for none. */
const uint8_t *UISaveCubes_Icon(const uiSaveCubesCell_t *cell, float seconds,
	uiMotionMode_t mode);

/* Moves motion dt seconds on and writes this frame's cubes to out: those at
 * rest first, then from *floating on those growing, shrinking, focused or
 * flying, farthest first. Returns how many. The focused cube grows to 1.5x,
 * comes forward and wobbles; every cube bobs out of step with its
 * neighbours (not with Reduced motion); a row outside the window fades out;
 * Motion Off snaps it all still. Each stack's cubes come in when its listing
 * does, spiralling out of the middle while the screen opens, and slide one
 * cell on when a save goes or comes; an operation's cube flies, hovers until
 * its result, then lands or goes back, or bursts. */
int UISaveCubes_Frame(uiSaveCubesMotion_t *motion,
	const uiSaveCubesGrid_t *grid, float dt, uiMotionMode_t mode,
	uiSaveCube_t out[UI_SAVE_CUBES_OUT], int *floating);

/* Where cell of stack rests, its window's first row at first (a fraction
 * while it scrolls). */
void UISaveCubes_Where(int stack, int cell, float first, float *x, float *y);

/* How long the cube of an operation of kind takes in phase: the menu thread
 * waits that long after publishing it, so a cube lands no sooner than its
 * flight ends and nothing is cut short. */
float UISaveCubes_OpSeconds(int kind, int phase, uiMotionMode_t mode);

/* How long the screen takes to go after B. */
float UISaveCubes_LeaveSeconds(uiMotionMode_t mode);

/* Whether a message seconds old stays: until UI_SAVE_CUBES_MESSAGE or A or
 * B is pressed. */
bool UISaveCubes_MessageHolds(float seconds, bool pressed);

/* A box of items beside the cube at (cubeX, cubeY), as the IPL's Move / Copy
 * / Erase: to its right, or its left past the middle or where it would leave
 * the stage, clear of the info bar; with titled, a title's box above it. */
#define UI_SAVE_CUBES_MENU_WIDTH 112.0f
#define UI_SAVE_CUBES_MENU_PITCH 24.0f
#define UI_SAVE_CUBES_MENU_TITLE 30.0f
typedef struct {
	float x, y, width, height;	/* the items' box */
	float titleY;			/* the title's, as wide, above it */
} uiSaveCubesBox_t;
void UISaveCubes_MenuBox(float cubeX, float cubeY, float width, int items,
	bool titled, uiSaveCubesBox_t *box);

/* How a stack's cells, before as ids, became after: the same, one gone or
 * one come at *at, or anything else (NEW). */
uiSaveCubesChange_t UISaveCubes_Change(const uint32_t *before, int beforeCount,
	const uint32_t *after, int afterCount, int *at);

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
