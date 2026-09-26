#include <string.h>

#include "ui_home_layout.h"

#define HOME_CENTER_X 320
#define HOME_TITLE_Y 68
#define HOME_SELECTED_Y 385

static const uiHomeLayoutRect_t titleBounds = {105, 62, 535, 74};
static const uiHomeLayoutRect_t selectedLabelBounds = {180, 375, 460, 395};
static const uiHomeLayoutRect_t commandGlyphBounds = {28, 428, 612, 438};
static const uiHomeLayoutRect_t cubeKeepOut = {
	UI_HOME_LAYOUT_CUBE_KEEP_OUT_LEFT,
	UI_HOME_LAYOUT_CUBE_KEEP_OUT_TOP,
	UI_HOME_LAYOUT_CUBE_KEEP_OUT_RIGHT,
	UI_HOME_LAYOUT_CUBE_KEEP_OUT_BOTTOM
};

static uiHomeLayoutPoint_t point(int x, int y)
{
	uiHomeLayoutPoint_t value;

	value.x = x;
	value.y = y;
	return value;
}

static uiHomeLayoutRect_t rect(int left, int top, int right, int bottom)
{
	uiHomeLayoutRect_t value;

	value.left = left;
	value.top = top;
	value.right = right;
	value.bottom = bottom;
	return value;
}

static uiHomeLayoutRect_t expand(uiHomeLayoutRect_t value, int amount)
{
	return rect(value.left - amount, value.top - amount,
		value.right + amount, value.bottom + amount);
}

static bool pointInside(uiHomeLayoutPoint_t value, uiHomeLayoutRect_t bounds)
{
	return value.x >= bounds.left && value.x <= bounds.right &&
		value.y >= bounds.top && value.y <= bounds.bottom;
}

bool UIHomeLayout_RectIsValid(uiHomeLayoutRect_t value)
{
	return value.left <= value.right && value.top <= value.bottom;
}

bool UIHomeLayout_RectIsSafe(uiHomeLayoutRect_t value)
{
	return UIHomeLayout_RectIsValid(value) &&
		value.left >= UI_HOME_LAYOUT_SAFE_LEFT &&
		value.top >= UI_HOME_LAYOUT_SAFE_TOP &&
		value.right <= UI_HOME_LAYOUT_SAFE_RIGHT &&
		value.bottom <= UI_HOME_LAYOUT_SAFE_BOTTOM;
}

bool UIHomeLayout_RectsDisjoint(uiHomeLayoutRect_t first,
	uiHomeLayoutRect_t second)
{
	if(!UIHomeLayout_RectIsValid(first) ||
		!UIHomeLayout_RectIsValid(second)) {
		return false;
	}
	return first.right < second.left || second.right < first.left ||
		first.bottom < second.top || second.bottom < first.top;
}

static void setItem(uiHomeLayoutItem_t *item, uiHomeLayoutRect_t panel,
	uiHomeLayoutRect_t label, int centerX, int centerY, bool selected)
{
	item->panelBounds = panel;
	item->glowBounds = expand(panel, UI_HOME_LAYOUT_GLOW_EXTENT);
	item->labelCenter = point(centerX, centerY);
	item->labelBounds = label;
	item->selected = selected;
}

static bool stateIsValid(const uiHomeState_t *state,
	uiHomeCapabilities_t capabilities)
{
	int count;

	if(state == NULL || !UIHome_IsFace((int)state->face) ||
		!UIHome_IsSurface((int)state->surface)) {
		return false;
	}
	count = UIHome_RowCount(state->surface, capabilities);
	if(state->surface == UI_HOME_SURFACE_RING) {
		return state->selection == 0;
	}
	return count > 0 && state->selection >= 0 && state->selection < count;
}

static void computeRing(uiHomeLayout_t *out)
{
	out->ringLabelsVisible = true;
	out->selectedLabelCenter = point(HOME_CENTER_X, HOME_SELECTED_Y);
	out->selectedLabelBounds = selectedLabelBounds;
}

static void computeRows(const uiHomeState_t *state,
	uiHomeCapabilities_t capabilities, uiHomeLayout_t *out)
{
	int count = UIHome_RowCount(state->surface, capabilities);

	out->rowCount = count;
	if(count == 1) {
		setItem(&out->rows[0], rect(142, 386, 498, 406),
			rect(160, 389, 480, 403), HOME_CENTER_X, 396,
			state->selection == 0);
		return;
	}
	/* Three rows share the band between the cube's rail and the command
	 * line at a 19-pixel pitch: 12-pixel panels, the glows a pixel apart. */
	if(count == 3) {
		int row;

		for(row = 0; row < 3; ++row) {
			int top = 372 + row * 19;

			setItem(&out->rows[row], rect(142, top, 498, top + 12),
				rect(160, top + 2, 480, top + 10), HOME_CENTER_X, top + 6,
				state->selection == row);
		}
		return;
	}
	setItem(&out->rows[0], rect(142, 372, 498, 388),
		rect(160, 374, 480, 386), HOME_CENTER_X, 380,
		state->selection == 0);
	setItem(&out->rows[1], rect(142, 400, 498, 416),
		rect(160, 402, 480, 414), HOME_CENTER_X, 408,
		state->selection == 1);
}

