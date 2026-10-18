#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static unsigned roots;
static float countedSqrt(float value) { roots++; return sqrtf(value); }
#define sqrtf countedSqrt
#include "ui_hint_geometry.c"
#undef sqrtf

static void check(float cx, float cy, const float (*points)[2], int count)
{
	const float (*normal)[2] = UIHintGeometry_Normals(cx, cy, points, count);
	/* The old emitter's formula, including its radial fallback. */
	for(int i = 0; i < count; ++i) {
		const float *a = points[(i + count - 1) % count];
		const float *b = points[(i + 1) % count];
		float nx = b[1] - a[1], ny = a[0] - b[0];
		float length = sqrtf(nx * nx + ny * ny);
		if(length <= 0.0f) {
			nx = points[i][0] - cx; ny = points[i][1] - cy;
			length = sqrtf(nx * nx + ny * ny);
		}
		if(length > 0.0f) { nx /= length; ny /= length; }
		if(nx * (points[i][0] - cx) + ny * (points[i][1] - cy) < 0.0f) {
			nx = -nx; ny = -ny;
		}
		assert(memcmp(&nx, &normal[i][0], sizeof(nx)) == 0);
		assert(memcmp(&ny, &normal[i][1], sizeof(ny)) == 0);
	}
}

int main(void)
{
	float points[UI_HINT_GEOMETRY_POINTS][2];
	assert(UIHintGeometry_Normals(0, 0, NULL, 4) == NULL);
	assert(UIHintGeometry_Normals(0, 0, (const float (*)[2])points, 2) == NULL);
	assert(UIHintGeometry_Normals(0, 0, (const float (*)[2])points, UI_HINT_GEOMETRY_POINTS + 1) == NULL);
	for(int round = 0; round < 200; ++round) {
		int count = 3 + round % (UI_HINT_GEOMETRY_POINTS - 2);
		float cx = 211.125f + (float)round * 0.125f;
		float cy = 17.75f + (float)(round % 7);
		for(int i = 0; i < count; ++i) {
			float angle = 6.2831853f * (float)i / (float)count;
			points[i][0] = cx + (7.0f + (float)(round % 5)) * cosf(angle);
			points[i][1] = cy + 6.5f * sinf(angle);
		}
		check(cx, cy, (const float (*)[2])points, count);
		unsigned before = roots;
		for(int frame = 0; frame < 60; ++frame) check(cx, cy, (const float (*)[2])points, count);
		assert(roots == before);
		points[0][0] += 0.03125f;
		check(cx, cy, (const float (*)[2])points, count);
		assert(roots > before);
	}
	memset(points, 0, sizeof(points));
	check(0, 0, (const float (*)[2])points, 4);
	puts("hint normals: exact edges, changed geometry, bounded eviction, zero warm roots PASS");
	return 0;
}
