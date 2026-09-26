#ifndef SAVES_H
#define SAVES_H

#include <stdbool.h>
#include <stddef.h>

/* Home > System > Memory Cards: the saves on the cards in Slot A and Slot B
 * and in folders on the settings device, with Copy, Move and Delete. */
void show_saves(void);

/* Settings > Storage > Save Folder: browse the settings device's folders and
 * choose one. Writes its path under the device's root ("swiss/saves", or "/"
 * for the root itself) to folder; false when cancelled or there's no device. */
bool saves_choose_folder(char *folder, size_t size);

/* The Save Folder in effect: the setting, or swiss/saves while it's unset. */
const char *saves_folder(void);

#endif
