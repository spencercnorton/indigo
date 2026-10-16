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
save that failed isn't read again, a stack's saves are read at once when
it comes (the screen opening, L or R, a folder) and otherwise only once
input has been quiet, the list of saves on screen goes with the listing it
points into,
and the pool outlives the page that draws from it. The cube screen lists
the saves of both stacks before it names a slot, and publishes only then;
B goes up a folder the SD card's stack opened, never past where it opened
or the card's root, and L or R looks again at a slot without a card before
swapping a stack. Copy and Move go only to the other stack, never to the
place a save is in, and Move, Copy and Erase take the save whose cube the
cursor is on, never a folder's "..". A card's listing reads each save's permissions, which
libogc2's listing leaves out, so the Move guard has them to read.

A card image (.raw) on the SD card is a card too: its saves move, copy and
erase only through its own writer (saves_raw.c), as they were listed, a
Move's original going only after the copy was checked, and a save marked
no-move stays; an image its device can't change keeps Move and Erase
dimmed, and an image file itself is never deleted. The writer opens an
image as it is, never through the device's writeFile (which creates the
file anew), never creating, truncating, growing or deleting one, and writes
an added save's blocks, then the block map, then the directory (an erase:
the directory, then the map), each made to last before the next, reading
every block back after.

An operation's cube flies while the card works: it starts before the save
is read, and lands (or goes back) only once the copy was written, read
back and a Move's original removed, after every place is read again; the
save that came takes the cube's art in a slot no published cube names, its
texels flushed before the slot is named; the cube in flight has its own
slot, filled before it is shown. An erase is called done only after the
card did it, and a failed one on a card shows the driver's box alone. The
screen leaves before the cards are let go. A message is fitted to its
box with an ellipsis. A save copied to a folder listed to its most may
not be listed after: no ghost shows, the folder's stack keeps its
selection on a cell it has, and the question says it may not show. A copy
the memory left can't hold twice, with the card driver's room, lets the
art's pool go first, the screen without it out before it is freed, and the
pool comes back afterwards with every slot read again.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
SAVES = ROOT / "cube/swiss/source/gui/saves.c"
WRITER = ROOT / "cube/swiss/source/gui/saves_raw.c"


