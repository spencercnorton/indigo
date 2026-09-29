#ifndef UI_LAUNCH_H
#define UI_LAUNCH_H

#include <stdbool.h>

/*
 * ui_launch -- what the launch screen shows while a game starts. Swiss reports
 * a launch as a run of progress boxes ("Checking Game...", "Reading File 2/3"
 * with a file name and its size, "Loading DOL" and "Do not remove SD"). Each
 * message is taken as data: it maps to a step, a short caption and a place on
 * the launch ring, and nothing that writes one changes. test_ui_launch pins
 * every message recognised here to the sources, so a Swiss merge that rewords
 * one fails there first.
 *
 * Pure: no GX, no locks. FrameBufferMagic keeps one uiLaunch_t under its
 * video mutex; the menu thread feeds it messages and the video thread moves
 * its fill once a frame.
 */

typedef enum {
	UI_LAUNCH_STEP_NONE = 0,	/* not a launch message */
	UI_LAUNCH_STEP_START,
	UI_LAUNCH_STEP_RECENT,
	UI_LAUNCH_STEP_VIDEO,
	UI_LAUNCH_STEP_CHEATS,
	UI_LAUNCH_STEP_CHECK,
	UI_LAUNCH_STEP_READ,	/* each of the game's executable files is read, */
	UI_LAUNCH_STEP_PATCH,	/* patched, */
	UI_LAUNCH_STEP_WRITE,	/* and its patched copy written to the card */
	UI_LAUNCH_STEP_AUDIO,
	UI_LAUNCH_STEP_LOAD,
	UI_LAUNCH_STEP_COUNT
} uiLaunchStep_t;

#define UI_LAUNCH_WARNING_LENGTH 64u

typedef struct {
	uiLaunchStep_t step;
	float target;	/* where the step's share of the ring starts; never falls */
	float ceiling;	/* where the next step's starts */
	float fill;	/* the part of the ring drawn full; never falls */
	char warning[UI_LAUNCH_WARNING_LENGTH];	/* "Do not remove SD", or "" */
} uiLaunch_t;

/* The step a progress message reports, or NONE. A file step also gives the
 * file's number and how many there are (index 1..count), 0 and 0 otherwise. */
uiLaunchStep_t UILaunch_Classify(const char *message, int *index, int *count);

/* Where a step's share of the ring starts (0..1). A step always starts after
 * the steps a launch goes through before it. */
float UILaunch_Target(uiLaunchStep_t step, int index, int count);

/* The one line the launch screen shows for a step. */
const char *UILaunch_Caption(uiLaunchStep_t step);

/* A launch has started: an empty ring, "Starting game". */
void UILaunch_Begin(uiLaunch_t *launch);

/* Moves the launch on to a message's step and keeps its "Do not remove"
 * line. Returns false, changing nothing, for a message it doesn't know. */
bool UILaunch_Message(uiLaunch_t *launch, const char *message);

/* The game is about to start: the ring runs on to full. */
void UILaunch_Finish(uiLaunch_t *launch);

/* Moves the fill on by deltaSeconds and returns it. The fill closes on the
 * step's start within about half a second, then creeps on towards the next
 * step's without reaching it, so a long step still moves. Without animation
 * the fill shows the step's start. */
float UILaunch_Update(uiLaunch_t *launch, float deltaSeconds, bool animate);

#endif
