#!/usr/bin/env python3
"""manual_pack.py -- convert a game's manual for Indigo's manual viewer.

Turns a manual, a PDF or a folder of page pictures, into the file Game
Details opens from its Manual row: /swiss/ui/manuals/<GAMEID>.pak. The
console can't read a PDF itself (scans are often JPEG 2000 or JBIG2, and a
decoded page would not fit beside the menus), so the work happens here:
each page is cut free of its empty margins, scaled to the screen's width
(640 pixels) and stored in the GameCube's own texture format, with a
half-size copy for the whole-page view. A picture wider than it is tall is
taken for two pages side by side and split.

usage: manual_pack.py MANUAL --id GAMEID [--out FILE] [--gray] [--no-trim]
                      [--pages 1-12,14] [--preview DIR]
       manual_pack.py MANUAL --disc GAME.iso [...]

MANUAL is a PDF, or a folder of PNG or JPEG pages in name order. --id names
the game (the six characters Game Details shows, such as GALE01); --disc
reads it from a disc image instead. --gray stores the pages in 16 grays
(GX I4) rather than colour (GX CMPR): the same size. --pages picks the PDF's
pages or the folder's pictures, counted from 1. --preview writes each page
as the console will show it.

Needs Python 3 with Pillow and NumPy, and pypdfium2 for a PDF:
pip install pillow numpy pypdfium2. Works offline; the same input gives the
same bytes. Format: docs/MANUALS.md.
"""

from __future__ import annotations

import argparse
import re
import struct
import sys
import zlib
from pathlib import Path

import numpy as np
from PIL import Image, ImageChops

MAGIC = b"SWMN"
VERSION = 1
HEADER_SIZE = 64
ENTRY_SIZE = 32
WIDTH = 640                 # the screen's width: a page at full width is one texel a pixel
MAX_HEIGHT = 1152           # rows; a taller page is scaled to fit. Two pages stay under 1 MB.
MAX_PAGES = 256
LEVELS = 2                  # the page, then a half-size copy for the whole-page view
GX_TF_I4, GX_TF_CMPR = 0, 14
RENDER_WIDTH = 3 * WIDTH    # a PDF page is drawn this wide (each half of a spread), then scaled down
SPREAD = 1.0                # wider than tall by this much: two pages side by side
TRIM_TOLERANCE = 40         # how far from the edge's colour counts as content (0-255)
TRIM_MIN_SHARE = 0.01       # a row or column is content when 1% of it is
TRIM_PAD = 0.03             # kept round trimmed content, of its width
ID = re.compile(r"[A-Z0-9]{6}")
DISC_MAGIC = 0xC2339F3D     # a GameCube disc header's, at 0x1C
PICTURES = (".png", ".jpg", ".jpeg")


class PackError(Exception):
    """A manual that can't be converted, said plainly."""


def record_bytes(height: int) -> int:
    """A page's record: the page (640 x height) then its half (320 x height/2), 4 bits a texel."""
    return WIDTH * height // 2 + (WIDTH // 2) * (height // 2) // 2


# -- reading the manual ---------------------------------------------------------------------

def wanted(spec: str | None, count: int) -> list[int]:
    """0-based indexes from a 1-based list such as 1-12,14 (all when None)."""
    if not spec:
        return list(range(count))
    out = []
    for part in spec.split(","):
        first, _, last = part.strip().partition("-")
        try:
            lo, hi = int(first), int(last or first)
        except ValueError:
            raise PackError(f"--pages: {part!r} is not a page or a range such as 3-9") from None
        if not 1 <= lo <= hi <= count:
            raise PackError(f"--pages: {part} is outside 1-{count}")
        out += range(lo - 1, hi)
    return out


