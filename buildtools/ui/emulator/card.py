#!/usr/bin/env python3
"""Build the demonstration disc the emulator test boots Indigo with.

Dolphin inserts it as a GameCube disc and Swiss reads its ISO 9660 file
system as a device. /games holds small images of fictitious games: a disc
header, a file table with one file, opening.bnr, and that banner (drawn
here), which is all the Library reads of a game. Two more images have a
missing or corrupt file table, which the Library must survive (DAMAGED).
/swiss/ui/posters.pak holds posters drawn here from gradients and shapes, for
all but two of the games, so the Library shows both kinds of card. Nothing
in it is anyone else's: no game, no box art, no font.

usage: card.py OUT.iso [--no-posters]
Needs genisoimage; the posters need gxtexconv (see buildtools/ui/poster_pack.py)
and are left out, with a notice, when it is missing.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

GAMES = (
    ("GACZ01", "Astral Circuit"), ("GCHZ01", "Cobalt Harrier"), ("GDRZ01", "Dune Rally"),
    ("GEIZ01", "Ember and Ivy"), ("GGPZ01", "Glacier Point"), ("GNTZ01", "Neon Tidepool"),
    ("GPLZ01", "Paper Lantern"), ("GRZZ01", "Rally Cross Zero"), ("GSSZ01", "Skyward Salvage"),
)
NO_POSTER = frozenset({"GPLZ01", "GSSZ01"})
DISC = ("QIDC00", "Indigo demonstration disc")
MAGIC = 0xC2339F3D
STUB_BYTES = 64 * 1024
FST_OFFSET = 0x5000       # the outer disc's (empty) file table
GAME_FST = 0x4000         # a game's file table, and its banner after it
GAME_BANNER = 0x5000
BANNER_BYTES = 0x1960     # BNR1: magic, padding, 96x32 RGB5A3 pixels, one description
SYSTEM_AREA = 0x8000  # ISO 9660 leaves the first 32 KiB to the platform


def disc_header(game_id: str, title: str) -> bytearray:
    """The start of a GameCube disc: ID, magic, and the game's name."""
    if len(game_id) != 6 or not game_id.isalnum() or not game_id.isupper():
        raise ValueError(f"not a disc ID: {game_id!r}")
    header = bytearray(0x440)
    header[0:6] = game_id.encode("ascii")
    struct.pack_into(">I", header, 0x1C, MAGIC)
    name = title.encode("ascii")
    header[0x20:0x20 + len(name)] = name
    return header


def banner(index: int, title: str) -> bytes:
    """An opening.bnr: a gradient in the game's colour, and its names."""
    data = bytearray(BANNER_BYTES)
    data[0:4] = b"BNR1"
    hue = (index * 67) % 360
    left, right = _rgb(hue, 0.6, 0.35), _rgb((hue + 40) % 360, 0.8, 0.85)
    offset = 0x20
    for tile_y in range(8):          # 96x32 in 4x4 tiles, row by row
        for tile_x in range(24):
            for y in range(4):
                for x in range(4):
                    t = (tile_x * 4 + x) / 95
                    r, g, b = (round(a + (c - a) * t) >> 3 for a, c in zip(left, right))
                    struct.pack_into(">H", data, offset, 0x8000 | r << 10 | g << 5 | b)
                    offset += 2
    for field, text, size in ((0x1820, title, 0x20), (0x1840, "Indigo test disc", 0x20),
                              (0x1860, title, 0x40), (0x1880, "Indigo demonstration disc", 0x40),
                              (0x18C0, "A fictitious game on the disc Indigo's emulator test boots with.", 0x80)):
        encoded = text.encode("ascii")[:size - 1]
        data[field:field + len(encoded)] = encoded
    return bytes(data)


def game_image(index: int, game_id: str, title: str) -> bytes:
    """A game as the Library reads it: its header, a one-file table, its banner."""
    image = bytearray(STUB_BYTES)
    image[:0x440] = disc_header(game_id, title)
    fst = struct.pack(">BBHII", 1, 0, 0, 0, 2) + struct.pack(">BBHII", 0, 0, 0, GAME_BANNER, BANNER_BYTES)
    fst += b"opening.bnr\0"
    struct.pack_into(">III", image, 0x424, GAME_FST, len(fst), len(fst))
    image[GAME_FST:GAME_FST + len(fst)] = fst
    image[GAME_BANNER:GAME_BANNER + BANNER_BYTES] = banner(index, title)
    return bytes(image)


def header_only(game_id: str, title: str) -> bytes:
    """A header-only dump: a disc header that declares no file table."""
    return bytes(disc_header(game_id, title))


RUNAWAY_COUNT = 0x0AAAAAAA


def runaway_table(game_id: str, title: str) -> bytes:
    """A game whose file table counts far more entries than it holds.

    RUNAWAY_COUNT puts the string table 0x7FFFFFF8 bytes past the table: an
    unchecked lookup wraps below 0x80000000, where nothing is mapped, and faults
    on its first name instead of reading on through memory."""
    image = bytearray(STUB_BYTES)
    image[:0x440] = disc_header(game_id, title)
    fst = struct.pack(">BBHII", 1, 0, 0, 0, RUNAWAY_COUNT)
    fst += struct.pack(">BBHII", 0, 0, 0, GAME_BANNER, BANNER_BYTES)
    struct.pack_into(">III", image, 0x424, GAME_FST, len(fst), len(fst))
    image[GAME_FST:GAME_FST + len(fst)] = fst
    return bytes(image)


