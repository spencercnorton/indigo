#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_menu_input.h"

static unsigned long checks;

#define CHECK(condition) do { \
	checks++; \
	if(!(condition)) { \
		fprintf(stderr, "check failed at %s:%d: %s\n", \
			__FILE__, __LINE__, #condition); \
		exit(1); \
	} \
} while(0)

static void clearSamples(uiMenuInputSample_t samples[UI_MENU_INPUT_CHANNEL_COUNT])
{
	int channel;

	memset(samples, 0,
		sizeof(uiMenuInputSample_t) * UI_MENU_INPUT_CHANNEL_COUNT);
	for(channel = 0; channel < UI_MENU_INPUT_CHANNEL_COUNT; ++channel) {
		samples[channel].valid = true;
	}
}

static void armAll(uiMenuInputState_t *state,
	uiMenuInputSample_t samples[UI_MENU_INPUT_CHANNEL_COUNT])
{
	clearSamples(samples);
	UIMenuInput_Init(state);
	CHECK(UIMenuInput_Update(state, samples, 0u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_NONE);
}

static void checkInitialEdges(void)
{
	static const struct {
		int x;
		int y;
		uiMenuInputDirection_t direction;
	} cases[] = {
		{-UI_MENU_INPUT_ENGAGE, 0, UI_MENU_INPUT_LEFT},
		{UI_MENU_INPUT_ENGAGE, 0, UI_MENU_INPUT_RIGHT},
		{0, UI_MENU_INPUT_ENGAGE, UI_MENU_INPUT_UP},
		{0, -UI_MENU_INPUT_ENGAGE, UI_MENU_INPUT_DOWN},
	};
	uiMenuInputSample_t samples[UI_MENU_INPUT_CHANNEL_COUNT];
	uiMenuInputState_t state;
	size_t index;

	for(index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
		armAll(&state, samples);
		samples[0].x = cases[index].x;
		samples[0].y = cases[index].y;
		CHECK(UIMenuInput_Update(&state, samples, 16667u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
			cases[index].direction);
		CHECK(state.owner == 0);
		CHECK(state.direction == cases[index].direction);
	}
	armAll(&state, samples);
	samples[0].x = UI_MENU_INPUT_ENGAGE - 1;
	CHECK(UIMenuInput_Update(&state, samples, 50000u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_NONE);
	CHECK(state.owner == UI_MENU_INPUT_NO_OWNER);
}

static void checkNeutralArmingAndOwnership(void)
{
	uiMenuInputSample_t samples[UI_MENU_INPUT_CHANNEL_COUNT];
	uiMenuInputState_t state;

	clearSamples(samples);
	samples[1].x = UI_MENU_INPUT_RELEASE + 7;
	UIMenuInput_Init(&state);
	CHECK(UIMenuInput_Update(&state, samples, 16667u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_NONE);
	CHECK(!state.channelArmed[1]);
	CHECK(state.channelArmed[0]);
	samples[1].x = 100;
	CHECK(UIMenuInput_Update(&state, samples, 16667u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_NONE);
	CHECK(state.owner == UI_MENU_INPUT_NO_OWNER);
	samples[1].x = 0;
	CHECK(UIMenuInput_Update(&state, samples, 16667u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_NONE);
	CHECK(state.channelArmed[1]);
	samples[1].x = 100;
	CHECK(UIMenuInput_Update(&state, samples, 16667u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_RIGHT);
	CHECK(state.owner == 1);

	/* The strongest intentional edge wins acquisition; ties are stable by
	 * physical channel order. Once acquired, another port cannot steal. */
	armAll(&state, samples);
	samples[0].x = 60;
	samples[2].x = -90;
	CHECK(UIMenuInput_Update(&state, samples, 16667u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_LEFT);
	CHECK(state.owner == 2);
	samples[0].x = 100;
	samples[2].x = -40;
	CHECK(UIMenuInput_Update(&state, samples, 50000u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_NONE);
	CHECK(state.owner == 2);
	samples[2].x = 0;
	CHECK(UIMenuInput_Update(&state, samples, 16667u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_NONE);
	CHECK(state.owner == UI_MENU_INPUT_NO_OWNER);
	CHECK(UIMenuInput_Update(&state, samples, 16667u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_NONE);
	CHECK(!state.channelArmed[0]);
	samples[0].x = 0;
	CHECK(UIMenuInput_Update(&state, samples, 16667u,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) == UI_MENU_INPUT_NONE);
	samples[0].x = 100;
	CHECK(UIMenuInput_Update(&state, samples, 16667u,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) == UI_MENU_INPUT_RIGHT);
	CHECK(state.owner == 0);

	armAll(&state, samples);
	samples[0].x = 64;
	samples[1].x = -64;
	CHECK(UIMenuInput_Update(&state, samples, 16667u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_RIGHT);
	CHECK(state.owner == 0);
}

static void checkHysteresisReversalAndAxisLock(void)
{
	uiMenuInputSample_t samples[UI_MENU_INPUT_CHANNEL_COUNT];
	uiMenuInputState_t state;

	armAll(&state, samples);
	samples[0].x = UI_MENU_INPUT_ENGAGE;
	CHECK(UIMenuInput_Update(&state, samples, 0u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_RIGHT);
	samples[0].x = UI_MENU_INPUT_RELEASE + 1;
	for(int index = 0; index < 6; ++index) {
		CHECK(UIMenuInput_Update(&state, samples,
			UI_MENU_INPUT_MAX_ELAPSED_US, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
			UI_MENU_INPUT_NONE);
	}
	CHECK(UIMenuInput_Update(&state, samples, 20000u,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) == UI_MENU_INPUT_RIGHT);
	CHECK(state.owner == 0);
	samples[0].x = UI_MENU_INPUT_RELEASE;
	CHECK(UIMenuInput_Update(&state, samples, 16667u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_NONE);
	CHECK(state.owner == UI_MENU_INPUT_NO_OWNER);

	armAll(&state, samples);
	samples[0].x = 70;
	CHECK(UIMenuInput_Update(&state, samples, 0u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_RIGHT);
	samples[0].x = -UI_MENU_INPUT_ENGAGE;
	CHECK(UIMenuInput_Update(&state, samples, 1u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_LEFT);
	CHECK(state.direction == UI_MENU_INPUT_LEFT);

	armAll(&state, samples);
	samples[0].x = 45;
	samples[0].y = 90;
	CHECK(UIMenuInput_Update(&state, samples, 0u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_UP);
	samples[0].x = -100;
	samples[0].y = 40;
	CHECK(UIMenuInput_Update(&state, samples, 50000u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_NONE);
	CHECK(state.direction == UI_MENU_INPUT_UP);
	samples[0].y = UI_MENU_INPUT_RELEASE;
	CHECK(UIMenuInput_Update(&state, samples, 1u,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) == UI_MENU_INPUT_NONE);
	CHECK(state.owner == UI_MENU_INPUT_NO_OWNER);
	CHECK(!state.channelArmed[0]);
	samples[0].y = 0;
	CHECK(UIMenuInput_Update(&state, samples, 1u,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) == UI_MENU_INPUT_NONE);
	samples[0].x = 0;
	CHECK(UIMenuInput_Update(&state, samples, 1u,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) == UI_MENU_INPUT_NONE);
	samples[0].x = -100;
	CHECK(UIMenuInput_Update(&state, samples, 1u,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) == UI_MENU_INPUT_LEFT);

	/* Exact diagonals resolve consistently instead of firing two intents. */
	armAll(&state, samples);
	samples[3].x = -50;
	samples[3].y = 50;
	CHECK(UIMenuInput_Update(&state, samples, 0u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_LEFT);
}

static void checkAllowedAxes(void)
{
	uiMenuInputSample_t samples[UI_MENU_INPUT_CHANNEL_COUNT];
	uiMenuInputState_t state;

	clearSamples(samples);
	UIMenuInput_Init(&state);
	samples[0].y = 100;
	CHECK(UIMenuInput_Update(&state, samples, 16667u,
		UI_MENU_INPUT_AXIS_HORIZONTAL, false) == UI_MENU_INPUT_NONE);
	CHECK(state.channelArmed[0]);
	samples[0].x = UI_MENU_INPUT_ENGAGE;
	CHECK(UIMenuInput_Update(&state, samples, 16667u,
		UI_MENU_INPUT_AXIS_HORIZONTAL, false) == UI_MENU_INPUT_RIGHT);

	/* Changing the permitted axis fails neutral: a deflected newly allowed
	 * axis cannot inherit the previous surface's ownership. */
	CHECK(UIMenuInput_Update(&state, samples, 16667u,
		UI_MENU_INPUT_AXIS_VERTICAL, false) == UI_MENU_INPUT_NONE);
	CHECK(!state.channelArmed[0]);
	samples[0].x = 0;
	CHECK(UIMenuInput_Update(&state, samples, 16667u,
		UI_MENU_INPUT_AXIS_VERTICAL, false) == UI_MENU_INPUT_NONE);
	samples[0].y = 0;
	CHECK(UIMenuInput_Update(&state, samples, 16667u,
		UI_MENU_INPUT_AXIS_VERTICAL, false) == UI_MENU_INPUT_NONE);
	samples[0].y = UI_MENU_INPUT_ENGAGE;
	CHECK(UIMenuInput_Update(&state, samples, 16667u,
		UI_MENU_INPUT_AXIS_VERTICAL, false) == UI_MENU_INPUT_UP);
	CHECK(UIMenuInput_Update(&state, samples, 16667u,
		UI_MENU_INPUT_AXIS_NONE, false) == UI_MENU_INPUT_NONE);
	CHECK(state.owner == UI_MENU_INPUT_NO_OWNER);
}

static void checkOneShotUntilNeutral(void)
{
	static const uint32_t frameTimes[] = {20000u, 16667u};
	uiMenuInputSample_t samples[UI_MENU_INPUT_CHANNEL_COUNT];
	uiMenuInputState_t state;
	size_t rate;

	for(rate = 0u; rate < sizeof(frameTimes) / sizeof(frameTimes[0]); ++rate) {
		int events = 0;
		int frame;

		clearSamples(samples);
		UIMenuInput_Init(&state);
		CHECK(UIMenuInput_Update(&state, samples, 0u,
			UI_MENU_INPUT_AXIS_HORIZONTAL, false) == UI_MENU_INPUT_NONE);
		samples[0].x = 80;
		if(UIMenuInput_Update(&state, samples, frameTimes[rate],
			UI_MENU_INPUT_AXIS_HORIZONTAL, false) == UI_MENU_INPUT_RIGHT) {
			events++;
		}
		for(frame = 0; frame < 80; ++frame) {
			CHECK(UIMenuInput_Update(&state, samples, frameTimes[rate],
				UI_MENU_INPUT_AXIS_HORIZONTAL, false) == UI_MENU_INPUT_NONE);
		}
		samples[0].x = -80;
		CHECK(UIMenuInput_Update(&state, samples, frameTimes[rate],
			UI_MENU_INPUT_AXIS_HORIZONTAL, false) == UI_MENU_INPUT_NONE);
		CHECK(events == 1);
		samples[0].x = 0;
		CHECK(UIMenuInput_Update(&state, samples, frameTimes[rate],
			UI_MENU_INPUT_AXIS_HORIZONTAL, false) == UI_MENU_INPUT_NONE);
		samples[0].x = -80;
		CHECK(UIMenuInput_Update(&state, samples, frameTimes[rate],
			UI_MENU_INPUT_AXIS_HORIZONTAL, false) == UI_MENU_INPUT_LEFT);
	}
}

static void checkValidityLifecycle(void)
{
	uiMenuInputSample_t samples[UI_MENU_INPUT_CHANNEL_COUNT];
	uiMenuInputState_t state;
	int channel;

	memset(samples, 0, sizeof(samples));
	UIMenuInput_Init(&state);
	CHECK(UIMenuInput_Update(&state, samples, 16667u,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_NONE);
	for(channel = 0; channel < UI_MENU_INPUT_CHANNEL_COUNT; ++channel) {
		CHECK(!state.channelArmed[channel]);
	}
	samples[0].valid = true;
	CHECK(UIMenuInput_Update(&state, samples, 16667u,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_NONE);
	samples[0].x = 80;
	CHECK(UIMenuInput_Update(&state, samples, 16667u,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_RIGHT);
	samples[0].valid = false;
	CHECK(UIMenuInput_Update(&state, samples, 16667u,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_NONE);
	CHECK(state.owner == UI_MENU_INPUT_NO_OWNER);
	CHECK(!state.channelArmed[0]);
	samples[0].valid = true;
	CHECK(UIMenuInput_Update(&state, samples, 16667u,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_NONE);
	CHECK(!state.channelArmed[0]);
	samples[0].x = 0;
	CHECK(UIMenuInput_Update(&state, samples, 16667u,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_NONE);
	CHECK(state.channelArmed[0]);
	samples[0].y = -80;
	CHECK(UIMenuInput_Update(&state, samples, 16667u,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_DOWN);

	/* If the owner disconnects while another port is held, that held gesture
	 * is consumed rather than surfacing as a queued action next frame. */
	clearSamples(samples);
	UIMenuInput_Init(&state);
	CHECK(UIMenuInput_Update(&state, samples, 0u,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_NONE);
	samples[0].x = 80;
	CHECK(UIMenuInput_Update(&state, samples, 0u,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_RIGHT);
	samples[0].valid = false;
	samples[1].y = 80;
	CHECK(UIMenuInput_Update(&state, samples, 16667u,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_NONE);
	CHECK(!state.channelArmed[1]);
	CHECK(UIMenuInput_Update(&state, samples, 16667u,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_NONE);
	samples[1].y = 0;
	CHECK(UIMenuInput_Update(&state, samples, 16667u,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_NONE);
	samples[1].y = 80;
	CHECK(UIMenuInput_Update(&state, samples, 16667u,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_UP);
}

static uint32_t firstRepeatAt(uint32_t frameMicroseconds)
{
	uiMenuInputSample_t samples[UI_MENU_INPUT_CHANNEL_COUNT];
	uiMenuInputState_t state;
	uint32_t elapsed = 0u;
	int guard = 0;

	armAll(&state, samples);
	samples[0].x = 80;
	CHECK(UIMenuInput_Update(&state, samples, 0u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_RIGHT);
	while(++guard < 1000) {
		elapsed += frameMicroseconds;
		if(UIMenuInput_Update(&state, samples, frameMicroseconds, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
			UI_MENU_INPUT_RIGHT) {
			return elapsed;
		}
	}
	CHECK(false);
	return 0u;
}

static int countEvents(uint32_t frameMicroseconds, uint32_t duration)
{
	uiMenuInputSample_t samples[UI_MENU_INPUT_CHANNEL_COUNT];
	uiMenuInputState_t state;
	uint32_t elapsed = 0u;
	int events = 0;

	armAll(&state, samples);
	samples[0].y = 80;
	if(UIMenuInput_Update(&state, samples, 0u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) == UI_MENU_INPUT_UP) {
		events++;
	}
	while(elapsed < duration) {
		uiMenuInputDirection_t event;
		elapsed += frameMicroseconds;
		event = UIMenuInput_Update(&state, samples, frameMicroseconds, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false);
		CHECK(event == UI_MENU_INPUT_NONE || event == UI_MENU_INPUT_UP);
		if(event == UI_MENU_INPUT_UP) {
			events++;
		}
	}
	return events;
}

static void checkRepeatCadence(void)
{
	uiMenuInputSample_t samples[UI_MENU_INPUT_CHANNEL_COUNT];
	uiMenuInputState_t state;
	uint32_t at50 = firstRepeatAt(20000u);
	uint32_t at60 = firstRepeatAt(16667u);
	int count50;
	int count60;
	int index;

	CHECK(at50 >= UI_MENU_INPUT_INITIAL_REPEAT_US);
	CHECK(at50 < UI_MENU_INPUT_INITIAL_REPEAT_US + 20000u);
	CHECK(at60 >= UI_MENU_INPUT_INITIAL_REPEAT_US);
	CHECK(at60 < UI_MENU_INPUT_INITIAL_REPEAT_US + 16667u);
	count50 = countEvents(20000u, 2000000u);
	count60 = countEvents(16667u, 2000000u);
	CHECK(count50 == count60 || count50 + 1 == count60 ||
		count60 + 1 == count50);

	armAll(&state, samples);
	samples[0].x = -80;
	CHECK(UIMenuInput_Update(&state, samples, 0u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_LEFT);
	for(index = 0; index < 6; ++index) {
		CHECK(UIMenuInput_Update(&state, samples, UINT32_MAX, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
			UI_MENU_INPUT_NONE);
	}
	/* MAX_ELAPSED_US prevents a one-second stall from crossing 320 ms in one
	 * call or emitting a catch-up burst. */
	CHECK(state.heldMicroseconds == 6u * UI_MENU_INPUT_MAX_ELAPSED_US);
	CHECK(UIMenuInput_Update(&state, samples, UINT32_MAX, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_LEFT);
	CHECK(state.heldMicroseconds < UI_MENU_INPUT_REPEAT_US);
}

static void checkDigitalPrecedence(void)
{
	uiMenuInputSample_t samples[UI_MENU_INPUT_CHANNEL_COUNT];
	uiMenuInputState_t state;

	armAll(&state, samples);
	samples[0].x = 90;
	CHECK(UIMenuInput_Update(&state, samples, 16667u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, true) ==
		UI_MENU_INPUT_NONE);
	CHECK(state.owner == UI_MENU_INPUT_NO_OWNER);
	CHECK(UIMenuInput_Update(&state, samples, 16667u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_NONE);
	CHECK(!state.channelArmed[0]);
	samples[0].x = 0;
	CHECK(UIMenuInput_Update(&state, samples, 16667u,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) == UI_MENU_INPUT_NONE);
	samples[0].x = 90;
	CHECK(UIMenuInput_Update(&state, samples, 16667u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_RIGHT);

	/* A stick that becomes deflected while a digital hold is draining is also
	 * consumed, rather than leaking as an action on the release frame. */
	samples[0].x = 0;
	CHECK(UIMenuInput_Update(&state, samples, 16667u,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, true) == UI_MENU_INPUT_NONE);
	samples[0].x = -90;
	CHECK(UIMenuInput_Update(&state, samples, 16667u,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, true) == UI_MENU_INPUT_NONE);
	CHECK(UIMenuInput_Update(&state, samples, 16667u,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) == UI_MENU_INPUT_NONE);
	CHECK(!state.channelArmed[0]);
}

static void checkExhaustiveCardinalSafety(void)
{
	uiMenuInputSample_t samples[UI_MENU_INPUT_CHANNEL_COUNT];
	uiMenuInputState_t state;
	int channel;
	int x;
	int y;

	CHECK(UIMenuInput_Update(NULL, NULL, UINT32_MAX, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_NONE);
	UIMenuInput_Init(NULL);
	for(channel = 0; channel < UI_MENU_INPUT_CHANNEL_COUNT; ++channel) {
		for(x = -128; x <= 127; ++x) {
			for(y = -128; y <= 127; ++y) {
				uiMenuInputDirection_t event;

				armAll(&state, samples);
				samples[channel].x = x;
				samples[channel].y = y;
				event = UIMenuInput_Update(&state, samples, 16667u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false);
				CHECK(event >= UI_MENU_INPUT_NONE &&
					event <= UI_MENU_INPUT_DOWN);
				if(event == UI_MENU_INPUT_NONE) {
					CHECK(state.owner == UI_MENU_INPUT_NO_OWNER);
				}
				else {
					CHECK(state.owner == channel);
					CHECK(state.direction == event);
				}
			}
		}
	}

	armAll(&state, samples);
	samples[0].x = INT_MIN;
	samples[0].y = INT_MAX;
	CHECK(UIMenuInput_Update(&state, samples, UINT32_MAX, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_LEFT);
	CHECK(UIMenuInput_Update(&state, samples, UINT32_MAX,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_NONE);
	samples[0].x = 0;
	samples[0].y = 0;
	CHECK(UIMenuInput_Update(&state, samples, 0u,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_NONE);
	samples[0].y = INT_MIN;
	CHECK(UIMenuInput_Update(&state, samples, 0u,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_DOWN);
	CHECK(UIMenuInput_Update(&state, samples, UINT32_MAX,
		UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_NONE);
	state.owner = INT_MAX;
	CHECK(UIMenuInput_Update(&state, samples, 0u, UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT, false) ==
		UI_MENU_INPUT_DOWN);
}

static void checkFreshActions(void)
{
	const uint32_t a = 1u, b = 2u, x = 4u, y = 8u, l = 16u;
	const uint32_t actions = a | b | x | y;
	uiMenuActionState_t state;
	uint32_t initial;
	uint32_t held;
	unsigned int frame;

	UIMenuAction_Init(NULL, 0u);
	CHECK(UIMenuAction_Update(NULL, a, actions, l, b) == 0u);
	/* Entering with A and a resting L cannot launch or lock out Back. */
	UIMenuAction_Init(&state, a | l);
	CHECK(UIMenuAction_Update(&state, a | l, actions, l, b) == 0u);
	CHECK(UIMenuAction_Update(&state, a | l | b, actions, l, b) == b);
	/* Holding a shortcut does not prevent another fresh action. */
	UIMenuAction_Init(&state, x | l);
	CHECK(UIMenuAction_Update(&state, x | l | y, actions, l, b) == (y | l));
	CHECK(UIMenuAction_Update(&state, x | l | b, actions, l, b) == b);
	/* L stays a modifier; it need not release before a clean-boot chord. */
	UIMenuAction_Init(&state, l);
	CHECK(UIMenuAction_Update(&state, l, actions, l, b) == 0u);
	CHECK(UIMenuAction_Update(&state, l | a, actions, l, b) == (l | a));
	for(frame = 0u; frame < 600u; ++frame) {
		CHECK(UIMenuAction_Update(&state, l | a, actions, l, b) == 0u);
	}
	CHECK(UIMenuAction_Update(&state, l, actions, l, b) == 0u);
	CHECK(UIMenuAction_Update(&state, l | a, actions, l, b) == (l | a));
	/* A held cancel also suppresses newly pressed A, and consumed chords
	 * do not emerge as a delayed launch when B releases. */
	UIMenuAction_Init(&state, b);
	CHECK(UIMenuAction_Update(&state, a | b, actions, l, b) == 0u);
	CHECK(UIMenuAction_Update(&state, a, actions, l, b) == 0u);
	CHECK(UIMenuAction_Update(&state, 0u, actions, l, b) == 0u);
	CHECK(UIMenuAction_Update(&state, a | b, actions, l, b) == b);
	/* Reset after a modal consumes its dismissal, but permits new presses. */
	UIMenuAction_Init(&state, b | y);
	CHECK(UIMenuAction_Update(&state, b | y, actions, l, b) == 0u);
	CHECK(UIMenuAction_Update(&state, y, actions, l, b) == 0u);
	CHECK(UIMenuAction_Update(&state, y | x, actions, l, b) == x);
	/* Independent reference truth table for all five-button transition pairs. */
	for(initial = 0u; initial < 32u; ++initial) {
		for(held = 0u; held < 32u; ++held) {
			uint32_t expected = 0u;
			uint32_t bit;
			for(bit = a; bit <= y; bit <<= 1u) {
				if((held & bit) != 0u && (initial & bit) == 0u) {
					expected |= bit;
				}
			}
			if((held & b) != 0u) expected &= b;
			else if(expected != 0u && (held & l) != 0u) expected |= l;
			UIMenuAction_Init(&state, initial);
			CHECK(UIMenuAction_Update(&state, held, actions, l, b) == expected);
			CHECK(UIMenuAction_Update(&state, held, actions, l, b) == 0u);
		}
	}
}

int main(void)
{
	checkFreshActions();
	checkInitialEdges();
	checkNeutralArmingAndOwnership();
	checkHysteresisReversalAndAxisLock();
	checkRepeatCadence();
	checkDigitalPrecedence();
	checkAllowedAxes();
	checkOneShotUntilNeutral();
	checkValidityLifecycle();
	checkExhaustiveCardinalSafety();
	printf("ui_menu_input: %lu checks passed\n", checks);
	return 0;
}
