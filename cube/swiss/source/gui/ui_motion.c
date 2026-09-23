#include <math.h>

#include "ui_motion.h"

#define UI_MOTION_MAX_DELTA_SECONDS 0.050f
#define UI_MOTION_REDUCED_RESPONSE_SCALE 1.75f
#define UI_MOTION_REDUCED_AMPLITUDE_SCALE 0.35f

uiMotionMode_t UIMotion_ModeFromFlags(int disableAnimations,
	int reduceAnimations)
{
	if(disableAnimations) {
		return UI_MOTION_OFF;
	}
	if(reduceAnimations) {
		return UI_MOTION_REDUCED;
	}
	return UI_MOTION_FULL;
}

uiMotionMode_t UIMotion_CycleMode(uiMotionMode_t mode, int direction)
{
	if(mode != UI_MOTION_FULL && mode != UI_MOTION_REDUCED &&
		mode != UI_MOTION_OFF) {
		mode = UI_MOTION_FULL;
	}
	if(direction > 0) {
		return (uiMotionMode_t)(((int)mode + 1) % UI_MOTION_COUNT);
	}
	if(direction < 0) {
		return (uiMotionMode_t)(((int)mode + UI_MOTION_COUNT - 1) %
			UI_MOTION_COUNT);
	}
	return mode;
}

static void snapToTarget(uiMotionSpring_t *spring)
{
	spring->value = spring->target;
	spring->velocity = 0.0f;
}

void UIMotion_SpringInit(uiMotionSpring_t *spring, float value, float response)
{
	spring->value = value;
	spring->velocity = 0.0f;
	spring->target = value;
	spring->response = response;
}

void UIMotion_SpringSnap(uiMotionSpring_t *spring, float value)
{
	spring->value = value;
	spring->velocity = 0.0f;
	spring->target = value;
}

void UIMotion_SpringRetarget(uiMotionSpring_t *spring, float target, uiMotionMode_t mode)
{
	if(mode == UI_MOTION_OFF) {
		UIMotion_SpringSnap(spring, target);
		return;
	}

	/* Full motion keeps momentum across an interrupt. Reduced motion starts
	 * a changed leg from the current pose without carrying secondary travel. */
	if(mode == UI_MOTION_REDUCED && target != spring->target) {
		spring->velocity = 0.0f;
	}
	spring->target = target;
}

float UIMotion_SpringUpdate(uiMotionSpring_t *spring, float deltaSeconds, uiMotionMode_t mode)
{
	float displacement;
	float response;
	float decay;
	float velocityTerm;

	if(mode == UI_MOTION_OFF || spring->response <= 0.0f) {
		snapToTarget(spring);
		return spring->value;
	}
	if(spring->value == spring->target && spring->velocity == 0.0f) {
		return spring->value;
	}

	if(deltaSeconds <= 0.0f) {
		return spring->value;
	}
	if(deltaSeconds > UI_MOTION_MAX_DELTA_SECONDS) {
		deltaSeconds = UI_MOTION_MAX_DELTA_SECONDS;
	}

	response = spring->response;
	if(mode == UI_MOTION_REDUCED) {
		response *= UI_MOTION_REDUCED_RESPONSE_SCALE;
	}

	/* Exact solution of x'' + 2wx' + w^2x = 0 over the clamped frame.
	 * It remains stable through frame-rate changes and needs no substeps. */
	displacement = spring->value - spring->target;
	decay = expf(-response * deltaSeconds);
	velocityTerm = (spring->velocity + response * displacement) * deltaSeconds;
	spring->value = spring->target + (displacement + velocityTerm) * decay;
	spring->velocity = (spring->velocity - response * velocityTerm) * decay;
	if(fabsf(spring->target - spring->value) <= 0.0001f &&
		fabsf(spring->velocity) <= 0.0005f) {
		snapToTarget(spring);
	}

	return spring->value;
}

bool UIMotion_SpringSettled(const uiMotionSpring_t *spring, float positionEpsilon, float velocityEpsilon)
{
	return fabsf(spring->target - spring->value) <= fabsf(positionEpsilon) &&
		fabsf(spring->velocity) <= fabsf(velocityEpsilon);
}

float UIMotion_EaseOutCubic(float progress)
{
	float remaining;

	if(progress <= 0.0f) {
		return 0.0f;
	}
	if(progress >= 1.0f) {
		return 1.0f;
	}

	remaining = 1.0f - progress;
	return 1.0f - remaining * remaining * remaining;
}

float UIMotion_Amplitude(float amplitude, uiMotionMode_t mode)
{
	if(mode == UI_MOTION_OFF) {
		return 0.0f;
	}
	if(mode == UI_MOTION_REDUCED) {
		return amplitude * UI_MOTION_REDUCED_AMPLITUDE_SCALE;
	}
	return amplitude;
}
