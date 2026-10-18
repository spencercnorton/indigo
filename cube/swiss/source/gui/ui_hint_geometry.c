#include "ui_hint_geometry.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

/* Sixteen recent outlines: bounded BSS, never a frame-time allocation.
 * Keeping the original coordinates avoids changing the rounding of an
 * edge normal when an outline moves to a different subpixel position. */
typedef struct {
	float cx, cy;
	int count;
	float points[UI_HINT_GEOMETRY_POINTS][2];
	float normals[UI_HINT_GEOMETRY_POINTS][2];
} hintGeometry_t;

static hintGeometry_t cache[16];
static unsigned next;

const float (*UIHintGeometry_Normals(float cx, float cy,
	const float (*points)[2], int count))[2]
{
	hintGeometry_t *shape;
	size_t bytes;

	if(points == NULL || count < 3 || count > UI_HINT_GEOMETRY_POINTS) {
		return NULL;
	}
	bytes = (size_t)count * sizeof(points[0]);
	for(unsigned i = 0u; i < sizeof(cache) / sizeof(cache[0]); ++i) {
		shape = &cache[i];
		if(shape->count == count && shape->cx == cx && shape->cy == cy &&
			memcmp(shape->points, points, bytes) == 0) {
			return shape->normals;
		}
	}
	shape = &cache[next];
	next = (next + 1u) % (sizeof(cache) / sizeof(cache[0]));
	shape->cx = cx;
	shape->cy = cy;
	shape->count = count;
	memcpy(shape->points, points, bytes);
	for(int i = 0; i < count; ++i) {
		const float *prev = points[(i + count - 1) % count];
		const float *here = points[i];
		const float *after = points[(i + 1) % count];
		float nx = after[1] - prev[1];
		float ny = prev[0] - after[0];
		float length = sqrtf(nx * nx + ny * ny);

		if(length <= 0.0f) {
			nx = here[0] - cx;
			ny = here[1] - cy;
			length = sqrtf(nx * nx + ny * ny);
		}
		if(length > 0.0f) {
			nx /= length;
			ny /= length;
		}
		if(nx * (here[0] - cx) + ny * (here[1] - cy) < 0.0f) {
			nx = -nx;
			ny = -ny;
		}
		shape->normals[i][0] = nx;
		shape->normals[i][1] = ny;
	}
	return shape->normals;
}
