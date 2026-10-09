#!/usr/bin/env python3
"""Game Detail's save choice, run from swiss.c itself: which copy Left and
Right step to, when there is a choice at all, and what Launch does with it.

gameflowSaveSlot, gameflowSaveChoosable, gameflowSaveWhere and
gameflowLoadChosenSave are extracted unchanged, with the Left/Right step from
the Detail controller; the boxes answer as told and Saves_LoadCopy only
counts. A Detail with no card, or with Emulate Memory Card on, must never be
left unable to launch (2.4 vetting, S1)."""

import argparse
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from test_cheats_gx_stream import extract_function

ROOT = Path(__file__).resolve().parents[3]
GUI = ROOT / "cube/swiss/source/gui"
SWISS = (ROOT / "cube/swiss/source/swiss.c").read_text()

HARNESS = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "saves_stats.h"
typedef uint8_t u8;
typedef uint32_t u32;
typedef int uiDrawObj_t;
static struct { int emulateMemoryCard; } swissSettings;
typedef struct {
	bool savesScanned; uiSavesGameStats_t saveStats; int saveSlot; int saveChoice;
} gameflowLaunchContext_t;
static savesCopies_t gameflowSaveCopies;
static bool answer = true;
static int asks, loads;
static bool gameflowSaveAsk(const char *text) { (void)text; ++asks; return answer; }
static uiDrawObj_t *DrawProgressBar(bool a, int b, const char *c) { (void)a; (void)b; (void)c; return NULL; }
static uiDrawObj_t *DrawPublish(uiDrawObj_t *o) { return o; }
static void DrawDispose(uiDrawObj_t *o) { (void)o; }
static char *getRelativeName(char *p) { char *s = strrchr(p, '/'); return s ? s + 1 : p; }
static bool Saves_LoadCopy(int slot, const savesCopy_t *copy, char *why, size_t n)
{
	(void)slot; (void)copy; (void)why; (void)n; ++loads; return true;
}
'''

MAIN = r'''
static void press(gameflowLaunchContext_t *context, bool right)
{
	u32 buttons = right ? 2u : 1u;
	enum { BUTTON_LEFT = 1u, BUTTON_RIGHT = 2u };
	STEP
}
/* Copies: two in the Save Folder, then one on the card in slot when slot >= 0. */
static void detail(gameflowLaunchContext_t *context, int emulate, bool card, int slot)
{
	memset(&gameflowSaveCopies, 0, sizeof(gameflowSaveCopies));
	if(slot >= 0) gameflowSaveCopies.copy[gameflowSaveCopies.count++].source = (savesCopySource_t)slot;
	gameflowSaveCopies.copy[gameflowSaveCopies.count].source = SAVES_COPY_FILE;
	strcpy(gameflowSaveCopies.copy[gameflowSaveCopies.count++].path, "sd:/swiss/saves/a.gci");
	gameflowSaveCopies.copy[gameflowSaveCopies.count].source = SAVES_COPY_IMAGE;
	strcpy(gameflowSaveCopies.copy[gameflowSaveCopies.count++].path, "sd:/swiss/saves/b.raw");
	gameflowSaveCopies.cards[0] = card;
	swissSettings.emulateMemoryCard = emulate;
	memset(context, 0, sizeof(*context));
	context->savesScanned = true;
	context->saveSlot = gameflowSaveSlot(&gameflowSaveCopies);
	context->saveChoice = -1;	/* the totals first, as gameflowPublishDetail opens */
	asks = loads = 0;
	answer = true;
}
int main(void)
{
	gameflowLaunchContext_t c;
	int i;

	/* No card: no choice. Left and Right change nothing, Launch starts the
	 * game without asking (it used to refuse, every time). */
	detail(&c, 0, false, -1);
	assert(c.saveSlot < 0 && !gameflowSaveChoosable(&c));
	press(&c, true); press(&c, false);
	assert(c.saveChoice == -1);
	assert(gameflowLoadChosenSave(&c) && asks == 0 && loads == 0);

	/* Emulate Memory Card on, a card in Slot A holding a copy: the game reads
	 * its card image, so no choice either. */
	detail(&c, 1, true, SAVES_COPY_SLOT_A);
	assert(c.saveSlot == 0 && !gameflowSaveChoosable(&c));
	press(&c, true);
	assert(c.saveChoice == -1 && gameflowLoadChosenSave(&c) && asks == 0);

	/* A card in Slot A holding a copy: each copy, then the totals again. */
	detail(&c, 0, true, SAVES_COPY_SLOT_A);
	assert(gameflowSaveChoosable(&c));
	press(&c, true); assert(c.saveChoice == 0);
	press(&c, true); assert(c.saveChoice == 1);
	press(&c, true); assert(c.saveChoice == 2);
	press(&c, true); assert(c.saveChoice == -1);
	press(&c, false); assert(c.saveChoice == 2);
	press(&c, false); press(&c, false); press(&c, false); assert(c.saveChoice == -1);
	/* The card's own copy launches as it is. */
	c.saveChoice = 0;
	assert(gameflowLoadChosenSave(&c) && asks == 0 && loads == 0);
	/* Another copy asks first; B keeps the card's copy and goes back to the
	 * totals, so the next Launch starts the game as it is. */
	c.saveChoice = 1; answer = false;
	assert(!gameflowLoadChosenSave(&c) && asks == 1 && loads == 0 && c.saveChoice == -1);
	assert(gameflowLoadChosenSave(&c) && asks == 1 && loads == 0);
	/* A puts it on the card. */
	c.saveChoice = 2; answer = true;
	assert(gameflowLoadChosenSave(&c) && asks == 2 && loads == 1);

	/* A card without the game's save: a copy goes onto it, and a choice can
	 * always be undone (the totals are one of the steps). */
	detail(&c, 0, true, -1);
	assert(c.saveSlot == 0 && gameflowSaveChoosable(&c));
	for(i = 0; i < 3; i++) press(&c, true);
	assert(c.saveChoice == -1);
	assert(gameflowLoadChosenSave(&c) && asks == 0 && loads == 0);

	/* Fewer than two copies: no choice. */
	detail(&c, 0, true, SAVES_COPY_SLOT_A);
	gameflowSaveCopies.count = 1u;
	assert(!gameflowSaveChoosable(&c));
	press(&c, true); assert(c.saveChoice == -1);

	puts("save choice: none without a card or with Emulate Memory Card on, each copy then the totals, B keeps the card's PASS");
	return 0;
}
'''


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    controller = SWISS[SWISS.index("\t\t\t/* Left and Right choose the save copy to start with, while there\n"):]
    step = controller[controller.index("\t\t\t\tif(gameflowSaveChoosable(context)) {"):]
    step = step[:step.index("\t\t\t\t\tgameflowPublishDetail(config, context);")]
    step += "\t\t\t\t}\n"
    # The controller's own lines, with the redraw and blip left out.
    assert step.count("context->saveChoice = (context->saveChoice + 1 + step + span) %") == 1
    source = HARNESS + "\n".join(extract_function(SWISS, marker) for marker in (
        "static int gameflowSaveSlot(const savesCopies_t *copies)",
        "static bool gameflowSaveChoosable(const gameflowLaunchContext_t *context)",
        "static const char *gameflowSaveWhere(const savesCopy_t *copy)",
        "static bool gameflowLoadChosenSave(gameflowLaunchContext_t *context)",
    )) + MAIN.replace("STEP", step)
    with tempfile.TemporaryDirectory(prefix="save-choice-") as tmp:
        path = Path(tmp)
        (path / "test.c").write_text(source)
        command = [os.environ.get("CC", "cc"), "-std=c11", "-D_POSIX_C_SOURCE=200112L",
                   "-Wall", "-Wextra", "-Werror", "-Wno-unused-function", "-g", "-I", str(GUI),
                   str(path / "test.c"), "-o", str(path / "test")]
        if args.sanitize:
            command += ["-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                        "-fno-omit-frame-pointer"]
            if sys.platform.startswith("linux"):
                command += ["-fno-pie", "-no-pie"]
        subprocess.run(command, check=True)
        subprocess.run([str(path / "test")], check=True)


if __name__ == "__main__":
    main()
