#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "ui_clock.h"

static unsigned long checks;

#define CHECK(condition) do { \
	checks++; \
	if(!(condition)) { \
		fprintf(stderr, "check failed at %s:%d: %s\n", \
			__FILE__, __LINE__, #condition); \
		exit(1); \
	} \
} while(0)

static bool near(float first, float second, float tolerance)
{
	return fabsf(first - second) <= tolerance;
}

static void checkVector(float x, float y, float expectedX, float expectedY)
{
	CHECK(near(x, expectedX, 0.0002f));
	CHECK(near(y, expectedY, 0.0002f));
	CHECK(near(x * x + y * y, 1.0f, 0.0003f));
}

static void checkCardinals(void)
{
	uiClockFrame_t frame;

	CHECK(UIClock_Compose(&frame, 0, 0, 0.0f));
	CHECK(frame.available);
	checkVector(frame.hourX, frame.hourY, 0.0f, 1.0f);
	checkVector(frame.minuteX, frame.minuteY, 0.0f, 1.0f);
	checkVector(frame.secondX, frame.secondY, 0.0f, 1.0f);

	CHECK(UIClock_Compose(&frame, 3, 0, 15.0f));
	CHECK(frame.hourX > 0.999f);
	checkVector(frame.secondX, frame.secondY, 1.0f, 0.0f);
	CHECK(UIClock_Compose(&frame, 6, 30, 30.0f));
	CHECK(frame.hourY < -0.95f);
	CHECK(frame.hourX < -0.25f);
	CHECK(frame.minuteY < -0.99f);
	CHECK(frame.secondY < -0.99f);
}

static void checkContinuityAndParity(void)
{
	uiClockFrame_t frame50;
	uiClockFrame_t frame60;
	uiClockFrame_t previous;
	int hour;
	int minute;
	int frame;

	for(hour = 0; hour < 24; ++hour) {
		for(minute = 0; minute < 60; ++minute) {
			CHECK(UIClock_Compose(&previous, hour, minute, 0.0f));
			for(frame = 1; frame <= 60; ++frame) {
				float seconds60 = (float)frame / 60.0f;
				CHECK(UIClock_Compose(&frame60, hour, minute, seconds60));
				CHECK(isfinite(frame60.hourX));
				CHECK(isfinite(frame60.minuteX));
				CHECK(isfinite(frame60.secondX));
				CHECK(fabsf(frame60.secondX - previous.secondX) < 0.002f);
				previous = frame60;
			}
			CHECK(UIClock_Compose(&frame50, hour, minute, 1.0f));
			CHECK(near(frame50.hourX, frame60.hourX, 0.00001f));
			CHECK(near(frame50.minuteY, frame60.minuteY, 0.00001f));
			CHECK(near(frame50.secondX, frame60.secondX, 0.00001f));
		}
	}
	CHECK(UIClock_Compose(&frame50, 23, 59, 60.0f));
	CHECK(frame50.available);
}

static void checkInvalid(void)
{
	uiClockFrame_t frame = {true, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

	CHECK(!UIClock_Compose(NULL, 0, 0, 0.0f));
	CHECK(!UIClock_Compose(&frame, -1, 0, 0.0f));
	CHECK(!frame.available);
	CHECK(!UIClock_Compose(&frame, 24, 0, 0.0f));
	CHECK(!UIClock_Compose(&frame, 0, -1, 0.0f));
	CHECK(!UIClock_Compose(&frame, 0, 60, 0.0f));
	CHECK(!UIClock_Compose(&frame, 0, 0, -0.1f));
	CHECK(!UIClock_Compose(&frame, 0, 0, 61.0f));
	CHECK(!UIClock_Compose(&frame, 0, 0, NAN));
	CHECK(!UIClock_Compose(&frame, 0, 0, INFINITY));
}

int main(void)
{
	checkCardinals();
	checkContinuityAndParity();
	checkInvalid();
	printf("ui_clock: %lu checks passed\n", checks);
	return 0;
}
