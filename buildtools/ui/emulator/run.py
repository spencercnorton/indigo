#!/usr/bin/env python3
"""Boot an Indigo DOL in Dolphin and drive its menus with a controller.

Dolphin (headless: software OpenGL on a virtual X server) boots the DOL with
the demonstration disc (card.py) in the drive. Once Home is up a controller
is plugged in (dsu_pad.py) and a route of button presses walks the cube and
opens each face. Every check compares the screen with itself earlier in the
same run, never with stored pictures, so a redesign does not break the test
while a crash, a hang, a black screen or a broken control does:

  - Home appears after boot, with a face's name under the cube;
  - turning the cube changes the name, five turns come back to the first
    (the disc has apps, so Apps is a fifth face), turning back undoes a
    turn, and the five names differ;
  - A on a face opens another screen, and B comes back to the same face;
  - in the Library, RIGHT and LEFT move between games and back again, and
    A opens a game's details; there UP and A open the game's settings, B
    comes back to the details, and B again to the game;
  - on the Source face, Change Source opens the device picker, RIGHT shows
    another device and B leaves it;
  - on the System face, Memory Cards opens with a memory card in each slot
    (GCI folders of made-up saves, ../qa/make_test_saves.py): RIGHT and LEFT
    move from save to save and across to Slot B and back, DOWN scrolls a
    stack, R and L choose a named storage place and back, A opens the box
    beside a save and B closes it, Copy puts the save on Slot B (its folder
    gains the file, the same blocks), Move is dimmed for it then, Erase
    removes it from Slot A's folder, and B leaves;
  - on the Settings face, Setup > Console > Apps Face Off takes Apps off the
    cube (System's next face is Library) and On puts it back;
  - Setup > Console > Cube Classic lays the faces out as the GameCube's menu
    does, Library between them: from Settings, LEFT goes nowhere and RIGHT
    twice is Library, then System; there UP goes nowhere and B is Library;
    UP is Source, DOWN Library, DOWN Apps and UP Library. Cube goes back to
    Infinite after;
  - Setup > Library > Library Folders On shows the disc's folders in the
    Library: an empty folder opens with only its way back, A opens a folder
    and a folder in it, which lists the game a level further down too, and B
    goes back up a folder at a time to the same card, then Home; a folder's
    picture shows as its poster, and a picture too big or damaged never does;
  - on Apps, RIGHT and LEFT move between the disc's apps and back, and A
    starts the probe (probe/probe.c), which reports what the hand-off left
    it: the menu music stopped, nothing still writing to memory, and its own
    path to start from;
  - nothing crashes: Dolphin emulates the MMU, so an invalid memory access
    stops Indigo on its exception screen as it would on a console, and
    Dolphin's own log reports no exception or invalid access.

The game route launches the probe from the Library as a game instead: the
probe must see the game's ID, the full 24 MB and the music stopped.

With --storage sd2sp2 (or sdgecko-b) there is no disc: the release zip is
unpacked onto an SD card image (card.build_card) and Indigo boots from the
zip's own ipl.dol. A new card has no settings yet, so Indigo starts in
Settings and the route saves them first; afterwards it reads back from the
card what Indigo wrote there.

The virtual-cards route leaves both physical slots empty, browses a public
synthetic RAW image on SD and exports a GCI into the other SD column. The
actual exported payload and unchanged RAW bytes are checked on the FAT image.

usage: run.py DOL --out DIR [--route smoke|tour|game|save|virtual-cards] [--probe DOL] [--region pal|pal60|ntsc]
              [--cable composite|component]
              [--storage dvd|sd2sp2|sdgecko-b|gcloader --card-zip ZIP] [--disc ISO]
Writes DIR/report.json, DIR/summary.md, DIR/sheet.png (every checkpoint),
the checkpoint pictures and Dolphin's output. Exit 0: passed. 1: Indigo
failed a check. 2: the harness or the emulator could not run.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import zipfile
import zlib
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(1, str(Path(__file__).resolve().parents[1] / "qa"))
import card  # noqa: E402
import dsu_pad  # noqa: E402
import make_test_saves  # noqa: E402

WIDTH, HEIGHT = 640, 480
# Where text the route reads sits (x0, y0, x1, y1): the face's name under the
# cube on Home, the selected game's title in the Library (the focused
# device's name in the Source picker sits there too), and its title on the
# game's details.
LABEL_BOX = (200, 372, 440, 396)
TITLE_BOX = (200, 338, 440, 362)
# The page title Settings opens with on a new card ("Storage").
SETTINGS_TITLE_BOX = (30, 46, 230, 80)
COUNTER_BOX = (560, 120, 610, 142)  # Settings' "row / rows", shown while a row has the focus
DETAIL_TITLE_BOX = (264, 106, 600, 134)
# Memory Cards: the focused save's name in the info bar, each stack's header
# ("A  Open" and its free blocks), the buttons along the bottom, where the
# arrow above the left stack shows once it scrolls, and the maroon box that
# says an operation is done. Its words are bold and close together, so two
# saves' names can overlap by more than half: they are told apart by
# same_text, not overlap.
INFO_BOX = (164, 377, 590, 396)
LEFT_HEADER_BOX, RIGHT_HEADER_BOX = (66, 56, 222, 92), (354, 56, 510, 92)
FOOTER_BOX = (30, 442, 610, 464)
SAVE_DETAILS_TITLE_BOX = (100, 180, 540, 206)
SAVE_DETAILS_SIZE_BOX = (100, 256, 540, 278)
SAVE_DETAILS_BLOCKS_BOX = (100, 256, 190, 278)  # block/KiB phrase; one digit aliases at 16:9
LIBRARY_SAVES_SUMMARY_BOX = (335, 204, 586, 224)
LIBRARY_SAVES_UPDATED_BOX = (266, 224, 586, 241)
SAVE_DETAILS_CREATED_BOX = (100, 281, 540, 302)
SAVE_DETAILS_UPDATED_BOX = (100, 303, 540, 325)
SAVE_DETAILS_ACTIONS_BOX = (100, 325, 540, 346)
RAW_ICON_BOX = (68, 100, 120, 161)  # selected cell 0; excludes banner/info bar
UP_ARROW_BOX = (169, 96, 184, 112)
MESSAGE_BOX = (160, 200, 480, 250)
MESSAGE_COLOUR = (120, 16, 36)  # the same under every Menu Color
MESSAGE_PIXELS = 9000  # of the box's 16000: its fill, not a red icon or two behind it
ARROW_PIXELS = 20
# A save's art loads once nothing has been held for 15 VSyncs: a moment
# after a step, its comment replaces its file name in the info bar.
ART_SECONDS = 0.8
# Dolphin writes a GCI folder's files a second after the card's last write.
FLUSH_SECONDS = 10
TEXT_LEVEL = 160          # label text is bright; the waves behind it are not
SAVE_DETAILS_SIZE_LEVEL = 128  # muted detail text becomes gray with Menu Color=Jet Black
SAME, DIFFERENT = 0.85, 0.5  # intersection over union of two label masks
# Waits count the console's own seconds (Emulator.emulated) when Dolphin
# reports them, so a busy machine slows a run instead of failing it; the
# machine's seconds still end a wait at WALL_FACTOR times as many.
BOOT_SECONDS = 120
SETTLE_SECONDS = 10
WALL_FACTOR = 5
TICKS_PER_SECOND = 486_000_000  # the GameCube's CPU clock, which Dolphin's ticks count
PRESS_SECONDS = 0.1  # of the console's time a button stays down, and up after
# Library Folders: how long the route holds a press that opens or leaves a
# folder (library_folders), and how many pixels of a folder picture's colour
# make it on screen: a card in front is tens of thousands.
FOLDER_PRESS_SECONDS = 0.5
PICTURE_PIXELS = 3000
# A press the menu was too busy to see changes nothing on the screen; a person
# presses again, and press_until does, after MISSED_SECONDS, up to PRESSES times.
MISSED_SECONDS = 3
PRESSES = 3
FATAL = re.compile("|".join((
    r"(?:DSI|ISI|Program|Machine Check|Alignment) Exception", r"Unhandled exception",
    r"Segmentation fault", r"core dumped", r"\bPANIC\b", r"ASSERT(?:ION)? FAILED",
    r"Invalid (?:read|write) (?:from|to)", r"Unknown (?:opcode|instruction)",
    r"FIFO (?:is )?(?:overflowed|desync)", r"failed to compile shader", r"device lost",
    r"poster stack .*\(overrun\)",
    r"DABR: (?:write to|read of)",  # a thread's stack reached its guard (patch 0007)
)), re.I)
# card_art's poster thread reports how much of its stack it used each time it
# stops (Indigo's debug output, a development console's). An overrun stops at
# the stack's guard (DABR, above); this catches one that came close: a run
# that used more than this share of it fails.
POSTER_STACK_SHARE = 0.75
POSTER_STACK = re.compile(r"card_art: poster stack (\d+) of (\d+) bytes used")
# The DSP runs its real microcode (LLE): Dolphin's high-level stand-ins know
# libogc's audio library but not libogc2's, so the menu music's stop before a
# launch would never be answered. It runs in step with the CPU, not on a thread
# of its own, where it sometimes missed the mail that stops the music for a
# launch (AESND_Reset waits for it with interrupts off) on a busy machine.
DOLPHIN_INI = """[Core]
DSPHLE = False
DSPThread = False
MMU = True
GFXBackend = OGL
SIDevice0 = 6
SIDevice1 = 0
SIDevice2 = 0
SIDevice3 = 0
[Analytics]
Enabled = False
PermissionAsked = True
[Interface]
ConfirmStop = False
OnScreenDisplayMessages = False
[Input]
BackgroundInput = True
[Display]
Fullscreen = True
RenderToMain = True
"""


# A console's region: Dolphin's region for a program without one (a DOL), which
# the video hardware starts in, and the video format and 60 Hz flag in its
# SRAM, which Swiss reads its video mode from. Dolphin's own SRAM is an NTSC
# console's whatever the region, so a PAL run would turn NTSC once Settings
# saved; each run gets the SRAM of the console it is.
REGIONS = {"ntsc": (1, 0, False), "pal": (2, 1, False), "pal60": (2, 1, True)}
# The region of the game whose launch the route lets fail: the console's
# other one (card.REGION_CODES), so the menu must return from its video mode.
FOREIGN = {"ntsc": 2, "pal": 1, "pal60": 1}
# The video cable Dolphin reports: with a component cable (its "progressive
# scan"), Swiss's Auto video mode is 480p; without one, interlaced.
CABLES = {"composite": False, "component": True}
# Where the SD card goes: SD2SP2 in Serial Port 2, or an SD Gecko in Memory
# Card Slot B. 15 is the SD card adapter the emulator runner's Dolphin adds
# (buildtools/ci/runner/dolphin/). In a GC Loader, the card is the drive's,
# which serves its boot.iso as the disc (DOLPHIN_GCLOADER, patch 0006).
STORAGES = {"dvd": None, "sd2sp2": "SerialPort2", "sdgecko-b": "SlotB", "gcloader": None}
SD_CARD_DEVICE = 15
# Dolphin's memory card that is a folder of .gci files, one per save.
GCI_FOLDER_DEVICE = 8
# Rows on Settings' Storage page: DOWN past them reaches Save & Exit.
STORAGE_ROWS = 7
# Lit pixels in COUNTER_BOX that are its digits rather than nothing (19 for "1 / 7").
COUNTER_PIXELS = 6
# Settings to start with, put on the card as swiss/settings/global.ini (--settings).
SETTINGS = Path(__file__).resolve().parent / "settings"


def seeded(text: str) -> dict[str, str]:
    """The Key=Value lines of a settings file."""
    pairs = (line.split("=", 1) for line in text.splitlines() if "=" in line and not line.startswith("#"))
    return {key.strip(): value.strip() for key, value in pairs}

# The probe's report, in the order of its strip (probe.c).
PROBE_WORDS = ("magic", "version", "id0", "id1", "memsize", "console", "video", "bus", "core",
               "arena_lo", "arena_hi", "top", "ai_dma", "ai_cr", "stray_writes", "argc",
               "argv0", "argv1", "argv2", "argv3", "vi_dcr", "vi_clk_dtv", "crc")
PROBE_MAGIC = 0x1D160B0E
AI_DMA_ENABLE = 0x8000


class Failed(Exception):
    """Indigo did something the route says it must not."""


class Broken(Exception):
    """The harness or the emulator could not do its part."""


def dolphin_ini(storage: str = "dvd", card: Path | None = None, cards: Path | None = None,
                empty_slots: bool = False) -> str:
    """Dolphin.ini: the SD card where the storage puts it, and with cards a
    GCI folder memory card in each slot, cards/A and cards/B."""
    core = ""
    if STORAGES[storage]:
        core += f"{STORAGES[storage]} = {SD_CARD_DEVICE}\nSP2SDCardImage = {card}\n"
    if cards:
        core += (f"SlotA = {GCI_FOLDER_DEVICE}\nSlotB = {GCI_FOLDER_DEVICE}\n"
                 f"GCIFolderAPathOverride = {cards / 'A'}\nGCIFolderBPathOverride = {cards / 'B'}\n")
    elif empty_slots:
        core += "SlotA = 0\nSlotB = 0\n"
    return DOLPHIN_INI.replace("[Core]\n", "[Core]\n" + core, 1)


def saves(folder: Path) -> set[str]:
    """The saves in a GCI folder memory card, by file name."""
    return {path.name for path in folder.glob("*.gci")}


def same_save(original: bytes, copy: bytes) -> bool:
    """A .gci and its copy on another card: the same game, maker and name in
    the entry, and the same blocks. The rest of the entry (where its blocks
    start on the card, when it was written) is the card's own."""
    return (len(copy) == len(original) > 64 and copy[0:6] == original[0:6] and
            copy[8:40] == original[8:40] and copy[64:] == original[64:])


