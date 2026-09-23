#ifndef UI_CLOCK_H
#define UI_CLOCK_H

#include <stdbool.h>

/* Pointer-free numeric clock presentation prepared before GX event traversal. */
typedef struct {
	bool available;
	float secondOfMinute;
	float hourX;
	float hourY;
	float minuteX;
	float minuteY;
	float secondX;
	float secondY;
} uiClockFrame_t;

/* Vectors use +Y as twelve o'clock and +X as three o'clock. */
bool UIClock_Compose(uiClockFrame_t *out, int hour, int minute,
	float secondOfMinute);

#endif
