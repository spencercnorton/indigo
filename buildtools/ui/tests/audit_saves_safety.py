#!/usr/bin/env python3
"""Structural and mutation gate for Memory Cards' data safety (saves.c).

The copy engine runs only against real card and FAT drivers, so these pin
the order that keeps a save from being lost: a Move removes the original
only after the copy was written and read back the same; a card that already
has the save is never written (the driver would write over it in place, at
any size); a folder copy never takes a name that exists; and the card
driver's .gci modes are switched off again after each use.

The art loader reads saves too, while the page is up: its reads switch the
.gci mode off again, a slot is named only once its texels are flushed, a
save that failed isn't read again, nothing is read until input has been
quiet, the list of saves on screen goes with the listing it points into,
and the pool outlives the page that draws from it. The cube screen lists
the saves of both stacks before it names a slot, and publishes only then;
B goes up a folder the SD card's stack opened, never past where it opened
or the card's root, and L or R looks again at a slot without a card before
swapping a stack.
"""

from __future__ import annotations

from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
SAVES = ROOT / "cube/swiss/source/gui/saves.c"


def function(source: str, name: str) -> str:
    start = re.search(r"^(?:static )?[a-z_][\w \*]*\b" + name + r"\(",
                      source, re.M)
    if start is None:
        raise AssertionError(f"{name} is missing")
    brace = source.index("{", start.end())
    depth = 0
    for index in range(brace, len(source)):
        depth += {"{": 1, "}": -1}.get(source[index], 0)
        if depth == 0:
            return source[start.start():index + 1]
    raise AssertionError(f"{name} never closes")


def ordered(text: str, *needles: str) -> None:
    position = -1
    for needle in needles:
        found = text.find(needle, position + 1)
        if found < 0:
            raise AssertionError(f"missing or out of order: {needle}")
        position = found


def check(source: str) -> None:
    transfer = function(source, "saveTransfer")
    card = function(source, "cardWrite")
    folder = function(source, "folderWrite")
    read = function(source, "saveRead")

    # A Move removes its original only once the copy is done and checked.
    ordered(transfer, "ok = cardWrite(", "ok = folderWrite(",
            "if(ok && move && !saveDelete(save))")
    if transfer.count("saveDelete(") != 1:
        raise AssertionError("saveTransfer removes a save outside its one guard")
    # The same-card folder Move is a rename, never a copy and delete.
    ordered(transfer, "renameFile(save, dest.name)", "return;",
            "saveRead(save, &length)")

    # A card that has the save already, or lacks the room, is never written.
    ordered(card, "has = cardFind(", "if(has) {", "return false;",
            "count - 1 >= CARD_MAXFILES", "return false;",
            "UISaves_Blocks(entry) > total - used", "return false;",
            "setGCIInfo(entry);", "device->writeFile(", "setGCIInfo(NULL);")
    # ...what a failed write left is removed, and what it wrote is read
    # back and compared, or removed again.
    ordered(card, "device->writeFile(", "if(written != (s32)blockBytes)",
            "device->deleteFile(copy)", "return false;",
            "saveRead(copy, &backLength)",
            "!memcmp(back + UI_SAVES_ENTRY_SIZE, blocks, blockBytes)",
            "if(!same)", "device->deleteFile(copy)", "return same;")

    # A folder copy looks for a free name before it creates the file.
    ordered(folder, "device->statFile(&dest) != 0", "break;",
            "device->writeFile(&dest, data, length)",
            "!memcmp(back, data, length)", "if(!same)",
            "device->deleteFile(&dest)")

    # The card driver's .gci read mode is on only around the read.
    ordered(read, "setCopyGCIMode(true);", "readFile(save, data, want)",
            "setCopyGCIMode(false);")

    art = function(source, "artRead")
    load = function(source, "artLoad")
    clear = function(source, "placeClear")
    show = function(source, "show_saves")
    wait = function(source, "inputNext")
    # Both art reads, the entry's and the art's, switch the .gci mode off.
    ordered(art, "setCopyGCIMode(true);", "readFile(save, scratch, UI_SAVES_ENTRY_SIZE)",
            "setCopyGCIMode(false);", "setCopyGCIMode(true);",
            "readFile(save, scratch, want)", "setCopyGCIMode(false);")
    if art.count("setCopyGCIMode(true);") != art.count("setCopyGCIMode(false);"):
        raise AssertionError("artRead leaves the .gci mode on")
    # A slot is named after its texels reach memory, never before.
    ordered(art, "DCFlushRange(texels, SAVES_SLOT_BYTES);", "slotTags[s] = tag;")
    if art.count("slotTags[") != 1:
        raise AssertionError("artRead names its slot twice")
    # A save already read, or failed, isn't read again; the slot written is
    # one no save on screen is in.
    ordered(load, "if(artSlot(wantTags[i]) >= 0) {", "continue;",
            "UISaves_SlotPick(slotTags, SAVES_SLOTS, wantTags, wantCount)",
            "artRead(wanted[i], s, wantTags[i]);")
    # Nothing is read until input has been quiet.
    ordered(wait, "input->quiet = 0u;", "return pressed;",
            "if(input->quiet < SAVES_QUIET) {", "input->quiet++;", "else if(artLoad()) {")
    # The saves on screen point into the listing placeClear frees.
    ordered(clear, "wantCount = 0;", "free(place->entries);")
    # The pool outlives the page.
    ordered(show, "pool = memalign(32,", "DrawDispose(page);", "free(pool);")

    build = function(source, "screenBuild")
    # Every cube the screen publishes is a save on screen: both stacks'
    # saves are listed before a slot is named, and the screen goes out
    # after that, before the loader runs again.
    ordered(build, "wantCount = 0;", "artWant(stacks[focus],",
            "artWant(stacks[s], -1);", "artSlot(saveTag(")
    ordered(show, "screenBuild(stacks, focus);", "screenShow(&page);",
            "pressed = inputNext(&input);")
    below = function(source, "folderBelowHome")
    ordered(below, "tab == SAVES_TAB_FOLDER", "strcmp(places[tab].dir.name, folderHome) != 0",
            "!folderIsRoot(&places[tab])")
    ordered(show, "if(focus < 0 || !folderBelowHome(stacks[focus])) {", "break;",
            "getParentPath(place->dir.name, place->dir.name);")
    ordered(show, "if(stacks[s] < SAVES_TAB_FOLDER && !places[stacks[s]].ready) {",
            "loadTab(stacks[s]);", "found = found || places[stacks[s]].ready;",
            "UISaveCubes_Swap(stacks[stack], stacks[!stack], found);")


