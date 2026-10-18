#!/usr/bin/env python3
"""ui_png: an app's PNG on the card becomes a Library poster.

Writes PNGs of every colour type, bit depth and row filter with its own small
writer (so each filter and chunk layout is chosen, not left to Pillow), runs
them through test_ui_png, and decodes the CMPR that comes back the way the
GameCube does (Dolphin's decoder: 8x8 tiles of four DXT1 blocks, big-endian
colours, blends of 5/8 and 3/8). Pillow draws the pictures and resizes the
references.

usage: test_ui_png.py [BINARY]   (default ./test_ui_png)
"""

from __future__ import annotations

import os
import resource
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

import numpy as np
from PIL import Image

BINARY = os.path.abspath(sys.argv.pop(1) if len(sys.argv) > 1 else "./test_ui_png")
POSTER_W, POSTER_H, CANVAS = 192, 256, 256
LEVELS = (256, 128, 64, 32, 16)
POSTER_BYTES = sum(side * side // 2 for side in LEVELS)
MARGIN, MAX_UPSCALE = 14.0, 4.0
NIGHT = np.array([16.0, 12.0, 40.0])
REFUSED = 3
# A run of the binary is bounded in time and in what it writes, so a hang or
# a sanitizer printing without end fails the test instead of the runner.
TIMEOUT, OUTPUT_CAP = 120, 16 << 20


def run(argv: list[str], text: bool = False, check: bool = False) -> subprocess.CompletedProcess:
    def cap() -> None:
        resource.setrlimit(resource.RLIMIT_FSIZE, (OUTPUT_CAP, OUTPUT_CAP))

    with tempfile.TemporaryFile() as out, tempfile.TemporaryFile() as err:
        code = subprocess.run(argv, stdout=out, stderr=err, timeout=TIMEOUT, preexec_fn=cap).returncode
        out.seek(0)
        err.seek(0)
        stdout, stderr = out.read(), err.read()
    if text:
        stdout, stderr = stdout.decode(errors="replace"), stderr.decode(errors="replace")
    result = subprocess.CompletedProcess(argv, code, stdout, stderr)
    if check:
        result.check_returncode()
    return result


# -- writing PNGs -------------------------------------------------------------

def chunk(kind: bytes, data: bytes, crc: int | None = None) -> bytes:
    if crc is None:
        crc = zlib.crc32(kind + data)
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", crc & 0xFFFFFFFF)


def pack_row(samples: np.ndarray, depth: int) -> bytes:
    """One row of samples (already flattened per pixel) at depth bits."""
    if depth == 16:
        return samples.astype(">u2").tobytes()
    if depth == 8:
        return samples.astype(np.uint8).tobytes()
    per_byte = 8 // depth
    out = bytearray((len(samples) * depth + 7) // 8)
    for i, value in enumerate(samples.tolist()):
        out[i // per_byte] |= int(value) << (8 - depth - (i % per_byte) * depth)
    return bytes(out)


def paeth(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def filter_row(kind: int, row: bytes, prev: bytes, bpp: int) -> bytes:
    out = bytearray(len(row))
    for i, x in enumerate(row):
        a = row[i - bpp] if i >= bpp else 0
        b = prev[i]
        c = prev[i - bpp] if i >= bpp else 0
        predictor = (0, a, b, (a + b) // 2, paeth(a, b, c))[kind]
        out[i] = (x - predictor) & 0xFF
    return bytes(out)


def write_png(samples: np.ndarray, color_type: int, depth: int, *, filters=(0,),
              palette: list | None = None, trns: bytes | None = None,
              idat_pieces: int = 1, interlace: int = 0, before_idat: bytes = b"",
              after_idat: bytes = b"", corrupt: str = "") -> bytes:
    """samples: height x width x channels. filters cycle row by row."""
    height, width = samples.shape[:2]
    channels = samples.shape[2]
    bpp = max(1, channels * depth // 8)
    raw, prev = bytearray(), bytes(((width * channels * depth + 7) // 8))
    for y in range(height):
        row = pack_row(samples[y].reshape(-1), depth)
        kind = filters[y % len(filters)]
        raw += bytes([kind]) + filter_row(kind, row, prev, bpp)
        prev = row
    stream = zlib.compress(bytes(raw), 9)
    if corrupt == "short-stream":
        stream = zlib.compress(bytes(raw[:len(raw) // 2]), 9)
    out = bytearray(b"\x89PNG\r\n\x1a\n")
    out += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, depth, color_type, 0, 0, interlace))
    out += before_idat
    if palette is not None:
        out += chunk(b"PLTE", bytes(c for rgb in palette for c in rgb))
    if trns is not None:
        out += chunk(b"tRNS", trns)
    step = max(1, -(-len(stream) // idat_pieces))
    for at in range(0, len(stream), step):
        out += chunk(b"IDAT", stream[at:at + step])
    out += after_idat
    out += chunk(b"IEND", b"")
    return bytes(out)


def rgb_png(image: Image.Image, **kw) -> bytes:
    return write_png(np.asarray(image.convert("RGB")), 2, 8, **kw)


# -- reading posters ----------------------------------------------------------

def unpack565(value: int) -> np.ndarray:
    r, g, b = value >> 11 & 31, value >> 5 & 63, value & 31
    return np.array([r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2], np.int32)


def decode_cmpr(data: bytes, side: int) -> np.ndarray:
    """A GX CMPR level as side x side x 4 (alpha 0 or 255), as Dolphin decodes it."""
    out = np.zeros((side, side, 4), np.int32)
    at = 0
    for tile_y in range(0, side, 8):
        for tile_x in range(0, side, 8):
            for block in range(4):
                x0, y0 = tile_x + (block & 1) * 4, tile_y + (block >> 1) * 4
                c0, c1 = struct.unpack(">HH", data[at:at + 4])
                p0, p1 = unpack565(c0), unpack565(c1)
                if c0 > c1:
                    colours = [p0, p1, (5 * p0 + 3 * p1) >> 3, (3 * p0 + 5 * p1) >> 3]
                    alpha = [255] * 4
                else:
                    colours = [p0, p1, (p0 + p1) // 2, (p0 + p1) // 2]
                    alpha = [255, 255, 255, 0]
                for y in range(4):
                    line = data[at + 4 + y]
                    for x in range(4):
                        index = line >> (6 - 2 * x) & 3
                        out[y0 + y, x0 + x, :3] = colours[index]
                        out[y0 + y, x0 + x, 3] = alpha[index]
                at += 8
    return out


def levels(poster: bytes) -> list[np.ndarray]:
    out, at = [], 0
    for side in LEVELS:
        out.append(decode_cmpr(poster[at:at + side * side // 2], side))
        at += side * side // 2
    return out


def fit(width: int, height: int) -> tuple[float, float, float]:
    """(scale, left, top): where ui_png puts a picture on the poster."""
    aspect, poster = width / height, POSTER_W / POSTER_H
    if poster * 0.875 <= aspect <= poster / 0.875:
        scale = max(POSTER_W / width, POSTER_H / height)
    else:
        scale = min((POSTER_W - 2 * MARGIN) / width, (POSTER_H - 2 * MARGIN) / height, MAX_UPSCALE)
    return scale, (POSTER_W - width * scale) / 2, (POSTER_H - height * scale) / 2


def backdrop(mean: np.ndarray) -> np.ndarray:
    """The backdrop's colour on each poster row."""
    rows = []
    for y in range(POSTER_H):
        tint = 0.42 - 0.26 * y / (POSTER_H - 1)
        rows.append(mean * tint + NIGHT * (1 - tint))
    return np.array(rows)


def picture(width: int, height: int) -> Image.Image:
    """Smooth, so CMPR's error stays small, with a tint per quarter so a
    picture turned or mirrored fails."""
    x = np.linspace(0, 1, width)[None, :]
    y = np.linspace(0, 1, height)[:, None]
    r = 60 + 150 * x + 20 * y
    g = 40 + 160 * y + 0 * x
    b = 200 - 120 * x * y
    return Image.fromarray(np.stack(np.broadcast_arrays(r, g, b), -1).clip(0, 255).astype(np.uint8))


class PosterTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_png(self, data: bytes, name: str = "in") -> bytes | None:
        source = os.path.join(self.dir.name, name + ".png")
        target = os.path.join(self.dir.name, name + ".bin")
        with open(source, "wb") as f:
            f.write(data)
        result = run([BINARY, "poster", source, target])
        if result.returncode == REFUSED:
            return None
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        with open(target, "rb") as f:
            poster = f.read()
        self.assertEqual(len(poster), POSTER_BYTES)
        return poster

    def poster_of(self, data: bytes, name: str = "in") -> np.ndarray:
        poster = self.run_png(data, name)
        self.assertIsNotNone(poster, f"{name} was refused")
        return levels(poster)[0]

    def expected(self, image: Image.Image) -> tuple[np.ndarray, np.ndarray]:
        """The poster content ui_png should make, and where the picture is."""
        rgba = image.convert("RGBA")
        width, height = rgba.size
        scale, left, top = fit(width, height)
        size = (max(1, round(width * scale)), max(1, round(height * scale)))
        placed = np.asarray(rgba.resize(size, Image.BILINEAR), np.float64)
        arr = np.asarray(rgba, np.float64)
        weight = arr[..., 3:] / 255
        mean = (arr[..., :3] * weight).sum((0, 1)) / max(weight.sum(), 1e-9)
        out = np.repeat(backdrop(mean)[:, None, :], POSTER_W, 1)
        mask = np.zeros((POSTER_H, POSTER_W), bool)
        x0, y0 = round(left), round(top)
        for y in range(size[1]):
            for x in range(size[0]):
                px, py = x0 + x, y0 + y
                if 0 <= px < POSTER_W and 0 <= py < POSTER_H:
                    a = placed[y, x, 3] / 255
                    out[py, px] = placed[y, x, :3] * a + out[py, px] * (1 - a)
                    mask[py, px] = True
        return out, mask

    def assert_close(self, got: np.ndarray, want: np.ndarray, mask=None, tolerance=9.0, inset=3):
        """Mean error inside the mask, a few pixels in from its edges."""
        got = got[:POSTER_H, :POSTER_W, :3].astype(np.float64)
        if mask is None:
            mask = np.ones(want.shape[:2], bool)
        core = mask.copy()
        for axis in (0, 1):
            for shift in range(1, inset + 1):
                core &= np.roll(mask, shift, axis) & np.roll(mask, -shift, axis)
        self.assertTrue(core.any(), "nothing to compare")
        error = np.abs(got - want)[core].mean()
        self.assertLess(error, tolerance, f"mean error {error:.2f}")

    # -- the picture --------------------------------------------------------

    def test_every_colour_type_and_depth(self):
        image = picture(45, 60)
        rgb = np.asarray(image)
        gray = np.asarray(image.convert("L"))[..., None]
        alpha = np.full(gray.shape, 255, np.uint8)
        quantized = image.quantize(64, dither=Image.Dither.NONE)
        palette = quantized.getpalette()[:64 * 3]
        indices = np.asarray(quantized)[..., None]
        cases = {
            "rgb8": (rgb, 2, 8, None, image),
            "rgb16": (rgb.astype(np.uint16) * 257, 2, 16, None, image),
            "rgba8": (np.concatenate([rgb, alpha], 2), 6, 8, None, image),
            "rgba16": (np.concatenate([rgb, alpha], 2).astype(np.uint16) * 257, 6, 16, None, image),
            "gray8": (gray, 0, 8, None, image.convert("L")),
            "gray16": (gray.astype(np.uint16) * 257, 0, 16, None, image.convert("L")),
            "grayalpha8": (np.concatenate([gray, alpha], 2), 4, 8, None, image.convert("L")),
            "grayalpha16": (np.concatenate([gray, alpha], 2).astype(np.uint16) * 257, 4, 16, None,
                            image.convert("L")),
            "palette8": (indices, 3, 8, palette, quantized.convert("RGB")),
        }
        for name, (samples, colour, depth, pal, reference) in cases.items():
            with self.subTest(name):
                pal_rgb = None if pal is None else [pal[i:i + 3] for i in range(0, len(pal), 3)]
                data = write_png(samples, colour, depth, palette=pal_rgb, filters=(0, 1, 2, 3, 4))
                want, mask = self.expected(reference)
                self.assert_close(self.poster_of(data, name), want, mask)

    def test_low_depths(self):
        """1, 2 and 4 bits a sample, gray and palette, packed high bit first."""
        width, height = 30, 40
        ramp = (np.arange(width)[None, :] * np.ones((height, 1))).astype(np.int64)
        for depth in (1, 2, 4):
            top = (1 << depth) - 1
            levels_ = (ramp * (top + 1) // width).clip(0, top)
            with self.subTest(f"gray{depth}"):
                gray = (levels_ * 255 // top).astype(np.uint8)
                data = write_png(levels_[..., None], 0, depth, filters=(1, 4))
                want, mask = self.expected(Image.fromarray(gray).convert("RGB"))
                self.assert_close(self.poster_of(data, f"gray{depth}"), want, mask, tolerance=12)
            with self.subTest(f"palette{depth}"):
                colours = [[(40 * i) % 256, (90 + 50 * i) % 256, (200 - 30 * i) % 256] for i in range(top + 1)]
                data = write_png(levels_[..., None], 3, depth, palette=colours, filters=(2, 3))
                image = Image.fromarray(np.array(colours, np.uint8)[levels_])
                want, mask = self.expected(image)
                self.assert_close(self.poster_of(data, f"palette{depth}"), want, mask, tolerance=12)

    def test_filters_and_split_data_decode_alike(self):
        """Every row filter, and the data in one IDAT or in many, give the same poster."""
        image = picture(60, 80)
        posters = {}
        for name, kw in {
            "none": {"filters": (0,)}, "sub": {"filters": (1,)}, "up": {"filters": (2,)},
            "average": {"filters": (3,)}, "paeth": {"filters": (4,)},
            "mixed": {"filters": (4, 0, 3, 1, 2)}, "split": {"filters": (1,), "idat_pieces": 97},
        }.items():
            posters[name] = self.run_png(rgb_png(image, **kw), name)
        for name, poster in posters.items():
            self.assertEqual(poster, posters["none"], name)

    def test_quarters_stay_where_they_are(self):
        """Red, green, blue and white quarters land in their places on every
        level: the tile, block, bit and level orders are the hardware's."""
        quarters = np.zeros((80, 60, 3), np.uint8)
        quarters[:40, :30] = (230, 20, 20)
        quarters[:40, 30:] = (20, 230, 20)
        quarters[40:, :30] = (20, 20, 230)
        quarters[40:, 30:] = (240, 240, 240)
        decoded = levels(self.run_png(write_png(quarters, 2, 8), "quarters"))
        for side, level in zip(LEVELS, decoded):
            scale = side / CANVAS
            w, h = POSTER_W * scale, POSTER_H * scale
            spots = {(0.25, 0.25): (230, 20, 20), (0.75, 0.25): (20, 230, 20),
                     (0.25, 0.75): (20, 20, 230), (0.75, 0.75): (240, 240, 240)}
            for (fx, fy), colour in spots.items():
                x, y = int(w * fx), int(h * fy)
                got = level[y, x, :3]
                with self.subTest(side=side, spot=(fx, fy)):
                    self.assertLess(np.abs(got - np.array(colour)).max(), 40, got)
                    self.assertEqual(level[y, x, 3], 255)

    def test_right_band_repeats_the_last_column(self):
        level = self.poster_of(rgb_png(picture(60, 80)), "band")
        last = level[:, POSTER_W - 1, :3]
        for x in range(POSTER_W, CANVAS, 4):
            self.assertLess(np.abs(level[:, x, :3] - last).mean(), 3.0, x)

    def test_mipmaps_average_the_level_above(self):
        decoded = levels(self.run_png(rgb_png(picture(60, 80)), "mips"))
        for above, below in zip(decoded, decoded[1:]):
            shrunk = above[:, :, :3].reshape(below.shape[0], 2, below.shape[1], 2, 3).mean((1, 3))
            self.assertLess(np.abs(shrunk - below[:, :, :3]).mean(), 8.0)

    def test_same_file_same_poster(self):
        data = rgb_png(picture(70, 50))
        self.assertEqual(self.run_png(data, "first"), self.run_png(data, "second"))

    # -- where the picture goes ----------------------------------------------

    def test_poster_shaped_picture_fills_the_poster(self):
        for size in ((150, 200), (300, 400), (140, 200), (160, 200)):
            with self.subTest(size=size):
                image = picture(*size)
                want, mask = self.expected(image)
                self.assertTrue(mask.all())
                self.assert_close(self.poster_of(rgb_png(image), "fill"), want, None)

    def test_other_shapes_sit_inside_on_their_backdrop(self):
        """A square icon, a Homebrew Channel banner, a tall strip and a tiny
        icon (grown four times at most) keep their shape, centred."""
        for size in ((128, 128), (128, 48), (40, 200), (16, 16), (1000, 90)):
            with self.subTest(size=size):
                image = picture(*size)
                got = self.poster_of(rgb_png(image), "fit")
                want, mask = self.expected(image)
                self.assertFalse(mask.all())
                self.assert_close(got, want, mask, tolerance=10)
                self.assert_close(got, want, ~mask, tolerance=6)

    def test_transparency_shows_the_backdrop(self):
        """Alpha, a palette's tRNS, and the transparent key of gray and RGB."""
        width, height = 60, 60
        opaque = np.zeros((height, width), bool)
        opaque[15:45, 15:45] = True
        rgb = np.zeros((height, width, 3), np.uint8)
        rgb[opaque] = (250, 200, 40)
        alpha = np.where(opaque, 255, 0).astype(np.uint8)
        image = Image.fromarray(np.concatenate([rgb, alpha[..., None]], 2), "RGBA")
        want, mask = self.expected(image)
        cases = {
            "alpha": write_png(np.asarray(image), 6, 8),
            "palette-trns": write_png(opaque.astype(np.uint8)[..., None], 3, 8,
                                      palette=[[0, 0, 0], [250, 200, 40]], trns=bytes([0, 255])),
            "rgb-key": write_png(rgb, 2, 8, trns=struct.pack(">HHH", 0, 0, 0)),
        }
        for name, data in cases.items():
            with self.subTest(name):
                got = self.poster_of(data, name)
                self.assert_close(got, want, mask, tolerance=10)
        gray = np.where(opaque, 200, 7).astype(np.uint8)
        got = self.poster_of(write_png(gray[..., None], 0, 8, trns=struct.pack(">H", 7)), "gray-key")
        gray_image = Image.fromarray(np.stack([gray, gray, gray, alpha], -1), "RGBA")
        want, mask = self.expected(gray_image)
        self.assert_close(got, want, mask, tolerance=10)

    def test_largest_picture(self):
        self.assertIsNotNone(self.run_png(rgb_png(picture(2048, 1536)), "large"))
        self.assertIsNotNone(self.run_png(rgb_png(picture(2048, 16)), "sliver"))

    # -- what it refuses ------------------------------------------------------

    def test_refuses_what_it_cannot_read(self):
        good = rgb_png(picture(20, 20))
        ihdr_at = 8
        pixels = np.asarray(picture(20, 20))
        cases = {
            "empty": b"",
            "not a png": b"GIF89a" + good[6:],
            "bad crc": good[:ihdr_at + 29] + bytes([good[ihdr_at + 29] ^ 1]) + good[ihdr_at + 30:],
            "truncated": good[:len(good) - 20],
            "no IEND": good[:-12],
            "interlaced": write_png(pixels, 2, 8, interlace=1),
            "zero width": self.header_only(0, 20, 8, 2),
            "zero height": self.header_only(20, 0, 8, 2),
            "too wide": write_png(np.zeros((1, 2049, 3), np.uint8), 2, 8),
            "too tall": self.header_only(1, 2049, 8, 2),
            "RGB at 4 bits": self.header_only(20, 20, 4, 2),
            "palette at 16 bits": self.header_only(20, 20, 16, 3),
            "colour type 5": self.header_only(20, 20, 8, 5),
            "palette without PLTE": write_png(np.zeros((4, 4, 1), np.uint8), 3, 8),
            "index past palette": write_png(np.full((4, 4, 1), 5, np.uint8), 3, 8,
                                            palette=[[1, 2, 3], [4, 5, 6]]),
            "unknown critical chunk": write_png(pixels, 2, 8, before_idat=chunk(b"CrIT", b"x")),
            "data split by a chunk": self.split_by_chunk(pixels),
            "stream ends early": write_png(pixels, 2, 8, corrupt="short-stream"),
            "bad filter": self.bad_filter(pixels),
            "tRNS on RGBA": write_png(np.concatenate([pixels, pixels[..., :1]], 2), 6, 8,
                                      trns=b"\x00\x01"),
        }
        for name, data in cases.items():
            with self.subTest(name):
                self.assertIsNone(self.run_png(data, "refused"), name)

    def test_ancillary_chunks_are_skipped(self):
        pixels = np.asarray(picture(20, 20))
        plain = self.run_png(write_png(pixels, 2, 8), "plain")
        noted = self.run_png(write_png(pixels, 2, 8, before_idat=chunk(b"tEXt", b"Title\x00app") +
                                       chunk(b"gAMA", struct.pack(">I", 45455)),
                                       after_idat=chunk(b"zzZz", b"later")), "noted")
        self.assertEqual(plain, noted)

    def test_info(self):
        path = os.path.join(self.dir.name, "info.png")
        with open(path, "wb") as f:
            f.write(rgb_png(picture(123, 45)))
        result = run([BINARY, "info", path], text=True)
        self.assertEqual((result.returncode, result.stdout.split()), (0, ["123", "45"]))

    @staticmethod
    def header_only(width: int, height: int, depth: int, colour_type: int) -> bytes:
        """A PNG whose header, CRC and all, says this, over a little data."""
        return (b"\x89PNG\r\n\x1a\n" +
                chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, depth, colour_type, 0, 0, 0)) +
                chunk(b"IDAT", zlib.compress(bytes(64))) + chunk(b"IEND", b""))

    @staticmethod
    def split_by_chunk(pixels: np.ndarray) -> bytes:
        """IDAT, another chunk, IDAT: the data must be one unbroken run."""
        data = write_png(pixels, 2, 8, idat_pieces=2)
        first = data.index(b"IDAT") - 4
        length = struct.unpack(">I", data[first:first + 4])[0]
        end = first + 12 + length
        return data[:end] + chunk(b"tEXt", b"a\x00b") + data[end:]

    @staticmethod
    def bad_filter(pixels: np.ndarray) -> bytes:
        height, width = pixels.shape[:2]
        raw = b"".join(bytes([7]) + pixels[y].tobytes() for y in range(height))
        return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
                chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


class NameTests(unittest.TestCase):
    """An app without a picture: its name as a poster, in the driver's block
    font (letters are 10-pixel-tall blocks in a 12-pixel cell)."""

    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def name(self, text: str, font: str = "name") -> np.ndarray | None:
        out = os.path.join(self.dir.name, "name.bin")
        result = run([BINARY, font, text, out])
        if result.returncode == REFUSED:
            return None
        self.assertEqual(result.returncode, 0, result.stderr)
        with open(out, "rb") as f:
            data = f.read()
        self.assertEqual(len(data), POSTER_BYTES)
        return levels(data)[0]

    @staticmethod
    def bands(canvas: np.ndarray) -> list[tuple[int, int]]:
        """The text's lines: runs of rows with a letter's light in them."""
        lit = (canvas[:, :POSTER_W].astype(int).sum(axis=2) > 600).any(axis=1)
        runs, start = [], None
        for y, on in enumerate(lit.tolist() + [False]):
            if on and start is None:
                start = y
            elif not on and start is not None:
                runs.append((start, y))
                start = None
        return runs

    def test_words_take_lines_of_their_own(self):
        canvas = self.name("gbihf-direct-hdmi")
        runs = self.bands(canvas)
        self.assertEqual(len(runs), 3, runs)
        # Three times the font: each line 30 pixels of letters.
        for start, end in runs:
            self.assertAlmostEqual(end - start, 30, delta=2)
        middle = (runs[0][0] + runs[-1][1]) / 2
        self.assertLess(abs(middle - (POSTER_H / 2 - 10)), 4)

    def test_a_plus_starts_a_word_and_a_long_word_shrinks(self):
        self.assertEqual(len(self.bands(self.name("gbihf-ossc+carby"))), 3)
        runs = self.bands(self.name("abcdefghijklmnopqrstuvwxyz0123456789"))
        self.assertEqual(len(runs), 2, runs)
        self.assertLess(runs[0][1] - runs[0][0], 14)

    def test_what_does_not_fit_stays_in_four_lines(self):
        canvas = self.name("x" * 63, font="name-wide")
        runs = self.bands(canvas)
        self.assertEqual(len(runs), 4, runs)
        lit = (canvas[:, :POSTER_W].astype(int).sum(axis=2) > 600).any(axis=0)
        columns = np.flatnonzero(lit)
        self.assertGreaterEqual(columns.min(), (POSTER_W - 164) // 2 - 2)
        self.assertLessEqual(columns.max(), (POSTER_W + 164) // 2 + 2)

    def test_a_family_shares_its_colour(self):
        def corner(text):
            return self.name(text)[4:40, 4:40].reshape(-1, 3).mean(axis=0)
        ossc, hdmi, other = corner("gbihf-ossc"), corner("gbihf-direct-hdmi"), corner("gbisr-ossc")
        self.assertLess(np.abs(ossc - hdmi).max(), 2)
        self.assertGreater(np.abs(ossc - other).max(), 8)

    def test_backdrop_band_and_repeat(self):
        canvas = self.name("swiss_r2119")
        self.assertEqual(self.name("swiss_r2119").tobytes(), canvas.tobytes())
        for x in range(POSTER_W, CANVAS):
            self.assertTrue(np.array_equal(canvas[:, x], canvas[:, POSTER_W - 1]))
        # Lighter at the top, like a picture's backdrop.
        self.assertGreater(canvas[4:12, 4:40].mean(), canvas[-12:-4, 4:40].mean())

    def test_a_stop_ends_the_work(self):
        """Another thread's stop (here set before the start) ends a poster
        early, of a picture or of a name, and nothing is kept."""
        path = os.path.join(self.dir.name, "stop.png")
        with open(path, "wb") as f:
            f.write(rgb_png(picture(40, 30)))
        out = os.path.join(self.dir.name, "stop.bin")
        for args in (["poster-stopped", path, out], ["name-stopped", "gbihf-ossc", out]):
            with self.subTest(args=args[0]):
                result = run([BINARY] + args)
                self.assertEqual(result.returncode, REFUSED, result.stderr)
        # The same work unstopped makes its poster.
        self.assertIsNotNone(self.name("gbihf-ossc"))

    def test_nothing_to_draw_is_refused(self):
        for text in ("", "---", "_ . -"):
            with self.subTest(text=text):
                self.assertIsNone(self.name(text))
        self.assertIsNone(self.name("gbi", font="name-empty"))


class EncoderTests(unittest.TestCase):
    def encode(self, rgb: np.ndarray) -> np.ndarray:
        side = rgb.shape[0]
        with tempfile.TemporaryDirectory() as d:
            source, target = os.path.join(d, "in.rgb"), os.path.join(d, "out.bin")
            with open(source, "wb") as f:
                f.write(rgb.astype(np.uint8).tobytes())
            subprocess.run([BINARY, "cmpr", source, str(side), target], check=True, timeout=TIMEOUT)
            with open(target, "rb") as f:
                data = f.read()
        self.assertEqual(len(data), side * side // 2)
        return decode_cmpr(data, side)

    def test_flat_blocks_keep_their_colour(self):
        rgb = np.zeros((16, 16, 3), np.uint8)
        rgb[:8, :8] = (255, 0, 0)
        rgb[:8, 8:] = (0, 255, 0)
        rgb[8:, :8] = (0, 0, 255)
        rgb[8:, 8:] = (255, 255, 255)
        got = self.encode(rgb)
        self.assertTrue((got[..., 3] == 255).all())
        self.assertTrue((got[..., :3] == rgb).all())

    def test_two_colour_blocks_are_exact(self):
        """Colours 565 holds exactly come back exactly, each in its place."""
        rgb = np.zeros((8, 8, 3), np.uint8)
        rgb[::2] = (247, 251, 247)
        rgb[1::2] = (8, 4, 8)
        rgb[:, 3] = (8, 4, 8)
        got = self.encode(rgb)
        self.assertTrue((got[..., :3] == rgb).all())
        self.assertTrue((got[..., 3] == 255).all())

    def test_smooth_image_is_close(self):
        side = 64
        x = np.linspace(0, 1, side)[None, :]
        y = np.linspace(0, 1, side)[:, None]
        rgb = np.stack(np.broadcast_arrays(255 * x, 255 * y, 128 + 100 * np.sin(6 * x * y)), -1)
        rgb = rgb.clip(0, 255).astype(np.uint8)
        got = self.encode(rgb)
        self.assertLess(np.abs(got[..., :3] - rgb).mean(), 4.0)
        self.assertTrue((got[..., 3] == 255).all())


# -- memory -------------------------------------------------------------------

def plain_png(width: int, height: int, colour_type: int, depth: int, pixels: bytes | None = None,
              extra: int = 0, level: int = 9) -> bytes:
    """A PNG of unfiltered rows, quick to make at any size: pixels is every
    row's bytes end to end (zeros when None), extra how many more bytes the
    data holds past its last row. A palette PNG gets a one-colour palette."""
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[colour_type]
    row = (width * channels * depth + 7) // 8
    if pixels is None:
        raw = (bytes(row + 1)) * height
    else:
        raw = b"".join(b"\0" + pixels[y * row:(y + 1) * row] for y in range(height))
    ihdr = struct.pack(">IIBBBBB", width, height, depth, colour_type, 0, 0, 0)
    palette = chunk(b"PLTE", b"\x20\x40\x60") if colour_type == 3 else b""
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + palette +
            chunk(b"IDAT", zlib.compress(raw + bytes(extra), level)) + chunk(b"IEND", b""))


class MemoryTests(unittest.TestCase):
    """What making a poster holds, counted by the driver (ui_png's every
    allocation, zlib's included): never more than UI_PNG_MAX_WORK at once,
    whatever the picture, and all of it given back, made or not. A file over
    UI_PNG_MAX_FILE is refused before anything is allocated, and a picture
    over UI_PNG_MAX_SIDE before its rows are. And zlib is never given more
    than CRC_PIECE bytes for one CRC: zlib-ng's CRC of a long run takes a
    table of 32 to 128 KB on the stack, and posters are made on a thread
    with 32 KB (a 2 MB chunk crashed a console)."""

    CRC_PIECE = 8192

    @classmethod
    def setUpClass(cls):
        limits = run([BINARY, "limits"], text=True, check=True)
        cls.max_file, cls.max_side, cls.max_work = map(int, limits.stdout.split())
        probe = run([BINARY, "peak-name", "x"], text=True)
        if probe.returncode != 0:
            raise AssertionError(f"{BINARY} doesn't count allocations: {probe.stderr}")
        cls.dir = tempfile.TemporaryDirectory()

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def peak(self, data: bytes, mode: str = "peak") -> tuple[bool, int, int]:
        """Made or not, the most ui_png held at once, and what it still
        holds; and zlib's CRC was never given too long a run at once."""
        path = os.path.join(self.dir.name, "peak.png")
        with open(path, "wb") as f:
            f.write(data)
        result = run([BINARY, mode, path], text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        made, peak, live, crc = map(int, result.stdout.split())
        self.assertLessEqual(crc, self.CRC_PIECE, "a CRC of a long run at once: the poster thread's stack")
        return bool(made), peak, live

    def test_the_largest_pictures_stay_within_the_bound(self):
        # The canvas and its first mipmap are held for every poster: a peak
        # above them shows the driver counts.
        levels = CANVAS * CANVAS * 3 + (CANVAS // 2) ** 2 * 3
        cases = {
            # The widest rows: 2048 pixels of 16-bit RGBA, 16 KB a row.
            "2048x2048 RGBA, 16 bits": plain_png(2048, 2048, 6, 16),
            "2048x2048 grey, 16 bits": plain_png(2048, 2048, 0, 16),
            "2048x2048 palette, 1 bit": plain_png(2048, 2048, 3, 1),
            "2048x1536 RGB": plain_png(2048, 1536, 2, 8),
            "1536x2048 RGBA, poster-shaped": plain_png(1536, 2048, 6, 8),
            "2048x1": plain_png(2048, 1, 6, 16),
            "1x2048": plain_png(1, 2048, 6, 16),
            # The largest pictures kept whole (reduced by 1): poster-shaped,
            # just over half the poster's scale.
            "383x583 RGBA, 16 bits": plain_png(383, 583, 6, 16),
            "438x511 RGBA, 16 bits": plain_png(438, 511, 6, 16),
            # Data past the last row is never inflated into anything.
            "4 MB more data than its rows": plain_png(64, 64, 2, 8, extra=4 << 20),
            # One chunk of nearly 2 MB, a reader skips it: its CRC is taken
            # a piece at a time (the console's crash, 2026-10-02).
            "a 2 MB ancillary chunk": self.padded(plain_png(64, 64, 2, 8), (2 << 20) - 64),
        }
        for name, data in cases.items():
            with self.subTest(name):
                made, peak, live = self.peak(data)
                self.assertTrue(made)
                self.assertLessEqual(peak, self.max_work)
                self.assertGreater(peak, levels)
                self.assertEqual(live, 0)

    @staticmethod
    def padded(data: bytes, size: int) -> bytes:
        """data padded to size bytes with one ancillary chunk before IEND."""
        pad = size - len(data) - 12
        return data[:-12] + chunk(b"paDd", bytes(pad)) + data[-12:]

    def test_a_file_over_the_limit_is_refused_at_once(self):
        noise = np.random.default_rng(7).integers(0, 256, 1024 * 1024 * 3, np.uint8).tobytes()
        over = plain_png(1024, 1024, 2, 8, noise, level=1)
        self.assertGreater(len(over), self.max_file)
        self.assertEqual(self.peak(over), (False, 0, 0))
        # The same noise, less of it, is under the limit and makes a poster.
        under = plain_png(800, 800, 2, 8, noise, level=1)
        self.assertLess(len(under), self.max_file)
        made, peak, live = self.peak(under)
        self.assertTrue(made)
        self.assertLessEqual(peak, self.max_work)
        self.assertEqual(live, 0)

    def test_a_picture_too_big_is_refused_from_its_header(self):
        for width, height in ((4096, 4096), (self.max_side + 1, 1), (1, self.max_side + 1),
                              (0xFFFFFFFF, 0xFFFFFFFF), (0, 0)):
            with self.subTest(f"{width}x{height}"):
                made, peak, live = self.peak(PosterTests.header_only(width, height, 8, 6))
                self.assertFalse(made)
                self.assertLessEqual(peak, 4096)  # the header, nothing for rows
                self.assertEqual(live, 0)

    def test_what_fails_partway_gives_everything_back(self):
        whole = plain_png(2048, 2048, 6, 16)
        data = zlib.compress(bytes(2048 * (2048 * 8 + 1)), 9)
        cut = (whole[:33] + chunk(b"IDAT", data[:len(data) // 2]) + chunk(b"IEND", b""))
        for name, png, mode in (("data cut short", cut, "peak"),
                                ("stopped", whole, "peak-stopped")):
            with self.subTest(name):
                made, peak, live = self.peak(png, mode)
                self.assertFalse(made)
                self.assertLessEqual(peak, self.max_work)
                self.assertEqual(live, 0)

    def test_a_name_poster_stays_within_the_bound(self):
        result = run([BINARY, "peak-name", "Old saves of every racing game I own"], text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        made, peak, live, _ = map(int, result.stdout.split())
        self.assertEqual(made, 1)
        self.assertLessEqual(peak, self.max_work)
        self.assertGreater(peak, 0)
        self.assertEqual(live, 0)


if __name__ == "__main__":
    unittest.main(verbosity=1)
