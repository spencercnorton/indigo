#include <stddef.h>

#include "ui_command_rail.h"

#define UI_COMMAND_RAIL_LIBRARY_END 0.45f
#define UI_COMMAND_RAIL_DETAIL_START 0.55f

static float clampUnit(float value)
{
	if(value <= 0.0f) {
		return 0.0f;
	}
	if(value >= 1.0f) {
		return 1.0f;
	}
	return value;
}

void UICommandRail_Gameflow(float detailProgress, uiCommandRailFrame_t *frame)
{
	float progress;

	if(frame == NULL) {
		return;
	}
	progress = clampUnit(detailProgress);
	frame->owner = UI_COMMAND_RAIL_NONE;
	frame->alpha = 0.0f;
	if(progress < UI_COMMAND_RAIL_LIBRARY_END) {
		frame->owner = UI_COMMAND_RAIL_LIBRARY;
		frame->alpha = 1.0f - progress / UI_COMMAND_RAIL_LIBRARY_END;
	}
	else if(progress > UI_COMMAND_RAIL_DETAIL_START) {
		frame->owner = UI_COMMAND_RAIL_DETAIL;
		frame->alpha = (progress - UI_COMMAND_RAIL_DETAIL_START) /
			(1.0f - UI_COMMAND_RAIL_DETAIL_START);
	}
}
