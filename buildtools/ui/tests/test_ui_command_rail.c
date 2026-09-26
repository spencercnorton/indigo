#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "ui_command_rail.h"

static unsigned checks;

static void check(int condition, const char *message)
{
	checks++;
	if(!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		exit(1);
	}
}

static void checkNear(float actual, float expected, const char *message)
{
	check(fabsf(actual - expected) <= 0.0001f, message);
}

int main(void)
{
	uiCommandRailFrame_t frame;
	float lastLibraryAlpha = 2.0f;
	float lastDetailAlpha = -1.0f;

	UICommandRail_Gameflow(-1.0f, &frame);
	check(frame.owner == UI_COMMAND_RAIL_LIBRARY, "negative progress clamps to Library");
	checkNear(frame.alpha, 1.0f, "Library starts fully visible");
	UICommandRail_Gameflow(2.0f, &frame);
	check(frame.owner == UI_COMMAND_RAIL_DETAIL, "progress above one clamps to Detail");
	checkNear(frame.alpha, 1.0f, "Detail ends fully visible");

	for(int sample = 0; sample <= 1000; ++sample) {
		float progress = (float)sample / 1000.0f;
		UICommandRail_Gameflow(progress, &frame);
		check(frame.owner >= UI_COMMAND_RAIL_NONE &&
			frame.owner <= UI_COMMAND_RAIL_DETAIL, "rail owner is valid");
		check(frame.alpha >= 0.0f && frame.alpha <= 1.0f,
			"rail alpha remains normalized");
		if(frame.owner == UI_COMMAND_RAIL_LIBRARY) {
			check(progress < 0.45f, "Library cannot own the Detail side");
			check(frame.alpha <= lastLibraryAlpha,
				"Library rail fades monotonically");
			lastLibraryAlpha = frame.alpha;
		}
		else if(frame.owner == UI_COMMAND_RAIL_DETAIL) {
			check(progress > 0.55f, "Detail cannot own the Library side");
			check(frame.alpha >= lastDetailAlpha,
				"Detail rail reveals monotonically");
			lastDetailAlpha = frame.alpha;
		}
		else {
			check(progress >= 0.45f && progress <= 0.55f,
				"quiet handoff is bounded to the center gap");
			checkNear(frame.alpha, 0.0f, "unowned rail is fully clear");
		}
	}

	printf("ui_command_rail: %u checks passed\n", checks);
	return 0;
}
