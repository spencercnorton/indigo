#ifndef SAVES_H
#define SAVES_H

#include <stdbool.h>
#include <stddef.h>
#include "saves_stats.h"

/* Home > System > Memory Cards: the saves on the cards in Slot A and Slot B
 * and in folders on the settings device, with Move, Copy and Erase. */
void show_saves(void);

/* Settings > Storage > Save Folder: browse the settings device's folders and
 * choose one. Writes its path under the device's root ("swiss/saves", or "/"
 * for the root itself) to folder; false when cancelled or there's no device. */
bool saves_choose_folder(char *folder, size_t size);

/* The Save Folder in effect: the setting, or swiss/saves while it's unset. */
const char *saves_folder(void);

/* Before a launch from Game Details: puts copy, a save Saves_CollectGameStats
 * found, on the card in slot (0 Slot A, 1 Slot B) as the save the game reads
 * there. The card's own copy of that save (the same game, maker and name) is
 * written to the Save Folder first, then replaced; the new one is read back,
 * and anything that fails puts the card's own copy back. False says why. */
bool Saves_LoadCopy(int slot, const savesCopy_t *copy, char *why,
	size_t whySize);

#endif
