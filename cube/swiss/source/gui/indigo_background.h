#ifndef INDIGO_BACKGROUND_H
#define INDIGO_BACKGROUND_H

#include <stdbool.h>

#include "ui_clock.h"
#include "ui_scene.h"

/* Drawn after the configured backdrop and before every foreground widget. */
void IndigoBackground_Draw(float seconds, bool backdropAnimated,
	bool cubeAnimated, const uiSceneFrame_t *scene,
	const uiClockFrame_t *clock);
/* Final pass used only during the short boot reveal, after foreground widgets. */
void IndigoBackground_DrawBootOverlay(float seconds, bool animated,
	const uiSceneFrame_t *scene, const uiClockFrame_t *clock);

#endif