def split(image: Image.Image) -> list[Image.Image]:
    """A spread, two pages side by side, as its two pages; any other page as it is."""
    w, h = image.size
    if w <= h * SPREAD:
        return [image]
    return [image.crop((0, 0, w // 2, h)), image.crop((w // 2, 0, w, h))]


def pdf_pages(path: Path, spec: str | None) -> list[Image.Image]:
    try:
        import pypdfium2 as pdfium
    except ImportError:
        raise PackError("reading a PDF needs pypdfium2: pip install pypdfium2") from None
    try:
        pdf = pdfium.PdfDocument(str(path))
    except Exception as error:  # pdfium raises its own types
        raise PackError(f"{path.name}: not a PDF pypdfium2 can read ({error})") from None
    pages = []
    for index in wanted(spec, len(pdf)):
        page = pdf[index]
        width, height = page.get_size()
        halves = 2 if width > height * SPREAD else 1
        pages += split(page.render(scale=RENDER_WIDTH * halves / width).to_pil().convert("RGB"))
    return pages


def folder_pages(path: Path, spec: str | None) -> list[Image.Image]:
    files = sorted(p for p in path.iterdir() if p.suffix.lower() in PICTURES and not p.name.startswith("."))
    pages = []
    for index in wanted(spec, len(files)):
        with Image.open(files[index]) as picture:
            pages += split(picture.convert("RGB"))
    return pages


def read_manual(path: Path, spec: str | None = None) -> list[Image.Image]:
    if path.is_dir():
        pages = folder_pages(path, spec)
    elif path.suffix.lower() == ".pdf":
        pages = pdf_pages(path, spec)
    else:
        raise PackError(f"{path}: give a PDF or a folder of PNG or JPEG pages")
    if not pages:
        raise PackError(f"{path}: no pages")
    return pages


def disc_id(path: Path) -> str:
    """The game ID a GameCube disc image (.iso, .gcm) starts with."""
    with open(path, "rb") as disc:
        header = disc.read(0x20)
    if len(header) < 0x20 or struct.unpack(">I", header[0x1C:0x20])[0] != DISC_MAGIC:
        raise PackError(f"{path.name}: not a plain GameCube disc image (.iso or .gcm); give --id instead")
    return header[:6].decode("ascii", "replace")


# -- trimming and scaling -------------------------------------------------------------------

def _peel(profile: bytes, limit: int) -> tuple[int, int]:
    """Indexes left once bands of 60%+ content at either end (at most limit each) are dropped."""
    lo, hi = 0, len(profile)
    while lo < limit and profile[lo] >= 153:
        lo += 1
    while len(profile) - hi < limit and profile[hi - 1] >= 153:
        hi -= 1
    return lo, hi


def content_box(image: Image.Image) -> tuple[int, int, int, int]:
    """The box round what differs from the page's edge colour, ignoring specks.
    Run twice: a scanner's dark border goes first, then the paper's margin.
    A page that is mostly empty (a lone logo) stays whole rather than zooming in."""
    box = (0, 0) + image.size
    for first in (True, False):
        part = image.crop(box)
        small = part.reduce(4) if min(part.size) >= 400 else part
        sw, sh = small.size
        colour = edge_colour(small)
        r, g, b = ImageChops.difference(small, Image.new("RGB", small.size, colour)).split()
        mask = ImageChops.lighter(ImageChops.lighter(r, g), b).point(
            lambda v: 255 if v > TRIM_TOLERANCE else 0)
        # A scanner's border along one side of the page: thin bands that are
        # mostly not the edge colour. Only at the page's own edges: inside,
        # a band like that is a heading or a picture.
        x0, x1, y0, y1 = 0, sw, 0, sh
        if first:
            x0, x1 = _peel(mask.resize((sw, 1), Image.Resampling.BOX).tobytes(), sw // 25)
            y0, y1 = _peel(mask.resize((1, sh), Image.Resampling.BOX).tobytes(), sh // 25)
        if x1 <= x0 or y1 <= y0:
            break
        inner = mask.crop((x0, y0, x1, y1))
        cols = [x0 + i for i, v in enumerate(inner.resize((x1 - x0, 1), Image.Resampling.BOX).tobytes())
                if v >= 255 * TRIM_MIN_SHARE]
        rows = [y0 + i for i, v in enumerate(inner.resize((1, y1 - y0), Image.Resampling.BOX).tobytes())
                if v >= 255 * TRIM_MIN_SHARE]
        if not cols or not rows:
            break
        scale = part.size[0] / sw
        inner = (box[0] + int(cols[0] * scale), box[1] + int(rows[0] * scale),
                 box[0] + int((cols[-1] + 1) * scale), box[1] + int((rows[-1] + 1) * scale))
        if inner == box:
            break
        box = inner
    pad = int((box[2] - box[0]) * TRIM_PAD)
    w, h = image.size
    box = (max(0, box[0] - pad), max(0, box[1] - pad), min(w, box[2] + pad), min(h, box[3] + pad))
    if box[2] - box[0] < w * 0.4 or box[3] - box[1] < h * 0.4:
        return (0, 0, w, h)
    return box


def edge_colour(image: Image.Image) -> tuple[int, int, int]:
    """The median colour of a picture's outermost pixels: its paper, usually."""
    px = np.asarray(image.convert("RGB"))
    edge = np.concatenate([px[0], px[-1], px[:, 0], px[:, -1]])
    return tuple(int(v) for v in np.median(edge, axis=0))


def fit(image: Image.Image) -> Image.Image:
    """The page as wide as the screen, or as tall as a page may be if it would be taller."""
    w, h = image.size
    scale = min(WIDTH / w, MAX_HEIGHT / h)
    size = (max(1, min(WIDTH, round(w * scale))), max(1, min(MAX_HEIGHT, round(h * scale))))
    return image.resize(size, Image.Resampling.LANCZOS)


# -- GX textures ----------------------------------------------------------------------------

def _q565(rgb: np.ndarray) -> np.ndarray:
    rgb = rgb.astype(np.int32)
    return (((rgb[..., 0] * 31 + 127) // 255) << 11 | ((rgb[..., 1] * 63 + 127) // 255) << 5 |
            (rgb[..., 2] * 31 + 127) // 255)


def _rgb565(c: np.ndarray) -> np.ndarray:
    """RGB565 as the GameCube widens it: each field's top bits repeated below it."""
    c = c.astype(np.int32)
    r, g, b = c >> 11 & 31, c >> 5 & 63, c & 31
    return np.stack([r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2], axis=-1)


def _palette(c0: np.ndarray, c1: np.ndarray) -> np.ndarray:
    """(N, 4, 3): a block's four colours, c0 > c1, blended as the GameCube does (3/8, 5/8)."""
    a, b = _rgb565(c0), _rgb565(c1)
    return np.stack([a, b, (5 * a + 3 * b) >> 3, (3 * a + 5 * b) >> 3], axis=1)


# What entries 0-3 take of the first and second colour.
WEIGHTS = np.array([[1, 0], [0, 1], [5 / 8, 3 / 8], [3 / 8, 5 / 8]])


def _fit(px: np.ndarray, ea: np.ndarray, eb: np.ndarray):
    """Blocks (N, 16, 3) with endpoints ea, eb (N, 3): (error, c0, c1, entries (N, 16), palette)."""
    c0, c1 = _q565(ea), _q565(eb)
    swap = c0 < c1
    c0, c1 = np.where(swap, c1, c0), np.where(swap, c0, c1)
    pal = _palette(c0, c1)
    d = ((px[:, :, None, :] - pal[:, None, :, :]) ** 2).sum(-1)       # (N, 16, 4)
    entries = d.argmin(-1)
    same = c0 == c1                       # one colour: every pixel takes entry 0
    entries[same] = 0
    error = np.take_along_axis(d, entries[..., None], -1)[..., 0].sum(-1)
    return error, c0, c1, entries, pal


def encode_cmpr(image: Image.Image) -> bytes:
    """GX_TF_CMPR: 8x8 tiles of four 4x4 blocks (top left, top right, bottom left, bottom
    right); a block is two big-endian RGB565 colours, then a byte a row, its first pixel
    in the top two bits. Endpoints: the pixel farthest from the block's mean and the one
    farthest from that, then refitted once by least squares; the better of the two stays.
    Always the four-colour mode, so no texel is transparent."""
    w, h = image.size
    px = np.asarray(image.convert("RGB"), dtype=np.int32)
    blocks = px.reshape(h // 8, 2, 4, w // 8, 2, 4, 3).transpose(0, 3, 1, 4, 2, 5, 6).reshape(-1, 16, 3)
    mean = blocks.mean(1)
    spread = ((blocks - mean[:, None]) ** 2).sum(-1)
    ea = np.take_along_axis(blocks, spread.argmax(1)[:, None, None], 1)[:, 0]
    eb = np.take_along_axis(blocks, ((blocks - ea[:, None]) ** 2).sum(-1).argmax(1)[:, None, None], 1)[:, 0]
    best = _fit(blocks, ea, eb)
    # Least squares for both endpoints, given which entry each pixel took.
    wt = WEIGHTS[best[3]]                                                # (N, 16, 2)
    aa, ab, bb = (wt[..., 0] ** 2).sum(1), (wt[..., 0] * wt[..., 1]).sum(1), (wt[..., 1] ** 2).sum(1)
    sa, sb = (wt[..., :1] * blocks).sum(1), (wt[..., 1:] * blocks).sum(1)
    det = aa * bb - ab * ab
    ok = det > 1e-6
    det = np.where(ok, det, 1.0)[:, None]
    fa = np.clip(np.rint((bb[:, None] * sa - ab[:, None] * sb) / det), 0, 255)
    fb = np.clip(np.rint((aa[:, None] * sb - ab[:, None] * sa) / det), 0, 255)
    refit = _fit(blocks, np.where(ok[:, None], fa, ea), np.where(ok[:, None], fb, eb))
    take = refit[0] < best[0]
    c0, c1 = np.where(take, refit[1], best[1]), np.where(take, refit[2], best[2])
    entries = np.where(take[:, None], refit[3], best[3])
    # Plain blocks, give or take scan noise: one colour, the mean.
    plain = spread.max(1) < 48
    flat = _q565(np.rint(mean))
    c0, c1 = np.where(plain, flat, c0), np.where(plain, flat, c1)
    entries[plain] = 0
    rows = (entries.reshape(-1, 4, 4) << np.array([6, 4, 2, 0])).sum(-1).astype(np.uint8)
    out = np.empty((len(blocks), 8), np.uint8)
    out[:, 0], out[:, 1] = c0 >> 8, c0 & 255
    out[:, 2], out[:, 3] = c1 >> 8, c1 & 255
    out[:, 4:] = rows
    return out.tobytes()


def encode_i4(image: Image.Image) -> bytes:
    """GX_TF_I4: 8x8 tiles, two texels a byte, the first in the high nibble."""
    w, h = image.size
    q = (np.asarray(image.convert("L"), dtype=np.int32) * 15 + 127) // 255
    tiles = q.reshape(h // 8, 8, w // 8, 8).transpose(0, 2, 1, 3).reshape(-1, 2)
    return (tiles[:, 0] << 4 | tiles[:, 1]).astype(np.uint8).tobytes()


def decode_cmpr(data: bytes, w: int, h: int) -> Image.Image:
    """As the GameCube draws it, for previews and tests."""
    raw = np.frombuffer(data, np.uint8).reshape(-1, 8).astype(np.int32)
    c0, c1 = raw[:, 0] << 8 | raw[:, 1], raw[:, 2] << 8 | raw[:, 3]
    a, b = _rgb565(c0), _rgb565(c1)
    four = np.stack([a, b, (5 * a + 3 * b) >> 3, (3 * a + 5 * b) >> 3], 1)
    three = np.stack([a, b, (a + b) >> 1, np.zeros_like(a)], 1)       # entry 3: transparent black
    pal = np.where((c0 > c1)[:, None, None], four, three)
    entries = (raw[:, 4:, None] >> np.array([6, 4, 2, 0])) & 3            # (N, 4 rows, 4)
    px = np.take_along_axis(pal, entries.reshape(-1, 16, 1), 1)          # (N, 16, 3)
    px = px.reshape(h // 8, w // 8, 2, 2, 4, 4, 3).transpose(0, 2, 4, 1, 3, 5, 6).reshape(h, w, 3)
    return Image.fromarray(px.astype(np.uint8), "RGB")


def decode_i4(data: bytes, w: int, h: int) -> Image.Image:
    raw = np.frombuffer(data, np.uint8).astype(np.int32)
    px = np.stack([raw >> 4, raw & 15], -1).reshape(h // 8, w // 8, 8, 8)
    return Image.fromarray((px.transpose(0, 2, 1, 3).reshape(h, w) * 17).astype(np.uint8), "L")


# -- the pack -------------------------------------------------------------------------------

def page_textures(pages: list[Image.Image], trim: bool = True) -> tuple[int, list[tuple[Image.Image, int, int]]]:
    """(canvas height, [(the page on its 640 x height canvas, its width, its height)]).
    Every page shares one canvas, a multiple of 16 rows: the half then tiles too."""
    fitted = [fit(page.crop(content_box(page)) if trim else page) for page in pages]
    height = -(-max(page.size[1] for page in fitted) // 16) * 16
    out = []
    for page in fitted:
        canvas = Image.new("RGB", (WIDTH, height), edge_colour(page))
        canvas.paste(page)
        out.append((canvas, *page.size))
    return height, out


def build(pages: list[Image.Image], gray: bool = False, trim: bool = True) -> bytes:
    """The .pak bytes of these pages (RGB pictures, in order)."""
    if not 1 <= len(pages) <= MAX_PAGES:
        raise PackError(f"{len(pages)} pages: a manual has 1 to {MAX_PAGES}")
    height, textures = page_textures(pages, trim)
    encode = encode_i4 if gray else encode_cmpr
    size = record_bytes(height)
    data_offset = HEADER_SIZE + len(pages) * ENTRY_SIZE
    index, records = bytearray(), []
    for n, (canvas, w, h) in enumerate(textures):
        record = encode(canvas) + encode(canvas.reduce(2))
        assert len(record) == size
        index += struct.pack(">IIIHH16x", data_offset + n * size, size, zlib.crc32(record), w, h)
        records.append(record)
    header = bytearray(struct.pack(">4sIIIIIIIIHHHHBBH16x", MAGIC, VERSION, 0, len(pages), HEADER_SIZE,
                                   len(index), data_offset, data_offset + len(pages) * size, size,
                                   WIDTH, height, WIDTH // 2, height // 2, LEVELS,
                                   GX_TF_I4 if gray else GX_TF_CMPR, 0))
    struct.pack_into(">I", header, 8, zlib.crc32(bytes(index), zlib.crc32(bytes(header))))
    return bytes(header + index) + b"".join(records)


def read(data: bytes) -> tuple[dict, list[dict]]:
    """A pack's header and pages, checked as Indigo checks them (gui/ui_manual.c)."""
    if len(data) < HEADER_SIZE:
        raise PackError("shorter than a header")
    (magic, version, crc, count, index_offset, index_length, data_offset, length, size, width, height,
     half_w, half_h, levels, fmt, zero) = struct.unpack_from(">4sIIIIIIIIHHHHBBH", data)
    if (magic != MAGIC or version != VERSION or not 1 <= count <= MAX_PAGES or index_offset != HEADER_SIZE
            or index_length != count * ENTRY_SIZE or data_offset != HEADER_SIZE + index_length
            or width != WIDTH or not 16 <= height <= MAX_HEIGHT or height % 16 or half_w != WIDTH // 2
            or half_h != height // 2 or levels != LEVELS or fmt not in (GX_TF_I4, GX_TF_CMPR) or zero
            or any(data[0x30:HEADER_SIZE]) or size != record_bytes(height)
            or length != data_offset + count * size or length != len(data)):
        raise PackError("not a manual pack this Indigo reads")
    blank = bytearray(data[:HEADER_SIZE])
    blank[8:12] = bytes(4)
    if zlib.crc32(data[HEADER_SIZE:data_offset], zlib.crc32(bytes(blank))) != crc:
        raise PackError("header or index damaged (CRC)")
    pages = []
    for n in range(count):
        offset, record, page_crc, w, h = struct.unpack_from(">IIIHH", data, HEADER_SIZE + n * ENTRY_SIZE)
        if (offset != data_offset + n * size or record != size or not 1 <= w <= WIDTH or not 1 <= h <= height
                or any(data[HEADER_SIZE + n * ENTRY_SIZE + 16:HEADER_SIZE + (n + 1) * ENTRY_SIZE])):
            raise PackError(f"page {n + 1}: bad index entry")
        pages.append({"offset": offset, "crc": page_crc, "width": w, "height": h,
                      "intact": zlib.crc32(data[offset:offset + size]) == page_crc})
    return {"pages": count, "height": height, "format": fmt, "record": size}, pages


def preview(data: bytes, folder: Path) -> None:
    """Each page as the console shows it: p01.png at full width, p01-whole.png the half copy."""
    info, pages = read(data)
    decode = decode_i4 if info["format"] == GX_TF_I4 else decode_cmpr
    height, full = info["height"], WIDTH * info["height"] // 2
    folder.mkdir(parents=True, exist_ok=True)
    for n, page in enumerate(pages, 1):
        record = data[page["offset"]:page["offset"] + info["record"]]
        decode(record[:full], WIDTH, height).crop((0, 0, page["width"], page["height"])).save(
            folder / f"p{n:02d}.png")
        decode(record[full:], WIDTH // 2, height // 2).crop(
            (0, 0, -(-page["width"] // 2), -(-page["height"] // 2))).save(folder / f"p{n:02d}-whole.png")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0],
                                     formatter_class=argparse.RawDescriptionHelpFormatter,
                                     epilog=__doc__.split("\n\n", 1)[1])
    parser.add_argument("manual", type=Path, help="a PDF, or a folder of page pictures")
    who = parser.add_mutually_exclusive_group(required=True)
    who.add_argument("--id", help="the game's six-character ID, such as GALE01")
    who.add_argument("--disc", type=Path, help="read the game ID from this disc image")
    parser.add_argument("--out", type=Path, help="the file to write (GAMEID.pak here by default)")
    parser.add_argument("--gray", action="store_true", help="16 grays (GX I4) instead of colour (GX CMPR)")
    parser.add_argument("--no-trim", action="store_true", help="keep each page's margins as scanned")
    parser.add_argument("--pages", help="the pages to keep, counted from 1: 1-12,14")
    parser.add_argument("--preview", type=Path, help="also write each page, as the console shows it, here")
    args = parser.parse_args(argv)
    try:
        game = (args.id or disc_id(args.disc)).upper()
        if not ID.fullmatch(game):
            raise PackError(f"{game!r}: a game ID is six letters and digits, such as GALE01")
        pages = read_manual(args.manual, args.pages)
        data = build(pages, gray=args.gray, trim=not args.no_trim)
        out = args.out or Path(f"{game}.pak")
        out.write_bytes(data)
        info, _ = read(data)
        if args.preview:
            preview(data, args.preview)
    except (PackError, OSError) as error:
        print(f"manual_pack.py: {error}", file=sys.stderr)
        return 1
    print(f"{out}: {info['pages']} pages, {len(data) / 1e6:.1f} MB, 640 x {info['height']}, "
          f"{'I4' if args.gray else 'CMPR'}. Copy it to /swiss/ui/manuals/ on the card.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
