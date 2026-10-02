#!/usr/bin/env python3
"""The emulator test's own parts, without Dolphin: the disc, the pad, the checks."""

import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import time
import unittest
from unittest import mock
import zipfile
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

    def test_the_probe_as_a_game(self):
        dol = bytes(range(256)) * 64
        image = card.probe_image(dol)
        self.assertEqual(image[:6], card.PROBE_GAME[0].encode())
        self.assertEqual(len(image) % 0x8000, 0)
        dol_offset, fst_offset, fst_size = struct.unpack_from(">III", image, 0x420)
        self.assertEqual(image[dol_offset:dol_offset + len(dol)], dol)
        self.assertEqual(struct.unpack_from(">II", image, 0x2440 + 0x10), (0x81200000, 0x20))
        fst = image[fst_offset:fst_offset + fst_size]
        self.assertEqual(fst[24:].split(b"\0")[0], b"opening.bnr")
        where, length = struct.unpack_from(">II", fst, 16)
        self.assertEqual(image[where:where + 4], b"BNR1")
        self.assertGreaterEqual(where, dol_offset + len(dol))
        self.assertEqual(struct.unpack_from(">I", image, card.REGION_CODE_AT)[0], 1, "GPRE01 is a US game")

    def test_the_launch_that_fails_is_a_game_from_the_other_region(self):
        with tempfile.TemporaryDirectory() as directory:
            root, work = Path(directory) / "root", Path(directory)
            card.populate(root, work, posters=False, foreign=card.REGION_CODES["P"])
            region = lambda game: struct.unpack_from(">I", (root / "games" / card.game_file(*game)).read_bytes(),
                                                     card.REGION_CODE_AT)[0]
            self.assertEqual(card.library_order(False)[0], card.GAMES[0][1], "the route launches the first game")
            self.assertEqual((region(card.GAMES[0]), region(card.GAMES[1])), (2, 0))

    def test_where_the_probe_sits_in_the_library_and_in_apps(self):
        order = card.library_order(True)
        self.assertEqual(order[:3], card.library_order(False)[:3],
                         "the probe stays out of the games the route browses")
        self.assertIn(card.PROBE_GAME[1], order[3:])
        self.assertEqual(card.app_order(False), sorted(card.APPS, key=str.lower))
        self.assertEqual(card.app_order(True).index("Probe"), 2)

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


