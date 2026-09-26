#ifndef UI_HOME_LAYOUT_H
#define UI_HOME_LAYOUT_H

#include <stdbool.h>

#include "ui_home.h"

/* Pure native-grid geometry for the four-face Home.  Rectangles use
 * inclusive edges: left <= x <= right and top <= y <= bottom. */
#define UI_HOME_LAYOUT_FRAME_WIDTH 640
#define UI_HOME_LAYOUT_FRAME_HEIGHT 480
#define UI_HOME_LAYOUT_SAFE_LEFT 24
#define UI_HOME_LAYOUT_SAFE_TOP 62
#define UI_HOME_LAYOUT_SAFE_RIGHT 616
#define UI_HOME_LAYOUT_SAFE_BOTTOM 438

/* The hero cube and its contact rail own the frame through this scanline.
 * Context panels and the selected ring label start below it. */
#define UI_HOME_LAYOUT_CUBE_RAIL_BOTTOM 368
#define UI_HOME_LAYOUT_GLOW_EXTENT 3

/* Explicit hero-cube keep-out. On the ring only the live selected title sits
 * beside it, below; context surfaces put their heading in the band above. */
#define UI_HOME_LAYOUT_CUBE_KEEP_OUT_LEFT 185
#define UI_HOME_LAYOUT_CUBE_KEEP_OUT_TOP 75
#define UI_HOME_LAYOUT_CUBE_KEEP_OUT_RIGHT 455
#define UI_HOME_LAYOUT_CUBE_KEEP_OUT_BOTTOM UI_HOME_LAYOUT_CUBE_RAIL_BOTTOM

/* Maximum signed live-title travel used by the retained Home renderer. */
#define UI_HOME_LAYOUT_SELECTED_TRAVEL 34
#define UI_HOME_LAYOUT_SELECTED_VERTICAL_TRAVEL 6

#define UI_HOME_LAYOUT_COMMAND_X 320
#define UI_HOME_LAYOUT_COMMAND_Y 433
#define UI_HOME_LAYOUT_COMMAND_HALF_HEIGHT 5

#define UI_HOME_LAYOUT_MAX_ROWS 3
#define UI_HOME_LAYOUT_MAX_OPTIONS 2

typedef struct {
	int x;
	int y;
} uiHomeLayoutPoint_t;

typedef struct {
	int left;
	int top;
	int right;
	int bottom;
} uiHomeLayoutRect_t;

typedef struct {
	/* panelBounds is the filled panel. glowBounds includes exactly the
	 * three-pixel focus glow on all four sides. */
	uiHomeLayoutRect_t panelBounds;
	uiHomeLayoutRect_t glowBounds;
	uiHomeLayoutPoint_t labelCenter;
	uiHomeLayoutRect_t labelBounds;
	bool selected;
} uiHomeLayoutItem_t;

typedef struct {
	uiHomeSurface_t surface;
	uiHomeFace_t face;
	int selection;
	bool hasSource;
	bool hasRecent;

	/* The title band exists on every surface; the ring leaves it empty. The
	 * selected-label anchor is present only on the root ring. */
	uiHomeLayoutPoint_t titleCenter;
	uiHomeLayoutRect_t titleBounds;
	bool ringLabelsVisible;
	uiHomeLayoutPoint_t selectedLabelCenter;
	uiHomeLayoutRect_t selectedLabelBounds;

	int rowCount;
	uiHomeLayoutItem_t rows[UI_HOME_LAYOUT_MAX_ROWS];

	int optionCount;
	uiHomeLayoutItem_t options[UI_HOME_LAYOUT_MAX_OPTIONS];

	/* Restart is the only modal Home surface. Its card deliberately overlays
	 * the lower hero-cube region after a full-frame scrim; copy and controls
	 * still remain on integer native-grid rows for 480i readability. */
	uiHomeLayoutRect_t modalBounds;
	uiHomeLayoutPoint_t consequenceCenter;
	uiHomeLayoutRect_t consequenceBounds;

	/* There is deliberately one command owner per snapshot. The conservative
	 * glyph box has a five-pixel half-height around the y=433 centre. */
	uiHomeLayoutPoint_t commandCenter;
	uiHomeLayoutRect_t commandGlyphBounds;
} uiHomeLayout_t;

/* Computes a pointer-free snapshot. Invalid state is rejected and leaves a
 * zeroed snapshot when out is non-NULL. */
bool UIHomeLayout_Compute(const uiHomeState_t *state,
	uiHomeCapabilities_t capabilities, uiHomeLayout_t *out);

/* Geometry helpers are exposed so host tests and renderer assertions share
 * the exact inclusive-edge rules. */
bool UIHomeLayout_RectIsValid(uiHomeLayoutRect_t rect);
bool UIHomeLayout_RectIsSafe(uiHomeLayoutRect_t rect);
bool UIHomeLayout_RectsDisjoint(uiHomeLayoutRect_t first,
	uiHomeLayoutRect_t second);
bool UIHomeLayout_Validate(const uiHomeLayout_t *layout);

#endif
