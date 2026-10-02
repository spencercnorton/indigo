#!/usr/bin/env python3
"""Build the demonstration disc the emulator test boots Indigo with.

Dolphin inserts it as a GameCube disc and Swiss reads its ISO 9660 file
system as a device. /games holds small images of fictitious games: a disc
header, a file table with one file, opening.bnr, and that banner (drawn
here), which is all the Library reads of a game. Two more images have a
missing or corrupt file table, which the Library must survive (DAMAGED).
A text file and an empty folder sit beside them, as they do on real cards:
the Library skips both (STRAYS), and the text file sorts first, so the
games move up past it.
/swiss/ui/posters.pak holds posters drawn here from gradients and shapes, for
all but two of the games, so the Library shows both kinds of card, and
/swiss/ui/stills.pak gameplay stills drawn the same way for all but three, so
Spotlight shows a still, a cover and a banner card, and
/swiss/ui/descriptions.txt a line about all but one, which keeps its
banner's. /apps holds stand-in programs for Apps, beside pictures drawn here
in each shape Indigo fits to a card, one in a Homebrew Channel folder whose
boot.dol Apps must leave out. Nothing in it is anyone else's: no game, no
box art, no screenshot, no text, no font.

Given the probe (probe/probe.c, built with the toolchain), the disc also holds
it twice, as a real program Indigo can launch: a game image whose boot program
is the probe (PROBE_GAME) and an app (PROBE_APP).

With --card-zip it makes an SD card image instead (build_card): the release
zip unpacked onto a FAT32 card, the same games, packs and apps beside it.

usage: card.py OUT.iso [--no-posters] [--probe probe.dol]
       card.py OUT.img --card-zip Indigo-<version>.zip [--no-posters] [--probe probe.dol]
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
STRAYS = ("About these games.txt", "Old saves/")
NO_STILL = frozenset({"GDRZ01", "GPLZ01", "GSSZ01"})
# Spotlight's descriptions; Rally Cross Zero keeps its banner's.
DESCRIPTIONS = {
    "GACZ01": "Race through orbiting circuits where every lap rewires the track, and the fastest line is the one nobody has drawn yet.",
    "GCHZ01": "Fly a patched-up harrier over a cobalt sea, trading cargo between islands that move a little further apart every night.",
    "GDRZ01": "Cross an ever-shifting desert in a rally car held together by tape and optimism. Dunes remember every wheel that crossed them.",
    "GEIZ01": "Two gardeners, one of fire and one of vines, tend a greenhouse at the end of the world. Neither can finish it alone.",
    "GGPZ01": "Guide a team of climbers to a summit that is never where the map says. Weather, rope and trust run short in turn.",
    "GNTZ01": "Dive among the glowing tidepools of a city that sank long ago, and bring its lights back to the surface one by one.",
    "GPLZ01": "Carry a paper lantern through a town of folded houses, where every door opens onto another story.",
    "GSSZ01": "Salvage what fell from the sky cities, then decide whether to sell it, keep it, or send it back up.",
}
DISC = ("QIDC00", "Indigo demonstration disc")
MAGIC = 0xC2339F3D
STUB_BYTES = 64 * 1024
FST_OFFSET = 0x5000       # the outer disc's (empty) file table
GAME_FST = 0x4000         # a game's file table, and its banner after it
GAME_BANNER = 0x5000
BANNER_BYTES = 0x1960     # BNR1: magic, padding, 96x32 RGB5A3 pixels, one description
DESCRIPTION = "A fictitious game on the disc Indigo's emulator test boots with."
SYSTEM_AREA = 0x8000  # ISO 9660 leaves the first 32 KiB to the platform


# A disc's region code (0x458, in bi2 after the header), which Swiss takes a
# game's region from: 0 Japan, 1 the Americas, 2 PAL.
REGION_CODES = {"J": 0, "E": 1, "P": 2}
REGION_CODE_AT = 0x458


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
    # Swiss's BNRDesc: a short name and publisher, then the full ones, then
    # the description, each field after the last (include/bnr.h).
    for field, text, size in ((0x1820, title, 0x20), (0x1840, "Indigo test disc", 0x20),
                              (0x1860, title, 0x40), (0x18A0, "Indigo demonstration disc", 0x40),
                              (0x18E0, DESCRIPTION, 0x80)):
        encoded = text.encode("ascii")[:size - 1]
        data[field:field + len(encoded)] = encoded
    return bytes(data)


def game_image(index: int, game_id: str, title: str, region: int = 0) -> bytes:
    """A game as the Library reads it: its header, a one-file table, its banner."""
    image = bytearray(STUB_BYTES)
    image[:0x440] = disc_header(game_id, title)
    struct.pack_into(">I", image, REGION_CODE_AT, region)
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


# The probe, as a game: an ID with a real region letter, and a title that
# sorts among the games the route does not browse.
PROBE_GAME = ("GPRE01", "Indigo Probe")
PROBE_APP = "Probe.dol"
PROBE_DOL_OFFSET = 0x10000
# A GC Loader boots boot.iso from its card: on a card for one, the release's
# ipl.dol made into a disc.
BOOT_ISO = ("GSWE01", "Indigo")


def probe_image(dol: bytes, game_id: str = PROBE_GAME[0], title: str = PROBE_GAME[1]) -> bytes:
    """A disc image whose boot program is the probe, laid out as Swiss reads a
    game: header, an apploader header at 0x2440 (Swiss patches the apploader
    too, but boots the DOL itself), the DOL, and a file table with the banner."""
    header = disc_header(game_id, title)
    apploader = bytearray(0x40)
    apploader[0:10] = b"2026/10/01"
    struct.pack_into(">IIII", apploader, 0x10, 0x81200000, 0x20, 0, 0)
    struct.pack_into(">I", apploader, 0x20, 0x4E800020)  # blr: never run
    bnr = banner(len(GAMES), title)
    bnr_offset = (PROBE_DOL_OFFSET + len(dol) + 0xFFF) & ~0xFFF
    fst_offset = (bnr_offset + len(bnr) + 0xFFF) & ~0xFFF
    fst = struct.pack(">BBHII", 1, 0, 0, 0, 2) + struct.pack(">BBHII", 0, 0, 0, bnr_offset, len(bnr))
    fst += b"opening.bnr\0"
    struct.pack_into(">IIII", header, 0x420, PROBE_DOL_OFFSET, fst_offset, len(fst), len(fst))
    image = bytearray((fst_offset + len(fst) + 0x7FFF) & ~0x7FFF)
    image[:0x440] = header
    struct.pack_into(">I", image, REGION_CODE_AT, REGION_CODES[game_id[3]])
    image[0x2440:0x2440 + len(apploader)] = apploader
    image[PROBE_DOL_OFFSET:PROBE_DOL_OFFSET + len(dol)] = dol
    image[bnr_offset:bnr_offset + len(bnr)] = bnr
    image[fst_offset:fst_offset + len(fst)] = fst
    return bytes(image)


def game_file(game_id: str, title: str) -> str:
    return f"{title} [{game_id}].iso"


def library_order(probe: bool) -> list[str]:
    """The titles in the order the Library lists them: by file name."""
    games = [(game_id, title) for game_id, title in GAMES] + [(i, t) for i, t, _ in DAMAGED]
    if probe:
        games.append(PROBE_GAME)
    return [title for game_id, title in sorted(games, key=lambda g: game_file(*g).lower())]


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


def still(index: int):
    """A gameplay still for one fictitious game: a sky, hills and a sun."""
    from PIL import Image, ImageDraw

    width, height = 640, 480
    hue = (index * 67) % 360
    sky, horizon = _rgb(hue, 0.45, 0.35), _rgb((hue + 30) % 360, 0.35, 0.9)
    image = Image.new("RGB", (width, height))
    draw = ImageDraw.Draw(image)
    for y in range(height):
        t = y / (height - 1)
        draw.line([(0, y), (width, y)], fill=tuple(round(a + (b - a) * t) for a, b in zip(sky, horizon)))
    sun = _rgb((hue + 180) % 360, 0.3, 0.98)
    draw.ellipse([420 - index * 12, 70, 500 - index * 12, 150], fill=sun)
    for n, (base, rise) in enumerate(((330, 90), (370, 60), (410, 40))):
        ground = _rgb((hue + 90 + n * 20) % 360, 0.6, 0.25 + n * 0.12)
        points = [(x, base - rise * abs(((x + index * 40 + n * 90) % 320) - 160) / 160)
                  for x in range(0, width + 1, 32)]
        draw.polygon([(0, height)] + points + [(width, height)], fill=ground)
    return image


def _rgb(hue: float, saturation: float, value: float) -> tuple[int, int, int]:
    import colorsys
    return tuple(round(c * 255) for c in colorsys.hsv_to_rgb(hue / 360, saturation, value))


def build_pack(folder: Path, pak: Path, kind: str) -> bool:
    """posters.pak or stills.pak, from pictures drawn here."""
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    import poster_pack

    try:
        poster_pack.resolve_gxtexconv(None)
    except poster_pack.PackError as error:
        print(f"card.py: no {kind} ({error})", file=sys.stderr)
        return False
    draw, missing = (poster, NO_POSTER) if kind == "posters" else (still, NO_STILL)
    art = folder / kind
    art.mkdir()
    records = []
    for index, (game_id, _) in enumerate(GAMES):
        if game_id in missing:
            continue
        path = art / f"{game_id}.png"
        draw(index).save(path)
        records.append({"game_id": game_id, "source": f"{kind}/{game_id}.png", "universal": False,
                        "source_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                        "note": "Drawn by buildtools/ui/emulator/card.py for the emulator test"})
    manifest = folder / f"{kind}.json"
    manifest.write_text(json.dumps({"version": 1, "kind": kind, "records": records}))
    poster_pack.generate(str(manifest), str(pak), art_root=str(folder))
    return True


# Apps, sorted as Apps shows them. Arcade, first, has no picture.
APPS = ("Arcade", "Pixel Painter", "Starfield", "toolbox")
APP_STUB = b"a stand-in for a program: Apps lists it, nothing can run it"


def app_picture(kind: str):
    """An app's picture: a small pixel-art icon, a poster, or a Homebrew
    Channel banner, so Apps shows each shape Indigo fits to a card."""
    from PIL import Image, ImageDraw

    if kind == "painter":
        image = Image.new("RGB", (16, 16), _rgb(260, 0.5, 0.3))
        for y in range(16):
            for x in range(16):
                if (x - 7.5) ** 2 + (y - 7.5) ** 2 < 36:
                    image.putpixel((x, y), _rgb((x * 24 + y * 8) % 360, 0.7, 0.95))
        return image
    if kind == "starfield":
        width, height = 300, 400
        image = Image.new("RGB", (width, height))
        draw = ImageDraw.Draw(image)
        for y in range(height):
            draw.line([(0, y), (width, y)], fill=_rgb(230, 0.8, 0.12 + 0.3 * y / height))
        for n in range(60):
            x, y = (n * 97) % width, (n * 53) % (height - 60)
            draw.ellipse([x, y, x + 2 + n % 3, y + 2 + n % 3], fill=(255, 250, 220))
        draw.polygon([(150, 250), (190, 330), (110, 330)], fill=_rgb(20, 0.6, 0.9))
        return image
    image = Image.new("RGB", (128, 48), _rgb(150, 0.6, 0.45))
    draw = ImageDraw.Draw(image)
    for n in range(4):
        draw.rectangle([10 + n * 30, 12, 30 + n * 30, 36], outline=(240, 240, 240), width=3)
    return image


def app_order(probe: bool) -> list[str]:
    """The apps in the order Apps lists them: by name."""
    return sorted(APPS + ((PROBE_APP[:-4],) if probe else ()), key=str.lower)


def build_apps(apps: Path) -> int:
    """/apps, as Apps reads it; returns how many apps it should list."""
    apps.mkdir()
    (apps / "Arcade.dol").write_bytes(APP_STUB)
    for name, kind in (("Pixel Painter", "painter"), ("Starfield", "starfield")):
        (apps / f"{name}.dol").write_bytes(APP_STUB)
        app_picture(kind).save(apps / f"{name}.png")
    toolbox = apps / "Toolbox"
    toolbox.mkdir()
    (toolbox / "toolbox.dol").write_bytes(APP_STUB)
    (toolbox / "boot.dol").write_bytes(APP_STUB)  # the Wii's
    (toolbox / "meta.xml").write_text("<app><name>Toolbox</name></app>\n")
    app_picture("banner").save(toolbox / "icon.png")
    (apps / "readme.txt").write_text("Not a program: Apps leaves it out.\n")
    return len(APPS)


def populate(root: Path, work: Path, posters: bool = True, probe: Path | None = None,
             foreign: int = 0) -> dict[str, object]:
    """What the disc and the card both hold: /games, the packs and
    descriptions in /swiss/ui, and /apps. The first game, whose launch the
    route lets fail, has the region code foreign: a console's other region,
    so the menu must come back from that region's video mode."""
    (root / "games").mkdir(parents=True, exist_ok=True)
    (root / "swiss/ui").mkdir(parents=True, exist_ok=True)
    for index, (game_id, title) in enumerate(GAMES):
        (root / "games" / game_file(game_id, title)).write_bytes(
            game_image(index, game_id, title, foreign if index == 0 else 0))
    for game_id, title, image in DAMAGED:
        (root / "games" / game_file(game_id, title)).write_bytes(image(game_id, title))
    if probe:
        (root / "games" / game_file(*PROBE_GAME)).write_bytes(probe_image(probe.read_bytes()))
    for name in STRAYS:
        if name.endswith("/"):
            (root / "games" / name).mkdir()
        else:
            (root / "games" / name).write_text("Not a game.\n")
    with_posters = posters and build_pack(work, root / "swiss/ui/posters.pak", "posters")
    with_stills = posters and build_pack(work, root / "swiss/ui/stills.pak", "stills")
    (root / "swiss/ui/descriptions.txt").write_text(
        "# Descriptions of the demonstration disc's fictitious games\n" +
        "".join(f"{game_id} {text}\n" for game_id, text in sorted(DESCRIPTIONS.items())))
    apps = build_apps(root / "apps")
    if probe:
        (root / "apps" / PROBE_APP).write_bytes(probe.read_bytes())
        apps += 1
    return {"games": len(GAMES), "damaged": len(DAMAGED),
            "posters": len(GAMES) - len(NO_POSTER) if with_posters else 0,
            "stills": len(GAMES) - len(NO_STILL) if with_stills else 0, "apps": apps,
            "probe": bool(probe)}


