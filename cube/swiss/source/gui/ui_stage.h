#ifndef UI_STAGE_H
#define UI_STAGE_H

#include <stdbool.h>

/* Every screen is laid out on a 640 x 480 stage, which each projection
 * scales about the frame's middle before it is loaded:
 *
 * Menu Widescreen draws it for a TV set to 16:9, which stretches the picture
 * by 4/3: x is squeezed to 3/4, so the stretch gives circles back their
 * shape, and the frame shows the stage from -320/3 to 640 + 320/3.
 *
 * Menu Screen Size draws it up to UI_STAGE_MAX_INSET percent smaller, for a
 * CRT that hides the picture's edges (overscan): x and y shrink alike, and
 * the frame shows a margin all round that the TV is expected to hide.
 *
 * Layouts stay in 0..640 x 0..480; only full-screen fills reach the frame's
 * edges. */
#define UI_STAGE_MAX_INSET 20

void UIStage_SetWide(bool wide);
/* The stage drawn inset percent smaller: 0 (the default) fills the frame,
 * and any value outside 0..UI_STAGE_MAX_INSET is taken as 0. */
void UIStage_SetInset(int inset);
/* The frame's edges, in stage units: where a full-screen fill must reach. */
float UIStage_Left(void);
float UIStage_Right(void);
float UIStage_Top(void);
float UIStage_Bottom(void);
/* The right edge the TV shows: the frame's, less the margin Menu Screen Size
 * leaves for overscan. Something that sits against the screen's edge, such
 * as the clock, measures from here. */
float UIStage_ShownRight(void);
/* Scales a projection (a libogc Mtx44) before it is loaded. */
void UIStage_Project(float projection[4][4]);
/* Where a stage point lands on a 640 x 480 frame, for the frame copies. */
float UIStage_FrameX(float x);
float UIStage_FrameY(float y);
/* Stage units one frame-buffer pixel spans across, per unit down: 1, or 4/3
 * through the squeeze. A fade meant to cover one pixel scales its x offset
 * by it; Menu Screen Size shrinks the fade with everything else. */
float UIStage_PixelWidth(void);

#endif
