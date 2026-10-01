#!/usr/bin/env python3
"""The emulator test's own parts, without Dolphin: the disc, the pad, the checks."""

import socket
import struct
import sys
import tempfile
import time
import unittest
import zlib
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import card  # noqa: E402
import dsu_pad  # noqa: E402
import run  # noqa: E402


class Disc(unittest.TestCase):
    def test_a_game_is_what_the_library_reads(self):
        image = card.game_image(0, "GACZ01", "Astral Circuit")
        self.assertEqual(len(image), card.STUB_BYTES)
        self.assertEqual(image[:6], b"GACZ01")
        self.assertEqual(struct.unpack_from(">I", image, 0x1C)[0], card.MAGIC)
        self.assertEqual(image[0x20:0x2E], b"Astral Circuit")
        offset, size = struct.unpack_from(">II", image, 0x424)
        fst = image[offset:offset + size]
        entries = struct.unpack_from(">I", fst, 8)[0]
        name = fst[12 * entries + int.from_bytes(fst[13:16], "big"):].split(b"\0")[0]
        self.assertEqual((fst[0], entries, fst[12], name), (1, 2, 0, b"opening.bnr"))
        where, length = struct.unpack_from(">II", fst, 16)
        banner = image[where:where + length]
        self.assertEqual((banner[:4], length), (b"BNR1", 0x1960))
        self.assertEqual(banner[0x1820:0x1840].rstrip(b"\0"), b"Astral Circuit")
        self.assertTrue(all(struct.unpack_from(">H", banner, 0x20 + 2 * i)[0] & 0x8000
                            for i in range(96 * 32)), "every banner pixel is opaque RGB555")

    def test_the_disc_header_and_its_empty_file_table(self):
        header = card.outer_header()
        self.assertEqual(header[:6], card.DISC[0].encode())
        self.assertEqual(struct.unpack_from(">I", header, 0x1C)[0], card.MAGIC)
        self.assertEqual(struct.unpack_from(">III", header, 0x424), (card.FST_OFFSET, 12, 12))
        self.assertEqual(header[card.FST_OFFSET:], bytes([1] + [0] * 10 + [1]))
        self.assertLess(len(header), card.SYSTEM_AREA)

    def test_damaged_images(self):
        broken = card.header_only("GBHZ01", "Broken Header")
        self.assertEqual((len(broken), broken[:6]), (0x440, b"GBHZ01"))
        self.assertEqual(struct.unpack_from(">III", broken, 0x424), (0, 0, 0))
        runaway = card.runaway_table("GCTZ01", "Corrupt Table")
        offset, size = struct.unpack_from(">II", runaway, 0x424)
        count = struct.unpack_from(">I", runaway, offset + 8)[0]
        self.assertGreater(12 * count, size)
        self.assertEqual(12 * count, 0x7FFFFFF8, "wraps an unchecked lookup below 0x80000000")
        third = sorted(title for _, title in card.GAMES)[2]
        for _, title, _ in card.DAMAGED:
            self.assertLess(title, third, "the route browses only the first games")

    def test_ids_and_names(self):
        ids = [game_id for game_id, _ in card.GAMES] + [game_id for game_id, _, _ in card.DAMAGED]
        self.assertEqual(len(set(ids)), len(ids))
        self.assertTrue(card.NO_POSTER < {game_id for game_id, _ in card.GAMES})
        # Spotlight shows every kind of picture: a still, a cover without a
        # still, and a banner card with neither.
        games = {game_id for game_id, _ in card.GAMES}
        self.assertTrue(card.NO_STILL < games)
        self.assertTrue(card.NO_STILL - card.NO_POSTER)
        self.assertTrue(card.NO_STILL & card.NO_POSTER)
        with self.assertRaises(ValueError):
            card.disc_header("gacz01", "lower case")

    def test_folders_go_past_the_second_level(self):
        games = {game_id for game_id, _ in card.GAMES}
        self.assertLessEqual(set(card.FOLDERS), games)
        depths = sorted(path.count("/") + 1 for path in card.FOLDERS.values())
        self.assertEqual(depths, [1, 2, 3], "a folder, a folder in it, and one past the second level")
        # The route reaches Racing with one RIGHT from the empty stray folder.
        self.assertLess(card.STRAYS[1].lower(), "racing/")

    def test_the_stray_file_sorts_before_every_game(self):
        titles = [title for _, title in card.GAMES] + [title for _, title, _ in card.DAMAGED]
        self.assertLess(card.STRAYS[0].lower(), min(titles).lower(),
                        "the Library must skip a stray that Swiss lists ahead of the games")
        self.assertTrue(card.STRAYS[1].endswith("/"), "and an empty folder")

    def test_banner_fields_sit_where_swiss_reads_them(self):
        # BNRDesc after the pixels: name 0x20, publisher 0x20, full name 0x40,
        # full publisher 0x40, description 0x80 (include/bnr.h).
        data = card.banner(0, "Astral Circuit")
        text = lambda at, size: data[at:at + size].split(b"\0")[0].decode()
        self.assertEqual(len(data), 0x20 + 96 * 32 * 2 + 0x140)
        self.assertEqual(text(0x1820, 0x20), "Astral Circuit")
        self.assertEqual(text(0x1840, 0x20), "Indigo test disc")
        self.assertEqual(text(0x1860, 0x40), "Astral Circuit")
        self.assertEqual(text(0x18A0, 0x40), "Indigo demonstration disc")
        self.assertEqual(text(0x18E0, 0x80), card.DESCRIPTION)

    def test_apps_folder(self):
        """/apps as Apps reads it: stand-in programs, pictures of each shape
        Indigo fits to a card, and a Homebrew Channel folder whose boot.dol
        (the Wii's) and other files Apps leaves out."""
        with tempfile.TemporaryDirectory() as directory:
            apps = Path(directory) / "apps"
            self.assertEqual(card.build_apps(apps), len(card.APPS))
            self.assertEqual((apps / "Arcade.dol").read_bytes(), card.APP_STUB)
            programs = [p for p in apps.rglob("*.dol") if p.name.lower() != "boot.dol"]
            self.assertEqual(tuple(sorted((p.stem for p in programs), key=str.lower)), card.APPS)
            self.assertTrue((apps / "Toolbox/boot.dol").exists())
            for picture, size in (("Pixel Painter.png", (16, 16)), ("Starfield.png", (300, 400)),
                                  ("Toolbox/icon.png", (128, 48))):
                from PIL import Image
                with Image.open(apps / picture) as image:
                    self.assertEqual(image.size, size, picture)
                    self.assertFalse(image.info.get("interlace"), picture)
            self.assertFalse((apps / "Arcade.png").exists())  # a card with its name

    def test_posters_differ(self):
        self.assertNotEqual(card.poster(0).tobytes(), card.poster(1).tobytes())

    def test_descriptions_leave_one_game_to_its_banner(self):
        games = {game_id for game_id, _ in card.GAMES}
        self.assertEqual(len(games - set(card.DESCRIPTIONS)), 1)
        self.assertTrue(set(card.DESCRIPTIONS) < games)
        for text in card.DESCRIPTIONS.values():
            self.assertTrue(text.isascii() and "\n" not in text and len(text) <= 300)

    def test_stills_are_4_3_and_differ(self):
        self.assertEqual(card.still(0).size, (640, 480))
        self.assertNotEqual(card.still(0).tobytes(), card.still(1).tobytes())


