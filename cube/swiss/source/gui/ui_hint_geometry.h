#ifndef UI_HINT_GEOMETRY_H
#define UI_HINT_GEOMETRY_H

#define UI_HINT_GEOMETRY_POINTS 40

/* Video-thread-only cache of exact outward unit normals. The returned
 * array belongs to the cache and is used before the next call. Geometry
 * must match byte for byte; moving or resizing an outline recomputes it.
 * No allocation or approximate square roots. NULL for an invalid count. */
const float (*UIHintGeometry_Normals(float cx, float cy,
	const float (*points)[2], int count))[2];

#endif
