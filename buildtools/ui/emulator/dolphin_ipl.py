#!/usr/bin/env python3
"""Copy ipl.dol so Dolphin can boot it, for the emulator test only.

ipl.dol is Indigo compressed by cube/packer. Before it unpacks anything, the
packer's crt0.S reads 0x10000000 and wants the bus error a GameCube raises
there; Dolphin raises none, so the unpacker takes its reset path and Dolphin
shows nothing. The copy turns that one branch into a no-op. Everything after
it, the unpacker, the .xz stream and Indigo, is the shipped bytes. Never put
the copy on a card.

usage: dolphin_ipl.py IPL OUT
"""

from __future__ import annotations

import struct
import sys
from pathlib import Path

# crt0.S startup: eciwx r0,r4,r3; andi. r0,r0,1; ecowx r0,r4,r3; beq reload
PROBE = bytes.fromhex("7c041a6c" "70000001" "7c041b6c")
BEQ = 0x41820000
RELOAD = 0x38601800  # reload: li r3, 0x1800
NOP = 0x60000000


def dolphin_copy(ipl: bytes) -> bytes:
    data = bytearray(ipl)
    offset, size = struct.unpack_from(">I", data, 0x00)[0], struct.unpack_from(">I", data, 0x90)[0]
    hits = [offset + i + 12 for i in range(0, size - 15, 4)
            if data[offset + i:offset + i + 12] == PROBE
            and struct.unpack_from(">I", data, offset + i + 12)[0] & 0xFFFF8003 == BEQ]
    if len(hits) != 1:
        raise ValueError(f"the unpacker's probe branch appears {len(hits)} times in its first text section")
    target = hits[0] + (struct.unpack_from(">I", data, hits[0])[0] & 0x7FFC)
    if struct.unpack_from(">I", data, target)[0] != RELOAD:
        raise ValueError("the probe branch does not lead to the unpacker's reset path")
    struct.pack_into(">I", data, hits[0], NOP)
    return bytes(data)


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    source, copy = map(Path, argv)
    try:
        copy.write_bytes(dolphin_copy(source.read_bytes()))
    except (ValueError, OSError, struct.error) as error:
        print(f"::error::dolphin_ipl.py: {error}", file=sys.stderr)
        return 1
    print(f"{copy}: {source.name} with the console-only probe branch turned off, for Dolphin")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