# Images the Library lists and must survive. It reads the file table of every
# image it lists, to find the banner, and an unchecked read of a missing or
# corrupt table crashed it. Their titles sort among the first games, which the
# emulator test browses.
DAMAGED = (("GBHZ01", "Broken Header", header_only), ("GCTZ01", "Corrupt Table", runaway_table))


def outer_header() -> bytes:
    """The disc itself: a header Dolphin accepts, with an empty file table."""
    area = bytearray(FST_OFFSET + 12)
    area[:0x440] = disc_header(*DISC)
    struct.pack_into(">III", area, 0x424, FST_OFFSET, 12, 12)
    struct.pack_into(">BBHII", area, FST_OFFSET, 1, 0, 0, 0, 1)  # root directory, one entry
    return bytes(area)


def poster(index: int):
    """Poster art for one fictitious game: a gradient and a few shapes."""
    from PIL import Image, ImageDraw

    width, height = 384, 512
    hue = (index * 67) % 360
    top, bottom = _rgb(hue, 0.55, 0.22), _rgb((hue + 40) % 360, 0.75, 0.62)
    image = Image.new("RGB", (width, height))
    draw = ImageDraw.Draw(image)
    for y in range(height):
        t = y / (height - 1)
        draw.line([(0, y), (width, y)], fill=tuple(round(a + (b - a) * t) for a, b in zip(top, bottom)))
    light = _rgb((hue + 180) % 360, 0.35, 0.92)
    cx, cy, r = width // 2, 200, 70 + index * 6
    if index % 3 == 0:
        draw.ellipse([cx - r, cy - r, cx + r, cy + r], outline=light, width=14)
    elif index % 3 == 1:
        draw.polygon([(cx, cy - r), (cx + r, cy), (cx, cy + r), (cx - r, cy)], outline=light, width=14)
    else:
        draw.rectangle([cx - r, cy - r, cx + r, cy + r], outline=light, width=14)
    for n in range(4):
        y = 360 + n * 26
        draw.rectangle([48 + n * 22, y, width - 48 - n * 22, y + 10], fill=light)
    return image


def _rgb(hue: float, saturation: float, value: float) -> tuple[int, int, int]:
    import colorsys
    return tuple(round(c * 255) for c in colorsys.hsv_to_rgb(hue / 360, saturation, value))


def build_posters(folder: Path, pak: Path) -> bool:
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    import poster_pack

    try:
        poster_pack.resolve_gxtexconv(None)
    except poster_pack.PackError as error:
        print(f"card.py: no posters ({error})", file=sys.stderr)
        return False
    art = folder / "art"
    art.mkdir()
    records = []
    for index, (game_id, _) in enumerate(GAMES):
        if game_id in NO_POSTER:
            continue
        path = art / f"{game_id}.png"
        poster(index).save(path)
        records.append({"game_id": game_id, "source": f"art/{game_id}.png", "universal": False,
                        "source_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                        "note": "Drawn by buildtools/ui/emulator/card.py for the emulator test"})
    manifest = folder / "manifest.json"
    manifest.write_text(json.dumps({"version": 1, "records": records}))
    poster_pack.generate(str(manifest), str(pak), art_root=str(folder))
    return True


def build(out: Path, posters: bool = True) -> dict[str, object]:
    if not shutil.which("genisoimage"):
        raise SystemExit("card.py: genisoimage is missing")
    with tempfile.TemporaryDirectory() as directory:
        folder = Path(directory)
        root = folder / "root"
        (root / "games").mkdir(parents=True)
        (root / "swiss/ui").mkdir(parents=True)
        for index, (game_id, title) in enumerate(GAMES):
            (root / "games" / f"{title} [{game_id}].iso").write_bytes(game_image(index, game_id, title))
        for game_id, title, image in DAMAGED:
            (root / "games" / f"{title} [{game_id}].iso").write_bytes(image(game_id, title))
        with_posters = posters and build_posters(folder, root / "swiss/ui/posters.pak")
        for path in sorted(root.rglob("*")) + [root]:
            os.utime(path, (1000000000, 1000000000))
        subprocess.run(["genisoimage", "-quiet", "-R", "-J", "-V", "INDIGO_DEMO", "-o", str(out), str(root)],
                       check=True)
    image = bytearray(out.read_bytes())
    if any(image[:SYSTEM_AREA]) or image[SYSTEM_AREA:SYSTEM_AREA + 6] != b"\x01CD001":
        raise SystemExit("card.py: genisoimage did not leave the system area free")
    header = outer_header()
    image[:len(header)] = header
    out.write_bytes(image)
    return {"games": len(GAMES), "damaged": len(DAMAGED),
            "posters": len(GAMES) - len(NO_POSTER) if with_posters else 0, "bytes": len(image)}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("out", type=Path)
    parser.add_argument("--no-posters", action="store_true")
    args = parser.parse_args(argv)
    print(json.dumps(build(args.out, posters=not args.no_posters)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
