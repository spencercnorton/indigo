#ifndef UI_COMMAND_RAIL_H
#define UI_COMMAND_RAIL_H

typedef enum {
	UI_COMMAND_RAIL_NONE = 0,
	UI_COMMAND_RAIL_LIBRARY,
	UI_COMMAND_RAIL_DETAIL
} uiCommandRailOwner_t;

typedef struct {
	uiCommandRailOwner_t owner;
	float alpha;
} uiCommandRailFrame_t;

/* Stages the shared Library/Detail command lane through an intentional quiet
 * handoff. At no progress value can both scenes own the rail. */
void UICommandRail_Gameflow(float detailProgress, uiCommandRailFrame_t *frame);

#endif
