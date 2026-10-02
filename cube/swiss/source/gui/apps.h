#ifndef APPS_H
#define APPS_H

#include <gccore.h>
#include <stdbool.h>

#include "deviceHandler.h"
#include "ui_gameflow.h"

/* Apps (Home > Apps): the programs in /apps on the source, and in the
 * folders in it, shown as the Library shows games. ui_apps.h says what
 * counts as an app. */

/* Whether device has an app: Home shows the Apps face while it does.
 * Reads /apps and, up to the first app, its folders. Menu thread. */
bool apps_available(DEVICEHANDLER_INTERFACE *device);

/* The Apps screen, until B; an app it starts never returns. Menu thread. */
void show_apps(void);

/* swiss.c's, so Apps browses as the Library does. */
uiGameflowLayout_t gameflowLayout(void);
u32 gameflowMenuInputPolicy(uiGameflowLayout_t layout);
u32 menuInputElapsedMicroseconds(u32 *lastRetrace);

#endif
