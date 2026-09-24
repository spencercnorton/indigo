#include <math.h>

#include "ui_color.h"

/* Each color turns Indigo's hue (degrees in the YCbCr chroma plane) and
 * scales its saturation. Same order as swiss.h's UI_COLOR_ values and
 * settings.c's uiColorStr: Right walks round the color wheel, then to
 * Jet Black. */
static const struct {
	float turn;
	float saturation;
} uiColors[] = {
	{   0.0f, 1.00f },	/* Indigo, as designed */
	{ -38.0f, 1.00f },	/* Azure */
	{ -95.0f, 0.95f },	/* Emerald */
	{ 154.0f, 0.80f },	/* Gold */
	{ 130.0f, 1.00f },	/* Spice */
	{ 104.0f, 0.95f },	/* Crimson */
	{  80.0f, 0.90f },	/* Rose */
	{   0.0f, 0.00f },	/* Jet Black */
};

#define UI_COLOR_COUNT ((int)(sizeof(uiColors) / sizeof(uiColors[0])))

static int selected;	/* 0: Indigo */
/* Q12 matrix over the chroma pair (B - Y, R - Y). */
static int32_t chroma[2][2];

void UIColor_Select(int color)
{
	float turn, cosine, sine;

	if(color < 0 || color >= UI_COLOR_COUNT) {
		color = 0;
	}
	if(color == selected) {
		return;
	}
	selected = color;
	turn = uiColors[color].turn * (3.14159265f / 180.0f);
	cosine = uiColors[color].saturation * cosf(turn);
	sine = uiColors[color].saturation * sinf(turn);
	/* The turn is in YCbCr, where Cb = (B - Y) / 1.772 and Cr = (R - Y) /
	 * 1.402, so the cross terms carry the ratio of the two scales. */
	chroma[0][0] = (int32_t)lroundf(cosine * 4096.0f);
	chroma[0][1] = (int32_t)lroundf(-sine * (1.772f / 1.402f) * 4096.0f);
	chroma[1][0] = (int32_t)lroundf(sine * (1.402f / 1.772f) * 4096.0f);
	chroma[1][1] = chroma[0][0];
}

static uint8_t clampChannel(int32_t value)
{
	return (uint8_t)(value < 0 ? 0 : (value > 255 ? 255 : value));
}

void UIColor_Apply(uint8_t *r, uint8_t *g, uint8_t *b)
{
	int32_t y, u, v, turnedU, turnedV;

	if(selected == 0) {
		return;
	}
	y = (77 * *r + 150 * *g + 29 * *b + 128) >> 8;
	u = *b - y;
	v = *r - y;
	/* Indigo's family: blue-violet chroma between 51 degrees below and 49
	 * above the indigo axis (u > 0, v = 0). Neutrals have u == 0; the
	 * cheats' teal sits below 59 degrees and warning red above 84. */
	if(u <= 0 || 4 * v < -5 * u || 6 * v > 7 * u) {
		return;
	}
	turnedU = (chroma[0][0] * u + chroma[0][1] * v + 2048) >> 12;
	turnedV = (chroma[1][0] * u + chroma[1][1] * v + 2048) >> 12;
	/* Y is unchanged: G comes back from the same 77/150/29 luma weights. */
	*r = clampChannel(y + turnedV);
	*b = clampChannel(y + turnedU);
	*g = clampChannel(y - ((526 * turnedV + 198 * turnedU + 512) >> 10));
}