def function(source: str, name: str) -> str:
    start = re.search(r"^(?:static )?[a-z_][\w \*]*\b" + name + r"\([^;{}]*\)\s*\{",
                      source, re.M)
    if start is None:
        raise AssertionError(f"{name} is missing")
    brace = source.index("{", start.start())
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
    card = function(source, "cardWriteBound")
    folder = function(source, "folderWrite")
    read = function(source, "saveRead")

    # A Move removes its original only once the copy is done and checked.
    ordered(transfer, "ok = cardWrite(", "SavesRaw_Add(",
            "ok = status == UI_SAVES_RAW_OK;", "ok = folderWrite(",
            "if(ok && move && !saveDelete(save))")
    if transfer.count("saveDelete(") != 1:
        raise AssertionError("saveTransfer removes a save outside its one guard")

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
    # Both clean-ups delete only on the card this write mounted: one taken
    # out or changed since is mounted again (card_mount_count).
    ordered(card, "mount = card_mount_count(slot);",
            "if(boundMount != NULL && mount != *boundMount) {", "return false;",
            "device->writeFile(",
            "card_mount_count(slot) == mount) {\n\t\t\tdevice->deleteFile(copy);",
            "saveRead(copy, &backLength)", "if(card_mount_count(slot) != mount) {",
            "if(!same)")

    # A folder copy looks for a free name before it creates the file.
    ordered(folder, "device->statFile(&dest) != 0", "break;",
            "device->writeFile(&dest, data, length)",
            "!memcmp(back, data, length)", "if(!same)",
            "device->deleteFile(&dest)")

    # A card's listing carries each save's permissions, which libogc2's
    # CARD_FindNext leaves out: the Move guard and the verdicts read them.
    ordered(function(source, "loadCard"), "readDir(device->initial, &place->entries, -1)",
            "CARD_GetAttributes(slot, dir->fileno, &dir->permissions);",
            "place->list[place->count++] = &place->entries[i];")

    # Both old and centralized readers keep .gci mode around only the read.
    centralized = "static bool readSaveAt(" in source
    if centralized:
        reader = function(source, "readSaveAt")
        ordered(reader, "if(raw != NULL)", "SavesRaw_ReadGci(",
                "if(card) setCopyGCIMode(true);", "seekFile(save, offset,",
                "readFile(save, data, length)", "if(card) setCopyGCIMode(false);",
                "closeFile(save);")
        ordered(read, "readSaveAt(save, 0u, data, want)")
        if "readFile(" in read:
            raise AssertionError("saveRead bypasses the checked reader")
    else:
        ordered(read, "setCopyGCIMode(true);", "readFile(save, data, want)",
                "setCopyGCIMode(false);")

    art = function(source, "artRead")
    load = function(source, "artLoad")
    clear = function(source, "placeClear")
    show = function(source, "show_saves")
    wait = function(source, "inputNext")
    # Entry, wrapper-prefix and full art reads share the same checked reader.
    if centralized:
        ordered(art, "readSaveAt(save, 0u, scratch, UI_SAVES_ENTRY_SIZE)",
                "readSaveAt(save, 0u, scratch, want)",
                "UISaves_ArtLayout(", "readSaveAt(save, 0u, scratch, want)")
        if "readFile(" in art:
            raise AssertionError("artRead bypasses the checked reader")
    else:
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
    # Presses come from the scans, so one made while a card was read isn't
    # lost; a page starts without the presses made before it.
    ordered(function(source, "inputInit"), "padsButtonsTaken(SAVES_BUTTONS);")
    ordered(wait, "held = padsButtonsHeld() & SAVES_BUTTONS;",
            "pressed = padsButtonsTaken(SAVES_BUTTONS);", "return pressed;")
    # A new listing's saves are read back to back, whatever is held, until
    # none is left; otherwise nothing is read until input has been quiet.
    ordered(wait, "bool fresh = artFresh;", "if(!fresh) {", "VIDEO_WaitVSync();",
            "input->quiet = 0u;", "return pressed;",
            "if(!fresh && input->quiet < SAVES_QUIET) {", "input->quiet++;",
            "else if(artLoad()) {", "return 0u;", "artFresh = false;")
    # The saves on screen point into the listing placeClear frees, and the
    # listing that comes after it is read at once.
    ordered(clear, "wantCount = 0;", "free(place->entries);", "artFresh = true;")
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
    if centralized:
        ordered(below, "tab >= SAVES_TAB_FOLDER", "places[tab].rawOpen",
                "folderHome[tab - SAVES_TAB_FOLDER]", "!folderIsRoot(&places[tab])")
        ordered(show, "if(focus < 0 || !folderBelowHome(stacks[focus])) break;",
                "if(place->rawOpen)", "place->selection = place->rawReturn;",
                "getParentPath(place->dir.name, place->dir.name);", "loadFolder(place, false);")
        choose = function(source, "chooseStorage")
        ordered(choose, "if(other < SAVES_TAB_FOLDER)", "dim |= 1u << other;",
                "choice = savesMenu(", "UISaves_StorageTab(stack, choice, other)", "loadTab(tab);")
        ordered(show, "chooseStorage(stacks, stack);", "focus = stackFocus(stacks, focus);")
    else:
        ordered(below, "tab == SAVES_TAB_FOLDER", "strcmp(places[tab].dir.name, folderHome) != 0",
                "!folderIsRoot(&places[tab])")
        ordered(show, "if(focus < 0 || !folderBelowHome(stacks[focus])) {", "break;",
                "getParentPath(place->dir.name, place->dir.name);")
        ordered(show, "if(stacks[s] < SAVES_TAB_FOLDER && !places[stacks[s]].ready) {",
                "loadTab(stacks[s]);", "found = found || places[stacks[s]].ready;",
                "UISaveCubes_Swap(stacks[stack], stacks[!stack], found);")

    # The flight starts before the save is read, and ends only after the
    # copy was written, read back and a Move's original removed: twice, the
    # Move whose original stayed and the rest.
    ordered(transfer, "opBegin(", "saveRead(save, &length)")
    if transfer.index("opEnd(") < transfer.index("if(ok && move && !saveDelete(save))"):
        raise AssertionError("a cube lands before the copy is checked")
    if transfer.count("opEnd(") != 2 or transfer.count("opBegin(") != 1:
        raise AssertionError("saveTransfer starts or ends its flight more than once")
    ordered(transfer, "if(ok && move && !saveDelete(save))", "opEnd(true);",
            "savesTell(D_WARN", "return;", "opEnd(ok);", "savesSay(done);")
    begin = function(source, "opBegin")
    end = function(source, "opEnd")
    # The cube in flight has its own slot, filled before a screen names it.
    ordered(begin, "memcpy(SAVES_FLIGHT,", "DCFlushRange(SAVES_FLIGHT, SAVES_SLOT_BYTES);",
            "over.op.cube.texels = SAVES_FLIGHT;", "screenRedraw();")
    # Every place is read again before a cube lands; the save that came is
    # given the cube's art only after the screen built from that reading
    # is out, in a slot it doesn't name, flushed before the slot is named.
    ordered(end, "savesWait(opStarted,", "placesReload();",
            "over.op.phase = ok ? UI_SAVE_CUBES_LAND : UI_SAVE_CUBES_BACK;",
            "screenRedraw();",
            "UISaves_SlotPick(slotTags, SAVES_SLOTS, wantTags, wantCount)",
            "DCFlushRange(pool + s * SAVES_SLOT_BYTES, SAVES_SLOT_BYTES);",
            "slotTags[s] = tag;", "screenRedraw();", "memset(&over.op, 0,")
    options = function(source, "saveOptions")
    # A save targets the other stack. Physical slots are exclusive; the SD
    # columns have independent navigation and use collision-safe file names.
    ordered(options, "int toTab = screenStacks[!screenFocus];", "saveRoom(toTab,",
            "saveTransfer(save, to, move);")
    # The save acted on is the cube the cursor is on, the cell the info bar
    # and A read, never a folder's ".." its selection still counts.
    ordered(options, "file_handle *save = placeAt(place, placeCell(place));",
            "saveEntry(save, entry)")
    ordered(show, "file_handle *chosen = placeAt(place, placeCell(place));",
            "saveOptions(stacks[focus]);")
    # An erase is done once the card did it; a card's failure shows only the
    # box its driver shows.
    ordered(options, "eraseBegin(", "ok = saveDelete(save);", "opEnd(ok);",
            "if(ok) {", 'savesSay("The data was erased.");', "else if(!card) {",
            'savesTell(D_FAIL, "The save couldn\'t be deleted.')
    # A copy that the memory left can't hold twice, with the card driver's
    # room, has the art's pool let go first: the screen without it goes out
    # before it is freed, and it comes back after the operation with every
    # slot to be read again into it.
    ordered(transfer, "artRoom(isCard(save->device) ? save->size + UI_SAVES_ENTRY_SIZE : save->size);",
            "opBegin(", "saveRead(save, &length)")
    ordered(function(source, "heapFree"), "mallinfo()", "info.fordblks + SYS_GetArena1Size()")
    ordered(function(source, "artRoom"), "UISaves_CopyCrowded(heapFree(), bytes)", "pool = NULL;",
            "screenRedraw();", "free(gone);")
    ordered(function(source, "artReturn"), "memalign(32, (SAVES_SLOTS + 1) * SAVES_SLOT_BYTES)",
            "memset(slotTags, 0, sizeof(slotTags));", "artFresh = true;")
    ordered(show, "saveOptions(stacks[focus]);", "artReturn();")
    # A save lands in the other stack's first free cell. A folder listed to
    # SAVES_LIST_MAX, its ".." counted, may not list it after, wherever its
    # directory puts it: then no ghost shows, the stack's selection stays on
    # a cell it has, and the question says the save may not show.
    ordered(options, '"This folder lists 256 already; the save may not show"',
            "plan.toCell = dest->count < SAVES_LIST_MAX ? dest->count - placeSkip(dest) : -1;",
            "dest->selection = plan.toCell >= 0 ? plan.toCell + placeSkip(dest) : dest->count - 1;",
            "ghosts = plan.toCell >= 0 ? 3u : 0u;", "savesMenu(title, answers, 2, 0, 0u, NULL, ghosts)",
            "ghosts ? NULL : unseen, ghosts & 1u);",
            "savesMenu(title, answers, 2, 0, 0u, ghosts ? NULL : unseen, ghosts)",
            "saveTransfer(save, to, move);")
    # A focused item's reason shows whether or not it is dimmed.
    ordered(function(source, "savesMenu"), "snprintf(over.reason, sizeof(over.reason), \"%s\",\n"
            "\t\t\treasons != NULL && reasons[focus] != NULL ? reasons[focus] : \"\");")
    # A message fits its box, an ellipsis ending what doesn't, rather than
    # being cut short where its buffer ends.
    ordered(function(source, "savesSay"),
            "UICheats_Fit(over.message, sizeof(over.message), text,",
            "UI_SAVE_CUBES_MESSAGE_WIDTH, 0.56f, GetTextSizeInPixels);", "screenRedraw();")
    if centralized:
        # A card image's save goes only through the image's writer, exactly
        # as listed; an image file itself is never deleted.
        ordered(function(source, "saveDelete"),
                "savesPlace_t *raw = rawSource(save, &ordinal);", "if(raw != NULL) {",
                "return SavesRaw_Erase(&raw->rawImage,",
                "UISavesRaw_Entry(raw->rawCard, ordinal)) == UI_SAVES_RAW_OK;",
                "if(SavesRaw_IsImageName(save->name)) return false;",
                "closeFile(save);", "deleteFile(save)")
        # Its save keeps the card's own no-copy and no-move.
        ordered(transfer, "raw != NULL ? UISavesRaw_Entry(raw->rawCard, ordinal)[0x34]",
                "if(move && (isCard(save->device) || raw != NULL) &&",
                "(permissions & (CARD_ATTRIB_NOCOPY | CARD_ATTRIB_NOMOVE))",
                "return;", "artRoom(", "saveRead(save, &length)")
        # The other stack's open image takes a save through its writer;
        # a folder only through destinationFolder.
        ordered(transfer, "bool toFolder = to == UI_SAVES_PLACE_FOLDER || to == UI_SAVES_PLACE_CHOOSE;",
                "if(toFolder && !destinationFolder(", "else if(to == UI_SAVES_PLACE_IMAGE) {",
                "SavesRaw_Add(&places[screenStacks[!screenFocus]].rawImage,")
        options_raw = function(source, "saveOptions")
        ordered(options_raw, "UISaves_Verdict(i == 0, card || place->rawOpen, entry[0x34], blocks,",
                "if(place->rawOpen && !SavesRaw_Writable(&place->rawImage)) {", "dim |= 5u;",
                "else if(dest->rawOpen) {", "to = UI_SAVES_PLACE_IMAGE;")
        ordered(function(source, "saveRoom"), "if(tab >= SAVES_TAB_FOLDER && place->rawOpen) {",
                "room->card = true;", "room->image = true;",
                "room->writable = SavesRaw_Writable(&place->rawImage);",
                "UISavesRaw_Find(place->rawCard, entry) < place->rawCard->count;")
        ordered(function(source, "destinationFolder"), "if(dest->rawOpen",
                "dest->device->features & FEAT_WRITE", "return false;",
                "if(to == UI_SAVES_PLACE_FOLDER)", "dest->dir.name")
        ordered(function(source, "loadRaw"),
                "file_handle image = *source;",
                "int selected = place->rawOpen ? place->selection : 0;", "placeClear(place);",
                "place->rawImage = image;", "place->rawReturn = returning;",
                "place->selection = selected;", "SavesRaw_Load(")
        ordered(show, "if(SavesRaw_IsImageName(chosen->name))", "loadRaw(place, chosen);",
                "if(!place->ready)", "savesSay(place->note[0]);",
                "place->selection = place->rawReturn;", "loadFolder(place, false);")

    # The screen goes before the cards it mounted are let go.
    ordered(show, "over.leaving = 1;", "screenRedraw();", "savesWait(",
            "placeClear(&places[i]);", "DrawDispose(page);", "free(pool);")

    # Detail's load keeps the card's own copy of the save on the SD card
    # before anything on the card changes, and replaces it only after that.
    load = function(source, "Saves_LoadCopy")
    ordered(load, "if(!config_set_device())", "return false;",
            "own = found != NULL ? saveRead(found, &ownLength) : NULL;",
            "if(own == NULL ||\n\t\t\t\tfolderWrite(folder, name, own, ownLength, why, whySize)) {",
            "ok = cardReplace(")
    if load.count("cardReplace(") != 1:
        raise AssertionError("Detail's load replaces a card's save outside its one guard")
    # And puts the card's own copy back when the chosen one doesn't go on.
    # It deletes the card's own copy only once that copy reads back as the
    # one just kept in the Save Folder: a card swapped since keeps its save.
    replace = function(source, "cardReplace")
    ordered(replace, "now = found != NULL ? saveRead(found, &nowLength) : NULL;",
            "same = now != NULL && nowLength == ownLength",
            "gone = same && card_mount_count(slot) == mount && device->deleteFile(found) == 0;",
            "if(!same || card_mount_count(slot) != mount) {", "return false;",
            "if(!gone) {", "return false;",
            "if(cardWriteBound(slot, entry, blocks, blockBytes, &mount, failed, sizeof(failed))) {",
            "else if(card_mount_count(slot) == mount && cardWriteBound(slot, own,\n\t\town + UI_SAVES_ENTRY_SIZE,")


