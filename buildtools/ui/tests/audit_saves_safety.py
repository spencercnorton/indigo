#!/usr/bin/env python3
"""Structural and mutation gate for Memory Cards' data safety (saves.c).

The copy engine runs only against real card and FAT drivers, so these pin
the order that keeps a save from being lost: a Move removes the original
only after the copy was written and read back the same; a card that already
has the save is never written (the driver would write over it in place, at
any size); a folder copy never takes a name that exists; and the card
driver's .gci modes are switched off again after each use.
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