static void computeOptions(const uiHomeState_t *state, uiHomeLayout_t *out)
{
	out->optionCount = UI_HOME_LAYOUT_MAX_OPTIONS;
	out->modalBounds = rect(112, 337, 528, 419);
	out->consequenceCenter = point(HOME_CENTER_X, 355);
	out->consequenceBounds = rect(148, 344, 492, 365);
	setItem(&out->options[0], rect(142, 381, 314, 411),
		rect(160, 389, 296, 403), 228, 396,
		state->selection == 0);
	setItem(&out->options[1], rect(326, 381, 498, 411),
		rect(344, 389, 480, 403), 412, 396,
		state->selection == 1);
}

bool UIHomeLayout_Compute(const uiHomeState_t *state,
	uiHomeCapabilities_t capabilities, uiHomeLayout_t *out)
{
	if(out == NULL) {
		return false;
	}
	memset(out, 0, sizeof(*out));
	if(!stateIsValid(state, capabilities)) {
		return false;
	}

	out->surface = state->surface;
	out->face = state->face;
	out->selection = state->selection;
	out->hasSource = capabilities.hasSource;
	out->hasRecent = capabilities.hasRecent;
	out->titleCenter = point(HOME_CENTER_X, HOME_TITLE_Y);
	out->titleBounds = titleBounds;
	out->commandCenter = point(UI_HOME_LAYOUT_COMMAND_X,
		UI_HOME_LAYOUT_COMMAND_Y);
	out->commandGlyphBounds = commandGlyphBounds;

	switch(state->surface) {
	case UI_HOME_SURFACE_RING:
		computeRing(out);
		break;
	case UI_HOME_SURFACE_SOURCE:
	case UI_HOME_SURFACE_SYSTEM:
		computeRows(state, capabilities, out);
		break;
	case UI_HOME_SURFACE_RESTART_CONFIRM:
		computeOptions(state, out);
		break;
	case UI_HOME_SURFACE_COUNT:
	default:
		return false;
	}
	return UIHomeLayout_Validate(out);
}

static bool itemIsValid(const uiHomeLayoutItem_t *item)
{
	uiHomeLayoutRect_t expectedGlow;

	if(item == NULL || !UIHomeLayout_RectIsSafe(item->panelBounds) ||
		!UIHomeLayout_RectIsSafe(item->glowBounds) ||
		!UIHomeLayout_RectIsSafe(item->labelBounds)) {
		return false;
	}
	/* Expand only after the safe-area guard, so adversarial INT_MIN/INT_MAX
	 * snapshots cannot overflow the validation arithmetic. */
	expectedGlow = expand(item->panelBounds, UI_HOME_LAYOUT_GLOW_EXTENT);
	return item->glowBounds.left == expectedGlow.left &&
		item->glowBounds.top == expectedGlow.top &&
		item->glowBounds.right == expectedGlow.right &&
		item->glowBounds.bottom == expectedGlow.bottom &&
		item->glowBounds.top > UI_HOME_LAYOUT_CUBE_RAIL_BOTTOM &&
		pointInside(item->labelCenter, item->labelBounds);
}

static bool itemSetIsValid(const uiHomeLayoutItem_t *items, int count,
	int selectedIndex, uiHomeLayoutRect_t command)
{
	int first;
	int second;

	if(count < 0 || count > UI_HOME_LAYOUT_MAX_ROWS) {
		return false;
	}
	if((count == 0 && selectedIndex != -1) ||
		(count > 0 && (selectedIndex < 0 || selectedIndex >= count))) {
		return false;
	}
	for(first = 0; first < count; ++first) {
		if(!itemIsValid(&items[first]) ||
			!UIHomeLayout_RectsDisjoint(items[first].glowBounds, command) ||
			items[first].selected != (first == selectedIndex)) {
			return false;
		}
		for(second = first + 1; second < count; ++second) {
			if(!UIHomeLayout_RectsDisjoint(items[first].glowBounds,
				items[second].glowBounds)) {
				return false;
			}
		}
	}
	return true;
}

