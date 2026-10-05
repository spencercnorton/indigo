#!/usr/bin/env python3
"""The emulator test's own parts, without Dolphin: the disc, the pad, the checks."""

import itertools
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import threading
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

    def test_folders_go_past_the_second_level(self):
        games = {game_id for game_id, _ in card.GAMES}
        self.assertLessEqual(set(card.FOLDERS), games)
        depths = sorted(path.count("/") + 1 for path in card.FOLDERS.values())
        self.assertEqual(depths, [1, 2, 3], "a folder, a folder in it, and one past the second level")
        # The first two root folder cards are Nintendo.GC and Racing.v1.
        self.assertLess(card.STRAYS[1].lower(), "racing.v1/")
        self.assertEqual(card.FOLDERS["GPLZ01"], "Racing.v1/Classics.Set/Old.Saves.v2")
        self.assertTrue(all("." in part and not part.startswith(".")
                            for folder in card.FOLDERS.values() for part in folder.split("/")))
        self.assertEqual(card.FOLDER_PICTURES["Racing.v1"], "shown")
        self.assertEqual(card.FOLDER_PICTURES["Racing.v1/Classics.Set"], "damaged")

    def test_populated_dotted_folders_and_sibling_png_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            root = work / "root"
            card.populate(root, work, posters=False)
            self.assertEqual(list((root / "games/Nintendo.GC").iterdir()), [])
            for game_id, title in card.GAMES:
                self.assertEqual((root / "games" / card.game_path(game_id, title)).read_bytes()[:6],
                                 game_id.encode("ascii"))
            for folder, kind in card.FOLDER_PICTURES.items():
                self.assertEqual((root / "games" / f"{folder}.png").read_bytes(),
                                 card.folder_picture(kind))

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

    def test_the_library_order_follows_paths(self):
        # Swiss sorts the flattened /games by path, case aside: a game in a
        # folder sorts by its folder, and a folder's own games by theirs.
        order = card.library_order(True)
        foldered = {title for game_id, title in card.GAMES if game_id in card.FOLDERS}
        self.assertEqual([title for title in order if title in foldered],
                         ["Neon Tidepool", "Paper Lantern", "Rally Cross Zero"])
        self.assertLess(order.index(card.PROBE_GAME[1]), order.index("Neon Tidepool"))

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
            card.build_card(image, package, posters=False, settings="# start\nClock=Left\n", boot_iso=True,
                            virtual_cards=True)
            self.assertEqual(card.read_card(image, "swiss/settings/global.ini"), b"# start\r\nClock=Left\r\n")
            boot = card.read_card(image, "boot.iso")
            self.assertEqual(boot[:6], card.BOOT_ISO[0].encode())
            self.assertEqual(boot[card.PROBE_DOL_OFFSET:card.PROBE_DOL_OFFSET + 17], b"the release's DOL")
            raw, _ = run.make_test_saves.virtual_card()
            folder = run.make_test_saves.SAVE_FOLDER
            self.assertEqual(card.read_card(image, f"{folder}/{run.make_test_saves.RAW_CARD_NAME}"), raw)
            self.assertIsNone(card.read_card(image, f"{folder}/{run.make_test_saves.RAW_EXPORT_NAME}"),
                              "only Indigo's Copy operation should create the export")

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

    def test_wide_save_details_seed_survives_readback(self):
        text = (run.SETTINGS / "save-details-wide.ini").read_text()
        start = run.seeded(text)
        self.assertEqual(start, {"Menu Widescreen": "Yes", "Swiss Video Mode": "Auto",
                                 "Hide Apps Face": "No"})
        route = run.Route.__new__(run.Route)
        route.checks, route.report, route.last_rgb = [], None, None
        route.folders_on = False
        route.emulator = mock.Mock(where=lambda: None)
        with mock.patch.object(card, "read_card", return_value=text.replace("\n", "\r\n").encode()):
            route.card_checks(Path("synthetic.img"), "virtual-cards", start)
        self.assertEqual([check["check"] for check in route.checks],
                         ["the configured settings remain on the card",
                          "the settings the card started with are all still there"])
        self.assertTrue(all(check["passed"] for check in route.checks))
        with mock.patch.object(card, "read_card", return_value=text.replace("Menu Widescreen=Yes",
                                                                          "Menu Widescreen=No").encode()):
            with self.assertRaises(run.Failed):
                route.card_checks(Path("synthetic.img"), "virtual-cards", start)


