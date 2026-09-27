#!/usr/bin/env python3
"""Write the fuzzers' starting inputs into OUT/<target>/: files of each kind
as Indigo meets them, so the fuzzers start from the real formats.

usage: seeds.py OUT
"""

import shutil
import struct
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def history(out: Path) -> None:
    def file(lines: list[str], generation: int) -> bytes:
        body = f"SWISS_PLAY_HISTORY 1\nGeneration={generation}\n" + "".join(lines)
        hash_ = 2166136261
        for byte in body.encode():
            hash_ = ((hash_ ^ byte) * 16777619) & 0xFFFFFFFF
        return (body + f"Checksum={hash_:08X}\n").encode()
    (out / "empty").write_bytes(file([], 0))
    (out / "three").write_bytes(file(["GALE01=1695000000\n", "GM4E01=1700000000\n", "GZLE01=1710000000\n"], 7))


def saves(out: Path) -> None:
    entry = bytearray(64)
    entry[0:6] = b"GALE01"
    entry[8:8 + 13] = b"SuperSmashBro"
    struct.pack_into(">H", entry, 0x38, 1)
    block = bytes(8192)
    (out / "save.gci").write_bytes(bytes(entry) + block)
    datel = bytearray(0x80)
    datel[:12] = b"DATELGC_SAVE"
    swapped = bytearray(entry)  # Action Replay swaps these byte pairs, as ui_saves.c undoes
    for start, length in ((6, 2), (0x2C, 20)):
        for i in range(start, start + length, 2):
            swapped[i], swapped[i + 1] = swapped[i + 1], swapped[i]
    (out / "save.sav").write_bytes(bytes(datel) + bytes(swapped) + block)
    gcs = bytearray(0x110)
    gcs[:6] = b"GCSAVE"
    (out / "save.gcs").write_bytes(bytes(gcs) + bytes(entry) + block)


def posters(out: Path) -> None:
    # A real pack from the generator the host tests use; it needs gxtexconv.
    result = subprocess.run([sys.executable, str(HERE.parent / "fixture_pack.py"), str(out / "fixture.pak")],
                            capture_output=True, text=True)
    if result.returncode not in (0, 3):
        raise SystemExit(result.stderr)
    for sidecar in out.glob("*.json"):  # the generator's provenance note, not a pack
        sidecar.unlink()
    if result.returncode == 3:
        print("seeds: no gxtexconv, so no real poster pack seed", file=sys.stderr)
        (out / "header-only").write_bytes(b"SWPK" + bytes(60))


def settings(out: Path) -> None:
    for example in sorted((ROOT / "docs/examples").rglob("*.ini")):
        shutil.copyfile(example, out / example.name)
    (out / "keys.ini").write_bytes(b"# a comment\nSystem Language=English\nAutoload=sd:/x.dol\n")


def main() -> int:
    out = Path(sys.argv[1])
    for target in (history, saves, posters, settings):
        folder = out / target.__name__
        folder.mkdir(parents=True, exist_ok=True)
        target(folder)
    return 0


if __name__ == "__main__":
    sys.exit(main())
