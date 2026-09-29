#include "ui_stage.h"

#define UI_STAGE_SQUEEZE 0.75f

static bool stageWide;
static float stageSize = 1.0f;

void UIStage_SetWide(bool wide)
{
	stageWide = wide;
}

void UIStage_SetInset(int inset)
{
	stageSize = inset > 0 && inset <= UI_STAGE_MAX_INSET ?
		1.0f - (float)inset / 100.0f : 1.0f;
}

/* A stage edge moved out to the frame's own. At full size it is returned
 * untouched, so the default draws exactly what it always did. */
static float frameEdge(float edge, float middle)
{
	return stageSize < 1.0f ? middle + (edge - middle) / stageSize : edge;
}

float UIStage_Left(void)
{
	return frameEdge(stageWide ? -320.0f / 3.0f : 0.0f, 320.0f);
}

float UIStage_Right(void)
{
	return frameEdge(UIStage_ShownRight(), 320.0f);
}

float UIStage_Top(void)
{
	return frameEdge(0.0f, 240.0f);
}

float UIStage_Bottom(void)
{
	return frameEdge(480.0f, 240.0f);
}

float UIStage_ShownRight(void)
{
	return stageWide ? 640.0f + 320.0f / 3.0f : 640.0f;
}

/* Row 0 makes clip x and row 1 clip y, for an orthographic and a
 * perspective projection alike, so scaling them scales about the frame's
 * middle. */
void UIStage_Project(float projection[4][4])
{
	float x = (stageWide ? UI_STAGE_SQUEEZE : 1.0f) * stageSize;

	for(int column = 0; column < 4; column++) {
		if(x != 1.0f) projection[0][column] *= x;
		if(stageSize != 1.0f) projection[1][column] *= stageSize;
	}
}

float UIStage_FrameX(float x)
{
	float scale = (stageWide ? UI_STAGE_SQUEEZE : 1.0f) * stageSize;

	return scale != 1.0f ? 320.0f + (x - 320.0f) * scale : x;
}

float UIStage_FrameY(float y)
{
	return stageSize != 1.0f ? 240.0f + (y - 240.0f) * stageSize : y;
}

float UIStage_PixelWidth(void)
{
	return stageWide ? 1.0f / UI_STAGE_SQUEEZE : 1.0f;
}
