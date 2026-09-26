#include "ui_stage.h"

#define UI_STAGE_SQUEEZE 0.75f

static bool stageWide;

void UIStage_SetWide(bool wide)
{
	stageWide = wide;
}

float UIStage_Left(void)
{
	return stageWide ? -320.0f / 3.0f : 0.0f;
}

float UIStage_Right(void)
{
	return stageWide ? 640.0f + 320.0f / 3.0f : 640.0f;
}

/* Row 0 makes clip x, for an orthographic and a perspective projection
 * alike, so scaling it squeezes x about the frame's middle. */
void UIStage_Project(float projection[4][4])
{
	if(!stageWide) return;
	for(int column = 0; column < 4; column++)
		projection[0][column] *= UI_STAGE_SQUEEZE;
}

float UIStage_FrameX(float x)
{
	return stageWide ? 320.0f + (x - 320.0f) * UI_STAGE_SQUEEZE : x;
}
