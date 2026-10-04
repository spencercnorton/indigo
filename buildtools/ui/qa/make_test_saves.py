#!/usr/bin/env python3
"""make_test_saves.py OUTDIR - fictitious GameCube saves for Memory Cards.

Writes the saves Memory Cards is tried with. Every game is made up (maker
ZZ, game codes starting Z) and every picture is drawn here: gradients,
stripes, rings and digits.

  OUTDIR/A               Slot A, a GCI folder: 21 saves, every kind of icon
  OUTDIR/B               Slot B: 18 saves, one of them also on Slot A
  OUTDIR/SD/swiss/saves  the Save Folder on the SD card: .gci, Action Replay
                         .sav and GameShark .gcs saves, and two folders of more

Slot A covers every icon Memory Cards draws: RGB5A3 frames, CI8 frames on a
shared palette and on their own palettes, a mix of the three, all three
speeds, a bounce, a frame with no pixels in the middle (it shows the next
frame) and at the end (it shows nothing), the most frames a save has (8), an
icon with no banner, a banner with no icon, neither, and saves whose game
won't let them move (NOMOVE) or be copied (NOCOPY). Each card and the folder
hold more than 16 saves, so every grid scrolls. Point Dolphin's slots at the
card folders:

  dolphin-emu-nogui ... -C Dolphin.Core.SlotA=8 -C Dolphin.Core.SlotB=8 \\
    -C Dolphin.Core.GCIFolderAPathOverride=OUTDIR/A \\
    -C Dolphin.Core.GCIFolderBPathOverride=OUTDIR/B

(8 is Dolphin's GCI folder device.) A run starts the folders afresh: it
removes the saves a run writes there, and the .gci.deleted files Dolphin
leaves behind. Needs Pillow.
"""
from __future__ import annotations

import colorsys
import os
import struct
import sys
from dataclasses import dataclass

from PIL import Image, ImageDraw

BLOCK = 8192
BANNER_W, BANNER_H = 96, 32
ICON = 32
EPOCH_2000 = 946684800
MAKER = "ZZ"
ICON_ADDR = 0x40          # the comment's 64 bytes come first
NO_ART = 0xFFFFFFFF       # an icon address of nothing: no banner, no icon
CI8_SHARED, RGB5A3, CI8_OWN = 1, 2, 3   # icon frames' forms; 0 has no pixels
BANNER_CI8, BANNER_RGB5A3 = 1, 2
BOUNCE = 0x04             # banner_fmt's bit for playing the frames back again
PUBLIC, NOCOPY, NOMOVE = 0x04, 0x08, 0x10
SAVE_FOLDER = "swiss/saves"


@dataclass
class Save:
    game: str                       # 4-character game code
    name: str                       # the card's file name
    comment: tuple[str, str]
    banner: int = BANNER_CI8        # 0 for none
    frames: tuple[tuple[int, int], ...] = ((RGB5A3, 2),)   # (form, speed 1-3)
    bounce: bool = False
    permissions: int = PUBLIC
    blocks: int = 1                 # at least; more when the art needs them
    icon_addr: int = ICON_ADDR


def rgb5a3(r: int, g: int, b: int, a: int = 255) -> int:
    if a >= 224:
        return 0x8000 | (r >> 3) << 10 | (g >> 3) << 5 | (b >> 3)
    return (a >> 5) << 12 | (r >> 4) << 8 | (g >> 4) << 4 | (b >> 4)


def colours(save: Save) -> tuple[tuple[int, int, int], tuple[int, int, int]]:
    """Two colours of the save's own hue, from its game code."""
    hue = (sum(map(ord, save.game)) * 37 % 360) / 360
    top = colorsys.hsv_to_rgb(hue, 0.75, 0.55)
    bottom = colorsys.hsv_to_rgb((hue + 0.08) % 1, 0.45, 1.0)
    return tuple(int(c * 255) for c in top), tuple(int(c * 255) for c in bottom)


def gradient(size: tuple[int, int], top, bottom) -> Image.Image:
    image = Image.new("RGBA", size)
    draw = ImageDraw.Draw(image)
    for y in range(size[1]):
        t = y / max(1, size[1] - 1)
        draw.line([(0, y), (size[0], y)],
                  fill=tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)) + (255,))
    return image


