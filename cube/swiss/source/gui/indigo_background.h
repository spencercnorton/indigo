#ifndef INDIGO_BACKGROUND_H
#define INDIGO_BACKGROUND_H

#include <gctypes.h>
#include <stdbool.h>

#include "ui_clock.h"
#include "ui_scene.h"

/* One controller sample per video frame, taken beside the clock so the
 * renderer never reads hardware. The Controller icon mirrors it. */
typedef struct indigoPadFrame {
	bool available;
	s8 stickX;
	s8 stickY;
	s8 substickX;
	s8 substickY;
	u32 buttons;
} indigoPadFrame_t;

/* Drawn after the configured backdrop and before every foreground widget.
 * pad may be NULL: the Controller icon then plays only its idle motion.
 * icons holds a uiHomeIcon_t for each uiHomeFace_t. */
void IndigoBackground_Draw(float seconds, bool backdropAnimated,
	bool cubeAnimated, const uiSceneFrame_t *scene,
	const uiClockFrame_t *clock, const indigoPadFrame_t *pad,
	const int icons[UI_HOME_FACE_COUNT]);
/* Final pass used only during the short boot reveal, after foreground widgets. */
void IndigoBackground_DrawBootOverlay(float seconds, bool animated,
	const uiSceneFrame_t *scene, const uiClockFrame_t *clock,
	const int icons[UI_HOME_FACE_COUNT]);

#endif
