#include <math.h>
#include <gctypes.h>
#include <ogc/lwp_watchdog.h>

#include "ui_anim.h"
#include "ui_perf.h"

#define UI_ANIM_MAX_DELTA_SECONDS 0.050f
/* Every ambient angular rate is an integer multiple of 0.001 rad/s. Wrapping
 * at 1000 * 2pi therefore preserves every phase instead of producing a rare
 * long-soak jump when the float clock is reduced. */
#define UI_ANIM_TIME_WRAP_SECONDS 6283.18530718f

static u64 lastTicks;
static float frameDelta;
static float elapsedSeconds;

void UIAnim_Reset(void)
{
	lastTicks = gettime();
	frameDelta = 0.0f;
	elapsedSeconds = 0.0f;
}

void UIAnim_BeginFrame(void)
{
	u64 now = gettime();
	u64 elapsedTicks = diff_ticks(lastTicks, now);
	u64 elapsedMicroseconds = ticks_to_microsecs(elapsedTicks);
	float delta = (float)elapsedMicroseconds / 1000000.0f;

	lastTicks = now;
	UIPerf_RecordMicroseconds(UI_PERF_METRIC_FRAME_PERIOD, elapsedMicroseconds);
	frameDelta = delta < UI_ANIM_MAX_DELTA_SECONDS ? delta : UI_ANIM_MAX_DELTA_SECONDS;
	elapsedSeconds += frameDelta;
	if(elapsedSeconds >= UI_ANIM_TIME_WRAP_SECONDS) {
		elapsedSeconds = fmodf(elapsedSeconds, UI_ANIM_TIME_WRAP_SECONDS);
	}
}

float UIAnim_Seconds(void)
{
	return elapsedSeconds;
}

float UIAnim_Delta(void)
{
	return frameDelta;
}

float UIAnim_Approach(float current, float target, float response)
{
	float blend = 1.0f - expf(-response * frameDelta);
	return current + ((target - current) * blend);
}
