#ifndef UI_HOME_H
#define UI_HOME_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
	UI_HOME_FACE_LIBRARY = 0,
	UI_HOME_FACE_SOURCE,
	UI_HOME_FACE_SETTINGS,
	UI_HOME_FACE_SYSTEM,
	UI_HOME_FACE_COUNT
} uiHomeFace_t;

typedef enum {
	UI_HOME_SURFACE_RING = 0,
	UI_HOME_SURFACE_SOURCE,
	UI_HOME_SURFACE_SYSTEM,
	UI_HOME_SURFACE_RESTART_CONFIRM,
	UI_HOME_SURFACE_COUNT
} uiHomeSurface_t;

typedef enum {
	UI_HOME_INPUT_NONE = 0,
	UI_HOME_INPUT_LEFT,
	UI_HOME_INPUT_RIGHT,
	UI_HOME_INPUT_UP,
	UI_HOME_INPUT_DOWN,
	UI_HOME_INPUT_ACTIVATE,
	UI_HOME_INPUT_BACK,
	UI_HOME_INPUT_RECENT
} uiHomeInput_t;

typedef enum {
	UI_HOME_EFFECT_NONE = 0,
	UI_HOME_EFFECT_OPEN_LIBRARY,
	UI_HOME_EFFECT_CHANGE_SOURCE,
	UI_HOME_EFFECT_REFRESH,
	UI_HOME_EFFECT_OPEN_SETTINGS,
	UI_HOME_EFFECT_OPEN_INFO,
	UI_HOME_EFFECT_RESTART,
	UI_HOME_EFFECT_OPEN_RECENT
} uiHomeEffect_t;

typedef struct {
	bool hasSource;
	bool hasRecent;
} uiHomeCapabilities_t;

typedef enum {
	UI_HOME_TURN_NONE = 0,
	UI_HOME_TURN_HORIZONTAL,
	UI_HOME_TURN_VERTICAL
} uiHomeTurnAxis_t;

/* Exact proper signed-permutation matrix, mapping body coordinates to screen
 * coordinates. Screen-axis quarter turns pre-multiply this orientation. */
typedef struct {
	int8_t m[3][3];
} uiHomeOrientation_t;

/* Menu-thread-owned state. The ordinal selects the menu independently of the
 * cube's physical orientation, so coalesced mixed-axis turns lose no state. */
typedef struct {
	uiHomeFace_t face;
	uiHomeSurface_t surface;
	int selection;
	int32_t turnOrdinal;
	uint32_t revision;
	uiHomeOrientation_t orientation;
	uiHomeTurnAxis_t turnAxis;
	int turnDirection;
} uiHomeState_t;

void UIHome_OrientationInit(uiHomeOrientation_t *orientation);
bool UIHome_OrientationValid(const uiHomeOrientation_t *orientation);
void UIHome_OrientationTurn(uiHomeOrientation_t *orientation,
	uiHomeTurnAxis_t axis, int direction);
void UIHome_OrientationMatrix(const uiHomeOrientation_t *orientation,
	float out[3][3]);

#define UI_HOME_QUARTER_TURN_RADIANS 1.57079632679f

bool UIHome_IsFace(int face);
bool UIHome_IsSurface(int surface);
uiHomeFace_t UIHome_FaceForTurn(int32_t turnOrdinal);
void UIHome_Init(uiHomeState_t *state, uiHomeCapabilities_t capabilities);
uiHomeEffect_t UIHome_Apply(uiHomeState_t *state, uiHomeInput_t input,
	uiHomeCapabilities_t capabilities);

int UIHome_RowCount(uiHomeSurface_t surface,
	uiHomeCapabilities_t capabilities);
bool UIHome_RowEnabled(uiHomeSurface_t surface, int row,
	uiHomeCapabilities_t capabilities);
const char *UIHome_FaceLabel(uiHomeFace_t face);
const char *UIHome_PrimaryHint(uiHomeFace_t face,
	uiHomeCapabilities_t capabilities);
const char *UIHome_SurfaceTitle(uiHomeSurface_t surface);
const char *UIHome_RowLabel(uiHomeSurface_t surface, int row);

#endif