def message_up(rgb: np.ndarray, wide: bool = False) -> bool:
    """Memory Cards' maroon box over the middle of the screen."""
    rgb = detection_frame(rgb, wide)
    x0, y0, x1, y1 = MESSAGE_BOX
    return coloured(rgb[y0:y1, x0:x1], MESSAGE_COLOUR) >= MESSAGE_PIXELS


def arrow_up(gray: np.ndarray) -> bool:
    """The arrow above Memory Cards' left stack: rows lie above its window."""
    x0, y0, x1, y1 = UP_ARROW_BOX
    return int((gray[y0:y1, x0:x1] >= 200).sum()) >= ARROW_PIXELS


def text_mask(frame: np.ndarray, box: tuple[int, int, int, int] = LABEL_BOX,
              level: int = TEXT_LEVEL) -> np.ndarray:
    x0, y0, x1, y1 = box
    return frame[y0:y1, x0:x1] >= level


def stage_box(box: tuple[int, int, int, int], wide: bool = False) -> tuple[int, int, int, int]:
    """Authored coordinates in Dolphin's actual 640x480 capture.

    With Menu Widescreen, Dolphin auto-aspect letterboxes 16:9 at y60..420.
    Both axes scale 3/4 about the capture centre. Authored x0..640 occupies
    x80..560; the extended widescreen margins occupy the remaining columns.
    """
    x0, y0, x1, y1 = box
    return (round(320 + (x0 - 320) * .75), round(240 + (y0 - 240) * .75),
            round(320 + (x1 - 320) * .75), round(240 + (y1 - 240) * .75)) if wide else box


def detection_frame(rgb: np.ndarray, wide: bool = False) -> np.ndarray:
    """Restore authored coordinates solely for comparisons, never screenshots.

    Nearest-neighbor uses only captured pixel values. It removes Dolphin's
    wide letterbox and restores the central authored stage, so existing
    field bounds and minimum text coverage apply in either screen shape.
    The real native capture remains unchanged for pictures and review.
    """
    if not wide:
        return rgb
    return np.asarray(Image.fromarray(rgb[60:420, 80:560]).resize(
        (WIDTH, HEIGHT), Image.Resampling.NEAREST))


def raw_icon_frame(rgb: np.ndarray, wide: bool = False) -> int | None:
    """Which unique texture patch is on the selected RAW cube.

    A moving cube with a static texture cannot manufacture the other patch
    colour. Ignore colors elsewhere (banner, footer, backdrop and cubes).
    """
    x0, y0, x1, y1 = stage_box(RAW_ICON_BOX, wide)
    crop = rgb[y0:y1, x0:x1]
    counts = [coloured(crop, colour) for colour in make_test_saves.RAW_ICON_COLOURS]
    best = int(np.argmax(counts))
    return best if counts[best] >= 12 and counts[best] >= counts[1 - best] * 3 else None


def backdrop(rgb: np.ndarray) -> np.ndarray:
    """The menu's backdrop colour, from the screen's bottom corners, where
    nothing else is drawn: settings/non-default.ini's Emerald, not Indigo's
    default purple."""
    return np.concatenate([rgb[-40:, :60].reshape(-1, 3), rgb[-40:, -60:].reshape(-1, 3)]).mean(axis=0)


def diagnose(rgb: np.ndarray) -> str:
    """What a failed step's screen shows, when it is not Indigo at all."""
    black = float((rgb.max(axis=2) < 8).mean())
    white = int((rgb.min(axis=2) > 200).sum())
    if black > 0.6 and white > 2000:
        return "Indigo crashed: the screen shows its exception handler (see the last picture)"
    if black > 0.95:
        return "the screen went black"
    return ""


def coloured(frame: np.ndarray, colour: tuple[int, int, int]) -> int:
    """How many pixels are within 55 of colour in every channel: card.py's
    folder pictures are pure colours nothing else on screen comes near."""
    return int((np.abs(frame.astype(np.int16) - np.array(colour, np.int16)) <= 55).all(axis=2).sum())


def overlap(a: np.ndarray, b: np.ndarray) -> float:
    union = np.logical_or(a, b).sum()
    return float(np.logical_and(a, b).sum() / union) if union else 1.0


