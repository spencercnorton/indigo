#ifndef UI_HINT_H
#define UI_HINT_H

#include <stddef.h>

/* The controller parts a hint line can show as an icon. */
typedef enum {
	UI_HINT_GLYPH_NONE = 0,
	UI_HINT_GLYPH_A,
	UI_HINT_GLYPH_B,
	UI_HINT_GLYPH_X,
	UI_HINT_GLYPH_Y,
	UI_HINT_GLYPH_Z,
	UI_HINT_GLYPH_L,
	UI_HINT_GLYPH_R,
	UI_HINT_GLYPH_START,
	UI_HINT_GLYPH_STICK,
	UI_HINT_GLYPH_DPAD,
	UI_HINT_GLYPH_COUNT
} uiHintGlyph_t;

#define UI_HINT_MAX_ITEMS 8
#define UI_HINT_MAX_GLYPHS 2
#define UI_HINT_LABEL_CAPACITY 96

/* Spacing, in icon heights: between alternative icons ("L/R"), from an
 * icon to its label, and between items. */
#define UI_HINT_ALTERNATIVE_GAP 0.2f
#define UI_HINT_LABEL_GAP 0.34f
#define UI_HINT_ITEM_GAP 0.9f
/* START's pill holds its name at this share of the line's scale. */
#define UI_HINT_START_TEXT_SCALE 0.86f

/* One control and what it does: "A  OPEN" is glyph A with label "OPEN".
 * Two glyphs are alternatives ("L/R", "STICK / D-PAD"), or a chord held
 * together ("L+A") when chord is set. An item without a glyph is plain
 * text. The label points into the parsed string. */
typedef struct {
	uiHintGlyph_t glyph[UI_HINT_MAX_GLYPHS];
	int glyphCount;
	int chord;
	const char *label;
	size_t labelLength;
} uiHintItem_t;

/* Splits a hint line into items, which are separated by three or more
 * spaces or by a middle dot ("\267"). A control name followed by two spaces
 * (or ending the item) becomes its glyph; anything else stays text. Returns
 * the number of items written, at most max. */
int UIHint_Parse(const char *text, uiHintItem_t *items, int max);

/* Width of text at scale 1, in pixels: GetTextSizeInPixels on the console. */
typedef int (*uiHintMeasureFn)(const char *text);

/* An icon's height for text at this scale, from the font's cell height at
 * scale 1. Icons grow with the text, so a line's width is linear in scale
 * and fitters can treat a hint like any other string. */
float UIHint_GlyphSize(int fontHeight, float scale);

/* One icon's width at that height; measure sizes START's name. */
float UIHint_GlyphWidth(uiHintGlyph_t glyph, float size, float scale,
	uiHintMeasureFn measure);

/* The space between an item's two icons: a small gap between alternatives,
 * or a "+" with that gap each side for a chord. */
float UIHint_JoinWidth(const uiHintItem_t *item, float size, float scale,
	uiHintMeasureFn measure);

/* One item's width. Its label is copied into label, cut to capacity and
 * terminated, which is the text the renderer draws. */
float UIHint_ItemWidth(const uiHintItem_t *item, float size, float scale,
	uiHintMeasureFn measure, char *label, size_t capacity);

/* A whole line's width at this scale. */
float UIHint_LineWidth(const char *text, int fontHeight, float scale,
	uiHintMeasureFn measure);

#endif