def mutants(source: str) -> list[tuple[str, str]]:
    cases = [
        ("Detail's load replaces a card's save without keeping it",
         source.replace("if(own == NULL ||\n\t\t\t\tfolderWrite(folder, name, own, ownLength, why, whySize)) {",
                        "if(true) {")),
        ("Detail's load deletes a card's save it didn't keep",
         source.replace("gone = same && card_mount_count(slot) == mount && device->deleteFile(found) == 0;",
                        "gone = found != NULL && device->deleteFile(found) == 0;")),
        ("Detail's load never puts the card's own copy back",
         source.replace("else if(card_mount_count(slot) == mount && cardWriteBound(slot, own,\n\t\town + UI_SAVES_ENTRY_SIZE,",
                        "else if(false && cardWriteBound(slot, own,\n\t\town + UI_SAVES_ENTRY_SIZE,")),
        ("Detail's load puts the card's own copy back on a card changed since",
         source.replace("else if(card_mount_count(slot) == mount && cardWriteBound(slot, own,\n\t\town + UI_SAVES_ENTRY_SIZE,",
                        "else if(cardWriteBound(slot, own,\n\t\town + UI_SAVES_ENTRY_SIZE,")),
        ("a bound card write ignores generation mismatch",
         source.replace("if(boundMount != NULL && mount != *boundMount) {",
                        "if(false) {")),
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
         source.replace("if(!fresh && input->quiet < SAVES_QUIET) {", "if(0) {")),
        ("a new stack's saves wait for quiet",
         source.replace("if(!fresh && input->quiet < SAVES_QUIET) {",
                        "if(input->quiet < SAVES_QUIET) {")),
        ("a new listing's saves aren't read at once",
         source.replace("\tplace->listing = ++listings;\n\tartFresh = true;\n",
                        "\tplace->listing = ++listings;\n")),
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
        ("a cube lands before the copy is written",
         source.replace("\topEnd(ok);\n\tif(ok) {\n\t\tsavesSay(done);",
                        "\tif(ok) {\n\t\tsavesSay(done);").replace(
             "\t\tok = cardWrite(", "\t\topEnd(ok);\n\t\tok = cardWrite(")),
        ("a cube lands before every place is read again",
         source.replace("\tplacesReload();\n\t/* Where it came", "\t/* Where it came")),
        ("the save that came is given art while the old screen is up",
         source.replace("\tover.op.phase = ok ? UI_SAVE_CUBES_LAND : UI_SAVE_CUBES_BACK;\n\tscreenRedraw();\n",
                        "\tover.op.phase = ok ? UI_SAVE_CUBES_LAND : UI_SAVE_CUBES_BACK;\n")),
        ("the save that came is named before its art is flushed",
         source.replace("\t\tDCFlushRange(pool + s * SAVES_SLOT_BYTES, SAVES_SLOT_BYTES);\n\t\tslotTags[s] = tag;\n",
                        "\t\tslotTags[s] = tag;\n\t\tDCFlushRange(pool + s * SAVES_SLOT_BYTES, SAVES_SLOT_BYTES);\n")),
        ("the cube in flight is shown before its art is in its slot",
         source.replace("\t\tDCFlushRange(SAVES_FLIGHT, SAVES_SLOT_BYTES);\n", "")),
        ("an erase is called done before the card did it",
         source.replace("\t\teraseBegin(placeCell(place));\n\t\tok = saveDelete(save);\n\t\topEnd(ok);\n",
                        "\t\teraseBegin(placeCell(place));\n\t\topEnd(true);\n\t\tok = saveDelete(save);\n")),
        ("a failed erase on a card shows two boxes",
         source.replace("\t\telse if(!card) {\n", "\t\telse {\n")),
        ("the cards go before the screen does",
         source.replace("\tover.leaving = 1;\n", "")),
        ("a crowded copy runs beside the art's pool",
         source.replace("\tartRoom(isCard(save->device) ? save->size + UI_SAVES_ENTRY_SIZE : save->size);\n", "")),
        ("the pool is freed while the screen still names it",
         source.replace("\tpool = NULL;\n\tscreenRedraw();\n\tfree(gone);\n",
                        "\tfree(gone);\n\tpool = NULL;\n\tscreenRedraw();\n")),
        ("the pool comes back with slots named that it doesn't hold",
         source.replace("\tmemset(slotTags, 0, sizeof(slotTags));\n\tartFresh = true;\n",
                        "\tartFresh = true;\n")),
        ("the pool never comes back",
         source.replace("\t\t\t\tartReturn();\n", "")),
        ("a save aims past the end of a folder's stack",
         source.replace("plan.toCell = dest->count < SAVES_LIST_MAX ? dest->count - placeSkip(dest) : -1;",
                        "plan.toCell = dest->count - placeSkip(dest);")),
        ("a ghost promises a cell a full folder's listing may not have",
         source.replace("plan.toCell = dest->count < SAVES_LIST_MAX ? dest->count - placeSkip(dest) : -1;",
                        "plan.toCell = dest->count - placeSkip(dest) < SAVES_LIST_MAX ?\n"
                        "\t\tdest->count - placeSkip(dest) : -1;")),
        ("a ghost shows where a save has nowhere to land",
         source.replace("ghosts = plan.toCell >= 0 ? 3u : 0u;", "ghosts = 3u;")),
        ("the question doesn't say why no ghost shows",
         source.replace("savesMenu(title, answers, 2, 0, 0u, ghosts ? NULL : unseen, ghosts)",
                        "savesMenu(title, answers, 2, 0, 0u, NULL, ghosts)")),
        ("only a dimmed item's reason shows",
         source.replace("snprintf(over.reason, sizeof(over.reason), \"%s\",\n"
                        "\t\t\treasons != NULL && reasons[focus] != NULL ? reasons[focus] : \"\");",
                        "snprintf(over.reason, sizeof(over.reason), \"%s\", (dim >> focus) & 1u &&\n"
                        "\t\t\treasons != NULL && reasons[focus] != NULL ? reasons[focus] : \"\");")),
        ("a message is cut short without an ellipsis",
         source.replace("\tUICheats_Fit(over.message, sizeof(over.message), text,\n"
                        "\t\tUI_SAVE_CUBES_MESSAGE_WIDTH, 0.56f, GetTextSizeInPixels);\n",
                        "\tsnprintf(over.message, sizeof(over.message), \"%s\", text);\n")),
        ("a card's permissions left as libogc2 lists them",
         source.replace("\t\tCARD_GetAttributes(slot, dir->fileno, &dir->permissions);\n", "")),
        ("a folder's save is taken by its selection, which counts its \"..\"",
         source.replace("file_handle *save = placeAt(place, placeCell(place));",
                        "file_handle *save = place->list[place->selection];")),
        ("a press made while a card is read is lost",
         source.replace("pressed = padsButtonsTaken(SAVES_BUTTONS);",
                        "pressed = held;")),
        ("a page takes the press that opened it",
         source.replace("\tpadsButtonsTaken(SAVES_BUTTONS);\n", "\n")),
        ("a save goes to the stack it is in",
         source.replace("int toTab = screenStacks[!screenFocus];", "int toTab = screenStacks[screenFocus];")),
        ("a half-written card copy is left on the card",
         source.replace("== mount) {\n\t\t\tdevice->deleteFile(copy);\n\t\t}\n\t\tfree(entries);\n\t\tsnprintf(why, whySize, \"%s: %s.\"",
                        "== mount) {\n\t\t}\n\t\tfree(entries);\n\t\tsnprintf(why, whySize, \"%s: %s.\"")),
        ("a failed write's clean-up deletes on a card changed since",
         source.replace(" &&\n\t\t\tcard_mount_count(slot) == mount) {", ") {")),
        ("a read-back from a card changed since is deleted",
         source.replace("\tif(card_mount_count(slot) != mount) {\n\t\tfree(entries);",
                        "\tif(false) {\n\t\tfree(entries);")),
    ]
    if "static bool readSaveAt(" in source:
        updates = {
            "the .gci read mode stays on": ("the centralized .gci read mode stays on",
                source.replace("if(card) setCopyGCIMode(false);", "")),
            "the .gci mode stays on after an art entry": ("an art entry bypasses the checked reader",
                source.replace("readSaveAt(save, 0u, scratch, UI_SAVES_ENTRY_SIZE)", "directRead(save)")),
            "the .gci mode stays on after a save's art": ("art bytes bypass the checked reader",
                source.replace("readSaveAt(save, 0u, scratch, want)", "directRead(save)")),
            "a slot is named before its texels are flushed": ("a slot is named before its texels are flushed",
                source.replace("\tslotTags[s] = tag;\n}", "\n}").replace(
                    "\tif(!slot->failed) {\n\t\tconst u8 *data", "\tslotTags[s] = tag;\n\tif(!slot->failed) {\n\t\tconst u8 *data")),
            "B leaves from a folder the SD card's stack opened": ("B leaves from a folder the SD column opened",
                source.replace("if(focus < 0 || !folderBelowHome(stacks[focus])) break;", "if(true) break;")),
            "B goes up past the card's root": ("B goes up past the card's root",
                re.sub(r"\s*&&\s*!folderIsRoot\(&places\[tab\]\)", "", source)),
            "L and R never look again for a card": ("the named storage choice never reloads",
                source.replace("\t\tloadTab(tab);\n", "")),
        }
        cases = [updates.get(name, (name, mutant)) for name, mutant in cases]
        cases += [
            ("storage choice moves the cursor to the chooser column", source.replace("focus = stackFocus(stacks, focus);\n\t\t\tinputInit(&input);", "focus = stackFocus(stacks, stack);\n\t\t\tinputInit(&input);")),
            ("an image file can be deleted", source.replace("\tif(SavesRaw_IsImageName(save->name)) return false;\n", "")),
            ("a card image's save is deleted through its device",
             source.replace("return SavesRaw_Erase(&raw->rawImage,\n\t\t\tUISavesRaw_Entry(raw->rawCard, ordinal)) == UI_SAVES_RAW_OK;",
                            "return save->device->deleteFile(save) == 0;")),
            ("a card image's save ignores no-move", source.replace("(isCard(save->device) || raw != NULL)", "isCard(save->device)")),
            ("a read-only image leaves Move and Erase open",
             source.replace("if(place->rawOpen && !SavesRaw_Writable(&place->rawImage)) {", "if(false) {")),
            ("an image's room is always writable",
             source.replace("room->writable = SavesRaw_Writable(&place->rawImage);", "room->writable = true;")),
            ("an image's room never has the save",
             source.replace("\t\troom->hasIt = known && place->ready && place->rawCard != NULL &&\n"
                            "\t\t\tUISavesRaw_Find(place->rawCard, entry) < place->rawCard->count;\n", "")),
            ("a read-only SD destination can be written", source.replace("!(dest->device->features & FEAT_WRITE)", "false")),
            ("a new RAW image inherits the folder selection", source.replace("place->rawOpen ? place->selection : 0", "place->selection")),
            ("an invalid RAW image loses its folder return position", source.replace("\t\t\t\t\tplace->selection = place->rawReturn;\n", "")),
            ("a RAW source reaches a physical handler", source.replace("return SavesRaw_ReadGci(", "return directReadGci(")),
            ("the checked reader leaves its physical handle open", source.replace("\tsave->device->closeFile(save);\n\treturn ok;", "\treturn ok;")),
        ]
    return cases


