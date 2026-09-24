#include "ui_hint.h"

#include <string.h>

/* Longest names first, so "STICK / D-PAD" wins over "STICK" and "L/R" over
 * "L". "\213  \233" (the left and right arrows) is the D-pad's left/right.
 * "L+A" is a chord: hold L, press A. */
static const struct {
	const char *name;
	uiHintGlyph_t glyph[UI_HINT_MAX_GLYPHS];
	int glyphCount;
	int chord;
} hintNames[] = {
	{"STICK / D-PAD", {UI_HINT_GLYPH_STICK, UI_HINT_GLYPH_DPAD}, 2, 0},
	{"\213  \233", {UI_HINT_GLYPH_DPAD, UI_HINT_GLYPH_NONE}, 1, 0},
	{"D-PAD", {UI_HINT_GLYPH_DPAD, UI_HINT_GLYPH_NONE}, 1, 0},
	{"STICK", {UI_HINT_GLYPH_STICK, UI_HINT_GLYPH_NONE}, 1, 0},
	{"START", {UI_HINT_GLYPH_START, UI_HINT_GLYPH_NONE}, 1, 0},
	{"L/R", {UI_HINT_GLYPH_L, UI_HINT_GLYPH_R}, 2, 0},
	{"L+A", {UI_HINT_GLYPH_L, UI_HINT_GLYPH_A}, 2, 1},
	{"A", {UI_HINT_GLYPH_A, UI_HINT_GLYPH_NONE}, 1, 0},
	{"B", {UI_HINT_GLYPH_B, UI_HINT_GLYPH_NONE}, 1, 0},
	{"X", {UI_HINT_GLYPH_X, UI_HINT_GLYPH_NONE}, 1, 0},
	{"Y", {UI_HINT_GLYPH_Y, UI_HINT_GLYPH_NONE}, 1, 0},
	{"Z", {UI_HINT_GLYPH_Z, UI_HINT_GLYPH_NONE}, 1, 0},
	{"L", {UI_HINT_GLYPH_L, UI_HINT_GLYPH_NONE}, 1, 0},
	{"R", {UI_HINT_GLYPH_R, UI_HINT_GLYPH_NONE}, 1, 0},
};

/* Length of the separator starting at text, or 0 when there is none. */
static size_t separatorLength(const char *text)
{
	size_t spaces = 0;

	while(text[spaces] == ' ') {
		spaces++;
	}
	if(text[spaces] == '\267') {
		spaces++;
		while(text[spaces] == ' ') {
			spaces++;
		}
		return spaces;
	}
	return spaces >= 3 ? spaces : 0;
}

static void parseItem(const char *start, size_t length, uiHintItem_t *item)
{
	size_t i;

	item->glyph[0] = item->glyph[1] = UI_HINT_GLYPH_NONE;
	item->glyphCount = 0;
	item->chord = 0;
	item->label = start;
	item->labelLength = length;
	for(i = 0; i < sizeof(hintNames) / sizeof(hintNames[0]); i++) {
		size_t nameLength = strlen(hintNames[i].name);
		size_t rest;

		if(nameLength > length || memcmp(start, hintNames[i].name, nameLength) != 0) {
			continue;
		}
		/* The name is the whole item, or two spaces lead to its label. */
		if(nameLength != length &&
			(length - nameLength < 3 || memcmp(start + nameLength, "  ", 2) != 0 ||
			start[nameLength + 2] == ' ')) {
			continue;
		}
		rest = nameLength == length ? length : nameLength + 2;
		item->glyph[0] = hintNames[i].glyph[0];
		item->glyph[1] = hintNames[i].glyph[1];
		item->glyphCount = hintNames[i].glyphCount;
		item->chord = hintNames[i].chord;
		item->label = start + rest;
		item->labelLength = length - rest;
		return;
	}
}

int UIHint_Parse(const char *text, uiHintItem_t *items, int max)
{
	int count = 0;
	const char *cursor = text;

	if(text == NULL || items == NULL || max <= 0) {
		return 0;
	}
	while(*cursor == ' ') {
		cursor++;
	}
	while(*cursor != '\0' && count < max) {
		const char *end = cursor;
		size_t length;

		while(*end != '\0' && separatorLength(end) == 0) {
			end++;
		}
		length = (size_t)(end - cursor);
		while(length > 0 && cursor[length - 1] == ' ') {
			length--;
		}
		if(length > 0) {
			parseItem(cursor, length, &items[count++]);
		}
		cursor = end;
		if(*cursor != '\0') {
			cursor += separatorLength(cursor);
		}
	}
	return count;
}

float UIHint_GlyphSize(int fontHeight, float scale)
{
	return (float)fontHeight * scale * 1.1f;
}

float UIHint_GlyphWidth(uiHintGlyph_t glyph, float size, float scale,
	uiHintMeasureFn measure)
{
	switch(glyph) {
		case UI_HINT_GLYPH_NONE: return 0.0f;
		case UI_HINT_GLYPH_B: return size * 0.86f;
		case UI_HINT_GLYPH_X: return size * 0.74f;
		case UI_HINT_GLYPH_Y: return size * 1.18f;
		case UI_HINT_GLYPH_Z: return size * 1.22f;
		case UI_HINT_GLYPH_L:
		case UI_HINT_GLYPH_R: return size * 1.12f;
		case UI_HINT_GLYPH_START:
			return (float)measure("START") * scale * UI_HINT_START_TEXT_SCALE +
				size * 0.6f;
		default: return size;
	}
}

float UIHint_JoinWidth(const uiHintItem_t *item, float size, float scale,
	uiHintMeasureFn measure)
{
	return item->chord ?
		size * UI_HINT_ALTERNATIVE_GAP * 2.0f + (float)measure("+") * scale :
		size * UI_HINT_ALTERNATIVE_GAP;
}

float UIHint_ItemWidth(const uiHintItem_t *item, float size, float scale,
	uiHintMeasureFn measure, char *label, size_t capacity)
{
	size_t length = item->labelLength < capacity - 1u ? item->labelLength : capacity - 1u;
	float width = 0.0f;
	int i;

	memcpy(label, item->label, length);
	label[length] = '\0';
	for(i = 0; i < item->glyphCount; i++) {
		width += UIHint_GlyphWidth(item->glyph[i], size, scale, measure) +
			(i > 0 ? UIHint_JoinWidth(item, size, scale, measure) : 0.0f);
	}
	if(item->glyphCount > 0 && length > 0u) {
		width += size * UI_HINT_LABEL_GAP;
	}
	return width + (length > 0u ? (float)measure(label) * scale : 0.0f);
}

float UIHint_LineWidth(const char *text, int fontHeight, float scale,
	uiHintMeasureFn measure)
{
	uiHintItem_t items[UI_HINT_MAX_ITEMS];
	char label[UI_HINT_LABEL_CAPACITY];
	float size = UIHint_GlyphSize(fontHeight, scale);
	float width = 0.0f;
	int count = UIHint_Parse(text, items, UI_HINT_MAX_ITEMS);
	int i;

	for(i = 0; i < count; i++) {
		width += UIHint_ItemWidth(&items[i], size, scale, measure, label,
			sizeof(label)) + (i > 0 ? size * UI_HINT_ITEM_GAP : 0.0f);
	}
	return width;
}
