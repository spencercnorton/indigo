#ifndef SAVES_STATS_H
#define SAVES_STATS_H

#include "ui_saves_metadata.h"

/* A bounded, read-only scan while opening Game Detail, never while drawing.
 * Reads the configured Save Folder's direct save files and RAW images plus
 * available physical cards. A partial result is explicitly marked. */
void Saves_CollectGameStats(const char gameId[6], uiSavesGameStats_t *stats);

/* The same scan in its two parts. The folder part starts stats over; the
 * slot part adds the cards in the two slots to them. A MemCard PRO changes
 * its card a moment after the GameID reaches it, so Detail reads the slots
 * again on top of the folder part it kept. */
void Saves_CollectFolderStats(const char gameId[6], uiSavesGameStats_t *stats);
void Saves_CollectSlotStats(const char gameId[6], uiSavesGameStats_t *stats);

#endif
