#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "ui_menu_input.h"

void UIMenuAction_Init(uiMenuActionState_t *state, uint32_t held)
{
	if(state != NULL) {
		state->previous = held;
	}
}

uint32_t UIMenuAction_Update(uiMenuActionState_t *state, uint32_t held,
	uint32_t actionMask, uint32_t modifierMask, uint32_t cancelMask)
{
	uint32_t fresh;

	if(state == NULL) {
		return 0u;
	}
	fresh = held & actionMask & ~state->previous;
	state->previous = held;
	if((held & cancelMask) != 0u) {
		return fresh & cancelMask;
	}
	return fresh != 0u ? fresh | (held & modifierMask) : 0u;
}

static int magnitude(int value)
{
	if(value == INT_MIN) {
		return INT_MAX;
	}
	return value < 0 ? -value : value;
}

static int sampleMagnitude(const uiMenuInputSample_t *sample,
	uint32_t allowedAxes)
{
	int x = (allowedAxes & UI_MENU_INPUT_AXIS_HORIZONTAL) != 0u ?
		magnitude(sample->x) : 0;
	int y = (allowedAxes & UI_MENU_INPUT_AXIS_VERTICAL) != 0u ?
		magnitude(sample->y) : 0;
	return x > y ? x : y;
}

static bool sampleReleased(const uiMenuInputSample_t *sample,
	uint32_t allowedAxes)
{
	return sample->valid &&
		((allowedAxes & UI_MENU_INPUT_AXIS_HORIZONTAL) == 0u ||
		magnitude(sample->x) <= UI_MENU_INPUT_RELEASE) &&
		((allowedAxes & UI_MENU_INPUT_AXIS_VERTICAL) == 0u ||
		magnitude(sample->y) <= UI_MENU_INPUT_RELEASE);
}

static uiMenuInputDirection_t sampleDirection(
	const uiMenuInputSample_t *sample, uint32_t allowedAxes)
{
	int x = (allowedAxes & UI_MENU_INPUT_AXIS_HORIZONTAL) != 0u ?
		magnitude(sample->x) : 0;
	int y = (allowedAxes & UI_MENU_INPUT_AXIS_VERTICAL) != 0u ?
		magnitude(sample->y) : 0;

	if(x < UI_MENU_INPUT_ENGAGE && y < UI_MENU_INPUT_ENGAGE) {
		return UI_MENU_INPUT_NONE;
	}
	/* A true diagonal resolves horizontally on acquisition. Once acquired,
	 * the active axis remains locked until release or an intentional reversal. */
	if(x >= y) {
		return sample->x < 0 ? UI_MENU_INPUT_LEFT : UI_MENU_INPUT_RIGHT;
	}
	return sample->y < 0 ? UI_MENU_INPUT_DOWN : UI_MENU_INPUT_UP;
}

static int directionStrength(uiMenuInputDirection_t direction,
	const uiMenuInputSample_t *sample)
{
	switch(direction) {
		case UI_MENU_INPUT_LEFT:
			return sample->x < 0 ? magnitude(sample->x) : 0;
		case UI_MENU_INPUT_RIGHT:
			return sample->x > 0 ? magnitude(sample->x) : 0;
		case UI_MENU_INPUT_UP:
			return sample->y > 0 ? magnitude(sample->y) : 0;
		case UI_MENU_INPUT_DOWN:
			return sample->y < 0 ? magnitude(sample->y) : 0;
		default:
			return 0;
	}
}

static bool oppositeDirections(uiMenuInputDirection_t first,
	uiMenuInputDirection_t second)
{
	return (first == UI_MENU_INPUT_LEFT && second == UI_MENU_INPUT_RIGHT) ||
		(first == UI_MENU_INPUT_RIGHT && second == UI_MENU_INPUT_LEFT) ||
		(first == UI_MENU_INPUT_UP && second == UI_MENU_INPUT_DOWN) ||
		(first == UI_MENU_INPUT_DOWN && second == UI_MENU_INPUT_UP);
}

static void releaseOwner(uiMenuInputState_t *state)
{
	state->owner = UI_MENU_INPUT_NO_OWNER;
	state->direction = UI_MENU_INPUT_NONE;
	state->heldMicroseconds = 0u;
	state->repeated = false;
}

static void acquire(uiMenuInputState_t *state, int owner,
	uiMenuInputDirection_t direction)
{
	state->owner = owner;
	state->direction = direction;
	state->heldMicroseconds = 0u;
	state->repeated = false;
}

void UIMenuInput_Init(uiMenuInputState_t *state)
{
	if(state == NULL) {
		return;
	}
	memset(state, 0, sizeof(*state));
	state->owner = UI_MENU_INPUT_NO_OWNER;
}

static int acquireOwner(uiMenuInputState_t *state,
	const uiMenuInputSample_t samples[UI_MENU_INPUT_CHANNEL_COUNT],
	uint32_t allowedAxes)
{
	int bestChannel = UI_MENU_INPUT_NO_OWNER;
	int bestMagnitude = UI_MENU_INPUT_ENGAGE - 1;
	int channel;

	for(channel = 0; channel < UI_MENU_INPUT_CHANNEL_COUNT; ++channel) {
		int currentMagnitude;

		if(!samples[channel].valid || !state->channelArmed[channel]) {
			continue;
		}
		currentMagnitude = sampleMagnitude(&samples[channel], allowedAxes);
		if(currentMagnitude > bestMagnitude) {
			bestMagnitude = currentMagnitude;
			bestChannel = channel;
		}
	}
	return bestChannel;
}