class Pad(unittest.TestCase):
    def test_wire_format(self):
        info = bytearray(dsu_pad.port_info(1, 0, True))
        self.assertEqual((len(info), info[:4], info[20:22]), (32, b"DSUS", b"\0\2"))
        crc = struct.unpack_from("<I", info, 8)[0]
        struct.pack_into("<I", info, 8, 0)
        self.assertEqual(zlib.crc32(info), crc)
        data = dsu_pad.pad_data(1, 7, frozenset({"A", "RIGHT", "START"}))
        self.assertEqual((len(data), struct.unpack_from("<H", data, 6)[0]), (100, 84))
        self.assertEqual((data[49], data[46], data[36], data[48]), (255, 255, 0x08, 0))
        self.assertEqual(data[40:44], bytes((128,) * 4))

    def test_unplugged_until_plugged_in_then_streams(self):
        pad = dsu_pad.Pad(0)
        client = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        client.settimeout(2)
        try:
            def send(kind, body=b""):
                payload = struct.pack("<I", kind) + body
                client.sendto(b"DSUC" + struct.pack("<HHII", 1001, len(payload), 0, 1) + payload,
                              ("127.0.0.1", pad.port))
            send(dsu_pad.PORT_INFO, struct.pack("<I4B", 4, 0, 1, 2, 3))
            self.assertEqual(client.recv(64)[21], 0)
            pad.plug_in()
            send(dsu_pad.PORT_INFO, struct.pack("<I4B", 4, 0, 1, 2, 3))
            self.assertEqual(client.recv(64)[21], 2)
            pad.hold("B")
            send(dsu_pad.PAD_DATA, bytes(8))
            deadline = time.monotonic() + 2
            while not pad.streaming and time.monotonic() < deadline:
                time.sleep(0.01)
            self.assertEqual(client.recv(128)[50], 255)
            with self.assertRaises(ValueError):
                pad.hold("SELECT")
        finally:
            client.close()
            pad.close()


class Screen(unittest.TestCase):
    def frame(self, words=()) -> np.ndarray:
        gray = np.full((run.HEIGHT, run.WIDTH), 40, np.uint8)
        for x0, x1 in words:
            gray[378:390, x0:x1:3] = 220
        return gray

    def test_a_label_is_found_and_compared(self):
        library, source = self.frame([(292, 346)]), self.frame([(300, 336)])
        a, b = run.text_mask(library), run.text_mask(source)
        self.assertTrue(run.has_label(a) and run.has_label(b))
        self.assertGreaterEqual(run.overlap(a, run.text_mask(library.copy())), run.SAME)
        self.assertLess(run.overlap(a, b), run.DIFFERENT)
        self.assertFalse(run.has_label(run.text_mask(self.frame())))

    def test_waves_behind_the_label_do_not_count(self):
        waves = self.frame()
        waves[372:396, 200:440] = 120
        self.assertFalse(run.text_mask(waves).any())

    def test_crash_and_black_screens_are_named(self):
        crash = np.zeros((run.HEIGHT, run.WIDTH, 3), np.uint8)
        crash[20:120, 20:400:2] = 255
        self.assertIn("crashed", run.diagnose(crash))
        self.assertIn("black", run.diagnose(np.zeros((run.HEIGHT, run.WIDTH, 3), np.uint8)))
        indigo = np.full((run.HEIGHT, run.WIDTH, 3), (35, 25, 60), np.uint8)
        self.assertEqual(run.diagnose(indigo), "")

    def test_fatal_lines(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "dolphin.log"
            log.write_text("This title might be incompatible with DSP HLE emulation.\n"
                           "DSPHLE: Unknown ucode (CRC = 8d527c50) - forcing AX.\n")
            self.assertEqual(run.fatal_lines(log), [])
            log.write_text("Invalid read from 0x05117080, PC = 0x8016cd18; the game probably would have crashed\n")
            self.assertEqual(len(run.fatal_lines(log)), 1)


if __name__ == "__main__":
    unittest.main()
