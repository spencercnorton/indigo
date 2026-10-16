#ifndef INDIGO_PROBE_GRID_H
#define INDIGO_PROBE_GRID_H

#include <stdbool.h>

typedef struct {
	int x, y, cell_width, cell_height;
} probeGrid_t;

/* XFB pixels, before a capture scales them. In a 240-line mode the old
 * fixed 16-line rows wrote well beyond the framebuffer and hid the CRC. */
static inline bool ProbeGrid_Layout(int width, int height, int words, probeGrid_t *grid)
{
	if(grid == 0 || words <= 0 || width < 192 || height < 112 + words)
		return false;
	grid->x = 64;
	grid->y = 112;
	grid->cell_width = ((width - 128) / 32) & ~1;
	if(grid->cell_width > 16) grid->cell_width = 16;
	grid->cell_height = (height - grid->y) / words;
	if(grid->cell_height > 16) grid->cell_height = 16;
	return true;
}

#endif
