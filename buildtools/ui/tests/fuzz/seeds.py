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
    # Real packs from the generator the host tests use, a poster pack and a
    # stills pack; it needs gxtexconv.
    result = subprocess.run([sys.executable, str(HERE.parent / "fixture_pack.py"), str(out / "fixture.pak"),
                             str(out / "fixture-stills.pak")],
                            capture_output=True, text=True)
    if result.returncode not in (0, 3):
        raise SystemExit(result.stderr)
    for sidecar in out.glob("*.json"):  # the generator's provenance note, not a pack
        sidecar.unlink()
    if result.returncode == 3:
        print("seeds: no gxtexconv, so no real pack seeds", file=sys.stderr)
        (out / "header-only").write_bytes(b"SWPK" + bytes(60))


def about(out: Path) -> None:
    # Game descriptions as the downloads write them, and as a person might.
    (out / "descriptions.txt").write_bytes(
        b"# Indigo game descriptions\n"
        b"GAFE01 Welcome to Animal Crossing, where something happens every day.\n"
        b"GMSE01 Clean up Isle Delfino with FLUDD, a water pack.\n"
        b"GTEE01 Wax your board. This is the sickest ride ever!\n")
    (out / "hand-written").write_bytes(b"GZLE01\tSail the Great Sea.\r\n\r\ngale01 not an ID\nGALE01 Melee\nGALE01 again")


def settings(out: Path) -> None:
    for example in sorted((ROOT / "docs/examples").rglob("*.ini")):
        shutil.copyfile(example, out / example.name)
    (out / "keys.ini").write_bytes(b"# a comment\nSystem Language=English\nAutoload=sd:/x.dol\n")


def fst(out: Path) -> None:
    # A game's file table as card.py writes it, then the two shapes that once
    # crashed the Library: no table at all (a header-only image), and a table
    # that counts more entries than it holds. get_fst_details reads the count
    # as a native u32, big-endian on the console. It is stored in the host's
    # byte order here, or a little-endian host would read a huge count, stop
    # at the count check and never reach the names.
    root, entry = struct.pack(">BBHI", 1, 0, 0, 0), struct.pack(">BBHII", 0, 0, 0, 0x5000, 0x1960)
    (out / "game").write_bytes(root + struct.pack("=I", 2) + entry + b"opening.bnr\0")
    (out / "empty").write_bytes(b"")
    (out / "runaway-count").write_bytes(root + struct.pack("=I", 0x00FFFFFF) + entry)


def png(out: Path) -> None:
    # An app's picture, in each colour type Pillow writes, small so the
    # fuzzer's mutations reach every chunk, and one it must refuse.
    from PIL import Image
    gradient = Image.linear_gradient("L").resize((24, 32))
    shapes = {
        "rgb.png": Image.merge("RGB", (gradient, gradient.transpose(Image.Transpose.FLIP_LEFT_RIGHT), gradient)),
        "rgba.png": Image.merge("RGBA", (gradient, gradient, gradient, gradient.rotate(90))),
        "gray.png": gradient,
        "bilevel.png": gradient.convert("1"),
        "palette.png": gradient.convert("RGB").quantize(16),
        "banner.png": Image.new("RGB", (32, 12), (200, 40, 60)),
    }
    for name, image in shapes.items():
        image.save(out / name)
    palette = shapes["palette.png"].copy()
    palette.info["transparency"] = 0
    palette.save(out / "palette-trns.png", transparency=0)
    Image.new("I;16", (8, 8), 40000).save(out / "gray16.png")
    shapes["rgb.png"].save(out / "interlaced.png", interlace=1)
    # Stored, not compressed: a mutation there is a changed filter or pixel.
    for name in ("rgba", "palette", "bilevel"):
        shapes[f"{name}.png"].save(out / f"{name}-stored.png", compress_level=0)
    # 'N' and an app's name: its poster of the name.
    (out / "name-variant").write_bytes(b"Ngbihf-ossc+carby")
    (out / "name-long").write_bytes(b"N" + b"swiss_r2119-" * 8)
    (out / "name-separators").write_bytes(b"N-_. +a")


def main() -> int:
    out = Path(sys.argv[1])
    for target in (history, saves, posters, about, settings, fst, png):
        folder = out / target.__name__
        folder.mkdir(parents=True, exist_ok=True)
        target(folder)
    return 0


if __name__ == "__main__":
    sys.exit(main())