class Card(unittest.TestCase):
    @unittest.skipUnless(shutil.which("mkfs.fat") and shutil.which("mcopy"), "needs dosfstools and mtools")
    def test_a_card_made_from_the_release_zip(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            package, probe, image = folder / "Indigo-test.zip", folder / "probe.dol", folder / "card.img"
            with zipfile.ZipFile(package, "w") as z:
                z.writestr("ipl.dol", b"the release's DOL")
                z.writestr("swiss/patches/apploader.img", b"In-Game Reset")
                z.writestr("swiss/ui/", b"")
            probe.write_bytes(bytes(range(256)) * 16)
            info = card.build_card(image, package, posters=False, probe=probe)
            self.assertEqual((image.stat().st_size, info["apps"]), (card.CARD_BYTES, len(card.APPS) + 1))
            self.assertEqual(card.read_card(image, "ipl.dol"), b"the release's DOL")
            self.assertEqual(card.read_card(image, "swiss/patches/apploader.img"), b"In-Game Reset")
            self.assertEqual(card.read_card(image, "apps/Probe.dol"), probe.read_bytes())
            games = subprocess.run(["mdir", "-b", "-i", str(image), "::/games"], capture_output=True, text=True,
                                   env=card.MTOOLS).stdout  # mcopy reads [ID] as a wildcard; mdir lists it
            self.assertIn(card.game_file(*card.PROBE_GAME), games)
            self.assertIn(card.game_file(*card.GAMES[0]), games)
            self.assertIsNone(card.read_card(image, "swiss/settings/global.ini"), "a new card has no settings")
            self.assertIsNone(card.read_card(image, "boot.iso"), "only a GC Loader's card has a boot.iso")
            card.build_card(image, package, posters=False, settings="# start\nClock=Left\n", boot_iso=True)
            self.assertEqual(card.read_card(image, "swiss/settings/global.ini"), b"# start\r\nClock=Left\r\n")
            boot = card.read_card(image, "boot.iso")
            self.assertEqual(boot[:6], card.BOOT_ISO[0].encode())
            self.assertEqual(boot[card.PROBE_DOL_OFFSET:card.PROBE_DOL_OFFSET + 17], b"the release's DOL")

    @unittest.skipUnless(shutil.which("mkfs.fat") and shutil.which("mcopy"), "needs dosfstools and mtools")
    def test_a_file_in_pieces(self):
        """fragment() leaves a file in that many runs of clusters, its bytes
        and the file system whole."""
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            package, image = folder / "Indigo-test.zip", folder / "card.img"
            size = card.MAX_FRAGMENTS * card.CLUSTER_BYTES + 1000
            data = (bytes(range(251)) * (size // 251 + 1))[:size]  # no two clusters alike
            with zipfile.ZipFile(package, "w") as z:
                z.writestr("ipl.dol", b"the release's DOL")
                z.writestr("swiss/patches/game.bin", data)
            card.build_card(image, package, posters=False)
            self.assertEqual(card.fragment(image, "swiss/patches/game.bin", card.MAX_FRAGMENTS + 1),
                             card.MAX_FRAGMENTS + 1)
            self.assertEqual(card.read_card(image, "swiss/patches/game.bin"), data)
            self.assertEqual(card.read_card(image, "ipl.dol"), b"the release's DOL")
            check = subprocess.run(["fsck.fat", "-n", str(image)], capture_output=True, text=True)
            self.assertEqual(check.returncode, 0, check.stdout + check.stderr)
            with self.assertRaises(ValueError):
                card.fragment(image, "ipl.dol", 2)  # one cluster can't be two pieces

    def test_settings_to_start_with(self):
        text = (run.SETTINGS / "non-default.ini").read_text()
        pairs = run.seeded(text)
        self.assertEqual(pairs["Clock"], "Left")
        self.assertNotIn("Menu Widescreen", pairs)
        self.assertTrue(all(not key.startswith("#") for key in pairs))
        self.assertEqual(run.seeded("# Clock=Right\nClock = Off\r\n"), {"Clock": "Off"})


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

    @staticmethod
    def probe_screen(words) -> np.ndarray:
        """The probe's screen as probe.c draws it into a 640x480 framebuffer."""
        rgb = np.zeros((480, 640, 3), np.uint8)
        rgb[88:] = (0, 128, 255)
        for row, word in enumerate(words):
            for bit in range(32):
                rgb[112 + row * 16:128 + row * 16, 64 + bit * 16:80 + bit * 16] = \
                    235 if word >> (31 - bit) & 1 else 16
        return rgb

    def test_the_probe_report_reads_back_at_any_scale(self):
        from PIL import Image
        words = [run.PROBE_MAGIC, 2, int.from_bytes(b"GPRE", "big"), int.from_bytes(b"01\0\0", "big"),
                 0x01800000, 3, 0, 162000000, 486000000, 0x80040000, 0x817FFFC0, 0x81800000, 0x8004, 0x42,
                 0, 1] + [int.from_bytes(b"apps/Probe.dol\0\0"[i:i + 4], "big") for i in range(0, 16, 4)]
        words += [0x0005, 0x00010001]  # VI: enabled, non-interlaced, 54 MHz, a component cable
        words.append(zlib.crc32(b"".join(w.to_bytes(4, "big") for w in words)))
        screen = self.probe_screen(words)
        report = run.probe_report(screen)
        self.assertTrue(report["valid"])
        self.assertEqual((report["disc_id"], report["memsize"], report["path"]),
                         ("GPRE01", 0x01800000, "apps/Probe.dol"))
        self.assertTrue(report["ai_dma"] & run.AI_DMA_ENABLE)
        self.assertEqual(run.video_mode(report), "progressive, NTSC timing, a component cable")
        self.assertEqual(run.video_mode({"vi_dcr": 0x0101, "vi_clk_dtv": 0}),
                         "interlaced, PAL timing, no component cable")
        # Dolphin draws a PAL picture squashed into the window, inside black borders.
        squashed = np.zeros_like(screen)
        squashed[40:440, 29:611] = np.asarray(Image.fromarray(screen).resize((582, 400), Image.NEAREST))
        self.assertEqual(run.probe_report(squashed)["disc_id"], "GPRE01")
        words[4] ^= 1
        self.assertFalse(run.probe_report(self.probe_screen(words))["valid"], "a misread fails its CRC")
        self.assertIsNone(run.probe_report(np.full((480, 640, 3), (35, 25, 60), np.uint8)))

    def test_sram_matches_dolphins_own_and_sets_the_region(self):
        ntsc = run.sram(0, False)
        self.assertEqual(len(ntsc), 0x44)
        self.assertEqual(ntsc[0x04:0x08], bytes.fromhex("002cffd0"), "Dolphin's default SRAM, checksums and all")
        pal60 = run.sram(1, True)
        self.assertEqual((pal60[0x17] & 3, pal60[0x15] >> 6 & 1), (1, 1))
        words = [int.from_bytes(pal60[i:i + 2], "big") for i in range(0x10, 0x18, 2)]
        self.assertEqual(int.from_bytes(pal60[4:6], "big"), sum(words) & 0xFFFF)

    def test_the_consoles_clock_times_a_wait(self):
        """Waits count the console's seconds from Dolphin's TICKS lines (patch 0005)."""
        with tempfile.TemporaryDirectory() as directory:
            emulator = run.Emulator.__new__(run.Emulator)
            emulator.log = Path(directory) / "dolphin.log"
            self.assertIsNone(emulator.emulated(), "no log yet")
            emulator.log.write_text("Booting\nTICKS 486000000 PC 80003100 LR 00000000\n")
            self.assertEqual(emulator.emulated(), 1.0)
            wait = run.Deadline(emulator, 2.0)
            self.assertFalse(wait.expired())
            with emulator.log.open("a") as log:
                log.write("N[OSREPORT]: a line between\nTICKS 1701000000 PC 801179a0 LR 8011774c\n")
            self.assertAlmostEqual(wait.elapsed(), 2.5)
            self.assertTrue(wait.expired())
            self.assertEqual(emulator.where(), "PC 801179a0 LR 8011774c at 3.5 s")

    def test_without_the_consoles_clock_a_wait_counts_the_machines(self):
        with tempfile.TemporaryDirectory() as directory:
            emulator = run.Emulator.__new__(run.Emulator)
            emulator.log = Path(directory) / "dolphin.log"
            emulator.log.write_text("Booting\n")
            wait = run.Deadline(emulator, 0.05)
            self.assertFalse(wait.expired())
            time.sleep(0.06)
            self.assertTrue(wait.expired())
            self.assertIsNone(emulator.where())

    def press_until(self, screens, answers):
        """press_until with the screen's text and settled_label's answers scripted."""
        route = run.Route.__new__(run.Route)
        route.pressed_again, presses = [], []
        route.press = lambda button, seconds=0: presses.append(button)
        route.gray = lambda: None
        route.settled_label = lambda *args, **kwargs: next(answers)
        with mock.patch.object(run, "text_mask", lambda frame, box=None: next(screens)):
            found = route.press_until("RIGHT")[0]
        return found, presses, route.pressed_again

    def test_a_press_the_menu_missed_is_pressed_again(self):
        """Nothing changed after the press: press again, and say so."""
        face = np.ones((4, 4), bool)
        found, presses, again = self.press_until(iter([face, face]), iter([(None, 0.0), (face, 1.0)]))
        self.assertIs(found, face)
        self.assertEqual((presses, again), (["RIGHT", "RIGHT"], ["RIGHT"]))

    def test_a_press_that_changed_the_screen_is_never_repeated(self):
        """The screen moved, just not yet to what was wanted: wait, never press twice."""
        before, moved = np.ones((4, 4), bool), np.zeros((4, 4), bool)
        found, presses, again = self.press_until(iter([before, moved]), iter([(None, 0.0), (None, 1.0)]))
        self.assertIsNone(found)
        self.assertEqual((presses, again), (["RIGHT"], []))

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
