/* 
 * Copyright (c) 2022, Extrems <extrems@extremscorner.org>
 * 
 * This file is part of Swiss.
 * 
 * Swiss is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Swiss is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * with Swiss.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef __GAMEID_H
#define __GAMEID_H

#include <gctypes.h>
#include "gcm.h"

void gameID_early_set(const DiskHeader *header);
/* The EXI channels whose MemCard PRO took the last gameID_early_set, a bit
 * each: that card changes to the game's own a moment later. */
u8 gameID_early_cards(void);
void gameID_set(const DiskHeader *header, u64 hash);
void gameID_unset(void);

#endif /* __GAMEID_H */
