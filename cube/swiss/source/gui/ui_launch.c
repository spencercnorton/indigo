#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "ui_launch.h"

/* The ring's shares. The game's executable files take the most: each is
 * read, patched and, the first time, written to the card, a third of its
 * share apiece. Loading the game runs on towards full; the hand-off fills
 * the rest. */
#define UI_LAUNCH_FILES_START 0.24f
#define UI_LAUNCH_FILES_SPAN 0.52f
#define UI_LAUNCH_LOAD_CEILING 0.97f
/* Per second. A step's jump closes within about half a second, the last
 * sweep inside the quarter-second fade, and a long step creeps on by about
 * a quarter of what is left of its share each second. */
#define UI_LAUNCH_CATCH_UP 6.0f
#define UI_LAUNCH_FINISH 14.0f
#define UI_LAUNCH_CREEP 0.3f
#define UI_LAUNCH_MAX_DELTA 0.05f

static const float stepStart[UI_LAUNCH_STEP_COUNT] = {
	[UI_LAUNCH_STEP_RECENT] = 0.04f,
	[UI_LAUNCH_STEP_VIDEO] = 0.08f,
	[UI_LAUNCH_STEP_CHEATS] = 0.10f,
	[UI_LAUNCH_STEP_CHECK] = 0.14f,
	[UI_LAUNCH_STEP_READ] = UI_LAUNCH_FILES_START,
	[UI_LAUNCH_STEP_PATCH] = UI_LAUNCH_FILES_START,
	[UI_LAUNCH_STEP_WRITE] = UI_LAUNCH_FILES_START,
	[UI_LAUNCH_STEP_AUDIO] = UI_LAUNCH_FILES_START + UI_LAUNCH_FILES_SPAN,
	[UI_LAUNCH_STEP_LOAD] = 0.80f
};

static const char *const captions[UI_LAUNCH_STEP_COUNT] = {
	[UI_LAUNCH_STEP_NONE] = "",
	[UI_LAUNCH_STEP_START] = "Starting game",
	[UI_LAUNCH_STEP_RECENT] = "Saving recent games",
	[UI_LAUNCH_STEP_VIDEO] = "Setting the video mode",
	[UI_LAUNCH_STEP_CHEATS] = "Applying cheats",
	[UI_LAUNCH_STEP_CHECK] = "Checking game",
	[UI_LAUNCH_STEP_READ] = "Preparing game files",
	[UI_LAUNCH_STEP_PATCH] = "Preparing game files",
	[UI_LAUNCH_STEP_WRITE] = "Preparing game files",
	[UI_LAUNCH_STEP_AUDIO] = "Setting up audio streaming",
	[UI_LAUNCH_STEP_LOAD] = "Loading game"
};

/* How each message Swiss shows during a launch begins, and its step. */
static const struct {
	const char *prefix;
	uiLaunchStep_t step;
} launchMessages[] = {
	/* swiss.c */
	{"Saving recent list", UI_LAUNCH_STEP_RECENT},
	{"Video Mode: ", UI_LAUNCH_STEP_VIDEO},
	{"Applied ", UI_LAUNCH_STEP_CHEATS},
	{"Checking Game", UI_LAUNCH_STEP_CHECK},
	{"Loading DOL", UI_LAUNCH_STEP_LOAD},
	{"Loading BS2", UI_LAUNCH_STEP_LOAD},
	/* gcm.c, patch_gcm: "Reading File 2/3\nmain.dol [3012KB]" */
	{"Reading File ", UI_LAUNCH_STEP_READ},
	{"Patching File ", UI_LAUNCH_STEP_PATCH},
	{"Writing File ", UI_LAUNCH_STEP_WRITE},
	/* devices/dvd: a multi-game disc that streams its audio */
	{"One moment, setting up audio streaming", UI_LAUNCH_STEP_AUDIO}
};

/* The second line of "Loading DOL" and "Loading BS2" when the patches live
 * on another card than the game. */
static const char warningPrefix[] = "Do not remove ";

static bool isFileStep(uiLaunchStep_t step)
{
	return step == UI_LAUNCH_STEP_READ || step == UI_LAUNCH_STEP_PATCH ||
		step == UI_LAUNCH_STEP_WRITE;
}

static bool validFile(int index, int count)
{
	return count > 0 && index >= 1 && index <= count;
}

/* "2/3..." -> 2 and 3; anything else leaves them 0. */
static void fileNumber(const char *text, int *index, int *count)
{
	char *end;
	long first = strtol(text, &end, 10);
	long total;

	if(end == text || *end != '/') {
		return;
	}
	text = end + 1;
	total = strtol(text, &end, 10);
	/* Swiss patches at most 512 files. */
	if(end == text || first < 1 || first > total || total > 512) {
		return;
	}
	*index = (int)first;
	*count = (int)total;
}