def banner_art(save: Save) -> Image.Image:
    image = gradient((BANNER_W, BANNER_H), *colours(save))
    ImageDraw.Draw(image).text((4, BANNER_H // 2 - 6), save.comment[0].upper()[:15],
                               fill=(255, 255, 255, 255))
    return image


def icon_art(save: Save, frame: int, rounded: bool) -> Image.Image:
    """Frame number frame: a ring, a stripe that moves along, and the frame's
    digit, so each frame looks different. A rounded icon is clear at its
    corners, as many games' are."""
    top, bottom = colours(save)
    image = gradient((ICON, ICON), top, bottom)
    draw = ImageDraw.Draw(image)
    x = 3 + frame * 3
    draw.rectangle([x, 0, x + 2, ICON - 1], fill=(255, 255, 255, 255))
    draw.ellipse([5, 5, 26, 26], outline=(20, 20, 40, 255), width=2)
    draw.text((12, 9), str(frame + 1), fill=(10, 10, 20, 255))
    if rounded:
        mask = Image.new("L", (ICON, ICON), 0)
        ImageDraw.Draw(mask).rounded_rectangle([0, 0, ICON - 1, ICON - 1], radius=8, fill=255)
        mask.paste(96, (0, 0, 3, 3))   # one corner half clear
        image.putalpha(mask)
    return image


def tiled(image: Image.Image, tile_w: int, tile_h: int, pixel) -> bytes:
    """GX texture layout: tiles left to right, top to bottom."""
    out = bytearray()
    width, height = image.size
    for ty in range(0, height, tile_h):
        for tx in range(0, width, tile_w):
            for y in range(ty, ty + tile_h):
                for x in range(tx, tx + tile_w):
                    out += pixel(image.getpixel((x, y)))
    return bytes(out)


def rgb_texture(image: Image.Image) -> bytes:
    return tiled(image.convert("RGBA"), 4, 4, lambda p: struct.pack(">H", rgb5a3(*p)))


def quantize(images: list[Image.Image]) -> tuple[list[Image.Image], bytes]:
    """Images on one palette of up to 256 colours, and that palette as a
    CI8 texture's 512-byte TLUT."""
    sheet = Image.new("RGB", (images[0].width, sum(i.height for i in images)))
    y = 0
    for image in images:
        sheet.paste(image.convert("RGB"), (0, y))
        y += image.height
    sheet = sheet.quantize(colors=256)
    palette = sheet.getpalette()[:768]
    palette += [0] * (768 - len(palette))
    tlut = b"".join(struct.pack(">H", rgb5a3(*palette[i * 3:i * 3 + 3])) for i in range(256))
    parts, y = [], 0
    for image in images:
        parts.append(sheet.crop((0, y, image.width, y + image.height)))
        y += image.height
    return parts, tlut


def ci_texture(image: Image.Image) -> bytes:
    return tiled(image, 8, 4, lambda p: bytes([p]))


def art(save: Save) -> tuple[bytes, dict]:
    """The banner and icon as they sit in the save from the icon address,
    and what Memory Cards should make of them: each frame and the banner as
    RGB5A3 texels (None where there are no pixels)."""
    out = bytearray()
    expected = {"banner": None, "frames": []}
    if save.icon_addr != ICON_ADDR:
        return b"", expected
    if save.banner == BANNER_RGB5A3:
        out += rgb_texture(banner_art(save))
        expected["banner"] = rgb_texture(banner_art(save))
    elif save.banner == BANNER_CI8:
        (pixels,), tlut = quantize([banner_art(save)])
        out += ci_texture(pixels) + tlut
        expected["banner"] = rgb_texture(pixels.convert("RGB"))
    images = [icon_art(save, i, form == RGB5A3) for i, (form, _) in enumerate(save.frames)]
    shared = [i for i, (form, _) in enumerate(save.frames) if form == CI8_SHARED]
    on_shared, shared_tlut = quantize([images[i] for i in shared]) if shared else ([], b"")
    for i, (form, _) in enumerate(save.frames):
        if form == RGB5A3:
            out += rgb_texture(images[i])
            expected["frames"].append(rgb_texture(images[i]))
        elif form == CI8_OWN:
            (pixels,), tlut = quantize([images[i]])
            out += ci_texture(pixels) + tlut
            expected["frames"].append(rgb_texture(pixels.convert("RGB")))
        elif form == CI8_SHARED:
            pixels = on_shared[shared.index(i)]
            out += ci_texture(pixels)
            expected["frames"].append(rgb_texture(pixels.convert("RGB")))
        else:
            expected["frames"].append(None)
    out += shared_tlut
    return bytes(out), expected


def encode(save: Save, when: int) -> bytes:
    """The save as a .gci: its 64-byte entry, then its blocks."""
    pictures, _ = art(save)
    comment = b"".join(line.encode("ascii")[:32].ljust(32, b"\0") for line in save.comment)
    used = len(comment) + len(pictures)
    blocks = max(save.blocks, -(-used // BLOCK))
    data = bytearray(blocks * BLOCK)
    data[0:64] = comment
    data[ICON_ADDR:ICON_ADDR + len(pictures)] = pictures
    # The rest of the save: something that isn't zeros, so a copy is checked.
    for i in range(used, len(data)):
        data[i] = (i * 7 + blocks) & 0xFF
    formats = sum(form << (2 * i) for i, (form, _) in enumerate(save.frames))
    speeds = sum(speed << (2 * i) for i, (_, speed) in enumerate(save.frames))
    entry = struct.pack(
        ">4s2sBB32sIIHHBBHHHI",
        save.game.encode(), MAKER.encode(), 0xFF, save.banner | (BOUNCE if save.bounce else 0),
        save.name.encode().ljust(32, b"\0"), when, save.icon_addr,
        formats, speeds, save.permissions, 0, 0, blocks, 0xFFFF, 0)
    return entry + bytes(data)


def datel(gci: bytes) -> bytes:
    """An Action Replay .sav: its header, then the entry with two stretches
    swapped in pairs (bytes 6-7, and the 20 from the icon address), then the
    blocks."""
    entry = bytearray(gci[:64])
    for start, length in ((6, 2), (0x2C, 20)):
        for i in range(start, start + length, 2):
            entry[i], entry[i + 1] = entry[i + 1], entry[i]
    return b"DATELGC_SAVE".ljust(0x80, b"\0") + bytes(entry) + gci[64:]


def gameshark(gci: bytes) -> bytes:
    """A GameShark .gcs: its header, then the .gci."""
    return b"GCSAVE".ljust(0x110, b"\0") + gci


def gci_name(save: Save) -> str:
    """Dolphin's name for a GCI folder's file: maker, game code, card name."""
    return f"{MAKER}-{save.game}-{save.name}.gci"


SR = ((CI8_SHARED, 1), (CI8_SHARED, 2), (CI8_SHARED, 3))
TINY_TANKS = Save("ZTTE", "TinyTanksOptions", ("Tiny Tanks", "Options"), BANNER_RGB5A3, blocks=2)

SLOT_A = [
    Save("ZIQE", "IndigoQuest_Save01", ("Indigo Quest", "Chapter 3, 12:40"), BANNER_RGB5A3,
         ((RGB5A3, 1), (RGB5A3, 2), (RGB5A3, 3), (RGB5A3, 2)), blocks=2),
    Save("ZSRE", "StarRacerGhosts", ("Star Racer", "Ghost data, 4 tracks"), BANNER_CI8, SR,
         bounce=True, blocks=2),
    Save("ZPPE", "puzzlepark.dat", ("Puzzle Park", "Stage 18"), 0,
         ((CI8_OWN, 2), (CI8_OWN, 2), (CI8_OWN, 2))),
    TINY_TANKS,
    Save("ZLKE", "LanternKeep", ("Lantern Keep", "Tower 2, a lantern lit"), BANNER_CI8,
         ((RGB5A3, 2), (0, 2), (RGB5A3, 2), (RGB5A3, 1))),
    Save("ZRPE", "RocketPicnic", ("Rocket Picnic", "Game won't let it move"), BANNER_CI8,
         ((RGB5A3, 3),), permissions=PUBLIC | NOMOVE),
    Save("ZGCE", "GlassCanyon", ("Glass Canyon", "Game won't let it copy"), BANNER_CI8,
         ((CI8_OWN, 3),), permissions=PUBLIC | NOCOPY),
    Save("ZPCE", "PaperComets", ("Paper Comets", "Its last frame is empty"), BANNER_RGB5A3,
         ((CI8_OWN, 2), (CI8_OWN, 2), (0, 3))),
    Save("ZTPE", "TidepoolTennis", ("Tidepool Tennis", "Eight frames, a bounce"), BANNER_RGB5A3,
         ((RGB5A3, 1), (CI8_SHARED, 1), (CI8_OWN, 1), (RGB5A3, 1),
          (CI8_SHARED, 2), (CI8_OWN, 2), (RGB5A3, 2), (CI8_SHARED, 3)), bounce=True),
    Save("ZCHE", "ClockworkHollow", ("Clockwork Hollow", "A banner, no icon"), BANNER_RGB5A3, ()),
    Save("ZVRE", "VelvetRally", ("Velvet Rally", "No banner, no icon"), BANNER_CI8, (),
         icon_addr=NO_ART),
]
FILLERS_A = ["Copper Orchard", "Snowglobe Derby", "Teacup Pirates", "Pebble Patrol",
             "Neon Lighthouse", "Cloud Bakery", "Thimble Quest", "Comet Kitchen",
             "Ribbon Rangers", "Maple Circuit"]
FILLERS_B = ["Sky Harbor Racing", "Starling Post", "Bramble Bells", "Origami Fleet",
             "Gumdrop Garage", "Lunar Laundry", "Windmill Way", "Quartz Quarry",
             "Fern Fortress", "Kettle Kingdom", "Sailcloth Sky", "Dune Diner",
             "Puddle Parade", "Harbor Lights", "Moss Garden", "Pocket Orchestra",
             "Chalk Comets"]
FILLERS_SD = ["Saltwater Stars", "Juniper Jam", "Acorn Arcade", "Hollow Harp",
              "Velour Voyage", "Tin Robot Tea", "Silver Sprout", "Blue Fig Bay",
              "Cobble Cart", "Ivy Island"]


def filler(title: str, index: int) -> Save:
    """A plain save: a CI8 banner and a two-frame RGB5A3 icon."""
    game = "Z" + chr(65 + index // 26) + chr(65 + index % 26) + "E"
    name = title.replace(" ", "") + "Data"
    return Save(game, name, (title, f"Slot {index % 9 + 1}"), BANNER_CI8,
                ((RGB5A3, 2), (RGB5A3, 2)))


def catalog() -> list[tuple[str, Save, str]]:
    """Every file written, under OUTDIR: (path, save, wrapper) with wrapper
    "gci", "sav" or "gcs"."""
    slot_b = [TINY_TANKS] + [filler(t, 100 + i) for i, t in enumerate(FILLERS_B)]
    sd = [filler(t, 200 + i) for i, t in enumerate(FILLERS_SD)] + SLOT_A[:4]
    files = [(f"A/{gci_name(s)}", s, "gci") for s in SLOT_A]
    files += [(f"A/{gci_name(s)}", s, "gci") for s in (filler(t, i) for i, t in enumerate(FILLERS_A))]
    files += [(f"B/{gci_name(s)}", s, "gci") for s in slot_b]
    folder = f"SD/{SAVE_FOLDER}"
    files += [(f"{folder}/{gci_name(s)}", s, "gci") for s in sd]
    files += [(f"{folder}/LanternKeep.sav", SLOT_A[4], "sav"),
              (f"{folder}/TidepoolTennis.gcs", SLOT_A[8], "gcs")]
    files += [(f"{folder}/Old saves/{gci_name(s)}", s, "gci") for s in SLOT_A[5:7]]
    files += [(f"{folder}/Backups/{gci_name(SLOT_A[1])}", SLOT_A[1], "gci")]
    return files


def clear(folder: str) -> None:
    """The saves an earlier run wrote, and Dolphin's .gci.deleted files."""
    if not os.path.isdir(folder):
        return
    for root, _, names in os.walk(folder):
        for name in names:
            if name.endswith((".gci", ".gci.deleted", ".sav", ".gcs")):
                os.remove(os.path.join(root, name))


def write(out: str) -> None:
    """Every file of catalog() under out, the folders cleared first."""
    for folder in ("A", "B", "SD"):
        clear(os.path.join(out, folder))
    when = 1790000000 - EPOCH_2000
    for index, (path, save, wrapper) in enumerate(catalog()):
        body = encode(save, when + index)
        body = datel(body) if wrapper == "sav" else gameshark(body) if wrapper == "gcs" else body
        target = os.path.join(out, path)
        os.makedirs(os.path.dirname(target), exist_ok=True)
        with open(target, "wb") as handle:
            handle.write(body)


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    write(sys.argv[1])
    return 0


if __name__ == "__main__":
    sys.exit(main())