bool UIHomeLayout_Validate(const uiHomeLayout_t *layout)
{
	int expectedRows;
	int expectedOptions;
	uiHomeCapabilities_t capabilities;
	uiHomeLayoutRect_t selectedTravelBounds;

	if(layout == NULL || !UIHome_IsFace((int)layout->face) ||
		!UIHome_IsSurface((int)layout->surface) ||
		!UIHomeLayout_RectIsSafe(layout->titleBounds) ||
		!pointInside(layout->titleCenter, layout->titleBounds) ||
		layout->commandCenter.x != UI_HOME_LAYOUT_COMMAND_X ||
		layout->commandCenter.y != UI_HOME_LAYOUT_COMMAND_Y ||
		layout->commandGlyphBounds.top !=
			UI_HOME_LAYOUT_COMMAND_Y - UI_HOME_LAYOUT_COMMAND_HALF_HEIGHT ||
		layout->commandGlyphBounds.bottom !=
			UI_HOME_LAYOUT_COMMAND_Y + UI_HOME_LAYOUT_COMMAND_HALF_HEIGHT ||
		!UIHomeLayout_RectIsSafe(layout->commandGlyphBounds) ||
		!pointInside(layout->commandCenter, layout->commandGlyphBounds)) {
		return false;
	}

	capabilities.hasSource = layout->hasSource;
	capabilities.hasRecent = layout->hasRecent;
	expectedRows = layout->surface == UI_HOME_SURFACE_SOURCE ||
		layout->surface == UI_HOME_SURFACE_SYSTEM ?
		UIHome_RowCount(layout->surface, capabilities) : 0;
	expectedOptions = layout->surface == UI_HOME_SURFACE_RESTART_CONFIRM ?
		UI_HOME_LAYOUT_MAX_OPTIONS : 0;
	if(layout->rowCount != expectedRows ||
		layout->optionCount != expectedOptions ||
		!itemSetIsValid(layout->rows, layout->rowCount,
			layout->rowCount > 0 ? layout->selection : -1,
			layout->commandGlyphBounds) ||
		!itemSetIsValid(layout->options, layout->optionCount,
			layout->optionCount > 0 ? layout->selection : -1,
			layout->commandGlyphBounds)) {
		return false;
	}

	if(layout->surface == UI_HOME_SURFACE_RING) {
		/* Validate before expanding: a hostile snapshot can carry INT_MIN or
		 * INT_MAX, and signed overflow here would make validation itself UB. */
		if(!UIHomeLayout_RectIsSafe(layout->selectedLabelBounds)) {
			return false;
		}
		selectedTravelBounds = rect(
			layout->selectedLabelBounds.left -
				UI_HOME_LAYOUT_SELECTED_TRAVEL,
			layout->selectedLabelBounds.top - UI_HOME_LAYOUT_SELECTED_VERTICAL_TRAVEL,
			layout->selectedLabelBounds.right +
				UI_HOME_LAYOUT_SELECTED_TRAVEL,
			layout->selectedLabelBounds.bottom + UI_HOME_LAYOUT_SELECTED_VERTICAL_TRAVEL);
		if(!layout->ringLabelsVisible || layout->rowCount != 0 ||
			layout->optionCount != 0 || layout->selection != 0 ||
			!pointInside(layout->selectedLabelCenter,
				layout->selectedLabelBounds) ||
			layout->selectedLabelBounds.top <=
				UI_HOME_LAYOUT_CUBE_RAIL_BOTTOM ||
			!UIHomeLayout_RectIsSafe(cubeKeepOut) ||
			!UIHomeLayout_RectIsSafe(selectedTravelBounds) ||
			!UIHomeLayout_RectsDisjoint(layout->titleBounds, cubeKeepOut) ||
			!UIHomeLayout_RectsDisjoint(selectedTravelBounds, cubeKeepOut) ||
			!UIHomeLayout_RectsDisjoint(layout->selectedLabelBounds,
				layout->commandGlyphBounds)) {
			return false;
		}
		return true;
	}

	if(layout->ringLabelsVisible) {
		return false;
	}
	if(layout->surface == UI_HOME_SURFACE_RESTART_CONFIRM) {
		return layout->rowCount == 0 &&
			layout->optionCount == UI_HOME_LAYOUT_MAX_OPTIONS &&
			layout->selection >= 0 &&
			layout->selection < layout->optionCount &&
			UIHomeLayout_RectIsSafe(layout->modalBounds) &&
			UIHomeLayout_RectIsSafe(layout->consequenceBounds) &&
			pointInside(layout->consequenceCenter,
				layout->consequenceBounds) &&
			layout->modalBounds.left <=
				layout->consequenceBounds.left &&
			layout->modalBounds.right >=
				layout->consequenceBounds.right &&
			layout->modalBounds.left <=
				layout->options[0].glowBounds.left &&
			layout->modalBounds.right >=
				layout->options[1].glowBounds.right &&
			layout->modalBounds.top <= layout->consequenceBounds.top &&
			layout->modalBounds.bottom >=
				layout->consequenceBounds.bottom &&
			layout->modalBounds.bottom >=
				layout->options[0].glowBounds.bottom &&
			UIHomeLayout_RectsDisjoint(layout->consequenceBounds,
				layout->options[0].glowBounds) &&
			UIHomeLayout_RectsDisjoint(layout->consequenceBounds,
				layout->options[1].glowBounds) &&
			UIHomeLayout_RectsDisjoint(layout->modalBounds,
				layout->commandGlyphBounds);
	}
	return layout->optionCount == 0 && layout->rowCount > 0 &&
		layout->selection >= 0 && layout->selection < layout->rowCount;
}
