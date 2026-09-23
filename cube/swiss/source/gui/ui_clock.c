#include <math.h>
#include <stddef.h>
#include <string.h>

#include "ui_clock.h"

#define UI_CLOCK_TAU 6.28318530717958647692f

static void clockVector(float turns, float *x, float *y)
{
	float angle = turns * UI_CLOCK_TAU;
	*x = sinf(angle);
	*y = cosf(angle);
}

bool UIClock_Compose(uiClockFrame_t *out, int hour, int minute,
	float secondOfMinute)
{
	float minuteWithSeconds;
	float hourWithMinutes;

	if(out == NULL) {
		return false;
	}
	memset(out, 0, sizeof(*out));
	if(hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
		!isfinite(secondOfMinute) || secondOfMinute < 0.0f ||
		secondOfMinute >= 61.0f) {
		return false;
	}
	minuteWithSeconds = (float)minute + secondOfMinute / 60.0f;
	hourWithMinutes = (float)(hour % 12) + minuteWithSeconds / 60.0f;
	clockVector(hourWithMinutes / 12.0f, &out->hourX, &out->hourY);
	clockVector(minuteWithSeconds / 60.0f, &out->minuteX, &out->minuteY);
	clockVector(secondOfMinute / 60.0f, &out->secondX, &out->secondY);
	out->secondOfMinute = secondOfMinute;
	out->available = true;
	return true;
}