class MemoryCards(unittest.TestCase):
    """The memory cards the smoke route opens Memory Cards with, and what it
    reads off the screen and the cards' folders."""

    def test_a_gci_folder_card_in_each_slot(self):
        ini = run.dolphin_ini(cards=Path("/work/cards"))
        core = ini.split("[Core]\n", 1)[1].split("[", 1)[0]
        self.assertIn("SlotA = 8\nSlotB = 8\n", core)
        self.assertIn("GCIFolderAPathOverride = /work/cards/A\n", core)
        self.assertIn("GCIFolderBPathOverride = /work/cards/B\n", core)
        both = run.dolphin_ini("sd2sp2", Path("/work/card.img"), Path("/work/cards"))
        self.assertIn("SerialPort2 = 15\nSP2SDCardImage = /work/card.img\nSlotA = 8\n", both)
        self.assertNotIn("Slot", run.dolphin_ini("gcloader"))
        self.assertEqual(run.dolphin_ini(), run.DOLPHIN_INI)

    def test_named_storage_menus_select_sd_then_restore_each_slot(self):
        for button, side, box, expected in (
                ("R", "right", run.RIGHT_HEADER_BOX, ["R", "DOWN", "A", "R", "UP", "A"]),
                ("L", "left", run.LEFT_HEADER_BOX, ["L", "DOWN", "DOWN", "A", "L", "UP", "UP", "A"])):
            with self.subTest(button=button):
                route = run.Route.__new__(run.Route)
                route.storage = "gcloader"
                pressed, checks = [], []
                route.press = pressed.append
                route.pause = lambda seconds: None
                route.gray = lambda: np.zeros((run.HEIGHT, run.WIDTH), np.uint8)
                route.emulator = mock.Mock()
                route.last_rgb = None
                route.shot = lambda *args: None
                route.differs = lambda before, where: where == box
                route.text_until = lambda where, predicate: np.zeros((2, 2), np.uint8)
                route.check = lambda name, passed, **detail: checks.append(passed)
                route.swap(button, box, side)
                self.assertEqual(pressed, expected)
                self.assertEqual(checks, [True, True])

    def test_storage_menu_cannot_select_sd_without_a_configuration_device(self):
        for button, side, box, expected in (
                ("R", "right", run.RIGHT_HEADER_BOX, ["R", "DOWN", "A", "B"]),
                ("L", "left", run.LEFT_HEADER_BOX, ["L", "DOWN", "DOWN", "A", "B"])):
            route = run.Route.__new__(run.Route)
            route.storage = "dvd"
            pressed, checks = [], []
            route.press = pressed.append
            route.pause = lambda seconds: None
            route.gray = lambda: np.zeros((run.HEIGHT, run.WIDTH), np.uint8)
            route.emulator, route.last_rgb = mock.Mock(), None
            route.shot = lambda *args: None
            route.text_until = lambda where, predicate: np.zeros((2, 2), np.uint8)
            route.check = lambda name, passed, **detail: checks.append(passed)
            route.swap(button, box, side)
            self.assertEqual(pressed, expected)
            self.assertEqual(checks, [True, True])

    def test_virtual_cards_has_no_physical_card_in_either_slot(self):
        ini = run.dolphin_ini("gcloader", Path("/work/card.img"), empty_slots=True)
        self.assertIn("SlotA = 0\nSlotB = 0\n", ini)
        self.assertNotIn("GCIFolder", ini)
        sp2 = run.dolphin_ini("sd2sp2", Path("/work/card.img"), empty_slots=True)
        self.assertIn("SerialPort2 = 15\nSP2SDCardImage = /work/card.img\n", sp2)
        self.assertIn("SlotA = 0\nSlotB = 0\n", sp2)

    def test_public_raw_fixture_has_real_metadata_and_a_fragmented_save(self):
        raw, gcis = run.make_test_saves.virtual_card()
        block = run.make_test_saves.BLOCK
        self.assertEqual(gcis[0][:6], b"GACZ01")
        self.assertEqual(struct.unpack_from(">I", gcis[0], 0x28)[0], 762525240)
        self.assertEqual(struct.unpack_from(">I", gcis[1], 0x28)[0], 0)
        alternate, variants = run.make_test_saves.virtual_card("GALE01", 1)
        self.assertEqual(variants[0][:6], b"GALE01")
        self.assertEqual(struct.unpack_from(">I", variants[0], 0x28)[0], 1)
        self.assertNotEqual(raw, alternate)
        for invalid in ("GACZ", "gacz01", "GACZ??", "GACZ01x"):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                run.make_test_saves.virtual_card(invalid)
        self.assertEqual((len(raw), len(gcis[0]), len(gcis[1])), (64 * block, 64 + 2 * block, 64 + block))
        self.assertEqual(raw[block:2 * block], raw[2 * block:3 * block])
        self.assertEqual(raw[3 * block:4 * block], raw[4 * block:5 * block])
        for part, start, end, stored in ((raw[:block], 0, 0x1FC, 0x1FC),
                                         (raw[block:2 * block], 0, 0x1FFC, 0x1FFC),
                                         (raw[3 * block:4 * block], 4, block, 0)):
            words = struct.unpack(f">{(end - start) // 2}H", part[start:end])
            total, inverse = sum(words) & 0xFFFF, sum(w ^ 0xFFFF for w in words) & 0xFFFF
            self.assertEqual(struct.unpack_from(">HH", part, stored),
                             (0 if total == 0xFFFF else total, 0 if inverse == 0xFFFF else inverse))
        self.assertEqual(struct.unpack_from(">HH", raw, 0x22), (4, 0))
        bat = raw[3 * block:4 * block]
        self.assertEqual(struct.unpack_from(">HH", bat, 6), (56, 9))
        self.assertEqual((struct.unpack_from(">H", bat, 5 * 2)[0],
                          struct.unpack_from(">H", bat, 9 * 2)[0]), (9, 0xFFFF))
        # A contiguous read accidentally includes the other save at block6.
        self.assertNotEqual(raw[5 * block:7 * block], gcis[0][64:])
        self.assertEqual(raw[5 * block:6 * block] + raw[9 * block:10 * block], gcis[0][64:])

    def test_details_proof_rejects_source_change_or_an_early_export(self):
        raw, gcis = run.make_test_saves.virtual_card()
        for actual, export, passes in ((raw, None, True),
                                      (raw, gcis[0], False),
                                      (raw[:-1] + bytes([raw[-1] ^ 1]), None, False)):
            with self.subTest(passes=passes):
                route = run.Route.__new__(run.Route)
                route.sd_image, route.checks, route.report, route.last_rgb = Path("test.img"), [], None, None
                route.emulator = mock.Mock(where=lambda: None)
                with mock.patch.object(card, "read_card", side_effect=[actual, export]):
                    if passes:
                        route.virtual_popup_checks()
                        self.assertEqual(len(route.checks), 2)
                    else:
                        with self.assertRaises(run.Failed):
                            route.virtual_popup_checks()

    def test_virtual_export_proof_rejects_wrong_payload_and_changed_source(self):
        raw, gcis = run.make_test_saves.virtual_card()
        exported = bytearray(gcis[0])
        exported[0x36:0x38] = b"\x00\x05"  # the image's first block is retained
        with tempfile.TemporaryDirectory() as directory:
            for changed_raw, changed_export, passed in ((raw, bytes(exported), True),
                    (raw, bytes(exported[:-1]) + bytes([exported[-1] ^ 1]), False),
                    (raw[:-1] + bytes([raw[-1] ^ 1]), bytes(exported), False)):
                with self.subTest(passed=passed):
                    route = run.Route.__new__(run.Route)
                    route.out, route.report, route.last_rgb = Path(directory), None, None
                    route.checks = []
                    route.emulator = mock.Mock(where=lambda: None)
                    with mock.patch.object(card, "read_card", side_effect=[changed_raw, changed_export]):
                        if passed:
                            route.virtual_card_checks(Path("test.img"))
                            self.assertEqual(len(route.checks), 2)
                        else:
                            with self.assertRaises(run.Failed):
                                route.virtual_card_checks(Path("test.img"))

    def test_the_cards_hold_saves_dolphin_lists(self):
        with tempfile.TemporaryDirectory() as directory:
            cards = Path(directory)
            run.make_test_saves.write(str(cards))
            slot_a, slot_b = run.saves(cards / "A"), run.saves(cards / "B")
            self.assertEqual((len(slot_a), len(slot_b)), (21, 18))
            # Slot A's first save in Dolphin's order (by name) isn't on Slot B
            # already, so the route's Copy of it isn't dimmed.
            self.assertNotIn(min(slot_a), slot_b)
            (cards / "A" / min(slot_a)).rename(cards / "A" / (min(slot_a) + ".deleted"))
            self.assertEqual(len(run.saves(cards / "A")), 20, "an erased save isn't one")

    def test_a_copy_on_another_card_is_the_same_save(self):
        original = run.make_test_saves.encode(run.make_test_saves.SLOT_A[0], 0)
        copy = bytearray(original)
        copy[0x10] ^= 1  # a letter of its name
        self.assertFalse(run.same_save(original, bytes(copy)))
        copy = bytearray(original)
        copy[0x28:0x2C] = b"\x00\x01\x02\x03"  # when it was written, and
        copy[0x36:0x38] = b"\x00\x40"  # its first block: the card's own
        self.assertTrue(run.same_save(original, bytes(copy)))
        copy[64 + 5000] ^= 1
        self.assertFalse(run.same_save(original, bytes(copy)), "a block that differs")
        self.assertFalse(run.same_save(original, original[:-1]))
        other = run.make_test_saves.encode(run.make_test_saves.SLOT_A[1], 0)
        self.assertFalse(run.same_save(original, other))

    def test_the_message_and_the_arrow_are_found(self):
        rgb = np.full((run.HEIGHT, run.WIDTH, 3), (35, 25, 60), np.uint8)
        self.assertFalse(run.message_up(rgb))
        x0, y0, x1, y1 = run.MESSAGE_BOX
        icons = rgb.copy()
        for x in range(x0, x1 - 32, 56):
            icons[y0:y0 + 32, x:x + 32] = (140, 35, 35)  # a red icon's darker half
        self.assertFalse(run.message_up(icons))
        rgb[y0:y1, x0:x1] = (112, 15, 34)  # the maroon box over the cubes
        rgb[y0 + 18:y0 + 32, x0 + 100:x1 - 100] = 255  # its words
        self.assertTrue(run.message_up(rgb))
        gray = np.full((run.HEIGHT, run.WIDTH), 40, np.uint8)
        self.assertFalse(run.arrow_up(gray))
        ax0, ay0, ax1, ay1 = run.UP_ARROW_BOX
        for row in range(9):  # a 14 x 9 triangle, point up
            middle = (ax0 + ax1) // 2
            gray[ay0 + 4 + row, middle - row * 7 // 9:middle + row * 7 // 9 + 1] = 255
        self.assertTrue(run.arrow_up(gray))

    def step(self, frames, unlike):
        """Route.info, a step in Memory Cards, over scripted frames."""
        with tempfile.TemporaryDirectory() as directory:
            emulator = run.Emulator.__new__(run.Emulator)
            emulator.log = Path(directory) / "dolphin.log"
            emulator.log.write_text("Booting\n")
            route = run.Route.__new__(run.Route)
            route.emulator, presses = emulator, []
            route.press = presses.append
            route.pause = lambda seconds: None
            frames = iter(frames)
            route.gray = lambda: next(frames)
            with mock.patch.object(run, "SETTLE_SECONDS", 0.3), mock.patch.object(run.time, "sleep"):
                found = route.info("RIGHT", unlike=unlike)
            self.assertEqual(presses, ["RIGHT"])
            return found

    def test_two_names_in_bold_are_told_apart(self):
        """Two saves' names in the info bar's bold words can overlap by more
        than half: a step waits, steady, for other words by same_text."""
        def words(*spans):
            gray = np.full((run.HEIGHT, run.WIDTH), 40, np.uint8)
            for x0, x1 in spans:
                gray[382:394, x0:x1] = 230
                gray[386:388, x0:x1:4] = 40
            return gray
        copper, snowglobe = words((168, 250)), words((168, 230), (236, 262))
        first = run.text_mask(copper, run.INFO_BOX)
        self.assertGreater(run.overlap(first, run.text_mask(snowglobe, run.INFO_BOX)), run.DIFFERENT)
        arriving = words((168, 200))  # the name as it comes, before it settles
        found = self.step([copper, arriving, snowglobe, snowglobe], first)
        self.assertTrue(found is not None and run.same_text(found, run.text_mask(snowglobe, run.INFO_BOX)))
        self.assertIsNone(self.step(itertools.repeat(copper), first), "the same name never counts")


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
    def folder_frame(self, name):
        from PIL import Image
        return np.asarray(Image.open(Path(__file__).parent / "fixtures" / name).convert("RGB"))

    def test_actual_legacy_folder_chrome_and_its_fades(self):
        legacy = self.folder_frame("folder-legacy-browser.png")
        for strength in (1.0, 0.75, 0.5, 0.25, 0.1):
            self.assertTrue(run.legacy_folder_browser((legacy * strength).astype(np.uint8)), strength)
        for name in ("folder-horizontal.png", "folder-vertical.png", "folder-grid.png",
                     "folder-spotlight.png", "folder-home.png"):
            modern = self.folder_frame(name)
            for strength in (1.0, 0.75, 0.5, 0.25, 0.1):
                self.assertFalse(run.legacy_folder_browser((modern * strength).astype(np.uint8)),
                                 (name, strength))
        for frame in (np.zeros_like(legacy), np.full_like(legacy, 255),
                      np.full_like(legacy, (128, 16, 96))):
            self.assertFalse(run.legacy_folder_browser(frame))
        for box in ((0, 80, 148, 214), (148, 100, 610, 350)):
            missing = legacy.copy()
            x0, y0, x1, y1 = box
            missing[y0:y1, x0:x1] = 0
            self.assertFalse(run.legacy_folder_browser(missing), box)
        with self.assertRaises(run.Broken):
            run.legacy_folder_browser(legacy[:400])
        self.assertEqual((run.TEXT_LEVEL, run.SAME, run.DIFFERENT), (160, 0.85, 0.5))

    def test_native_frame_reader_catches_one_legacy_frame_and_checks_each_index(self):
        from PIL import Image
        for inject in (None, 5, 10):
            with self.subTest(inject=inject), tempfile.TemporaryDirectory() as directory:
                folder = Path(directory) / "dump"
                folder.mkdir()
                out = Path(directory) / "proof"
                reader = run.PresentedFrames(folder, out)
                modern = self.folder_frame("folder-horizontal.png")
                legacy = self.folder_frame("folder-legacy-browser.png")
                def write(index, rgb):
                    Image.fromarray(rgb).save(folder / f"framedump_{index}.png")
                write(1, modern)
                write(2, modern)
                span = reader.begin("one-frame-control")
                for index in range(3, 9):
                    write(index, legacy if index == inject else modern)
                # Reader must wait for the next file: no partially written
                # image is accepted merely because its name exists.
                result = []
                end = threading.Thread(target=lambda: (reader.end(span), result.append(True)))
                end.start()
                time.sleep(0.05)
                self.assertTrue(end.is_alive())
                # Delayed encoder/readback tail: frame10 arrives after the
                # end request at8. A one-frame legacy flash there must still
                # be included, and N is consumed only after N+1 appears.
                for index in range(9, 30):
                    write(index, legacy if index == inject else modern)
                    time.sleep(0.02)
                    if not end.is_alive():
                        break
                end.join(3)
                self.assertEqual(result, [True])
                reader.finish()
                proof = reader.evidence(span)
                self.assertEqual((proof["first"], proof["end_requested"]), (2, 8))
                self.assertGreaterEqual(proof["last"], 10)
                self.assertEqual(proof["frames"], proof["last"] - proof["first"] + 1)
                self.assertEqual(proof["bad_frames"], [f"legacy-{inject}.png"] if inject else [])
                self.assertEqual(len(proof["samples"]), 3)
                self.assertEqual(len(proof["sha256_rgb_sequence"]), 64)
                self.assertFalse(list(folder.iterdir()), "temporary dump files are consumed")

    def test_native_frame_reader_missing_index_is_an_error(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory) / "dump"
            folder.mkdir()
            reader = run.PresentedFrames(folder, Path(directory) / "proof")
            with self.assertRaises(run.Broken):
                reader.begin("not-started")
            modern = self.folder_frame("folder-horizontal.png")
            for index in (1, 3, 4):
                Image.fromarray(modern).save(folder / f"framedump_{index}.png")
            until = time.monotonic() + 3
            while not reader.error and time.monotonic() < until:
                time.sleep(0.01)
            with self.assertRaisesRegex(run.Broken, "gap at 2"):
                reader.finish()

    def test_native_frame_span_registration_cannot_race_the_processed_watermark(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory) / "dump"
            folder.mkdir()
            reader = run.PresentedFrames(folder, Path(directory) / "proof")
            modern = self.folder_frame("folder-horizontal.png")
            Image.fromarray(modern).save(folder / "framedump_1.png")
            entered, release = threading.Event(), threading.Event()
            original = reader.latest
            def old_directory_snapshot():
                entered.set()
                if not release.wait(3):
                    raise RuntimeError("test never released directory enumeration")
                return 1
            spans = []
            with mock.patch.object(reader, "latest", side_effect=old_directory_snapshot):
                begin = threading.Thread(target=lambda: spans.append(reader.begin("registration")))
                begin.start()
                self.assertTrue(entered.wait(3))
                Image.fromarray(modern).save(folder / "framedump_2.png")
                time.sleep(0.05)
                self.assertEqual(reader.next, 1, "registration holds the consumer lock")
                release.set()
                begin.join(3)
            self.assertEqual(spans[0]["first"], 1)
            with reader.lock:
                spans[0]["last"] = 2
            reader.finish()
            self.assertEqual(spans[0]["frames"], 2)
            # A stale directory snapshot must also not include an index
            # already processed before registration acquired the lock.
            with mock.patch.object(reader, "latest", return_value=1):
                self.assertEqual(reader.begin("stale-snapshot")["first"], reader.next)

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

    def test_a_label_half_a_line_lower_is_the_same(self):
        library = self.frame([(292, 346)])
        a = run.text_mask(library)
        lower = run.text_mask(np.roll(library, 1, axis=0))
        self.assertLess(run.overlap(a, lower), run.SAME)
        self.assertTrue(run.same_text(a, lower))
        self.assertFalse(run.same_text(a, run.text_mask(self.frame([(300, 336)]))))

    def test_waves_behind_the_label_do_not_count(self):
        waves = self.frame()
        waves[372:396, 200:440] = 120
        self.assertFalse(run.text_mask(waves).any())

    def test_stage_fields_follow_menu_widescreen_projection(self):
        self.assertEqual(run.stage_box((100, 10, 540, 20)), (100, 10, 540, 20))
        self.assertEqual(run.stage_box((100, 10, 540, 20), True), (155, 68, 485, 75))

    def test_wide_detection_restores_authored_fields_without_editing_capture(self):
        rgb = np.zeros((run.HEIGHT, run.WIDTH, 3), np.uint8)
        # A small actual-wide glyph band:52 native bright pixels is below the
        # unchanged60-pixel floor, though it is visible in the letterbox.
        x0, y0, _, _ = run.stage_box(run.LIBRARY_SAVES_UPDATED_BOX, True)
        rgb[y0 + 4:y0 + 8, x0 + 8:x0 + 60:4] = 183
        original = rgb.copy()
        native = run.text_mask(rgb.max(axis=2), run.stage_box(run.LIBRARY_SAVES_UPDATED_BOX, True))
        self.assertEqual(int(native.sum()), 52)
        self.assertFalse(run.has_label(native))
        detected = run.detection_frame(rgb, True)
        restored = run.text_mask(detected.max(axis=2), run.LIBRARY_SAVES_UPDATED_BOX)
        self.assertTrue(run.has_label(restored))
        self.assertEqual(set(np.unique(detected)), {0, 183}, "no invented pixel values")
        np.testing.assert_array_equal(rgb, original, "screenshots stay native and unchanged")
        self.assertIs(run.detection_frame(rgb), rgb)

    def test_actual_wide_date_capture_and_missing_field(self):
        from PIL import Image
        source = Path(__file__).resolve().parent / "fixtures/wide-save-stats.png"
        rgb = np.asarray(Image.open(source).convert("RGB"))
        original = rgb.copy()
        detected = run.detection_frame(rgb, True).max(axis=2)
        self.assertTrue(run.has_label(run.text_mask(detected, run.LIBRARY_SAVES_UPDATED_BOX)))
        missing = rgb.copy()
        x0, y0, x1, y1 = run.stage_box(run.LIBRARY_SAVES_UPDATED_BOX, True)
        missing[y0:y1, x0:x1] = (16, 14, 40)
        self.assertFalse(run.has_label(run.text_mask(
            run.detection_frame(missing, True).max(axis=2), run.LIBRARY_SAVES_UPDATED_BOX)))
        self.assertGreater(int(run.text_mask(run.detection_frame(missing, True).max(axis=2),
                                            (266, 224, 307, 241)).sum()), 0,
                           "the Updated label cannot replace its missing date")
        np.testing.assert_array_equal(rgb, original, "the native capture remains untouched")

    def test_actual_block_values_distinguish_one_and_two_in_both_shapes(self):
        from PIL import Image
        root = Path(__file__).resolve().parent / "fixtures"
        for shape, wide in (("default", False), ("wide", True)):
            gray = [run.detection_frame(np.asarray(Image.open(root /
                    (shape + "-save-details-" + name + ".png")).convert("RGB")), wide).max(axis=2)
                    for name in ("known", "unknown")]
            values = [run.text_mask(frame, run.SAVE_DETAILS_BLOCKS_BOX) for frame in gray]
            self.assertTrue(all(run.has_save_number(value) for value in values), shape)
            self.assertFalse(run.same_text(*values), shape + " actually distinguishes 2 from 1")
            self.assertTrue(all(run.same_text(value, value.copy()) for value in values))
            kib = [run.text_mask(frame, run.SAVE_DETAILS_SIZE_BOX) for frame in gray]
            self.assertTrue(all(run.has_save_number(value) for value in kib), shape)
            self.assertFalse(run.same_text(*kib), shape + " actually distinguishes 16 from 8")
            dates = [run.text_mask(frame, run.SAVE_DETAILS_UPDATED_BOX) for frame in gray]
            self.assertFalse(run.same_text(*dates), "recorded date and Unknown differ")
            metadata = [run.text_mask(frame, run.SAVE_DETAILS_ICON_BOX) for frame in gray]
            self.assertTrue(all(run.has_label(value) for value in metadata),
                            "the save icon status must be visible in both native captures")
            self.assertFalse(run.same_text(*metadata), "Animated and None stored differ")

    def test_actual_popup_value_only_boxes_reject_erasure_and_label_only_frames(self):
        from PIL import Image
        root = Path(__file__).resolve().parent / "fixtures"
        fields = ((run.SAVE_DETAILS_BLOCKS_BOX, (122, 214, 302, 234), True),
                  (run.SAVE_DETAILS_SIZE_BOX, (350, 214, 528, 234), True),
                  (run.SAVE_DETAILS_SOURCE_BOX, (100, 244, 234, 270), False),
                  (run.SAVE_DETAILS_ICON_BOX, (100, 304, 234, 330), False),
                  (run.SAVE_DETAILS_UPDATED_BOX, (100, 274, 234, 300), False))
        for name, wide in (("default-save-details-known.png", False),
                           ("default-save-details-unknown.png", False),
                           ("wide-save-details-known.png", True),
                           ("wide-save-details-unknown.png", True),
                           ("themed-save-details.png", False)):
            rgb = np.asarray(Image.open(root / name).convert("RGB"))
            original = rgb.copy()
            gray = run.detection_frame(rgb, wide).max(axis=2)
            self.assertTrue(run.save_details_panel(gray), name)
            for value_box, label_box, numeric in fields:
                predicate = run.has_save_number if numeric else run.has_label
                self.assertTrue(predicate(run.text_mask(gray, value_box)), (name, value_box))
                missing_native = rgb.copy()
                x0, y0, x1, y1 = run.stage_box(value_box, wide)
                missing_native[y0:y1, x0:x1] = 0
                missing = run.detection_frame(missing_native, wide).max(axis=2)
                self.assertFalse(predicate(run.text_mask(missing, value_box)), (name, "erased", value_box))
                self.assertGreater(int(run.text_mask(missing, label_box).sum()), 0,
                                   (name, "caption/label is still visible", value_box))
                self.assertTrue(run.save_details_panel(missing), "an erased value cannot hide the dialog")
            label_only_native = rgb.copy()
            for value_box, _, _ in fields:
                x0, y0, x1, y1 = run.stage_box(value_box, wide)
                label_only_native[y0:y1, x0:x1] = 0
            label_only = run.detection_frame(label_only_native, wide).max(axis=2)
            for value_box, _, numeric in fields:
                predicate = run.has_save_number if numeric else run.has_label
                self.assertFalse(predicate(run.text_mask(label_only, value_box)), (name, "labels only"))
            for box in (run.SAVE_DETAILS_TITLE_BOX, run.SAVE_DETAILS_ICON_BOX,
                        run.SAVE_DETAILS_UPDATED_BOX, run.SAVE_DETAILS_ACTIONS_BOX):
                self.assertTrue(run.has_label(run.text_mask(gray, box)), (name, box))
            np.testing.assert_array_equal(rgb, original, "positive native capture remains untouched")

    def test_numeric_metric_rejects_pixel_short_bar_and_tall_thin_noise(self):
        self.assertEqual(run.TEXT_LEVEL, 160)
        empty = np.zeros((34, 180), bool)
        self.assertFalse(run.has_save_number(empty))
        pixel = empty.copy(); pixel[10, 8] = True
        self.assertFalse(run.has_save_number(pixel))
        bar = empty.copy(); bar[10, 8:40] = True
        self.assertFalse(run.has_save_number(bar), "32-pixel bar is too short to be a glyph")
        thin = empty.copy(); thin[2:32, 8] = True
        self.assertFalse(run.has_save_number(thin), "30-pixel line is too narrow")
        short = empty.copy(); short[8:15, 8:12] = True
        self.assertFalse(run.has_save_number(short), "28-pixel short patch is too low")
        self.assertFalse(run.has_save_number(np.ones_like(empty)), "dense block is not sparse text")

    def test_failed_check_keeps_its_native_frame(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as directory:
            route = run.Route.__new__(run.Route)
            route.out, route.shots, route.checks, route.report = Path(directory), [], [], None
            route.emulator = mock.Mock(where=lambda: None)
            route.last_rgb = np.full((run.HEIGHT, run.WIDTH, 3), (12, 23, 34), np.uint8)
            with self.assertRaises(run.Failed):
                route.check("a synthetic failed checkpoint", False)
            self.assertEqual(len(route.shots), 1)
            self.assertEqual(route.checks[0]["picture"], route.shots[0][1].name)
            np.testing.assert_array_equal(np.asarray(Image.open(route.shots[0][1])), route.last_rgb)

    def test_popup_frame_rejects_browser_cube_pixels(self):
        from PIL import Image
        root = Path(__file__).resolve().parent / "fixtures"
        for name, wide in (("themed-save-details.png", False),
                           ("wide-save-details-known.png", True),
                           ("wide-save-details-unknown.png", True)):
            gray = run.detection_frame(np.asarray(Image.open(root/name).convert("RGB")), wide).max(axis=2)
            self.assertTrue(run.save_details_panel(gray), name)
        browser = np.asarray(Image.open(root/"themed-save-browser.png").convert("RGB")).max(axis=2)
        self.assertFalse(run.save_details_panel(browser))

    def test_popup_open_marker_is_independent_of_variable_name_and_requires_every_border(self):
        from PIL import Image
        root = Path(__file__).resolve().parent / "fixtures"
        for name, wide in (("default-save-details-known.png", False),
                           ("default-save-details-unknown.png", False),
                           ("wide-save-details-known.png", True),
                           ("wide-save-details-unknown.png", True),
                           ("themed-save-details.png", False)):
            gray = run.detection_frame(np.asarray(Image.open(root/name).convert("RGB")), wide).max(axis=2)
            self.assertTrue(run.save_details_panel(gray), name)
            marker = run.text_mask(gray, run.SAVE_DETAILS_EYEBROW_BOX)
            self.assertTrue(run.has_save_details_eyebrow(marker), name)
            if name == "themed-save-details.png":
                self.assertLess(int(marker.sum()), 60, "the small themed marker is not a body word")
            missing_name = gray.copy()
            x0, y0, x1, y1 = run.SAVE_DETAILS_TITLE_BOX
            missing_name[y0:y1, x0:x1] = 0
            self.assertTrue(run.save_details_panel(missing_name), "opening does not depend on name length")
            missing_marker = gray.copy()
            x0, y0, x1, y1 = run.SAVE_DETAILS_EYEBROW_BOX
            missing_marker[y0:y1, x0:x1] = 0
            self.assertFalse(run.save_details_panel(missing_marker), name)
            for box in ((70, 108, 78, 386), (564, 108, 572, 386),
                        (92, 88, 548, 96), (92, 399, 548, 407)):
                missing_edge = gray.copy()
                x0, y0, x1, y1 = box
                missing_edge[y0:y1, x0:x1] = 0
                self.assertFalse(run.save_details_panel(missing_edge), (name, box))

    def test_fixed_marker_rejects_pixel_bar_narrow_tall_and_dense_noise(self):
        self.assertEqual(run.TEXT_LEVEL, 160)
        empty = np.zeros((24, 174), bool)
        self.assertFalse(run.has_save_details_eyebrow(empty))
        pixel = empty.copy(); pixel[10, 8] = True
        self.assertFalse(run.has_save_details_eyebrow(pixel))
        bar = empty.copy(); bar[10, 8:60] = True
        self.assertFalse(run.has_save_details_eyebrow(bar), "long bar is too low")
        narrow = empty.copy(); narrow[8:14, 8:20] = True
        self.assertFalse(run.has_save_details_eyebrow(narrow), "short patch is too narrow")
        tall = empty.copy(); tall[2:18, 8:68:12] = True
        self.assertFalse(run.has_save_details_eyebrow(tall), "tall noise exceeds the fixed glyph height")
        self.assertFalse(run.has_save_details_eyebrow(np.ones_like(empty)), "dense block is not sparse text")

    def test_save_details_retry_never_presses_a_after_popup_or_context_change(self):
        from PIL import Image
        root = Path(__file__).resolve().parent / "fixtures"
        browser = np.asarray(Image.open(root/"themed-save-browser.png").convert("RGB")).max(axis=2)
        popup = np.asarray(Image.open(root/"themed-save-details.png").convert("RGB")).max(axis=2)
        changed = browser.copy()
        x0, y0, x1, y1 = run.INFO_BOX
        changed[y0:y1, x0:x1] = 0

        class ShortWait:
            def __init__(self, *args):
                self.calls = 0

            def expired(self):
                self.calls += 1
                return self.calls > 2

        def opening(initial, after):
            route = run.Route.__new__(run.Route)
            route.emulator, route.pressed_again = object(), []
            presses = []
            route.press = lambda button: presses.append(button)
            route.gray = lambda: initial if not presses else after(len(presses))
            with mock.patch.object(run, "Deadline", ShortWait), mock.patch.object(run.time, "sleep"):
                opened = route.open_save_details()
            return opened, presses, route.pressed_again

        self.assertEqual(opening(browser, lambda n: browser if n == 1 else popup),
                         (True, ["A", "A"], ["A"]), "one missed press retries, then stops at popup")
        self.assertEqual(opening(browser, lambda n: popup), (True, ["A"], []))
        self.assertEqual(opening(popup, lambda n: popup), (True, [], []), "an existing popup never gets Actions")
        self.assertEqual(opening(browser, lambda n: changed), (False, ["A"], []),
                         "another screen change forbids any repeated A")
        reverted = iter((changed, browser))
        self.assertEqual(opening(browser, lambda n: next(reverted, browser)), (False, ["A"], []),
                         "a transient context change still forbids any repeated A")

    def test_raw_animation_proof_requires_both_texture_colors_on_the_cube(self):
        rgb = np.full((run.HEIGHT, run.WIDTH, 3), (35, 25, 60), np.uint8)
        # Arbitrary cube/background motion without authored patch colors is
        # not a frame. Nor are the colors somewhere else in the picture.
        rgb[220:240, 300:330] = run.make_test_saves.RAW_ICON_COLOURS[0]
        self.assertIsNone(run.raw_icon_frame(rgb))
        for wide in (False, True):
            x0, y0, _, _ = run.stage_box(run.RAW_ICON_BOX, wide)
            for frame, colour in enumerate(run.make_test_saves.RAW_ICON_COLOURS):
                picture = rgb.copy()
                picture[y0+15:y0+25, x0+15:x0+25] = colour
                self.assertEqual(run.raw_icon_frame(picture, wide), frame)
                # The other color must appear; translating this static
                # texture cannot be mistaken for that second frame.
                self.assertEqual(run.raw_icon_frame(np.roll(picture, 2, axis=1), wide), frame)

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
            log.write_text("MMU.cpp:599 N[PowerPC]: DABR: write to 0x8082f278 at PC 0x8018e9c0, "
                           "the doubleword the DABR guards\n")
            self.assertEqual(len(run.fatal_lines(log)), 1, "a stack reached its guard")


if __name__ == "__main__":
    unittest.main()