def build(out: Path, posters: bool = True, probe: Path | None = None, foreign: int = 0) -> dict[str, object]:
    if not shutil.which("genisoimage"):
        raise SystemExit("card.py: genisoimage is missing")
    with tempfile.TemporaryDirectory() as directory:
        folder = Path(directory)
        root = folder / "root"
        info = populate(root, folder, posters, probe, foreign)
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
    return {**info, "bytes": len(image)}


# The SD card: as big as a small real one, formatted as the SD Association's
# formatter does an SDHC card (FAT32, 32 KiB clusters). The file is sparse.
CARD_BYTES = 8 << 30
MTOOLS = dict(os.environ, MTOOLS_SKIP_CHECK="1")


def build_card(out: Path, card_zip: Path, posters: bool = True, probe: Path | None = None,
               foreign: int = 0, settings: str | None = None, boot_iso: bool = False) -> dict[str, object]:
    """A FAT32 SD card image set up as someone would: the release zip
    unpacked onto it, then games, the packs and apps beside it. Without
    settings it has no swiss/settings/global.ini, so Indigo starts in
    Settings, as on a new card; with them that file holds them."""
    import zipfile
    for tool in ("mkfs.fat", "mcopy"):
        if not shutil.which(tool):
            raise SystemExit(f"card.py: {tool} is missing (dosfstools, mtools)")
    with tempfile.TemporaryDirectory() as directory:
        folder = Path(directory)
        root = folder / "root"
        root.mkdir()
        with zipfile.ZipFile(card_zip) as package:
            package.extractall(root)
        info = populate(root, folder, posters, probe, foreign)
        if settings is not None:  # a card that has been used: its settings folders made
            (root / "swiss/settings/game").mkdir(parents=True, exist_ok=True)
            (root / "swiss/settings/global.ini").write_text(settings.replace("\n", "\r\n"))
        if boot_iso:
            (root / "boot.iso").write_bytes(probe_image((root / "ipl.dol").read_bytes(), *BOOT_ISO))
        with open(out, "wb") as image:
            image.truncate(CARD_BYTES)
        subprocess.run(["mkfs.fat", "-F", "32", "-s", "64", "-n", "INDIGO", str(out)], check=True,
                       capture_output=True)
        for entry in sorted(root.iterdir()):
            subprocess.run(["mcopy", "-s", "-i", str(out), str(entry), "::/"], check=True, env=MTOOLS,
                           capture_output=True)
    return {**info, "zip": card_zip.name, "bytes": CARD_BYTES}


def read_card(card: Path, path: str) -> bytes | None:
    """A file from the card image, or None when it isn't there."""
    result = subprocess.run(["mcopy", "-n", "-i", str(card), f"::/{path}", "-"], capture_output=True, env=MTOOLS)
    return result.stdout if result.returncode == 0 else None


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("out", type=Path)
    parser.add_argument("--no-posters", action="store_true")
    parser.add_argument("--probe", type=Path, help="the probe DOL, to add as a game and an app")
    parser.add_argument("--card-zip", type=Path, help="make an SD card image from this release zip instead")
    args = parser.parse_args(argv)
    if args.card_zip:
        print(json.dumps(build_card(args.out, args.card_zip, posters=not args.no_posters, probe=args.probe)))
    else:
        print(json.dumps(build(args.out, posters=not args.no_posters, probe=args.probe)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