def same_text(a: np.ndarray, b: np.ndarray) -> bool:
    """The same words. A 480i picture can sit half a line higher after a slow
    frame (it is drawn for the other field), which moves an antialiased word's
    rows across TEXT_LEVEL, so the words are compared a pair of rows at a time."""
    def rows(mask: np.ndarray) -> np.ndarray:
        return mask[:mask.shape[0] // 2 * 2].reshape(-1, 2, mask.shape[1]).any(axis=1)
    return overlap(rows(a), rows(b)) >= SAME


def has_label(mask: np.ndarray) -> bool:
    """A word of text: enough lit pixels, spread across the band but not filling it."""
    lit = int(mask.sum())
    if not 60 <= lit <= mask.size // 3:
        return False
    columns = np.flatnonzero(mask.any(axis=0))
    return columns.size > 0 and 20 <= columns[-1] - columns[0] <= mask.shape[1] - 4


def save_details_panel(gray: np.ndarray) -> bool:
    """The presentation panel in authored coordinates, distinct from save cubes.

    Its four long border edges remain visible in both the normal and gray
    palettes. A cube may overlap the title band, but cannot supply this frame.
    """
    edges = (((70, 130, 78, 342), 1), ((564, 130, 572, 342), 1),
             ((92, 108, 548, 115), 0), ((92, 361, 548, 368), 0))
    for (x0, y0, x1, y1), axis in edges:
        if (gray[y0:y1, x0:x1] >= 64).any(axis=axis).mean() < .9:
            return False
    return has_label(text_mask(gray, SAVE_DETAILS_TITLE_BOX))


def probe_field(rgb: np.ndarray) -> np.ndarray:
    """The probe's signature colour, an azure no screen of Indigo's uses."""
    r, g, b = (rgb[..., i].astype(np.int16) for i in range(3))
    return (b > 190) & (r < 80) & (g > 80) & (g < 180)


def probe_report(rgb: np.ndarray) -> dict[str, object] | None:
    """The probe's results, read from a picture of its screen, or None when
    the screen is not the probe's. The strip is found in the picture itself
    (the black and white rectangle inside the azure field), so the scale
    Dolphin or a capture card draws it at does not matter."""
    azure = probe_field(rgb)
    if azure.mean() < 0.1:
        return None
    rows = azure.mean(axis=1)
    field = np.flatnonzero(rows > 0.5)
    strip = [y for y in range(int(field[0]), rgb.shape[0]) if 0.05 < rows[y] < 0.45] if field.size else []
    if not strip:
        return None
    y0, y1 = strip[0], strip[-1] + 1
    edge = np.flatnonzero(azure[field[0]:y0].mean(axis=0) > 0.9)
    columns = azure[y0:y1].mean(axis=0)
    xs = [x for x in range(int(edge[0]), int(edge[-1]) + 1) if columns[x] < 0.1] if edge.size else []
    if not xs:
        return None
    x0, x1 = xs[0], xs[-1] + 1
    width, height = (x1 - x0) / 32, (y1 - y0) / len(PROBE_WORDS)
    words = []
    for row in range(len(PROBE_WORDS)):
        value = 0
        for bit in range(32):
            pixel = rgb[int(y0 + (row + 0.5) * height), int(x0 + (bit + 0.5) * width)]
            value = value << 1 | int(pixel.mean() > 128)
        words.append(value)
    report: dict[str, object] = dict(zip(PROBE_WORDS, words))
    packed = b"".join(w.to_bytes(4, "big") for w in words)
    report["valid"] = words[0] == PROBE_MAGIC and zlib.crc32(packed[:-4]) == words[-1]
    report["disc_id"] = printable(packed[8:14])
    report["path"] = printable(packed[64:80].rstrip(b"\0"))
    return report


def sram(video: int, sixty_hz: bool) -> bytes:
    """A GameCube's SRAM (as Dolphin keeps it in GC/SRAM.raw): Dolphin's own
    defaults, with the video format and PAL's 60 Hz flag set and the
    checksums over them made again."""
    data = bytearray(0x44)
    data[0x17] = 0x2C | video          # flags: stereo, set up; bits 0-1 the format
    data[0x15] = 0x40 if sixty_hz else 0  # ntd: bit 6, PAL at 60 Hz
    data[0x18:0x30] = b"DOLPHINSLOTADOLPHINSLOTB"
    data[0x3E:0x40] = b"\x6e\x6d"
    words = [int.from_bytes(data[i:i + 2], "big") for i in range(0x10, 0x18, 2)]
    data[0x04:0x06] = (sum(words) & 0xFFFF).to_bytes(2, "big")
    data[0x06:0x08] = (sum(~w & 0xFFFF for w in words) & 0xFFFF).to_bytes(2, "big")
    return bytes(data)


def video_mode(report: dict[str, object]) -> str:
    """The video mode the probe found the screen in, in words."""
    dcr, clock = int(report["vi_dcr"]), int(report["vi_clk_dtv"])
    scan = "progressive" if clock >> 16 & 1 else "double-strike" if dcr >> 2 & 1 else "interlaced"
    timing = ("NTSC", "PAL", "MPAL", "debug")[dcr >> 8 & 3]
    return f"{scan}, {timing} timing, {'a' if clock & 1 else 'no'} component cable"


def printable(data: bytes) -> str:
    """Bytes as text for a report: anything outside printable ASCII as a dot."""
    return "".join(chr(b) if 32 <= b < 127 else "." for b in data)


class Deadline:
    """A wait of some seconds of the console's own time when Dolphin reports
    it (patch 0005, DOLPHIN_TICKS), else of the machine's."""

    def __init__(self, emulator: "Emulator", seconds: float) -> None:
        self.emulator, self.seconds = emulator, seconds
        self.wall = time.monotonic()
        self.start = emulator.emulated()

    def elapsed(self) -> float:
        now = self.emulator.emulated()
        if now is not None and self.start is None:
            self.start = now  # Dolphin began reporting after the wait began
        if now is not None:
            return now - self.start
        return time.monotonic() - self.wall

    def expired(self) -> bool:
        return self.elapsed() >= self.seconds or time.monotonic() - self.wall >= self.seconds * WALL_FACTOR


class Emulator:
    def __init__(self, dol: Path, disc: Path | None, work: Path, out: Path, region: str = "pal",
                 storage: str = "dvd", card: Path | None = None, cable: str = "composite",
                 faults: str | None = None, cards: Path | None = None,
                 empty_slots: bool = False) -> None:
        self.out = out
        self.user = work / "dolphin"
        (self.user / "Config").mkdir(parents=True)
        (self.user / "Config/Dolphin.ini").write_text(dolphin_ini(storage, card, cards, empty_slots))
        # Swiss's own debug output (its OSReport lines) goes to dolphin.log.
        (self.user / "Config/Logger.ini").write_text(
            "[Logs]\nOSREPORT = True\nPOWERPC = True\n[Options]\nVerbosity = 1\nWriteToConsole = True\nWriteToFile = False\n")
        (self.user / "GC").mkdir()
        (self.user / "GC/SRAM.raw").write_bytes(sram(*REGIONS[region][1:]))
        (self.user / "Config/GFX.ini").write_text("[Settings]\nInternalResolution = 1\nShowFPS = False\n")
        (self.user / "Config/GCPadNew.ini").write_text(dsu_pad.GCPAD_INI)
        self.pad = dsu_pad.Pad(0)  # any free port; Dolphin is told which
        (self.user / "Config/DSUClient.ini").write_text(
            f"[Server]\nEnabled = True\nEntries = {dsu_pad.DESCRIPTION}:127.0.0.1:{self.pad.port};\n")
        # X keeps its sockets here; a container user cannot create it at a
        # system path otherwise.
        os.makedirs("/tmp/.X11-unix", mode=0o1777, exist_ok=True)
        read, write = os.pipe()
        self.xvfb = subprocess.Popen(
            ["Xvfb", "-displayfd", str(write), "-screen", "0", f"{WIDTH}x{HEIGHT}x24", "-nolisten", "tcp"],
            pass_fds=(write,), stdout=subprocess.DEVNULL, stderr=open(out / "xvfb.log", "w"))
        os.close(write)
        with os.fdopen(read) as display:
            number = display.readline().strip()
        if not number.isdigit():
            raise Broken("Xvfb did not start")
        self.display = f":{number}"
        env = dict(os.environ, DISPLAY=self.display, LIBGL_ALWAYS_SOFTWARE="1")
        env["DOLPHIN_TICKS"] = "1"  # the console's clock and PC in dolphin.log (patch 0005)
        env["DOLPHIN_DABR"] = "1"  # a thread's stack guarded, as on a console (patch 0007)
        env.pop("DOLPHIN_SD_FAULTS", None)
        env.pop("DOLPHIN_GCLOADER", None)
        if storage == "gcloader":
            env["DOLPHIN_GCLOADER"] = str(card)
        if faults:  # the SD card fails as asked (buildtools/ci/runner/dolphin/0004)
            env["DOLPHIN_SD_FAULTS"] = faults
        self.log = out / "dolphin.log"
        self.dolphin = subprocess.Popen(
            ["dolphin-emu-nogui", "-u", str(self.user), "-p", "x11", "-v", "OGL",
             *(["-C", f"Dolphin.Core.DefaultISO={disc}"] if disc else []),
             "-C", f"Dolphin.Core.FallbackRegion={REGIONS[region][0]}",
             "-C", f"SYSCONF.IPL.PGS={CABLES[cable]}", "-e", str(dol)],
            stdout=open(self.log, "w"), stderr=subprocess.STDOUT, env=env)
        self.started = time.monotonic()

    def frame(self) -> np.ndarray:
        if self.dolphin.poll() is not None:
            raise Failed(f"Dolphin exited ({self.dolphin.returncode}); see dolphin.log")
        raw = subprocess.run(
            ["ffmpeg", "-loglevel", "error", "-f", "x11grab", "-video_size", f"{WIDTH}x{HEIGHT}",
             "-draw_mouse", "0", "-i", f"{self.display}+0,0", "-frames:v", "1",
             "-f", "rawvideo", "-pix_fmt", "rgb24", "-"],
            capture_output=True, timeout=20)
        if raw.returncode or len(raw.stdout) != WIDTH * HEIGHT * 3:
            raise Broken(f"cannot read the screen: {raw.stderr.decode(errors='replace')[-200:]}")
        return np.frombuffer(raw.stdout, np.uint8).reshape(HEIGHT, WIDTH, 3)

    def _ticks(self) -> tuple[float, str] | None:
        """The last TICKS line in Dolphin's output: the console's seconds, and its PC and LR."""
        try:
            with open(self.log, "rb") as log:
                log.seek(0, os.SEEK_END)
                log.seek(max(0, log.tell() - 4096))
                tail = log.read().decode(errors="replace")
        except OSError:
            return None
        found = re.findall(r"^TICKS (\d+) (PC \S+ LR \S+)", tail, re.M)
        return (int(found[-1][0]) / TICKS_PER_SECOND, found[-1][1]) if found else None

    def emulated(self) -> float | None:
        """The console's own seconds since it started, or None from a Dolphin without patch 0005."""
        ticks = self._ticks()
        return ticks[0] if ticks else None

    def where(self) -> str | None:
        """Where the console's CPU was last seen, and when, for a check that failed."""
        ticks = self._ticks()
        return f"{ticks[1]} at {ticks[0]:.1f} s" if ticks else None

    def close(self) -> None:
        for process in (self.dolphin, self.xvfb):
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(5)
                except subprocess.TimeoutExpired:
                    process.kill()
        self.pad.close()


class Route:
    """Steps through the menus and records every checkpoint."""

    def __init__(self, emulator: Emulator, out: Path, probe: bool = False, fresh_card: bool = False,
                 cable: str = "composite", region: str = "pal", fragments: int = 0,
                 cards: Path | None = None, storage: str = "dvd", menu_wide: bool = False,
                 sd_image: Path | None = None) -> None:
        self.emulator = emulator
        self.cards = cards  # the memory cards' GCI folders, cards/A and cards/B
        self.storage = storage
        self.menu_wide = menu_wide
        self.sd_image = sd_image
        self.fragments = fragments  # the pieces the probe's game is in on the card
        self.cable = cable
        self.region = region
        self.out = out
        self.probe = probe
        self.fresh_card = fresh_card
        self.report: dict[str, object] | None = None
        self.folders_on = False  # the route turned Library Folders on and saved it
        self.pressed_again: list[str] = []  # presses the menu missed, pressed again
        self.checks: list[dict[str, object]] = []
        self.shots: list[tuple[str, Path]] = []
        self.pad = emulator.pad

    # -- evidence
    def shot(self, name: str, frame: np.ndarray) -> np.ndarray:
        path = self.out / f"{len(self.shots) + 1:02d}-{name}.png"
        Image.fromarray(frame).save(path)
        self.shots.append((name, path))
        return frame

    def check(self, name: str, passed: bool, **detail: object) -> None:
        probe_up = bool(self.report and self.report["valid"])  # the probe's screen is no crash
        if not passed and getattr(self, "last_rgb", None) is not None:
            self.shot("failed-check", self.last_rgb)
            detail["picture"] = self.shots[-1][1].name
        if not passed and getattr(self, "last_rgb", None) is not None and not probe_up:
            if why := diagnose(self.last_rgb):
                detail["screen"] = why
        if not passed and (where := self.emulator.where()):
            detail["console"] = where
        self.checks.append({"check": name, "passed": bool(passed), **detail})
        print(f"{'PASS' if passed else 'FAIL'} {name} {json.dumps(detail) if detail else ''}", flush=True)
        if not passed:
            raise Failed(f"{name}: {detail['screen']}" if "screen" in detail else name)

    # -- waiting for the screen
    def gray(self) -> np.ndarray:
        rgb = self.emulator.frame()
        self.last_rgb = rgb
        return detection_frame(rgb, self.menu_wide).max(axis=2)

    def settled_label(self, seconds: float = SETTLE_SECONDS, unlike: np.ndarray | None = None,
                      like: np.ndarray | None = None,
                      box: tuple[int, int, int, int] = LABEL_BOX,
                      level: int = TEXT_LEVEL) -> tuple[np.ndarray | None, float]:
        """Wait for steady text in a box (the face's name by default), optionally unlike or like a given one."""
        deadline = Deadline(self.emulator, seconds)
        previous, steady = None, 0
        while not deadline.expired():
            mask = text_mask(self.gray(), box, level)
            ok = has_label(mask)
            if ok and unlike is not None:
                ok = overlap(mask, unlike) < DIFFERENT
            if ok and like is not None:
                ok = same_text(mask, like)
            steady = steady + 1 if ok and previous is not None and overlap(mask, previous) >= 0.95 else 0
            previous = mask if ok else None
            if steady >= 2:
                return mask, time.monotonic()
            time.sleep(0.15)
        return None, time.monotonic()

    def press_until(self, button: str, seconds: float = SETTLE_SECONDS, unlike: np.ndarray | None = None,
                    like: np.ndarray | None = None,
                    box: tuple[int, int, int, int] = LABEL_BOX) -> tuple[np.ndarray | None, float]:
        """Press a button, then wait for steady text in a box as settled_label does.
        A press the menu was too busy to see leaves the box as it was, and a person
        presses again: so does this, noting each in the report. A press that
        changed the box, to anything, is never repeated."""
        before = text_mask(self.gray(), box)
        for attempt in range(1, PRESSES + 1):
            self.press(button)
            last = attempt == PRESSES
            mask, now = self.settled_label(seconds if last else MISSED_SECONDS, unlike, like, box)
            if mask is not None or last:
                return mask, now
            if overlap(text_mask(self.gray(), box), before) < SAME:
                return self.settled_label(seconds, unlike, like, box)  # it moved: let it arrive
            self.pressed_again.append(button)
        return None, time.monotonic()

    def press(self, button: str, seconds: float = PRESS_SECONDS) -> None:
        """Hold a button for some of the console's time, then leave it up for
        a little: a busy machine must not shorten a press below a frame."""
        self.pad.hold(button)
        self.pause(seconds)
        self.pad.hold()
        self.pause(PRESS_SECONDS)

    def pause(self, seconds: float) -> None:
        """Let some of the console's time pass: a menu's animation takes as
        much of it however busy the machine is."""
        wait = Deadline(self.emulator, seconds)
        while not wait.expired():
            time.sleep(0.02)

    # -- the route
    def boot(self) -> np.ndarray:
        if self.fresh_card:
            return self.first_run()
        mask, now = self.settled_label(BOOT_SECONDS)
        boot = round(now - self.emulator.started, 1)
        self.shot("home", self.last_rgb)
        self.check("Home appears after boot", mask is not None, seconds=boot)
        self.plug_in()
        return mask

    def first_run(self) -> np.ndarray:
        """A new card has no swiss/settings/global.ini: Indigo starts in
        Settings, on Storage, and says so. DOWN past the page's rows reaches
        Save & Exit, and A writes the settings to the card and goes Home.

        DOWN is pressed while the rows' counter shows, so one the menu was too
        busy to see (a new card's first moments) is pressed again; then LEFT
        twice, which ends on Save & Exit even from Discard & Exit, one row on."""
        title, now = self.settled_label(BOOT_SECONDS, box=SETTINGS_TITLE_BOX)
        self.shot("first-run", self.last_rgb)
        self.check("a new card opens Settings first", title is not None,
                   seconds=round(now - self.emulator.started, 1))
        self.plug_in()
        for _ in range(STORAGE_ROWS + 4):
            if text_mask(self.gray(), COUNTER_BOX).sum() < COUNTER_PIXELS:
                break
            self.press("DOWN")
            self.pause(0.4)
        self.check("DOWN takes the focus past the page's rows",
                   text_mask(self.gray(), COUNTER_BOX).sum() < COUNTER_PIXELS)
        for _ in range(2):
            self.press("LEFT")
            self.pause(0.3)
        self.shot("first-run-save", self.emulator.frame())
        self.press("A")
        mask, now = self.settled_label(BOOT_SECONDS)
        self.shot("home", self.last_rgb)
        self.check("Save & Exit writes the settings and goes Home", mask is not None,
                   seconds=round(now - self.emulator.started, 1))
        return mask

    def plug_in(self) -> None:
        self.pad.plug_in()
        deadline = time.monotonic() + 30  # Dolphin's own polling, in the machine's time
        while not self.pad.streaming and time.monotonic() < deadline:
            time.sleep(0.1)
        if not self.pad.streaming:
            raise Broken("Dolphin never asked for the controller")
        time.sleep(0.5)

    def card_checks(self, image: Path, route: str, start: dict[str, str] | None = None) -> None:
        """Read settings back from the SD image. A seeded card must retain
        its configured values, including on routes that do not save settings."""
        def text(path: str) -> str:
            return (card.read_card(image, path) or b"").decode("latin-1")

        settings = text("swiss/settings/global.ini")
        settings_check = ("the configured settings remain on the card" if start else
                          "the settings Indigo saved are on the card")
        self.check(settings_check,
                   "Swiss Video Mode=" in settings and "Hide Apps Face=No" in settings, bytes=len(settings))
        if start:
            kept = seeded(settings)
            lost = {key: value for key, value in start.items() if kept.get(key) != value}
            self.check("the settings the card started with are all still there", not lost, lost=lost)
        if self.folders_on:
            # Library Folders, turned on by the route (library_folders): saved
            # on, and the card keeps its own FlattenDir, not the pattern Library
            # Folders uses while it is on, for when it goes off again.
            self.check("Library Folders is saved on, and the card keeps its own FlattenDir",
                       "\nLibrary Folders=Yes\r" in settings and "\nFlattenDir=*/games\r" in settings,
                       lines=[line.strip() for line in settings.splitlines()
                              if line.startswith(("Library Folders=", "FlattenDir="))])
        if route != "game" or not self.report:  # no game started
            return
        recent = text("swiss/settings/recent.ini").split("Recent_1=")[0]
        self.check("the launched game is first in the recent list",
                   card.game_file(*card.PROBE_GAME) in recent, recent=recent.strip().splitlines()[-1:])
        played = text("swiss/settings/play-history-0.ini") + text("swiss/settings/play-history-1.ini")
        self.check("Indigo's play history records the game", f"\n{card.PROBE_GAME[0]}=" in played)

    def turn(self, faces: list[np.ndarray], button: str, name: str) -> np.ndarray:
        mask, _ = self.press_until(button, unlike=faces[-1])
        self.shot(name, self.last_rgb)
        self.check(f"{button} turns the cube to another face", mask is not None, after=name)
        return mask

    def smoke(self) -> None:
        home = self.boot()
        faces = [home]
        # The disc has apps: Library, Source, Settings, System and Apps.
        for n in range(1, 5):
            faces.append(self.turn(faces, "RIGHT", f"right-{n}"))
        back = self.turn(faces, "RIGHT", "right-5")
        self.check("five turns come back to the first face", same_text(back, home),
                   overlap=round(overlap(back, home), 3))
        distinct = max(overlap(a, b) for i, a in enumerate(faces) for b in faces[i + 1:])
        self.check("the five faces have five different names", distinct < DIFFERENT,
                   largest_overlap=round(distinct, 3))
        left = self.turn([back], "LEFT", "left-1")
        self.check("LEFT turns the other way", same_text(left, faces[-1]),
                   overlap=round(overlap(left, faces[-1]), 3))
        mask, _ = self.press_until("RIGHT", like=home)
        self.check("RIGHT undoes LEFT", mask is not None)
        for n, face in enumerate(faces[:4]):
            # Home starts on the Library face: browse it while it is open,
            # change the source on the Source face, and turn Apps Face off
            # and on again in Settings, then walk a Classic cube.
            if n == 2:
                self.apps_face_off_and_on(faces)
                self.classic_cube(faces)
                self.library_folders(faces)
            else:
                inside = {0: self.browse_library, 1: self.change_source,
                          3: self.memory_cards if self.cards else None}.get(n)
                self.open_and_close(face, n, inside)
            mask, _ = self.press_until("RIGHT", like=faces[n + 1])
            self.check("the cube turns on to the next face", mask is not None, face=n + 1)
        # Apps last: a launch never comes back.
        self.start_an_app(faces[4])

    def start_an_app(self, face: np.ndarray) -> None:
        """A on Apps shows the disc's apps as the Library shows games: RIGHT
        and LEFT move between them and back. A starts one: the Apps screen
        gives way to the launch screen, dimmed but for the app's card, its
        name and the ring. With the probe on the disc, the app started is the
        probe, and the hand-off must reach it (handoff); without it the
        route stops at the launch screen.

        The A that opens Apps is held longer than Apps takes to read /apps:
        on a console an SD card is read before a thumb lets go, and that A
        once started the first app with no chance to choose."""
        self.press("A", 2.5)
        opened = self.covered(face)
        self.shot("apps", self.last_rgb)
        self.check("A opens Apps", opened)
        first, _ = self.settled_label(box=TITLE_BOX)
        self.check("Apps shows an app's name", first is not None)
        other, _ = self.press_until("RIGHT", unlike=first, box=TITLE_BOX)
        self.shot("apps-right", self.last_rgb)
        self.check("RIGHT moves to the next app", other is not None)
        again, _ = self.press_until("LEFT", like=first, box=TITLE_BOX)
        self.check("LEFT goes back an app", again is not None)
        if self.probe:
            name = again
            for n in range(card.app_order(True).index(card.PROBE_APP[:-4])):
                name, _ = self.press_until("RIGHT", unlike=name, box=TITLE_BOX)
                self.check("RIGHT moves to the next app", name is not None, step=n + 1)
            self.shot("apps-probe", self.last_rgb)
        apps = float(self.emulator.frame().mean())
        self.press("A")
        # With the probe the launch must go on, and reading a program off an
        # emulated SD card can crawl on a busy machine: allow it a boot's
        # time. The probe's own checks follow.
        window = BOOT_SECONDS if self.probe else SETTLE_SECONDS
        launched, deadline = False, Deadline(self.emulator, window)
        while not launched and not deadline.expired():
            time.sleep(0.3)
            rgb = self.emulator.frame()
            self.last_rgb = rgb
            # The launch screen dims all but the app's card: the probe's bright
            # name card leaves 70 to 85% of the light, a dark card under 60%.
            # With the probe the launch goes on, so the hand-off's black frame
            # counts too (the probe then proves the launch); without it black
            # means a crash.
            mean = float(rgb.mean())
            launched = mean < (0.9 if self.probe else 0.6) * apps and (self.probe or mean > 0.02)
        self.shot("app-launch", self.last_rgb)
        self.check("A starts the app: the launch screen dims the Apps screen", launched,
                   apps=round(apps, 1), now=round(float(self.last_rgb.mean()), 1))
        if self.probe:
            report = self.handoff("app")
            self.check("the app starts with its own path", str(report["path"]).lower().endswith("probe.dol"),
                       path=report["path"], argc=report["argc"])
            # Settings' Video Mode is Auto: 480p with a component cable, else
            # interlaced, at 50 Hz (PAL timing) only on a PAL console set to 50.
            progressive = bool(int(report["vi_clk_dtv"]) >> 16 & 1)
            pal_timing = int(report["vi_dcr"]) >> 8 & 3 == 1
            self.check("the menu ran in the video mode its console and cable ask for",
                       progressive == CABLES[self.cable] and
                       pal_timing == (self.region == "pal" and not CABLES[self.cable]),
                       region=self.region, cable=self.cable, video=video_mode(report))

    def handoff(self, tag: str) -> dict[str, object]:
        """After a launch, wait for the probe's screen and check what the
        hand-off left it. The controller stays connected, as on a console."""
        report, deadline = None, Deadline(self.emulator, BOOT_SECONDS)
        while not (report and report["valid"]) and not deadline.expired():
            time.sleep(0.5)
            rgb = self.emulator.frame()
            self.last_rgb = rgb
            report = probe_report(rgb)
        self.shot(f"{tag}-probe", self.last_rgb)
        self.report = report
        self.check("the launch reaches the probe, and its report reads back", bool(report and report["valid"]),
                   seconds_waited=round(deadline.elapsed(), 1))
        self.check("the menu music is stopped before the hand-off",
                   not int(report["ai_dma"]) & AI_DMA_ENABLE, audio_dma=f"{report['ai_dma']:04X}")
        self.check("nothing writes to memory after the hand-off", report["stray_writes"] == 0,
                   words_changed=report["stray_writes"])
        return report

    def save(self) -> None:
        """Change one setting and save it: Settings > Setup > Console > Apps
        Face Off, then Save & Exit. Run on a card that fails (--sd-faults), the
        next boot of the same card (main) shows whether the settings survived."""
        home = self.boot()
        self.home_label, self.home_backdrop = home, backdrop(self.last_rgb)
        face = home
        for n in (1, 2):  # Library, Source, Settings
            face = self.turn([face], "RIGHT", f"right-{n}")
        self.flip_apps_face(face, "off")

    def boot_again(self) -> None:
        """The same card's next boot: its settings must still load, so Indigo
        starts at Home, not in Settings as on a card without them."""
        # Home is the face name the first boot showed; Settings' rows can put
        # text in that box too, but not that word.
        home, _ = self.settled_label(BOOT_SECONDS, like=self.home_label)
        self.shot("next-boot", self.last_rgb)
        self.check("the next boot still has the card's settings: Indigo starts at Home", home is not None)
        # A settings file that reads back broken boots Home too, on the defaults.
        drift = float(np.linalg.norm(backdrop(self.last_rgb) - self.home_backdrop))
        self.check("... in the colours the card's settings chose, not the defaults", drift < 15,
                   drift=round(drift, 1))

    def game(self) -> None:
        """Boot, open the Library, move to the probe's game, open its details
        and launch it. The probe must see the game's own disc ID and the
        24 MB a game is promised."""
        home = self.boot()
        self.press("A")
        opened = self.covered(home)
        self.shot("library", self.last_rgb)
        self.check("A opens the Library", opened)
        title, _ = self.settled_label(box=TITLE_BOX)
        self.check("the Library shows a game's title", title is not None)
        for n in range(card.library_order(True).index(card.PROBE_GAME[1])):
            title, _ = self.press_until("RIGHT", unlike=title, box=TITLE_BOX)
            self.check("RIGHT moves to the next game", title is not None, step=n + 1)
        self.shot("library-probe", self.last_rgb)
        self.press("A")
        self.check("A opens the game's details", self.covered(title, TITLE_BOX))
        self.shot("probe-details", self.last_rgb)
        self.press("A")
        if self.fragments > card.MAX_FRAGMENTS:
            self.check("a game in more pieces than can be served is refused, and the Library comes back",
                       self.back_to_library(title, "launch-refused"), pieces=self.fragments)
            return
        report = self.handoff("game")
        self.check("the game starts with its own disc ID", report["disc_id"] == card.PROBE_GAME[0],
                   disc_id=report["disc_id"])
        self.check("the game is given the full 24 MB", report["memsize"] == 0x01800000,
                   memory=f"{report['memsize']:08X}")

    def flip_apps_face(self, settings: np.ndarray, tag: str) -> None:
        """Apps Face, eight DOWNs into Console (flip_console)."""
        self.flip_console(settings, 8, "apps-face", tag)

    def flip_console(self, settings: np.ndarray, downs: int, name: str, tag: str) -> None:
        """From the Settings face: R and R to Setup, DOWN and A into Console,
        downs DOWNs to a row and RIGHT to flip it. B goes back to Setup and
        B again saves and exits (the demo disc can't keep the file; the
        setting holds until Indigo restarts), back to the Settings face."""
        detail = {name.replace("-", "_"): tag}
        self.press("A")
        opened = self.covered(settings)
        self.shot(f"settings-{name}-{tag}", self.last_rgb)
        self.check("A opens Settings", opened, **detail)
        for button, pause in (("R", 1.0), ("R", 1.0), ("DOWN", 0.6), ("A", 1.5)):
            self.press(button)
            self.pause(pause)
        for _ in range(downs):
            self.press("DOWN")
            self.pause(0.4)
        self.press("RIGHT")
        self.pause(1.0)
        self.shot(f"{name}-{tag}", self.emulator.frame())
        self.press("B")
        self.pause(1.0)
        self.press("B")
        mask, _ = self.settled_label(like=settings)
        self.shot(f"{name}-{tag}-home", self.last_rgb)
        self.check("Save & Exit comes back to the Settings face", mask is not None, **detail)

    def classic_cube(self, faces: list[np.ndarray]) -> None:
        """From the Settings face: Setup > Console > Cube (nine DOWNs, just
        after Apps Face) to Classic. The faces then sit as on the GameCube's
        menu, Library in front and the way between them: Settings is its
        left side, so LEFT goes nowhere, and RIGHT twice is Library and then
        System, not round. From System UP goes nowhere and B is Library; UP
        is Source, DOWN back, DOWN Apps (the disc has the probe app) and UP
        back. LEFT returns to Settings, where Cube goes back to Infinite.
        A press that should go nowhere has had a second of the console's
        time to turn the cube when its face is checked."""
        library, source, settings, system, apps = faces[:5]
        self.flip_console(settings, 9, "cube", "classic")
        walk = (("LEFT", "Settings", False), ("LEFT", "Settings", False), ("RIGHT", "Library", True),
                ("RIGHT", "System", True), ("UP", "System", False), ("B", "Library", True),
                ("UP", "Source", True), ("DOWN", "Library", True), ("DOWN", "Apps", True),
                ("UP", "Library", True), ("LEFT", "Settings", True))
        named = {"Library": library, "Source": source, "Settings": settings, "System": system,
                 "Apps": apps}
        for n, (button, name, turns) in enumerate(walk, 1):
            if turns:
                mask, _ = self.press_until(button, like=named[name])
            else:
                self.press(button)
                self.pause(1.0)
                mask, _ = self.settled_label(like=named[name])
            self.shot(f"classic-{n}-{button.lower()}", self.last_rgb)
            self.check(f"Cube Classic: {button} {'turns to' if turns else 'stays on'} {name}",
                       mask is not None, step=n)
        self.flip_console(settings, 9, "cube", "infinite")

    def apps_face_off_and_on(self, faces: list[np.ndarray]) -> None:
        """Setup > Console > Apps Face: Off takes Apps off the cube, so the
        face after System is Library; On puts it back after System."""
        library, settings, system, apps = faces[0], faces[2], faces[3], faces[4]
        for tag, after_system in (("off", library), ("on", apps)):
            self.flip_apps_face(settings, tag)
            self.check("RIGHT turns to System", self.press_until("RIGHT", like=system)[0] is not None,
                       apps_face=tag)
            mask, _ = self.press_until("RIGHT", unlike=system)
            self.shot(f"after-system-apps-face-{tag}", self.last_rgb)
            self.check(f"Apps Face {tag.title()}: after System comes "
                       f"{'Library' if tag == 'off' else 'Apps'}",
                       mask is not None and same_text(mask, after_system),
                       overlap=round(overlap(mask, after_system), 3) if mask is not None else None)
            for back in (system, settings):
                self.check("LEFT turns back a face", self.press_until("LEFT", like=back)[0] is not None,
                           apps_face=tag)

    def pictures(self, seconds: float, until_shown: bool = False) -> tuple[int, int]:
        """Watches the screen for up to seconds of the console's time, or
        until the shown folder picture is up: its pixels in the last frame,
        and the most of a refused picture's in any."""
        deadline = Deadline(self.emulator, seconds)
        shown = refused = 0
        while not deadline.expired():
            self.last_rgb = self.emulator.frame()
            shown = coloured(self.last_rgb, card.SHOWN_PICTURE)
            refused = max(refused, coloured(self.last_rgb, card.REFUSED_PICTURE))
            if until_shown and shown >= PICTURE_PIXELS:
                break
            time.sleep(0.25)
        return shown, refused

    def library_folders(self, faces: list[np.ndarray]) -> None:
        """From the Settings face: R and R to Setup, four DOWNs and A into
        Library, DOWN to Library Folders and RIGHT to turn it on; B and B save
        and exit. Then on Library, A opens the empty Old saves folder, which
        stays in the Library with only its way back, and B returns to it;
        RIGHT from there to Racing, A into it and A into Classics, which holds three cards (a game,
        the game in Old below it, and the way back): RIGHT twice is not back at
        the first, a third RIGHT is. B goes back to the same folder card each
        level up, and from /games to Home. Back on the Settings face after.
        Racing's picture is its poster; Old saves' (too big) and Classics'
        (damaged) never show, while their cards are in front."""
        library, source, settings = faces[0], faces[1], faces[2]
        self.press("A")
        self.check("A opens Settings", self.covered(settings), library_folders="on")
        for button, pause in (("R", 1.0), ("R", 1.0), ("DOWN", 0.4), ("DOWN", 0.4),
                              ("DOWN", 0.4), ("DOWN", 0.6), ("A", 1.5), ("DOWN", 0.6),
                              ("RIGHT", 1.0)):
            self.press(button)
            self.pause(pause)
        self.shot("library-folders-on", self.emulator.frame())
        self.press("B")
        self.pause(1.0)
        self.press("B")
        mask, _ = self.settled_label(like=settings)
        self.check("Save & Exit comes back to the Settings face", mask is not None,
                   library_folders="on")
        self.folders_on = True
        for back in (source, library):
            self.check("LEFT turns back a face", self.press_until("LEFT", like=back)[0] is not None,
                       library_folders="on")
        self.press("A")
        stray, _ = self.settled_label(box=TITLE_BOX)
        self.shot("folders-games", self.last_rgb)
        self.check("the Library shows a folder's name", stray is not None)
        _, refused = self.pictures(8.0)
        self.check("a folder's picture too big to read never shows", refused < PICTURE_PIXELS,
                   pixels=refused)
        # Old saves is empty: it opens in the Library with only the way back,
        # and B comes back to its card (Swiss's list would go Home). The
        # presses that open or leave a folder are held for half a second of
        # the console's time: the Library is still reading covers then, and a
        # tap can fall between two of its pad reads. Indigo waits for the
        # release after a folder changes, so a held press acts once.
        self.press("A", FOLDER_PRESS_SECONDS)
        empty, _ = self.settled_label(unlike=stray, box=TITLE_BOX)
        self.shot("folders-empty", self.last_rgb)
        self.check("an empty folder opens with only the way back", empty is not None)
        self.press("B", FOLDER_PRESS_SECONDS)
        self.check("B comes back to the empty folder's card",
                   self.settled_label(like=stray, box=TITLE_BOX)[0] is not None)
        racing, _ = self.press_until("RIGHT", unlike=stray, box=TITLE_BOX)
        self.check("RIGHT moves to the next folder", racing is not None)
        shown, refused = self.pictures(30.0, until_shown=True)
        self.shot("folders-picture", self.last_rgb)
        self.check("a folder's picture is its poster", shown >= PICTURE_PIXELS, pixels=shown)
        self.check("no refused picture shows beside it", refused < PICTURE_PIXELS, pixels=refused)
        self.press("A", FOLDER_PRESS_SECONDS)
        classics, _ = self.settled_label(unlike=racing, box=TITLE_BOX)
        self.shot("folders-racing", self.last_rgb)
        self.check("A opens a folder", classics is not None)
        _, refused = self.pictures(8.0)
        self.check("a damaged folder picture never shows", refused < PICTURE_PIXELS, pixels=refused)
        self.press("A", FOLDER_PRESS_SECONDS)
        first, _ = self.settled_label(unlike=classics, box=TITLE_BOX)
        self.shot("folders-classics", self.last_rgb)
        self.check("A opens a folder in it", first is not None)
        shown = first
        for n in (1, 2):
            shown, _ = self.press_until("RIGHT", unlike=shown, box=TITLE_BOX)
            self.shot(f"folders-classics-right-{n}", self.last_rgb)
            self.check("RIGHT moves to the next card", shown is not None, step=n)
        self.check("the folder lists the game a level down: RIGHT twice is not back at the first",
                   overlap(shown, first) < DIFFERENT, overlap=round(overlap(shown, first), 3))
        self.check("a third RIGHT is back at the first card",
                   self.press_until("RIGHT", like=first, box=TITLE_BOX)[0] is not None)
        for name, expected in (("Classics", classics), ("Racing", racing)):
            self.press("B", FOLDER_PRESS_SECONDS)
            back, _ = self.settled_label(like=expected, box=TITLE_BOX)
            self.shot(f"folders-back-to-{name.lower()}", self.last_rgb)
            self.check("B goes up a folder, to the same card", back is not None, card=name)
        self.press("B", FOLDER_PRESS_SECONDS)
        mask, _ = self.settled_label(like=library)
        self.shot("folders-home", self.last_rgb)
        self.check("B from /games goes Home", mask is not None, library_folders="on")
        for ahead in (source, settings):
            self.check("RIGHT turns on a face", self.press_until("RIGHT", like=ahead)[0] is not None,
                       library_folders="on")

    def covered(self, reference: np.ndarray, box: tuple[int, int, int, int] = LABEL_BOX) -> bool:
        """Wait for the text in a box to stop matching reference: another screen opened over it."""
        deadline = Deadline(self.emulator, SETTLE_SECONDS)
        while not deadline.expired():
            time.sleep(0.3)
            if overlap(text_mask(self.gray(), box), reference) < DIFFERENT:
                self.pause(1.0)  # let the screen finish arriving before the picture
                self.gray()
                return True
        return False

    def open_and_close(self, face: np.ndarray, n: int, inside=None) -> None:
        self.press("A")
        opened = self.covered(face)
        self.shot(f"face-{n + 1}-open", self.last_rgb)
        self.check("A opens the face", opened, face=n + 1)
        if inside:
            inside()
        self.press("B")
        mask, _ = self.settled_label(like=face)
        self.shot(f"face-{n + 1}-back", self.last_rgb)
        self.check("B comes back to the same face", mask is not None, face=n + 1)

    def browse_library(self) -> None:
        """RIGHT twice then LEFT twice: a new title each way, and back to the first.
        Then A opens that game's details, UP and A its settings, B comes back
        to the details and B again to the game; then a launch that fails comes
        back to it too."""
        titles = []
        first, _ = self.settled_label(box=TITLE_BOX)
        self.check("the Library shows a game's title", first is not None)
        titles.append(first)
        for n in (1, 2):
            title, _ = self.press_until("RIGHT", unlike=titles[-1], box=TITLE_BOX)
            self.shot(f"library-right-{n}", self.last_rgb)
            self.check("RIGHT moves to the next game", title is not None, step=n)
            titles.append(title)
        for n, expected in ((1, titles[1]), (2, titles[0])):
            title, _ = self.press_until("LEFT", like=expected, box=TITLE_BOX)
            self.shot(f"library-left-{n}", self.last_rgb)
            self.check("LEFT goes back a game", title is not None, step=n)
        self.press("A")
        opened = self.covered(titles[0], TITLE_BOX)
        self.shot("game-details", self.last_rgb)
        self.check("A opens the game's details", opened)
        details, _ = self.settled_label(box=DETAIL_TITLE_BOX)
        self.check("the details show the game's title", details is not None)
        # Two presses reach Settings whether or not a Cheats row sits between.
        for _ in range(2):
            self.press("UP")
            self.pause(0.3)
        self.press("A")
        opened = self.covered(details, DETAIL_TITLE_BOX)
        self.shot("game-settings", self.last_rgb)
        self.check("UP and A open the game's settings", opened)
        self.press("B")
        back, _ = self.settled_label(like=details, box=DETAIL_TITLE_BOX)
        self.shot("game-details-back", self.last_rgb)
        self.check("B comes back to the game's details", back is not None)
        self.press("B")
        title, _ = self.settled_label(like=titles[0], box=TITLE_BOX)
        self.shot("library-back", self.last_rgb)
        self.check("B comes back to the same game", title is not None)
        self.launch_fails_cleanly(titles[0])

    def launch_fails_cleanly(self, title: np.ndarray) -> None:
        """A and A launch the game. Dolphin has no IPL ROM and the demo disc no
        swiss/patches/ipl.bin, so the launch cannot read BS2: it must say so and,
        once A dismisses the message, come back to the same game in the Library.
        It used to free a pointer it had never set and crash. The game is from
        the console's other region, so the launch switches to that region's
        video mode first; the menu must switch back, which the probe checks at
        the end of the route (a launch that failed used to leave the menu at a
        PAL game's 50 Hz on an NTSC console)."""
        self.press("A")
        self.check("A opens the game's details again", self.covered(title, TITLE_BOX))
        self.press("A")
        self.check("a launch that cannot read BS2 comes back to the Library",
                   self.back_to_library(title, "launch-failure"))

    def back_to_library(self, title: np.ndarray, tag: str) -> bool:
        """After a launch that fails: A dismisses its message, and the Library
        must come back on the same game."""
        back, deadline = None, Deadline(self.emulator, BOOT_SECONDS / 2)
        while back is None and not deadline.expired():
            time.sleep(2.0)
            self.gray()
            self.shot(tag, self.last_rgb)
            if diagnose(self.last_rgb):
                break  # a crash or a black screen: check() reports which
            self.press("A")  # dismisses the failure message once it is up
            back, _ = self.settled_label(4, like=title, box=TITLE_BOX)
        self.shot(f"{tag}-back", self.last_rgb)
        return back is not None

    def change_source(self) -> None:
        """A on Change Source opens the device picker, RIGHT shows the next device
        (the disc drive, then Memory Card Slot A in Dolphin) and B leaves it."""
        before = text_mask(self.gray(), TITLE_BOX)
        self.press("A")
        first, _ = self.settled_label(unlike=before, box=TITLE_BOX)
        self.shot("source-picker", self.last_rgb)
        self.check("Change Source opens the device picker", first is not None)
        name, _ = self.press_until("RIGHT", unlike=first, box=TITLE_BOX)
        self.shot("source-picker-right", self.last_rgb)
        self.check("RIGHT shows the next device", name is not None)
        self.press("B")
        self.check("B leaves the device picker", self.covered(name, TITLE_BOX))

    def text_until(self, box: tuple[int, int, int, int], wanted) -> np.ndarray | None:
        """Steady text in a box, or none, that wanted(mask) accepts: None when
        it never comes."""
        deadline = Deadline(self.emulator, SETTLE_SECONDS)
        previous = None
        while not deadline.expired():
            mask = text_mask(self.gray(), box)
            if wanted(mask) and previous is not None and same_text(mask, previous):
                return mask
            previous = mask if wanted(mask) else None
            time.sleep(0.15)
        return None

    def info(self, button: str, like: np.ndarray | None = None,
             unlike: np.ndarray | None = None) -> np.ndarray | None:
        """A step in Memory Cards, and the focused save's name it comes to:
        its comment, once the save's art is read, rather than its file name."""
        self.press(button)
        self.pause(ART_SECONDS)
        return self.text_until(INFO_BOX, lambda mask: has_label(mask) and
                               (like is None or same_text(mask, like)) and
                               (unlike is None or not same_text(mask, unlike)))

    def differs(self, reference: np.ndarray, box: tuple[int, int, int, int]) -> bool:
        """Waits for the text in a box to be other words than reference, or none."""
        found = self.text_until(box, lambda mask: not same_text(mask, reference)) is not None
        self.pause(0.5)
        self.gray()
        return found

    def message(self, seconds: float = BOOT_SECONDS / 4) -> bool:
        """Waits for Memory Cards' maroon box: an operation is done."""
        deadline = Deadline(self.emulator, seconds)
        while not deadline.expired():
            self.last_rgb = self.emulator.frame()
            if message_up(self.last_rgb, self.menu_wide):
                return True
            time.sleep(0.2)
        return False

    def message_closes(self) -> bool:
        """Waits for the maroon box to close by itself (2 s)."""
        deadline = Deadline(self.emulator, SETTLE_SECONDS)
        while not deadline.expired():
            self.last_rgb = self.emulator.frame()
            if not message_up(self.last_rgb, self.menu_wide):
                return True
            time.sleep(0.2)
        return False

    def folder_changes(self, folder: Path, before: set[str]) -> set[str]:
        """A memory card's saves once Dolphin has written them to its folder."""
        deadline = Deadline(self.emulator, FLUSH_SECONDS)
        while not deadline.expired() and saves(folder) == before:
            time.sleep(0.25)
        self.pause(1.0)  # the file whole, not as it is being written
        return saves(folder)

    def steps(self, buttons: str, seconds: float = 0.6) -> None:
        for button in buttons.split():
            self.press(button)
            self.pause(seconds)

    def memory_cards(self) -> None:
        """On the System face, DOWN and A open Memory Cards, with a GCI folder
        memory card in each slot (self.cards: make_test_saves.py's Slot A and
        Slot B). Slot A's first save has the focus and its name is in the info
        bar. RIGHT moves along Slot A's first row, a new save each time, and a
        fourth RIGHT crosses to Slot B: LEFT then comes back to the row's last
        save, not the bump of a stack's edge, and on to the first. DOWN four
        times scrolls Slot A's stack (the arrow above it shows), and UP four
        times comes back. R swaps the right stack for another place and back.
        A opens save details, B returns without an action, and A Actions
        opens the box beside the save, which changes the bottom buttons. B
        closes that box. Move, Copy, Yes copies the
        save to Slot B: the maroon box says so and closes by itself, and Slot
        B's folder gains the save, its blocks the same. A on it again opens the
        box with Move dimmed, as Slot B has it now, and the buttons say why.
        Erase, then Yes over the No it starts on, erases it, and Slot A's
        folder no longer has it. L swaps the left stack and back, and B
        leaves, back to the System face."""
        slot_a, slot_b = self.cards / "A", self.cards / "B"
        self.press("DOWN")
        self.pause(0.6)
        self.press("A")
        self.pause(2.0)  # the screen opens, and the saves on it are read
        first, _ = self.settled_label(box=INFO_BOX)
        self.shot("memory-cards", self.last_rgb)
        self.check("A on Memory Cards opens the cube screen, a save's name below it",
                   first is not None)
        gray = self.gray()
        self.check("both stacks show a memory card: a letter, Open and the free blocks",
                   has_label(text_mask(gray, LEFT_HEADER_BOX)) and
                   has_label(text_mask(gray, RIGHT_HEADER_BOX)))
        browsing = text_mask(gray, FOOTER_BOX)
        seen = [first]
        for n in (1, 2, 3):
            mask = self.info("RIGHT", unlike=seen[-1])
            self.shot(f"memory-cards-right-{n}", self.last_rgb)
            self.check("RIGHT moves to the next save", mask is not None, step=n)
            seen.append(mask)
        crossed = self.info("RIGHT", unlike=seen[-1])
        self.shot("memory-cards-slot-b", self.last_rgb)
        self.check("RIGHT from the last column moves to another save",
                   crossed is not None and not any(same_text(crossed, mask) for mask in seen))
        self.check("LEFT from Slot B comes back to Slot A's last column, not a stack's edge",
                   self.info("LEFT", like=seen[3]) is not None)
        for n in (2, 1, 0):
            self.check("LEFT goes back a save", self.info("LEFT", like=seen[n]) is not None, to=n)
        for n in range(4):
            self.press("DOWN")
            self.pause(0.6)
        self.pause(ART_SECONDS)
        gray = self.gray()
        self.shot("memory-cards-scrolled", self.last_rgb)
        self.check("DOWN past the window's last row scrolls the stack: the arrow above it shows",
                   arrow_up(gray))
        for n in range(4):
            self.press("UP")
            self.pause(0.6)
        back = self.text_until(INFO_BOX, lambda mask: same_text(mask, first))
        self.check("UP comes back to the first save, the stack scrolled back", back is not None and
                   not arrow_up(self.gray()))
        self.swap("R", RIGHT_HEADER_BOX, "right")
        self.save_details("memory-cards-details")
        self.press("B")
        closed = self.text_until(FOOTER_BOX, lambda mask: same_text(mask, browsing))
        self.check("B closes details without opening an action", closed is not None)
        self.save_details("memory-cards-details-again")
        self.press("A")
        opened = self.differs(browsing, FOOTER_BOX)
        self.shot("memory-cards-box", self.last_rgb)
        self.check("A Actions opens the box beside the save", opened)
        menu = text_mask(self.gray(), FOOTER_BOX)
        self.press("B")
        closed = self.text_until(FOOTER_BOX, lambda mask: same_text(mask, browsing))
        self.check("B closes the box", closed is not None)
        before_b = saves(slot_b)
        self.steps("A A DOWN A")
        self.shot("memory-cards-copy-to", self.emulator.frame())
        self.press("A")
        self.check("Copy, Yes: the maroon box says the save is copied", self.message())
        self.shot("memory-cards-copied", self.last_rgb)
        gained = self.folder_changes(slot_b, before_b) - before_b
        name = next(iter(gained)) if len(gained) == 1 else ""
        self.check("Slot B's folder gains the save, its blocks the same as on Slot A",
                   bool(name) and (slot_a / name).exists() and
                   same_save((slot_a / name).read_bytes(), (slot_b / name).read_bytes()),
                   gained=sorted(gained))
        self.check("the maroon box closes by itself", self.message_closes())
        self.steps("A A", 0.3)
        self.differs(browsing, FOOTER_BOX)
        reason = text_mask(self.gray(), FOOTER_BOX)
        self.shot("memory-cards-dimmed", self.last_rgb)
        self.check("A on the copied save: Move is dimmed, and the buttons say why",
                   not same_text(reason, menu) and not same_text(reason, browsing))
        self.press("B")
        self.text_until(FOOTER_BOX, lambda mask: same_text(mask, browsing))
        before_a = saves(slot_a)
        self.steps("A A DOWN DOWN A")
        self.shot("memory-cards-erase", self.emulator.frame())
        self.steps("UP")
        self.press("A")
        self.check("Erase, Yes: the maroon box says the save is erased", self.message())
        self.shot("memory-cards-erased", self.last_rgb)
        gone = before_a - self.folder_changes(slot_a, before_a)
        self.check("the save is gone from Slot A's folder", bool(name) and gone == {name},
                   gone=sorted(gone))
        self.check("the maroon box closes by itself", self.message_closes())
        # The left storage menu also selects SD, then restores Slot A.
        self.swap("L", LEFT_HEADER_BOX, "left")
        left = text_mask(self.gray(), LEFT_HEADER_BOX)
        self.press("B")
        self.check("B leaves Memory Cards", self.differs(left, LEFT_HEADER_BOX))
        self.shot("memory-cards-left", self.last_rgb)

    def swap(self, button: str, box: tuple[int, int, int, int], side: str) -> None:
        """Choose SD by name with L or R, then restore that side's card.
        Each menu starts on its current place: A, B, SD in that order."""
        header = text_mask(self.gray(), box)
        self.press(button)
        self.pause(0.3)
        self.shot(f"memory-cards-{button.lower()}-storage", self.emulator.frame())
        if self.storage == "dvd":
            # This route has no configuration SD device. SD is named but
            # dimmed, with its reason; A must not replace a working card.
            self.steps("DOWN" if button == "R" else "DOWN DOWN")
            self.press("A")
            self.pause(0.3)
            self.check(f"{button}, unavailable SD leaves the {side} card in place",
                       same_text(text_mask(self.gray(), box), header))
            self.shot(f"memory-cards-{button.lower()}-sd-unavailable", self.last_rgb)
            self.press("B")
            self.check("B cancels the storage menu with its card still shown",
                       self.text_until(box, lambda mask: same_text(mask, header)) is not None)
            return
        self.steps("DOWN A" if button == "R" else "DOWN DOWN A")
        swapped = self.differs(header, box)
        self.shot(f"memory-cards-{button.lower()}", self.last_rgb)
        self.check(f"{button}, SD card chooses the {side} stack's storage", swapped)
        self.press(button)
        self.steps("UP A" if button == "R" else "UP UP A")
        again = self.text_until(box, lambda mask: same_text(mask, header))
        self.check(f"{button}, Slot {'B' if button == 'R' else 'A'} brings its memory card back",
                   again is not None)

    def open_save_details(self) -> bool:
        """Retry a missed A only while the browser's static context is unchanged.

        Once a panel or another screen change appears, A must not be repeated:
        on details it means Actions. Cube animation is not an input response,
        so compare the storage headers, info text and footer, not moving art.
        """
        boxes = (LEFT_HEADER_BOX, RIGHT_HEADER_BOX, INFO_BOX, FOOTER_BOX)
        before = self.gray()
        if save_details_panel(before):
            return True
        context = tuple(text_mask(before, box) for box in boxes)
        changed = False
        for attempt in range(PRESSES):
            self.press("A")
            deadline = Deadline(self.emulator, SETTLE_SECONDS)
            while not deadline.expired():
                gray = self.gray()
                if save_details_panel(gray):
                    return True
                changed = changed or any(not same_text(text_mask(gray, box), original)
                                         for box, original in zip(boxes, context))
                time.sleep(.15)
            if changed or attempt + 1 == PRESSES:
                return False
            self.pressed_again.append("A")
        return False

    def save_details(self, name: str) -> dict[str, np.ndarray]:
        """Open details and verify authored fields on the real screen.

        Host renderer contracts assert the exact words/date/block values;
        these masks prove the fields are actually visible in Dolphin. No
        action is selected until another A press.
        """
        self.check("A opens the save details panel", self.open_save_details())
        boxes = {"title": SAVE_DETAILS_TITLE_BOX, "size": SAVE_DETAILS_SIZE_BOX,
                 "created": SAVE_DETAILS_CREATED_BOX, "updated": SAVE_DETAILS_UPDATED_BOX,
                 "actions": SAVE_DETAILS_ACTIONS_BOX}
        fields = {}
        for field, box in boxes.items():
            # The presentation's size/source line uses the renderer's muted
            # color. Jet Black removes its violet chroma; its visible gray
            # peaks below TEXT_LEVEL. Keep normal label checks unchanged.
            level = SAVE_DETAILS_SIZE_LEVEL if field == "size" else TEXT_LEVEL
            mask, _ = self.settled_label(box=box, level=level)
            self.check(f"save details displays its {field} field", mask is not None,
                       aspect="16:9" if self.menu_wide else "4:3")
            fields[field] = mask
        fields["blocks"] = text_mask(self.gray(), SAVE_DETAILS_BLOCKS_BOX, SAVE_DETAILS_SIZE_LEVEL)
        self.shot(name, self.last_rgb)
        return fields

    def raw_animation(self) -> None:
        """Capture both colors authored in RAW's animated icon, not cube motion."""
        seen = set()
        deadline = Deadline(self.emulator, 4.0)
        while not deadline.expired() and len(seen) < 2:
            rgb = self.emulator.frame()
            frame = raw_icon_frame(rgb, self.menu_wide)
            if frame is not None and frame not in seen:
                self.shot(f"virtual-cards-icon-frame-{frame}", rgb)
                seen.add(frame)
            self.pause(.05)
        self.check("RAW icon plays both distinct texture frames on its selected cube",
                   seen == {0, 1}, frames=sorted(seen))

    def library_save_stats(self, name: str) -> tuple[dict[str, np.ndarray], np.ndarray]:
        """From Home's Library face, inspect the synthetic game's saves inset."""
        self.press("A")
        title, _ = self.settled_label(box=TITLE_BOX)
        self.check("Library opens the demonstration game", title is not None)
        self.press("A")
        self.check("A opens Library game details", self.covered(title, TITLE_BOX))
        fields = {}
        for field, box in (("summary", LIBRARY_SAVES_SUMMARY_BOX),
                           ("updated", LIBRARY_SAVES_UPDATED_BOX)):
            mask, _ = self.settled_label(box=box)
            self.check(f"Library SAVES displays its {field}", mask is not None)
            fields[field] = mask
        self.shot(name, self.last_rgb)
        self.press("B")
        self.check("B returns from details to the same Library game",
                   self.settled_label(like=title, box=TITLE_BOX)[0] is not None)
        self.press("B")
        home, _ = self.settled_label()
        self.check("B returns to Home Library", home is not None)
        return fields, home

    def virtual_cards(self) -> None:
        """No physical cards: both columns start on SD, one opens a RAW image
        and exports a save through Copy into the other's SD folder."""
        home = self.boot()
        stats_before, home = self.library_save_stats("virtual-cards-library-before-export")
        faces = [home]
        for n in range(1, 4):
            faces.append(self.turn(faces, "RIGHT", f"virtual-cards-home-right-{n}"))
        self.press("A")
        self.check("A opens System", self.covered(faces[-1]))
        self.steps("DOWN A")
        self.pause(2.0)
        left = text_mask(self.gray(), LEFT_HEADER_BOX)
        right = text_mask(self.gray(), RIGHT_HEADER_BOX)
        self.shot("virtual-cards-sd", self.last_rgb)
        self.check("empty physical slots open SD storage in both columns",
                   has_label(left) and has_label(right) and self.cards is None)
        self.press("R")
        self.pause(0.3)
        self.shot("virtual-cards-storage-menu", self.emulator.frame())
        self.press("A")  # keep the right column on its currently selected SD
        self.check("choosing right storage keeps both SD folders",
                   same_text(text_mask(self.gray(), LEFT_HEADER_BOX), left) and
                   same_text(text_mask(self.gray(), RIGHT_HEADER_BOX), right))
        # The source focus must stay on the left after choosing right storage.
        self.press("A")  # the only SD item is Demo Card.raw
        self.check("A opens the SD card's RAW image", self.differs(left, LEFT_HEADER_BOX))
        self.pause(ART_SECONDS)
        first, _ = self.settled_label(box=INFO_BOX)
        self.check("the RAW card shows a save's comment", first is not None)
        self.shot("virtual-cards-raw", self.last_rgb)
        self.check("opening RAW on the left keeps the right SD folder",
                   same_text(text_mask(self.gray(), RIGHT_HEADER_BOX), right))
        self.raw_animation()
        known = self.save_details("virtual-cards-details-known")
        self.press("B")
        self.check("B closes RAW save details to browsing",
                   self.text_until(INFO_BOX, lambda mask: same_text(mask, first)) is not None)
        second = self.info("RIGHT", unlike=first)
        self.check("RIGHT browses another save in RAW", second is not None)
        unknown = self.save_details("virtual-cards-details-unknown")
        self.check("known and unknown save dates have different text",
                   not same_text(known["updated"], unknown["updated"]))
        self.check("both saves report creation date as not recorded",
                   same_text(known["created"], unknown["created"]))
        self.check("two-block and one-block save sizes have different text",
                   not same_text(known["blocks"], unknown["blocks"]))
        self.press("B")
        again = self.info("LEFT", like=first)
        self.check("LEFT returns to the RAW save to export", again is not None)
        # Open the same image independently on the right. All actions are
        # unavailable, but details still opens and B always returns.
        self.steps("RIGHT RIGHT RIGHT RIGHT")
        self.press("A")
        self.check("right SD independently opens RAW", self.differs(right, RIGHT_HEADER_BOX))
        self.pause(ART_SECONDS)
        both = self.save_details("virtual-cards-details-both-raw")
        self.check("details works with two read-only RAW columns",
                   same_text(known["updated"], both["updated"]))
        self.press("A")
        self.pause(.3)
        self.shot("virtual-cards-all-actions-unavailable", self.emulator.frame())
        disabled = text_mask(self.gray(), FOOTER_BOX)
        self.press("A")  # dimmed Copy cannot export into a RAW image
        self.pause(.3)
        self.check("an unavailable RAW Copy keeps its action menu",
                   same_text(text_mask(self.gray(), FOOTER_BOX), disabled) and
                   not message_up(self.last_rgb, self.menu_wide))
        self.press("B")  # action menu -> right RAW browsing
        self.press("B")  # right RAW -> independent writable folder
        self.check("right RAW closes to the independent SD folder",
                   self.text_until(RIGHT_HEADER_BOX, lambda mask: same_text(mask, right)) is not None)
        self.steps("LEFT LEFT LEFT LEFT")
        self.check("left source stays in its RAW image",
                   self.text_until(INFO_BOX, lambda mask: same_text(mask, first)) is not None)
        self.virtual_popup_checks()
        self.save_details("virtual-cards-details-to-export")
        self.press("A")  # Actions: read-only RAW starts on Copy
        self.pause(0.3)
        self.shot("virtual-cards-copy-menu", self.emulator.frame())
        self.press("A")
        self.pause(0.3)
        self.shot("virtual-cards-copy-to-sd", self.emulator.frame())
        self.press("A")
        self.check("Copy exports RAW's save to the SD folder", self.message())
        self.shot("virtual-cards-exported", self.last_rgb)
        self.check("the export message closes by itself", self.message_closes())
        self.press("B")
        back = self.text_until(LEFT_HEADER_BOX, lambda mask: same_text(mask, left))
        self.check("B closes RAW to its containing SD folder", back is not None)
        self.check("the other SD column keeps its folder after export",
                   same_text(text_mask(self.gray(), RIGHT_HEADER_BOX), right))
        self.shot("virtual-cards-back-to-sd", self.last_rgb)
        self.press("B")
        self.check("B leaves Memory Cards from the SD root", self.differs(left, LEFT_HEADER_BOX))
        self.press("B")  # System -> Home
        self.check("B returns to Home System", self.settled_label(like=faces[3])[0] is not None)
        for n in (2, 1, 0):
            mask, _ = self.press_until("LEFT", like=faces[n])
            self.check("LEFT returns toward Home Library", mask is not None, to=n)
        stats_after, _ = self.library_save_stats("virtual-cards-library-after-export")
        self.check("Library counts change after one GCI export",
                   not same_text(stats_before["summary"], stats_after["summary"]))
        self.check("Library retains the save's recorded latest date after export",
                   same_text(stats_before["updated"], stats_after["updated"]))

    def virtual_popup_checks(self) -> None:
        """The private emulator image after dialogs, before authorizing Copy."""
        if self.sd_image is None:
            raise Broken("virtual-card proof needs its emulator SD image")
        folder = make_test_saves.SAVE_FOLDER
        original, _ = make_test_saves.virtual_card()
        raw = card.read_card(self.sd_image, f"{folder}/{make_test_saves.RAW_CARD_NAME}")
        exported = card.read_card(self.sd_image, f"{folder}/{make_test_saves.RAW_EXPORT_NAME}")
        self.check("details, B Back and unusable RAW actions leave the source image unchanged",
                   raw == original)
        self.check("details and unusable actions create no exported GCI", exported is None)

    def virtual_card_checks(self, image: Path) -> None:
        """The actual FAT bytes, after navigating and exporting in Indigo."""
        original, gcis = make_test_saves.virtual_card()
        folder = make_test_saves.SAVE_FOLDER
        raw = card.read_card(image, f"{folder}/{make_test_saves.RAW_CARD_NAME}")
        exported = card.read_card(image, f"{folder}/{make_test_saves.RAW_EXPORT_NAME}")
        self.check("the source RAW image is byte-identical after browsing and export", raw == original,
                   bytes=len(raw) if raw is not None else None)
        self.check("the exported GCI has the selected identity and exact BAT-ordered payload",
                   exported is not None and same_save(gcis[0], exported),
                   file=make_test_saves.RAW_EXPORT_NAME,
                   bytes=len(exported) if exported is not None else None)
        if exported is not None:
            (self.out / "virtual-card-export.gci").write_bytes(exported)

    def tour(self) -> None:
        self.smoke()


def sheet(shots: list[tuple[str, Path]], path: Path) -> None:
    if not shots:
        return
    columns, width, height = 4, 320, 240
    rows = (len(shots) + columns - 1) // columns
    canvas = Image.new("RGB", (columns * width, rows * (height + 18)), "black")
    draw = ImageDraw.Draw(canvas)
    for index, (name, picture) in enumerate(shots):
        x, y = index % columns * width, index // columns * (height + 18)
        canvas.paste(Image.open(picture).convert("RGB").resize((width, height)), (x, y + 18))
        draw.text((x + 4, y + 3), f"{index + 1}. {name}", fill=(255, 220, 90))
    canvas.save(path)


def fatal_lines(log: Path) -> list[str]:
    text = log.read_text(errors="replace") if log.exists() else ""
    return [line.strip() for line in text.splitlines() if FATAL.search(line)][:20]


def poster_stack(log: Path) -> dict[str, int] | None:
    """The most of its stack the poster thread used in a run, or None when
    it never ran."""
    text = log.read_text(errors="replace") if log.exists() else ""
    used = [(int(m.group(1)), int(m.group(2))) for m in POSTER_STACK.finditer(text)]
    return {"most": max(u for u, _ in used), "of": used[0][1], "stops": len(used)} if used else None


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("dol", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--route", choices=("smoke", "tour", "game", "save", "virtual-cards"), default="smoke")
    parser.add_argument("--probe", type=Path, help="the probe DOL (probe/), launched as an app and a game")
    parser.add_argument("--region", choices=tuple(REGIONS), default="pal")
    parser.add_argument("--storage", choices=tuple(STORAGES), default="dvd")
    parser.add_argument("--cable", choices=tuple(CABLES), default="composite")
    parser.add_argument("--card-zip", type=Path, help="the release zip, unpacked onto the SD card")
    parser.add_argument("--settings", help="start the card with settings/<name>.ini as its global.ini")
    parser.add_argument("--sd-faults", help="the SD card's faults (DOLPHIN_SD_FAULTS), such as write-error-after=4")
    parser.add_argument("--fragments", type=int, default=0,
                        help=f"the probe's game in that many pieces on the card; over {card.MAX_FRAGMENTS} "
                             "its launch must be refused")
    parser.add_argument("--disc", type=Path, help="a disc to use instead of building card.py's")
    args = parser.parse_args(argv)
    if args.route == "game" and not args.probe:
        parser.error("the game route launches the probe: give --probe")
    if (args.storage != "dvd") != bool(args.card_zip):
        parser.error("an SD card (--storage) is made from the release zip (--card-zip), and only then")
    if args.settings and not args.card_zip:
        parser.error("settings to start with go on the SD card (--storage)")
    if args.route == "save" and not args.settings:
        parser.error("the save route changes settings a card starts with: give --settings")
    if args.fragments and not (args.card_zip and args.probe and args.route == "game"):
        parser.error("--fragments splits the probe's game on the SD card for the game route")
    if args.route == "virtual-cards" and args.storage not in ("gcloader", "sd2sp2"):
        parser.error("virtual-cards needs an SD card in GC Loader or SD2SP2, with both slots empty")
    start = (SETTINGS / f"{args.settings}.ini").read_text() if args.settings else None
    args.out.mkdir(parents=True, exist_ok=True)
    report: dict[str, object] = {"schema": "indigo.emulator-test.v1", "route": args.route,
                                 "region": args.region, "storage": args.storage, "cable": args.cable,
                                 "settings": args.settings, "sd_faults": args.sd_faults,
                                 "fragments": args.fragments,
                                 "dol": str(args.dol),
                                 "dolphin": _version()}
    status, emulator, route = 0, None, None
    with tempfile.TemporaryDirectory() as directory:
        work = Path(directory)
        try:
            dol, disc, sd = args.dol.resolve(), args.disc, None
            if args.card_zip:
                sd = work / "card.img"
                report["card"] = card.build_card(sd, args.card_zip, probe=args.probe,
                                                 foreign=FOREIGN[args.region], settings=start,
                                                 boot_iso=args.storage == "gcloader", fragments=args.fragments,
                                                 virtual_cards=args.route == "virtual-cards")
                if args.storage == "gcloader":  # the drive's disc until a game's is set
                    disc = work / "boot.iso"
                    disc.write_bytes(card.read_card(sd, "boot.iso"))
                dol = work / "ipl.dol"  # what a loader starts: the zip's own
                with zipfile.ZipFile(args.card_zip) as package:
                    dol.write_bytes(package.read("ipl.dol"))
                report["dol"] = f"{args.card_zip.name}:ipl.dol"
            elif disc is None:
                disc = work / "demo.iso"
                report["disc"] = card.build(disc, probe=args.probe, foreign=FOREIGN[args.region])
            # The smoke route opens Memory Cards with a memory card in each
            # slot, unless an SD Gecko has Slot B.
            cards = None
            if args.route in ("smoke", "tour") and args.storage != "sdgecko-b":
                cards = work / "cards"
                make_test_saves.write(str(cards))
            emulator = Emulator(dol, disc.resolve() if disc else None, work, args.out, args.region,
                                args.storage, sd, args.cable, args.sd_faults, cards,
                                empty_slots=args.route == "virtual-cards")
            route = Route(emulator, args.out, probe=bool(args.probe), fresh_card=bool(sd) and start is None,
                          cable=args.cable, region=args.region, fragments=args.fragments, cards=cards,
                          storage=args.storage, menu_wide=bool(start and "Menu Widescreen=Yes" in start),
                          sd_image=sd)
            getattr(route, args.route.replace("-", "_"))()
            if args.route == "save":
                # Power off and on again, with a card that works.
                emulator.close()
                (work / "again").mkdir()
                (args.out / "next-boot").mkdir(exist_ok=True)  # Dolphin's output of the second boot
                emulator = Emulator(dol, disc.resolve() if disc else None, work / "again",
                                    args.out / "next-boot", args.region, args.storage, sd, args.cable)
                route.emulator, route.pad = emulator, emulator.pad
                route.boot_again()
            elif sd:
                if args.route == "virtual-cards":
                    route.virtual_card_checks(sd)
                route.card_checks(sd, args.route, seeded(start) if start else None)
        except Failed as failure:
            status = 1
            report["failure"] = str(failure)
        except (Broken, OSError, subprocess.SubprocessError) as error:
            status = 2
            report["error"] = f"{type(error).__name__}: {error}"
        finally:
            if emulator:
                emulator.close()
    fatal = fatal_lines(args.out / "dolphin.log")
    if fatal and status == 0:
        status = 1
        report["failure"] = "Dolphin reported a crash or an invalid access"
    stack = poster_stack(args.out / "dolphin.log")
    if stack and stack["most"] > POSTER_STACK_SHARE * stack["of"] and status == 0:
        status = 1
        report["failure"] = (f"the poster thread used {stack['most']} of its {stack['of']} bytes of "
                             f"stack, more than {POSTER_STACK_SHARE:.0%}")
    report["poster_stack"] = stack
    report.update({
        "passed": status == 0,
        "checks": route.checks if route else [],
        "fatal_log_lines": fatal,
        "probe": route.report if route else None,
        "pressed_again": route.pressed_again if route else [],
        "pictures": [path.name for _, path in (route.shots if route else [])],
    })
    (args.out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    sheet(route.shots if route else [], args.out / "sheet.png")
    (args.out / "summary.md").write_text(summary(report))
    print(f"emulator test {'passed' if status == 0 else 'FAILED'}: {args.out / 'report.json'}")
    return status


def summary(report: dict[str, object]) -> str:
    where = ("the demonstration disc" if report["storage"] == "dvd" else
             f"an SD card in {report['storage'].upper()} made from the release zip")
    lines = [f"### Emulator ({report['route']}, {report['region'].upper()}"
             f"{'' if report['storage'] == 'dvd' else ', ' + report['storage'].upper()}): "
             f"{'passed' if report['passed'] else 'failed'}", "",
             f"{report['dolphin']}, {where}, a controller plugged in once Home is up.", "",
             "| Check | Result | Detail |", "| --- | --- | --- |"]
    for check in report["checks"]:
        detail = ", ".join(f"{k} {v}" for k, v in check.items() if k not in ("check", "passed"))
        lines.append(f"| {check['check']} | {'✅' if check['passed'] else '❌'} | {detail} |")
    for key in ("failure", "error"):
        if key in report:
            lines.append(f"\n**{key.capitalize()}:** {report[key]}")
    if report["fatal_log_lines"]:
        lines.append("\n**Dolphin reported:**\n```\n" + "\n".join(report["fatal_log_lines"]) + "\n```")
    if report.get("pressed_again"):
        lines.append(f"\n**Pressed again:** {len(report['pressed_again'])} press(es) the menu was too busy "
                     f"to see ({', '.join(report['pressed_again'])}): the screen had not changed, so the "
                     f"route pressed again, as a person would.")
    probe = report.get("probe")
    if probe:
        lines.append(f"\n**The probe reported:** disc `{probe['disc_id']}`, memory `{probe['memsize']:08X}`, "
                     f"the screen {video_mode(probe)}, audio DMA `{probe['ai_dma']:04X}`, "
                     f"{probe['stray_writes']} stray writes, started as `{probe['path'] or '(no path)'}`.")
    lines.append("\nEvery checkpoint is in the `sheet.png` of this run's `emulator` artifact.")
    return "\n".join(lines) + "\n"


def _version() -> str:
    try:
        return subprocess.run(["dolphin-emu-nogui", "--version"], capture_output=True, text=True,
                              timeout=20).stdout.strip() or "Dolphin (unknown version)"
    except (OSError, subprocess.SubprocessError):
        return "Dolphin (not found)"


if __name__ == "__main__":
    sys.exit(main())