def mutants(source: str) -> list[tuple[str, str]]:
    return [
        ("a Move deletes without a good copy",
         source.replace("if(ok && move && !saveDelete(save))",
                        "if(move && !saveDelete(save))")),
        ("a card write skips the check for the same save",
         source.replace("\tif(has) {\n", "\tif(0) {\n")),
        ("a card write skips the room check",
         source.replace("\tif((int)UISaves_Blocks(entry) > total - used) {\n",
                        "\tif(0) {\n")),
        ("a card copy isn't read back",
         source.replace("!memcmp(back + UI_SAVES_ENTRY_SIZE, blocks, blockBytes)",
                        "true")),
        ("a folder copy takes a name that exists",
         source.replace("if(device->statFile(&dest) != 0) {",
                        "if(true) {")),
        ("the .gci read mode stays on",
         source.replace("\tif(card) {\n\t\tsetCopyGCIMode(false);\n\t}\n", "")),
        ("a copy that didn't read back is left on the card",
         source.replace("\tif(!same) {\n\t\tif(copy != NULL) {\n\t\t\tdevice->deleteFile(copy);\n",
                        "\tif(!same) {\n\t\tif(copy != NULL) {\n")),
        ("the .gci mode stays on after an art entry",
         source.replace("UI_SAVES_ENTRY_SIZE);\n\t\tsetCopyGCIMode(false);\n",
                        "UI_SAVES_ENTRY_SIZE);\n")),
        ("the .gci mode stays on after a save's art",
         source.replace("scratch, want);\n\t\tif(card) {\n\t\t\tsetCopyGCIMode(false);\n\t\t}\n",
                        "scratch, want);\n")),
        ("a slot is named before its texels are flushed",
         source.replace("\tslotTags[s] = tag;\n}", "\t}\n}").replace(
             "\tsave->device->closeFile(save);\n\tif(!slot->failed) {",
             "\tsave->device->closeFile(save);\n\tslotTags[s] = tag;\n\tif(!slot->failed) {\n\t{")),
        ("a failed save is read again",
         source.replace("if(artSlot(wantTags[i]) >= 0) {", "if(0) {")),
        ("a save is read while a button is held",
         source.replace("if(input->quiet < SAVES_QUIET) {", "if(0) {")),
        ("the saves on screen outlive their listing",
         source.replace("\twantCount = 0;\n\tfor(i = 0; i < place->entryCount;",
                        "\tfor(i = 0; i < place->entryCount;")),
        ("the pool goes before the page",
         source.replace("slots. */\n\tfree(pool);\n", "slots. */\n").replace(
             "\tDrawDispose(page);\n\t/* Only now", "\tfree(pool);\n\tDrawDispose(page);\n\t/* Only now")),
        ("the other stack's saves aren't on screen",
         source.replace("\t\t\tartWant(stacks[s], -1);\n", "")),
        ("the screen goes out before its saves are listed",
         source.replace("\t\tscreenBuild(stacks, focus);\n\t\tscreenShow(&page);\n",
                        "\t\tscreenShow(&page);\n\t\tscreenBuild(stacks, focus);\n")),
        ("B leaves from a folder the SD card's stack opened",
         source.replace("if(focus < 0 || !folderBelowHome(stacks[focus])) {", "if(1) {")),
        ("B goes up past the card's root",
         source.replace(" &&\n\t\t!folderIsRoot(&places[tab]);", ";")),
        ("L and R never look again for a card",
         source.replace("\t\t\t\t\tfound = found || places[stacks[s]].ready;\n", "")),
        ("a half-written card copy is left on the card",
         source.replace("!= NULL) {\n\t\t\tdevice->deleteFile(copy);\n\t\t}\n\t\tfree(entries);\n\t\tsnprintf(why, whySize, \"%s: %s.\"",
                        "!= NULL) {\n\t\t}\n\t\tfree(entries);\n\t\tsnprintf(why, whySize, \"%s: %s.\"")),
    ]


def main() -> None:
    source = SAVES.read_text()
    check(source)
    rejected = 0
    for name, mutant in mutants(source):
        if mutant == source:
            raise AssertionError(f"mutant '{name}' changed nothing")
        try:
            check(mutant)
        except AssertionError:
            rejected += 1
            continue
        raise AssertionError(f"mutant survived: {name}")
    print(f"Memory Cards safety audit passed ({rejected} mutants rejected)")


if __name__ == "__main__":
    main()