def check_writer(source: str) -> None:
    """saves_raw.c: an image changes in place, in the console's order."""
    code = re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.S)
    for forbidden in ("FA_CREATE_ALWAYS", "FA_CREATE_NEW", "FA_OPEN_ALWAYS", "FA_OPEN_APPEND",
                      "f_truncate(", "f_expand(", "f_unlink(", "->writeFile(", "->deleteFile(",
                      "->renameFile("):
        if forbidden in code:
            raise AssertionError(f"the image writer uses {forbidden}")
    ordered(function(source, "SavesRaw_Writable"), "(image->device->features & FEAT_WRITE)",
            "image->device->writeFile == deviceHandler_FAT_writeFile")
    # Never past the image's end: FatFs grows a file a write seeks beyond.
    ordered(function(source, "imageAt"), "offset <= f_size(fp) && length <= f_size(fp) - offset",
            "f_lseek(fp, offset)")
    ordered(function(source, "imageOpen"), "image->device->closeFile(image);",
            "f_open(fp, image->name, mode)", "if(f_size(fp) != image->size) {", "f_close(fp);")
    ordered(function(source, "writeSystem"), "imageWrite(fp, blocks[i] * UI_SAVES_BLOCK_SIZE,",
            "f_sync(fp) != FR_OK")
    add = function(source, "SavesRaw_Add")
    ordered(add, "if(!SavesRaw_Writable(image)) return UI_SAVES_RAW_READ_ONLY;",
            "imageOpen(image, &fp, FA_READ | FA_WRITE, metadata)",
            "UISavesRaw_PlanAdd(metadata, image->size, gci, card, &change)",
            "imageWrite(&fp, at * UI_SAVES_BLOCK_SIZE, block, UI_SAVES_BLOCK_SIZE)",
            "f_sync(&fp) != FR_OK", "writeSystem(&fp, metadata, change.map)",
            "writeSystem(&fp, metadata, change.directory)", "f_close(&fp);",
            "readBack(image, metadata, &change, card, gci, block)",
            "status = UI_SAVES_RAW_MISMATCH;", "eraseOpen(")
    erase = function(source, "eraseOpen")
    ordered(erase, "UISavesRaw_PlanErase(", "if(status != UI_SAVES_RAW_OK) {",
            "writeSystem(fp, metadata, change.directory)", "writeSystem(fp, metadata, change.map)",
            "readBack(image, metadata, &change, card, NULL, block)")
    ordered(function(source, "SavesRaw_Erase"),
            "if(!SavesRaw_Writable(image)) return UI_SAVES_RAW_READ_ONLY;",
            "imageOpen(image, &fp, FA_READ | FA_WRITE, metadata)", "eraseOpen(")
    ordered(function(source, "readBack"), "imageOpen(image, &fp, FA_READ, NULL)",
            "!memcmp(block, gci + UI_SAVES_ENTRY_SIZE + i * UI_SAVES_BLOCK_SIZE,",
            "!memcmp(block, metadata + system * UI_SAVES_BLOCK_SIZE, UI_SAVES_BLOCK_SIZE);")


