#ifndef UI_SCENE_H
#define UI_SCENE_H

#include <stdbool.h>
#include <stdint.h>

#include "ui_home.h"
#include "ui_motion.h"

typedef enum {
	UI_SCENE_BOOT = 0,
	UI_SCENE_HOME,
	UI_SCENE_SOURCE,
	UI_SCENE_LIBRARY,
	UI_SCENE_GAME_DETAIL,
	UI_SCENE_SYSTEM,
	UI_SCENE_SETTINGS,
	UI_SCENE_COUNT
} uiSceneId_t;

/*
 * A read-only render snapshot. Requests originate on the menu thread, while
 * every animated value is owned and advanced by the video thread.
 */
typedef struct {
	uiSceneId_t scene;
	float cubeX;
	float cubeY;
	float cubeScale;
	float cubePitch;
	float cubeYaw;
	float orbitStrength;
	float introProgress;
	float chromeProgress;
	/* Poster opacity follows the cube retreat, separate from the boot chrome. */
	float libraryReveal;
	uiHomeFace_t homeFace;
	int homeTurnDirection;
	uiHomeTurnAxis_t homeTurnAxis;
	/* Row-major body-to-screen navigation transform, separate from authored
	 * cubePitch/cubeYaw. Non-Home destinations animate toward identity. */
	float homeOrientation[3][3];
	float homeTargetOrientation[3][3];
	float homeMotifBasis[UI_HOME_FACE_COUNT][3][3];
	float homeMotifAlpha;
	float homeFocusProgress;
	float homeIdleBlend;
	uint32_t homeRevision;
	uiHomeSurface_t homeSurface;
	int homeSelection;
	bool visible;
	bool transitioning;
} uiSceneFrame_t;

void UIScene_Reset(void);
void UIScene_Activate(void);
void UIScene_Request(uiSceneId_t scene);
/* Publish one complete reducer snapshot. Absolute orientation preserves mixed
 * axis commands even when several menu updates coalesce before one frame. */
void UIScene_RequestHome(const uiHomeState_t *home);
void UIScene_Update(float deltaSeconds, uiMotionMode_t motionMode);
const uiSceneFrame_t *UIScene_Frame(void);

#endif
