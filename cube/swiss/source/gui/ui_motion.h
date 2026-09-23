#ifndef UI_MOTION_H
#define UI_MOTION_H

#include <stdbool.h>

typedef enum {
	UI_MOTION_FULL = 0,
	UI_MOTION_REDUCED,
	UI_MOTION_OFF,
	UI_MOTION_COUNT
} uiMotionMode_t;

/* Translate the persisted compatibility flags into one explicit motion mode.
 * The legacy disable flag remains authoritative so existing configurations
 * continue to map exactly to Full (0) or Off (1). */
uiMotionMode_t UIMotion_ModeFromFlags(int disableAnimations,
	int reduceAnimations);

/* Cycle the user-facing Full -> Reduced -> Off preference. Positive and
 * negative directions mirror Settings' Right/Left controls; zero is inert. */
uiMotionMode_t UIMotion_CycleMode(uiMotionMode_t mode, int direction);

/*
 * A scalar critically damped spring. Response is the spring's angular
 * frequency in inverse seconds; larger values settle more quickly.
 */
typedef struct {
	float value;
	float velocity;
	float target;
	float response;
} uiMotionSpring_t;

void UIMotion_SpringInit(uiMotionSpring_t *spring, float value, float response);
void UIMotion_SpringSnap(uiMotionSpring_t *spring, float value);
void UIMotion_SpringRetarget(uiMotionSpring_t *spring, float target, uiMotionMode_t mode);
float UIMotion_SpringUpdate(uiMotionSpring_t *spring, float deltaSeconds, uiMotionMode_t mode);
bool UIMotion_SpringSettled(const uiMotionSpring_t *spring, float positionEpsilon, float velocityEpsilon);

/* Clamped, normalized ease-out curve for finite scene transitions. */
float UIMotion_EaseOutCubic(float progress);

/* Scales optional decorative travel while preserving required layout motion. */
float UIMotion_Amplitude(float amplitude, uiMotionMode_t mode);

#endif
