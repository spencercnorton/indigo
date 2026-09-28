#ifndef UI_STAGE_H
#define UI_STAGE_H

#include <stdbool.h>

/* Every screen is laid out on a 640 x 480 stage. Menu Widescreen draws it
 * for a TV set to 16:9, which stretches the picture by 4/3: each projection
 * squeezes x to 3/4 about the middle, so the stretch gives circles back
 * their shape, and the frame then shows the stage from -320/3 to 640 +
 * 320/3. Layouts stay in 0..640; only full-screen fills reach the edges. */
void UIStage_SetWide(bool wide);
float UIStage_Left(void);
float UIStage_Right(void);
/* Squeezes a projection (a libogc Mtx44) before it is loaded. */
void UIStage_Project(float projection[4][4]);
/* Where a stage x lands across a 640-unit frame, for the frame copies. */
float UIStage_FrameX(float x);
/* Stage units one frame-buffer pixel spans across: 1, or 4/3 through the
 * squeeze. A fade meant to cover one pixel scales its x offset by it. */
float UIStage_PixelWidth(void);

#endif