uiMenuInputDirection_t UIMenuInput_Update(uiMenuInputState_t *state,
	const uiMenuInputSample_t samples[UI_MENU_INPUT_CHANNEL_COUNT],
	uint32_t elapsedMicroseconds, uint32_t policy, bool inhibited)
{
	uiMenuInputDirection_t candidate;
	uint32_t deadline;
	uint32_t allowedAxes;
	int channel;

	if(state == NULL || samples == NULL) {
		return UI_MENU_INPUT_NONE;
	}
	policy &= UI_MENU_INPUT_AXIS_MASK | UI_MENU_INPUT_REPEAT;
	allowedAxes = policy & UI_MENU_INPUT_AXIS_MASK;
	if(policy != state->policy) {
		releaseOwner(state);
		memset(state->channelArmed, 0, sizeof(state->channelArmed));
		state->policy = policy;
	}
	if(allowedAxes == (uint32_t)UI_MENU_INPUT_AXIS_NONE) {
		return UI_MENU_INPUT_NONE;
	}
	for(channel = 0; channel < UI_MENU_INPUT_CHANNEL_COUNT; ++channel) {
		if(!samples[channel].valid) {
			state->channelArmed[channel] = false;
		}
		else if(sampleReleased(&samples[channel], allowedAxes)) {
			state->channelArmed[channel] = true;
		}
	}
	if(inhibited) {
		releaseOwner(state);
		for(channel = 0; channel < UI_MENU_INPUT_CHANNEL_COUNT; ++channel) {
			state->channelArmed[channel] =
				sampleReleased(&samples[channel], allowedAxes);
		}
		return UI_MENU_INPUT_NONE;
	}
	if(elapsedMicroseconds > UI_MENU_INPUT_MAX_ELAPSED_US) {
		elapsedMicroseconds = UI_MENU_INPUT_MAX_ELAPSED_US;
	}

	if(state->owner < 0 || state->owner >= UI_MENU_INPUT_CHANNEL_COUNT) {
		channel = acquireOwner(state, samples, allowedAxes);
		if(channel == UI_MENU_INPUT_NO_OWNER) {
			releaseOwner(state);
			return UI_MENU_INPUT_NONE;
		}
		candidate = sampleDirection(&samples[channel], allowedAxes);
		if(candidate == UI_MENU_INPUT_NONE) {
			return UI_MENU_INPUT_NONE;
		}
		acquire(state, channel, candidate);
		return candidate;
	}
	if(!samples[state->owner].valid) {
		/* A disconnect must not expose a gesture another port was holding
		 * underneath the owner on this same sample. */
		for(channel = 0; channel < UI_MENU_INPUT_CHANNEL_COUNT; ++channel) {
			if(channel != state->owner &&
				!sampleReleased(&samples[channel], allowedAxes)) {
				state->channelArmed[channel] = false;
			}
		}
		releaseOwner(state);
		return UI_MENU_INPUT_NONE;
	}
	/* A non-owner deflection is observed and consumed while ownership is live;
	 * it can never queue an action for the owner's release frame. */
	for(channel = 0; channel < UI_MENU_INPUT_CHANNEL_COUNT; ++channel) {
		if(channel != state->owner &&
			!sampleReleased(&samples[channel], allowedAxes)) {
			state->channelArmed[channel] = false;
		}
	}
	if((policy & UI_MENU_INPUT_REPEAT) == 0u) {
		if(sampleReleased(&samples[state->owner], allowedAxes)) {
			releaseOwner(state);
		}
		return UI_MENU_INPUT_NONE;
	}

	candidate = sampleDirection(&samples[state->owner], allowedAxes);
	if(candidate != UI_MENU_INPUT_NONE &&
		oppositeDirections(state->direction, candidate)) {
		acquire(state, state->owner, candidate);
		return candidate;
	}
	if(directionStrength(state->direction, &samples[state->owner]) <=
		UI_MENU_INPUT_RELEASE) {
		int releasedOwner = state->owner;
		bool fullyReleased = sampleReleased(&samples[releasedOwner], allowedAxes);
		releaseOwner(state);
		/* An orthogonal deflection cannot slide across the diagonal and become a
		 * new gesture. Both permitted axes must return to neutral first. */
		if(!fullyReleased) {
			state->channelArmed[releasedOwner] = false;
		}
		return UI_MENU_INPUT_NONE;
	}

	if(UINT32_MAX - state->heldMicroseconds < elapsedMicroseconds) {
		state->heldMicroseconds = UINT32_MAX;
	}
	else {
		state->heldMicroseconds += elapsedMicroseconds;
	}
	deadline = state->repeated ? UI_MENU_INPUT_REPEAT_US :
		UI_MENU_INPUT_INITIAL_REPEAT_US;
	if(state->heldMicroseconds < deadline) {
		return UI_MENU_INPUT_NONE;
	}
	state->heldMicroseconds -= deadline;
	state->repeated = true;
	return state->direction;
}
