#ifndef SAVES_STATS_H
#define SAVES_STATS_H

#include "ui_saves_metadata.h"

/* A bounded, read-only scan while opening Game Detail, never while drawing.
 * Reads the configured Save Folder's direct save files and RAW images plus
 * available physical cards. A partial result is explicitly marked. */
/* slots: the cards it may read (bit 0 Slot A, bit 1 Slot B); a card still
 * switching to the game is left alone. */
void Saves_CollectGameStats(const char gameId[6], uiSavesGameStats_t *stats,
	unsigned slots);

#endif
