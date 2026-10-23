#!/usr/bin/env python3
"""manual_pack.py: a manual's pages become the file Game Details opens.

Draws pages with Pillow (paper, a scanner's dark edge, lines standing for
text, a spread, a tall page, a near-empty one), converts them, and checks
the GX textures against what they were made from, decoded the way the
GameCube draws them. Then runs the files it writes, whole and damaged,
through test_ui_manual, the console's own checks (gui/ui_manual.c). A PDF
is converted too when pypdfium2 is installed; no real manual is used.

usage: test_manual_pack.py [BINARY]   (BINARY: test_ui_manual; without it the
                                       console checks are skipped)
"""

from __future__ import annotations

import os
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

BINARY = os.path.abspath(sys.argv.pop(1)) if len(sys.argv) > 1 else None
TOOL = Path(__file__).resolve().parents[1] / "manual_pack.py"
sys.path.insert(0, str(TOOL.parent))
import manual_pack as mp  # noqa: E402

PAPER = (236, 232, 222)
INK = (20, 20, 24)


def scan(width: int = 1425, height: int = 2160, border: bool = True, lines: int = 30) -> Image.Image:
    """A 300 dpi scan of a GameCube-sized page: paper, a dark edge the scanner
    left along the top, lines of 'text' inside a margin, a red picture."""
    page = Image.new("RGB", (width, height), PAPER)
    draw = ImageDraw.Draw(page)
    if border:
        draw.rectangle((0, 0, width - 1, 40), fill=(30, 30, 30))
    margin = width // 6
    for n in range(lines):
        y = height // 7 + n * (height // 2 // lines)
        draw.rectangle((margin, y, margin + (width - 2 * margin) * (60 + n * 13 % 40) // 100, y + 9), fill=INK)
    draw.rectangle((margin, height * 4 // 5, width - margin, height * 4 // 5 + 200), fill=(200, 40, 40))
    return page


def luma(image: Image.Image) -> np.ndarray:
    return np.asarray(image.convert("L"), dtype=np.int32)


def decoded(data: bytes, info: dict, page: dict, half: bool = False) -> Image.Image:
    record = data[page["offset"]:page["offset"] + info["record"]]
    full = mp.WIDTH * info["height"] // 2
    decode = mp.decode_i4 if info["format"] == mp.GX_TF_I4 else mp.decode_cmpr
    if half:
        return decode(record[full:], mp.WIDTH // 2, info["height"] // 2)
    return decode(record[:full], mp.WIDTH, info["height"])


class EncoderTests(unittest.TestCase):
    def test_cmpr_blocks_land_in_place(self):
        # Every 4x4 block its own colour: a block in the wrong place shows.
        tile = Image.new("RGB", (16, 16))
        for by in range(4):
            for bx in range(4):
                tile.paste((bx * 80, by * 80, 255 - bx * 40), (bx * 4, by * 4, bx * 4 + 4, by * 4 + 4))
        back = mp.decode_cmpr(mp.encode_cmpr(tile), 16, 16)
        self.assertLessEqual(np.abs(np.asarray(back, int) - np.asarray(tile, int)).max(), 8)

    def test_cmpr_never_transparent(self):
        rng = np.random.default_rng(7)
        noise = Image.fromarray(rng.integers(0, 256, (64, 64, 3), dtype=np.uint8), "RGB")
        blocks = np.frombuffer(mp.encode_cmpr(noise), np.uint8).reshape(-1, 8).astype(int)
        c0, c1 = blocks[:, 0] << 8 | blocks[:, 1], blocks[:, 2] << 8 | blocks[:, 3]
        # Four-colour blocks, or one colour with every texel on it.
        self.assertTrue(np.all((c0 > c1) | ((c0 == c1) & (blocks[:, 4:] == 0).all(1))))

    def test_i4_tiles(self):
        ramp = Image.frombytes("L", (16, 16), bytes((x * 16 + y) & 255 for y in range(16) for x in range(16)))
        back = mp.decode_i4(mp.encode_i4(ramp), 16, 16)
        self.assertLessEqual(np.abs(luma(back) - luma(ramp)).max(), 8)

    def test_page_reads_as_it_was(self):
        page = mp.fit(scan(border=False))
        canvas = Image.new("RGB", (640, -(-page.size[1] // 16) * 16), PAPER)
        canvas.paste(page)
        for encode, decode in ((mp.encode_cmpr, mp.decode_cmpr), (mp.encode_i4, mp.decode_i4)):
            back = decode(encode(canvas), *canvas.size)
            error = np.abs(luma(back) - luma(canvas))
            self.assertLess(error.mean(), 6, encode.__name__)
            # The 'text' stays dark: what the eye reads.
            ink = luma(canvas) < 80
            self.assertLess(luma(back)[ink].mean(), 60, encode.__name__)


class PackTests(unittest.TestCase):
    def test_pages_and_their_halves(self):
        data = mp.build([scan(), scan(lines=10)])
        info, pages = mp.read(data)
        self.assertEqual((info["pages"], info["format"]), (2, mp.GX_TF_CMPR))
        self.assertEqual(info["height"] % 16, 0)
        self.assertEqual(info["record"], 640 * info["height"] // 2 + 320 * info["height"] // 4)
        self.assertTrue(all(page["intact"] for page in pages))
        for page in pages:
            self.assertEqual(page["width"], 640)
            whole = decoded(data, info, page, half=True)
            full = decoded(data, info, page).reduce(2)
            box = (0, 0, 320, page["height"] // 2)
            self.assertLess(np.abs(luma(whole.crop(box)) - luma(full.crop(box))).mean(), 4)
        self.assertEqual(data, mp.build([scan(), scan(lines=10)]), "the same input gives the same bytes")

    def test_header_layout(self):
        data = mp.build([scan()], gray=True)
        fields = struct.unpack_from(">4sIIIIIIIIHHHHBBH", data)
        self.assertEqual(fields[0:2], (b"SWMN", 1))
        self.assertEqual(fields[3:8], (1, 64, 32, 96, len(data)))
        height = fields[10]
        self.assertEqual(fields[9:16], (640, height, 320, height // 2, 2, mp.GX_TF_I4, 0))
        self.assertFalse(any(data[0x30:64]) or any(data[64 + 16:96]))

    def test_margins_and_scanner_edge_trimmed(self):
        # The side margins (a sixth of the page each) go, so the same 640
        # pixels hold less of the page: it is drawn bigger. So do the
        # scanner's dark edge and the paper above the text.
        x0, y0, x1, y1 = mp.content_box(scan())
        self.assertLess(x1 - x0, 1425 * 0.75)
        self.assertGreater(y0, 200)
        # ...but nothing of what is printed: not the first line of text, not
        # the picture at the bottom (a band of content at the trimmed edge is
        # not a scanner's border).
        self.assertLess(y0, 2160 // 7)
        self.assertGreater(y1, 2160 * 4 // 5 + 200)
        data = mp.build([scan()])
        info, pages = mp.read(data)
        top = luma(decoded(data, info, pages[0]))[:4]
        self.assertGreater(top.min(), 200, "the scanner's dark edge is gone")
        kept = mp.build([scan()], trim=False)
        info, pages = mp.read(kept)
        self.assertLess(luma(decoded(kept, info, pages[0]))[:4].mean(), 100, "kept as scanned")

    def test_near_empty_page_kept_whole(self):
        page = Image.new("RGB", (1425, 2160), PAPER)
        ImageDraw.Draw(page).ellipse((650, 1000, 775, 1125), fill=INK)
        self.assertEqual(mp.content_box(page), (0, 0, 1425, 2160))

    def test_spread_is_two_pages(self):
        spread = Image.new("RGB", (2850, 2160), PAPER)
        spread.paste(scan(border=False), (0, 0))
        spread.paste(scan(border=False, lines=5), (1425, 0))
        self.assertEqual(len(mp.split(spread)), 2)
        self.assertEqual(len(mp.split(scan())), 1)

    def test_tall_page_fits(self):
        info, pages = mp.read(mp.build([scan(900, 3000, border=False)], trim=False))
        self.assertEqual(info["height"], mp.MAX_HEIGHT)
        self.assertEqual(pages[0]["height"], mp.MAX_HEIGHT)
        self.assertLess(pages[0]["width"], 640)

    def test_page_count_limits(self):
        with self.assertRaises(mp.PackError):
            mp.build([])
        with self.assertRaises(mp.PackError):
            mp.build([Image.new("RGB", (8, 8))] * (mp.MAX_PAGES + 1))

    def test_damage_is_found(self):
        data = bytearray(mp.build([scan(), scan()]))
        info, pages = mp.read(bytes(data))
        data[pages[1]["offset"] + 100] ^= 1
        _, pages = mp.read(bytes(data))
        self.assertEqual([page["intact"] for page in pages], [True, False])
        data[70] ^= 1                                   # the index
        with self.assertRaises(mp.PackError):
            mp.read(bytes(data))
        with self.assertRaises(mp.PackError):
            mp.read(bytes(data[:-1]))


class CommandTests(unittest.TestCase):
    def run_tool(self, *args: str, cwd: str | None = None) -> subprocess.CompletedProcess:
        return subprocess.run([sys.executable, str(TOOL), *args], capture_output=True, text=True,
                              cwd=cwd, timeout=300)

    def test_folder_of_pages(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp, "scans")
            folder.mkdir()
            scan().save(folder / "01.png")
            spread = Image.new("RGB", (2850, 2160), PAPER)
            spread.paste(scan(border=False), (0, 0))
            spread.paste(scan(border=False, lines=5), (1425, 0))
            spread.save(folder / "02.jpg", quality=95)
            (folder / "notes.txt").write_text("not a page")
            out = Path(tmp, "out.pak")
            result = self.run_tool(str(folder), "--id", "gchz01", "--out", str(out), "--preview",
                                   str(Path(tmp, "preview")))
            self.assertEqual(result.returncode, 0, result.stderr)
            info, _ = mp.read(out.read_bytes())
            self.assertEqual(info["pages"], 3)
            self.assertTrue(Path(tmp, "preview", "p03-whole.png").exists())
            result = self.run_tool(str(folder), "--id", "GCHZ01", "--pages", "2", "--gray", cwd=tmp)
            self.assertEqual(result.returncode, 0, result.stderr)
            info, _ = mp.read(Path(tmp, "GCHZ01.pak").read_bytes())
            self.assertEqual((info["pages"], info["format"]), (2, mp.GX_TF_I4))

    def test_game_id(self):
        with tempfile.TemporaryDirectory() as tmp:
            scan().save(Path(tmp, "p.png"))
            result = self.run_tool(tmp, "--id", "GCH01")
            self.assertEqual(result.returncode, 1)
            self.assertIn("six letters and digits", result.stderr)
            disc = Path(tmp, "game.iso")
            disc.write_bytes(b"GDRZ01" + bytes(0x16) + struct.pack(">I", mp.DISC_MAGIC) + bytes(64))
            result = self.run_tool(tmp, "--disc", str(disc), cwd=tmp)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue(Path(tmp, "GDRZ01.pak").exists())
            disc.write_bytes(bytes(64))
            self.assertEqual(self.run_tool(tmp, "--disc", str(disc)).returncode, 1)
            self.assertEqual(self.run_tool(tmp, "--id", "GDRZ01", "--pages", "2-3").returncode, 1)

    def test_pdf(self):
        try:
            import pypdfium2  # noqa: F401
        except ImportError:
            self.skipTest("pypdfium2 not installed: the PDF path is untested here")
        with tempfile.TemporaryDirectory() as tmp:
            spread = Image.new("RGB", (2850, 2160), PAPER)
            spread.paste(scan(border=False), (0, 0))
            spread.paste(scan(border=False, lines=5), (1425, 0))
            pdf = Path(tmp, "Manual.pdf")
            scan().save(pdf, save_all=True, append_images=[spread], resolution=300)
            out = Path(tmp, "m.pak")
            result = self.run_tool(str(pdf), "--id", "GEIZ01", "--out", str(out))
            self.assertEqual(result.returncode, 0, result.stderr)
            info, pages = mp.read(out.read_bytes())
            self.assertEqual(info["pages"], 3)
            self.assertTrue(all(page["intact"] for page in pages))
            Path(tmp, "broken.pdf").write_bytes(b"%PDF-1.4 not really")
            self.assertEqual(self.run_tool(str(Path(tmp, "broken.pdf")), "--id", "GEIZ01").returncode, 1)


@unittest.skipUnless(BINARY, "no test_ui_manual binary given")
class ConsoleTests(unittest.TestCase):
    """The console's checks, on files manual_pack.py writes."""

    def read(self, *datas: bytes) -> list[str]:
        with tempfile.TemporaryDirectory() as tmp:
            paths = []
            for n, data in enumerate(datas):
                paths.append(str(Path(tmp, f"{n}.pak")))
                Path(paths[-1]).write_bytes(data)
            result = subprocess.run([BINARY, *paths], capture_output=True, text=True, timeout=120)
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout.splitlines()

    def test_files(self):
        colour = mp.build([scan(), scan(lines=10), scan(900, 3000, border=False)])
        gray = mp.build([scan()], gray=True)
        info, pages = mp.read(colour)
        damaged = bytearray(colour)
        damaged[pages[2]["offset"] + info["record"] - 1] ^= 0x80
        index = bytearray(colour)
        index[64 + 32 + 12] ^= 1                        # a page's width
        self.assertEqual(self.read(colour, gray, bytes(damaged), colour[:-1], colour + b"\0", bytes(index),
                                   b"SWMN"), [
            f"ok pages=3 height={info['height']} format=14 intact=3",
            f"ok pages=1 height={mp.read(gray)[0]['height']} format=0 intact=1",
            f"ok pages=3 height={info['height']} format=14 intact=2",
            "refused", "refused", "refused", "refused"])


if __name__ == "__main__":
    unittest.main(verbosity=1)
