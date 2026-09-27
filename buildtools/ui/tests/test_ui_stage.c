#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_stage.h"

static unsigned long checks;

#define CHECK(condition) do { \
	checks++; \
	if(!(condition)) { \
		fprintf(stderr, "check failed at %s:%d: %s\n", \
			__FILE__, __LINE__, #condition); \
		exit(1); \
	} \
} while(0)

static bool near(float a, float b)
{
	return fabsf(a - b) < 0.0005f;
}

/* The two projections the menu loads, as guOrtho(m, 0, 480, 0, 640, 0, 1)
 * and guPerspective(m, 42, 640 / 480, 0.1, 20) build them. */
static void ortho(float m[4][4])
{
	memset(m, 0, sizeof(float[4][4]));
	m[0][0] = 2.0f / 640.0f; m[0][3] = -1.0f;
	m[1][1] = -2.0f / 480.0f; m[1][3] = 1.0f;
	m[2][2] = -1.0f; m[2][3] = -1.0f;
	m[3][3] = 1.0f;
}

static float cotangent(void)
{
	return 1.0f / tanf(21.0f * 3.14159265f / 180.0f);
}

static void perspective(float m[4][4])
{
	memset(m, 0, sizeof(float[4][4]));
	m[0][0] = cotangent() / (640.0f / 480.0f);
	m[1][1] = cotangent();
	m[2][2] = -0.1f / 19.9f; m[2][3] = -2.0f / 19.9f;
	m[3][2] = -1.0f;
}

static float orthoClipX(float m[4][4], float x)
{
	return m[0][0] * x + m[0][3];
}

int main(void)
{
	float m[4][4], plain[4][4];
	static const float xs[] = {-106.0f, 0.0f, 100.0f, 320.0f, 555.5f, 640.0f, 746.0f};

	/* 4:3 changes nothing. */
	UIStage_SetWide(false);
	CHECK(UIStage_Left() == 0.0f && UIStage_Right() == 640.0f);
	CHECK(UIStage_FrameX(123.5f) == 123.5f);
	ortho(m); ortho(plain); UIStage_Project(m);
	CHECK(memcmp(m, plain, sizeof(m)) == 0);
	perspective(m); perspective(plain); UIStage_Project(m);
	CHECK(memcmp(m, plain, sizeof(m)) == 0);

	/* 16:9: the frame shows a stage 16:9 at 480 high, centred on 320. */
	UIStage_SetWide(true);
	CHECK(near((UIStage_Right() - UIStage_Left()) / 480.0f, 16.0f / 9.0f));
	CHECK(near(UIStage_Left() + UIStage_Right(), 640.0f));

	/* The squeezed 2D projection puts the stage's edges on the frame's. */
	ortho(m); ortho(plain); UIStage_Project(m);
	CHECK(near(orthoClipX(m, UIStage_Left()), -1.0f));
	CHECK(near(orthoClipX(m, UIStage_Right()), 1.0f));
	CHECK(near(orthoClipX(m, 320.0f), 0.0f));
	CHECK(memcmp(m[1], plain[1], sizeof(float[3][4])) == 0);
	/* The glass reads its copy where the projection drew. */
	for(size_t i = 0; i < sizeof(xs) / sizeof(xs[0]); i++)
		CHECK(near(UIStage_FrameX(xs[i]), (orthoClipX(m, xs[i]) + 1.0f) * 320.0f));

	/* The cube gets a 16:9 aspect... */
	float squeezed[4][4];
	perspective(squeezed); perspective(plain); UIStage_Project(squeezed);
	CHECK(near(squeezed[0][0], cotangent() / (16.0f / 9.0f)));
	CHECK(memcmp(squeezed[1], plain[1], sizeof(float[3][4])) == 0);
	/* ...and lands where its rails do: they place a point at stage x
	 * 320 + scaleX X / -Z, with scaleX from the unsqueezed projection. */
	for(float x = -2.5f; x <= 2.5f; x += 0.5f) {
		float z = -5.4f + x * 0.3f;
		float cubeClipX = squeezed[0][0] * x / -z;
		float railX = 320.0f + plain[0][0] * 320.0f * x / -z;
		CHECK(near(cubeClipX, orthoClipX(m, railX)));
	}

	/* And back to 4:3. */
	UIStage_SetWide(false);
	CHECK(UIStage_Left() == 0.0f && UIStage_FrameX(-50.0f) == -50.0f);

	printf("test_ui_stage: %lu checks passed\n", checks);
	return 0;
}
