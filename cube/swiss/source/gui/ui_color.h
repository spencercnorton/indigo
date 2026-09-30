#ifndef UI_COLOR_H
#define UI_COLOR_H

#include <stdint.h>

/*
 * ui_color -- Menu Color. Indigo's palette is one family of blue-violet
 * around the GameCube's own indigo. A menu color turns that family's hue
 * and scales its saturation. Luma stays as it is, so contrast, glow and the
 * glass cube's depth are the same in every color.
 *
 * Colors outside the family keep their own: neutrals (white text, the
 * white that draws posters and banners, grays, black) and the colors that
 * mean something (red warnings, teal enabled cheats, the legacy green tick,
 * yellow star and orange progress bar).
 *
 * Pure: no GX and no settings. The video thread selects the color once a
 * frame, and each emitter recolors a vertex color just before GX gets it,
 * so retained draw objects follow a change on the next frame.
 */

/* Select the color UIColor_Apply uses, a UI_COLOR_ value from swiss.h.
 * Anything out of range is Indigo. */
void UIColor_Select(int color);

/* Recolor one color in place for the selected color. Indigo leaves every
 * color exactly as designed. */
void UIColor_Apply(uint8_t *r, uint8_t *g, uint8_t *b);

/* What Settings colors on its own: the menus (the cube, its light, the
 * panels and the text), the backdrop behind the cube with its rings, and
 * the waves in front of the backdrop. */
enum {
	UI_COLOR_LAYER_MENU,
	UI_COLOR_LAYER_BACKDROP,
	UI_COLOR_LAYER_WAVES,
	UI_COLOR_LAYERS
};

/* The UI_COLOR_ value a backdrop or wave setting shows: 0 follows the menu
 * color, anything else is 1 + a UI_COLOR_ value. */
int UIColor_Layer(int setting, int menuColor);

#endif