def writer_mutants(source: str) -> list[tuple[str, str]]:
    return [
        ("an image opens through the device's writeFile",
         source.replace("f_open(fp, image->name, mode)", "f_open(fp, image->name, mode | FA_CREATE_ALWAYS)")),
        ("any device's images are writable",
         source.replace("\t\timage->device->writeFile == deviceHandler_FAT_writeFile;", "\t\ttrue;")),
        ("a write may seek past the image's end",
         source.replace("offset <= f_size(fp) && length <= f_size(fp) - offset &&", "")),
        ("a reader still holds the image when it is opened to write",
         source.replace("\timage->device->closeFile(image);\n\tif(f_open(", "\tif(f_open(")),
        ("another file of another size is written",
         source.replace("if(f_size(fp) != image->size) {", "if(false) {")),
        ("the directory goes before the block map on an add",
         source.replace("!writeSystem(&fp, metadata, change.map) ||\n\t\t!writeSystem(&fp, metadata, change.directory)",
                        "!writeSystem(&fp, metadata, change.directory) ||\n\t\t!writeSystem(&fp, metadata, change.map)")),
        ("the block map goes before the directory on an erase",
         source.replace("if(!writeSystem(fp, metadata, change.directory) ||\n\t\t!writeSystem(fp, metadata, change.map)) {",
                        "if(!writeSystem(fp, metadata, change.map) ||\n\t\t!writeSystem(fp, metadata, change.directory)) {")),
        ("a system block isn't made to last before the next",
         source.replace(" ||\n\t\t\tf_sync(fp) != FR_OK) return false;", ") return false;")),
        ("an add isn't read back",
         source.replace("if(readBack(image, metadata, &change, card, gci, block)) goto done;", "goto done;")),
        ("a copy that doesn't read back the same stays",
         source.replace("(void)eraseOpen(image, &fp, metadata, added, card, block);", "f_close(&fp);")),
    ]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=ROOT)
    args = parser.parse_args()
    source = (args.source_root / "cube/swiss/source/gui/saves.c").read_text()
    writer = (args.source_root / "cube/swiss/source/gui/saves_raw.c").read_text()
    check(source)
    check_writer(writer)
    rejected = 0
    for name, mutant, gate in ([(n, m, check) for n, m in mutants(source)] +
                               [(n, m, check_writer) for n, m in writer_mutants(writer)]):
        if mutant in (source, writer):
            raise AssertionError(f"mutant '{name}' changed nothing")
        try:
            gate(mutant)
        except AssertionError:
            rejected += 1
            continue
        raise AssertionError(f"mutant survived: {name}")
    print(f"Memory Cards safety audit passed ({rejected} mutants rejected)")


if __name__ == "__main__":
    main()
