#ifndef MANUAL_H
#define MANUAL_H

#include <stdint.h>
#include "deviceHandler.h"

/* manual: a game's manual, read from the card (ui_manual.h has its rules,
 * docs/MANUALS.md its file). Game Details asks Manual_Pages for its Manual
 * row, and A on the row opens Manual_Show. Menu thread. */

/* The game's manual on the device the Library browses, in
 * swiss/ui/manuals/: <GAMEID>.pak, else the first four characters' .pak.
 * Its pages, 0 when there is none, -1 when its file can't be read. The
 * last game's answer is kept: Game Details asks on every publish. */
int32_t Manual_Pages(DEVICEHANDLER_INTERFACE *device, const char *gameId);
/* Forgets that answer, as the device changes. Safe in a reset callback. */
void Manual_Forget(void);
/* Shows the manual until B: at full width from page 1's top. Up and Down
 * scroll, Left and Right turn the page, Y shows the whole page, L and R go
 * to the first and last page. Says so, and returns, when it can't. */
void Manual_Show(DEVICEHANDLER_INTERFACE *device, const char *gameId);

#endif