uiLaunchStep_t UILaunch_Classify(const char *message, int *index, int *count)
{
	int fileIndex = 0;
	int fileCount = 0;
	uiLaunchStep_t step = UI_LAUNCH_STEP_NONE;
	size_t i;

	for(i = 0u; message != NULL &&
		i < sizeof(launchMessages) / sizeof(launchMessages[0]); ++i) {
		size_t length = strlen(launchMessages[i].prefix);

		if(strncmp(message, launchMessages[i].prefix, length) == 0) {
			step = launchMessages[i].step;
			if(isFileStep(step)) {
				fileNumber(message + length, &fileIndex, &fileCount);
			}
			break;
		}
	}
	if(index != NULL) {
		*index = fileIndex;
	}
	if(count != NULL) {
		*count = fileCount;
	}
	return step;
}

float UILaunch_Target(uiLaunchStep_t step, int index, int count)
{
	if((unsigned)step >= (unsigned)UI_LAUNCH_STEP_COUNT) {
		return 0.0f;
	}
	if(isFileStep(step) && validFile(index, count)) {
		float phase = (float)((int)step - (int)UI_LAUNCH_STEP_READ);

		return UI_LAUNCH_FILES_START + UI_LAUNCH_FILES_SPAN *
			((float)(index - 1) + phase / 3.0f) / (float)count;
	}
	return stepStart[step];
}

/* Where the step after this one starts: the fill creeps towards it. */
static float nextStart(uiLaunchStep_t step, int index, int count)
{
	if(isFileStep(step)) {
		return validFile(index, count) ?
			UILaunch_Target(step, index, count) +
				UI_LAUNCH_FILES_SPAN / (3.0f * (float)count) :
			stepStart[UI_LAUNCH_STEP_AUDIO];
	}
	if(step == UI_LAUNCH_STEP_LOAD) {
		return UI_LAUNCH_LOAD_CEILING;
	}
	return stepStart[(unsigned)step + 1u];
}

const char *UILaunch_Caption(uiLaunchStep_t step)
{
	if((unsigned)step >= (unsigned)UI_LAUNCH_STEP_COUNT) {
		return captions[UI_LAUNCH_STEP_NONE];
	}
	return captions[step];
}

const char *UILaunch_AppCaption(uiLaunchStep_t step)
{
	switch(step) {
		case UI_LAUNCH_STEP_START: return "Starting app";
		case UI_LAUNCH_STEP_RECENT: return "Saving recent list";
		case UI_LAUNCH_STEP_LOAD: return "Loading app";
		default: return UILaunch_Caption(step);
	}
}

void UILaunch_Begin(uiLaunch_t *launch)
{
	if(launch == NULL) {
		return;
	}
	memset(launch, 0, sizeof(*launch));
	launch->step = UI_LAUNCH_STEP_START;
	launch->ceiling = nextStart(UI_LAUNCH_STEP_START, 0, 0);
}

bool UILaunch_Message(uiLaunch_t *launch, const char *message)
{
	int index;
	int count;
	uiLaunchStep_t step = UILaunch_Classify(message, &index, &count);
	const char *line;
	float target;
	float ceiling;

	if(launch == NULL || step == UI_LAUNCH_STEP_NONE) {
		return false;
	}
	/* Steps only move forwards: a message shown again, or out of order,
	 * leaves the ring where it is. */
	target = UILaunch_Target(step, index, count);
	if(target >= launch->target) {
		launch->step = step;
		launch->target = target;
		ceiling = nextStart(step, index, count);
		if(ceiling > launch->ceiling) {
			launch->ceiling = ceiling;
		}
	}
	line = strchr(message, '\n');
	if(line != NULL && strncmp(line + 1, warningPrefix,
		sizeof(warningPrefix) - 1u) == 0) {
		size_t length = strcspn(line + 1, "\n");

		if(length >= UI_LAUNCH_WARNING_LENGTH) {
			length = UI_LAUNCH_WARNING_LENGTH - 1u;
		}
		memcpy(launch->warning, line + 1, length);
		launch->warning[length] = '\0';
	}
	return true;
}

void UILaunch_Finish(uiLaunch_t *launch)
{
	if(launch != NULL) {
		launch->target = 1.0f;
		launch->ceiling = 1.0f;
	}
}

float UILaunch_Update(uiLaunch_t *launch, float deltaSeconds, bool animate)
{
	float fill;
	float speed = 0.0f;

	if(launch == NULL) {
		return 0.0f;
	}
	fill = launch->fill;
	if(!animate) {
		if(fill < launch->target) {
			fill = launch->target;
		}
	}
	else if(deltaSeconds > 0.0f) {
		if(deltaSeconds > UI_LAUNCH_MAX_DELTA) {
			deltaSeconds = UI_LAUNCH_MAX_DELTA;
		}
		if(fill < launch->target) {
			speed += (launch->target >= 1.0f ? UI_LAUNCH_FINISH :
				UI_LAUNCH_CATCH_UP) * (launch->target - fill);
		}
		if(fill < launch->ceiling) {
			speed += UI_LAUNCH_CREEP * (launch->ceiling - fill);
		}
		fill += speed * deltaSeconds;
		/* The fill never passes the next step's start, which never falls. */
		if(fill > launch->ceiling) {
			fill = launch->ceiling;
		}
	}
	launch->fill = fill;
	return fill;
}
