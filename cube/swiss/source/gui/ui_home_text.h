#ifndef UI_HOME_TEXT_H
#define UI_HOME_TEXT_H

#include <stdbool.h>
#include <stddef.h>

/* Essential Home copy remains readable at the native 640x480 source grid.
 * The IPL font maps its single ellipsis glyph to ANSI byte 0x85. */
#define UI_HOME_TEXT_SCALE_FLOOR 0.46f
#define UI_HOME_TEXT_ELLIPSIS_BYTE 0x85u

typedef int (*uiHomeTextMeasureFn)(const char *text);

/* Fits static, bounded copy. The returned scale is always at least the Home
 * readability floor; callers must reserve enough width for their static
 * labels at that floor. */
float UIHomeText_FitScale(const char *text, int maxWidth, float maxScale,
	uiHomeTextMeasureFn measure);

/* Copies source into destination and fits it with the supplied font metric.
 * If the full copy would require a scale below the readability floor, the
 * tail is replaced with the IPL ellipsis glyph until it fits at the floor.
 * No allocation is performed and the measure callback is never retained. */
float UIHomeText_CopyFitted(char *destination, size_t capacity,
	const char *source, int maxWidth, float maxScale,
	uiHomeTextMeasureFn measure, bool *ellipsized);

#endif
