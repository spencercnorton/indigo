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
  - on the Settings face, Setup > Console > Apps Face Off takes Apps off the
    cube (System's next face is Library) and On puts it back;
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

usage: run.py DOL --out DIR [--route smoke|tour|game] [--probe DOL] [--region pal|pal60|ntsc]
              [--cable composite|component]
              [--storage dvd|sd2sp2|sdgecko-b --card-zip ZIP] [--disc ISO]
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
import card  # noqa: E402
import dsu_pad  # noqa: E402

WIDTH, HEIGHT = 640, 480
# Where text the route reads sits (x0, y0, x1, y1): the face's name under the
# cube on Home, the selected game's title in the Library (the focused
# device's name in the Source picker sits there too), and its title on the
# game's details.
LABEL_BOX = (200, 372, 440, 396)
TITLE_BOX = (200, 338, 440, 362)
# The page title Settings opens with on a new card ("Storage").
SETTINGS_TITLE_BOX = (30, 46, 230, 80)
DETAIL_TITLE_BOX = (264, 106, 600, 134)
TEXT_LEVEL = 160          # label text is bright; the waves behind it are not
SAME, DIFFERENT = 0.85, 0.5  # intersection over union of two label masks
# Waits count the console's own seconds (Emulator.emulated) when Dolphin
# reports them, so a busy machine slows a run instead of failing it; the
# machine's seconds still end a wait at WALL_FACTOR times as many.
BOOT_SECONDS = 120
SETTLE_SECONDS = 10
WALL_FACTOR = 5
TICKS_PER_SECOND = 486_000_000  # the GameCube's CPU clock, which Dolphin's ticks count
PRESS_SECONDS = 0.1  # of the console's time a button stays down, and up after
FATAL = re.compile("|".join((
    r"(?:DSI|ISI|Program|Machine Check|Alignment) Exception", r"Unhandled exception",
    r"Segmentation fault", r"core dumped", r"\bPANIC\b", r"ASSERT(?:ION)? FAILED",
    r"Invalid (?:read|write) (?:from|to)", r"Unknown (?:opcode|instruction)",
    r"FIFO (?:is )?(?:overflowed|desync)", r"failed to compile shader", r"device lost",
)), re.I)
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
# Rows on Settings' Storage page: DOWN past them reaches Save & Exit.
STORAGE_ROWS = 7
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


def text_mask(frame: np.ndarray, box: tuple[int, int, int, int] = LABEL_BOX) -> np.ndarray:
    x0, y0, x1, y1 = box
    return frame[y0:y1, x0:x1] >= TEXT_LEVEL


def diagnose(rgb: np.ndarray) -> str:
    """What a failed step's screen shows, when it is not Indigo at all."""
    black = float((rgb.max(axis=2) < 8).mean())
    white = int((rgb.min(axis=2) > 200).sum())
    if black > 0.6 and white > 2000:
        return "Indigo crashed: the screen shows its exception handler (see the last picture)"
    if black > 0.95:
        return "the screen went black"
    return ""


def overlap(a: np.ndarray, b: np.ndarray) -> float:
    union = np.logical_or(a, b).sum()
    return float(np.logical_and(a, b).sum() / union) if union else 1.0


def has_label(mask: np.ndarray) -> bool:
    """A word of text: enough lit pixels, spread across the band but not filling it."""
    lit = int(mask.sum())
    if not 60 <= lit <= mask.size // 3:
        return False
    columns = np.flatnonzero(mask.any(axis=0))
    return columns.size > 0 and 20 <= columns[-1] - columns[0] <= mask.shape[1] - 4


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
                 faults: str | None = None) -> None:
        self.out = out
        self.user = work / "dolphin"
        (self.user / "Config").mkdir(parents=True)
        ini = DOLPHIN_INI
        if STORAGES[storage]:
            ini = ini.replace("[Core]\n", f"[Core]\n{STORAGES[storage]} = {SD_CARD_DEVICE}\n"
                                          f"SP2SDCardImage = {card}\n", 1)
        (self.user / "Config/Dolphin.ini").write_text(ini)
        # Swiss's own debug output (its OSReport lines) goes to dolphin.log.
        (self.user / "Config/Logger.ini").write_text(
            "[Logs]\nOSREPORT = True\n[Options]\nVerbosity = 1\nWriteToConsole = True\nWriteToFile = False\n")
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
                 cable: str = "composite", region: str = "pal", fragments: int = 0) -> None:
        self.emulator = emulator
        self.fragments = fragments  # the pieces the probe's game is in on the card
        self.cable = cable
        self.region = region
        self.out = out
        self.probe = probe
        self.fresh_card = fresh_card
        self.report: dict[str, object] | None = None
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
        return rgb.max(axis=2)

    def settled_label(self, seconds: float = SETTLE_SECONDS, unlike: np.ndarray | None = None,
                      like: np.ndarray | None = None,
                      box: tuple[int, int, int, int] = LABEL_BOX) -> tuple[np.ndarray | None, float]:
        """Wait for steady text in a box (the face's name by default), optionally unlike or like a given one."""
        deadline = Deadline(self.emulator, seconds)
        previous, steady = None, 0
        while not deadline.expired():
            mask = text_mask(self.gray(), box)
            ok = has_label(mask)
            if ok and unlike is not None:
                ok = overlap(mask, unlike) < DIFFERENT
            if ok and like is not None:
                ok = overlap(mask, like) >= SAME
            steady = steady + 1 if ok and previous is not None and overlap(mask, previous) >= 0.95 else 0
            previous = mask if ok else None
            if steady >= 2:
                return mask, time.monotonic()
            time.sleep(0.15)
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
        Save & Exit, and A writes the settings to the card and goes Home."""
        title, now = self.settled_label(BOOT_SECONDS, box=SETTINGS_TITLE_BOX)
        self.shot("first-run", self.last_rgb)
        self.check("a new card opens Settings first", title is not None,
                   seconds=round(now - self.emulator.started, 1))
        self.plug_in()
        for _ in range(STORAGE_ROWS):
            self.press("DOWN")
            self.pause(0.4)
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
        """What Indigo wrote to the SD card, read back from its image. Settings
        the card started with must have lasted through Indigo's own saves."""
        def text(path: str) -> str:
            return (card.read_card(image, path) or b"").decode("latin-1")

        settings = text("swiss/settings/global.ini")
        self.check("the settings Indigo saved are on the card",
                   "Swiss Video Mode=" in settings and "Hide Apps Face=No" in settings, bytes=len(settings))
        if start:
            kept = seeded(settings)
            lost = {key: value for key, value in start.items() if kept.get(key) != value}
            self.check("the settings the card started with are all still there", not lost, lost=lost)
        if route != "game" or not self.report:  # no game started
            return
        recent = text("swiss/settings/recent.ini").split("Recent_1=")[0]
        self.check("the launched game is first in the recent list",
                   card.game_file(*card.PROBE_GAME) in recent, recent=recent.strip().splitlines()[-1:])
        played = text("swiss/settings/play-history-0.ini") + text("swiss/settings/play-history-1.ini")
        self.check("Indigo's play history records the game", f"\n{card.PROBE_GAME[0]}=" in played)

    def turn(self, faces: list[np.ndarray], button: str, name: str) -> np.ndarray:
        self.press(button)
        mask, _ = self.settled_label(unlike=faces[-1])
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
        self.check("five turns come back to the first face", overlap(back, home) >= SAME,
                   overlap=round(overlap(back, home), 3))
        distinct = min(overlap(a, b) for i, a in enumerate(faces) for b in faces[i + 1:])
        self.check("the five faces have five different names", distinct < DIFFERENT,
                   largest_overlap=round(distinct, 3))
        left = self.turn([back], "LEFT", "left-1")
        self.check("LEFT turns the other way", overlap(left, faces[-1]) >= SAME,
                   overlap=round(overlap(left, faces[-1]), 3))
        self.press("RIGHT")
        mask, _ = self.settled_label(like=home)
        self.check("RIGHT undoes LEFT", mask is not None)
        for n, face in enumerate(faces[:4]):
            # Home starts on the Library face: browse it while it is open,
            # change the source on the Source face, and turn Apps Face off
            # and on again in Settings.
            if n == 2:
                self.apps_face_off_and_on(faces)
            else:
                inside = {0: self.browse_library, 1: self.change_source}.get(n)
                self.open_and_close(face, n, inside)
            self.press("RIGHT")
            mask, _ = self.settled_label(like=faces[n + 1])
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
        self.press("RIGHT")
        other, _ = self.settled_label(unlike=first, box=TITLE_BOX)
        self.shot("apps-right", self.last_rgb)
        self.check("RIGHT moves to the next app", other is not None)
        self.press("LEFT")
        again, _ = self.settled_label(like=first, box=TITLE_BOX)
        self.check("LEFT goes back an app", again is not None)
        if self.probe:
            name = again
            for n in range(card.app_order(True).index(card.PROBE_APP[:-4])):
                self.press("RIGHT")
                name, _ = self.settled_label(unlike=name, box=TITLE_BOX)
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
        self.home_label = home
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
            self.press("RIGHT")
            title, _ = self.settled_label(unlike=title, box=TITLE_BOX)
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
        """From the Settings face: R and R to Setup, DOWN and A into Console,
        seven DOWNs to Apps Face and RIGHT to flip it. B goes back to Setup and
        B again saves and exits (the demo disc can't keep the file; the
        setting holds until Indigo restarts), back to the Settings face."""
        self.press("A")
        opened = self.covered(settings)
        self.shot(f"settings-apps-face-{tag}", self.last_rgb)
        self.check("A opens Settings", opened, apps_face=tag)
        for button, pause in (("R", 1.0), ("R", 1.0), ("DOWN", 0.6), ("A", 1.5)):
            self.press(button)
            self.pause(pause)
        for _ in range(7):
            self.press("DOWN")
            self.pause(0.4)
        self.press("RIGHT")
        self.pause(1.0)
        self.shot(f"apps-face-{tag}", self.emulator.frame())
        self.press("B")
        self.pause(1.0)
        self.press("B")
        mask, _ = self.settled_label(like=settings)
        self.shot(f"apps-face-{tag}-home", self.last_rgb)
        self.check("Save & Exit comes back to the Settings face", mask is not None, apps_face=tag)

    def apps_face_off_and_on(self, faces: list[np.ndarray]) -> None:
        """Setup > Console > Apps Face: Off takes Apps off the cube, so the
        face after System is Library; On puts it back after System."""
        library, settings, system, apps = faces[0], faces[2], faces[3], faces[4]
        for tag, after_system in (("off", library), ("on", apps)):
            self.flip_apps_face(settings, tag)
            self.press("RIGHT")
            self.check("RIGHT turns to System", self.settled_label(like=system)[0] is not None,
                       apps_face=tag)
            self.press("RIGHT")
            mask, _ = self.settled_label(unlike=system)
            self.shot(f"after-system-apps-face-{tag}", self.last_rgb)
            self.check(f"Apps Face {tag.title()}: after System comes "
                       f"{'Library' if tag == 'off' else 'Apps'}",
                       mask is not None and overlap(mask, after_system) >= SAME,
                       overlap=round(overlap(mask, after_system), 3) if mask is not None else None)
            for back in (system, settings):
                self.press("LEFT")
                self.check("LEFT turns back a face", self.settled_label(like=back)[0] is not None,
                           apps_face=tag)

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
            self.press("RIGHT")
            title, _ = self.settled_label(unlike=titles[-1], box=TITLE_BOX)
            self.shot(f"library-right-{n}", self.last_rgb)
            self.check("RIGHT moves to the next game", title is not None, step=n)
            titles.append(title)
        for n, expected in ((1, titles[1]), (2, titles[0])):
            self.press("LEFT")
            title, _ = self.settled_label(like=expected, box=TITLE_BOX)
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
        self.press("RIGHT")
        name, _ = self.settled_label(unlike=first, box=TITLE_BOX)
        self.shot("source-picker-right", self.last_rgb)
        self.check("RIGHT shows the next device", name is not None)
        self.press("B")
        self.check("B leaves the device picker", self.covered(name, TITLE_BOX))

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


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("dol", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--route", choices=("smoke", "tour", "game", "save"), default="smoke")
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
                                                 boot_iso=args.storage == "gcloader", fragments=args.fragments)
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
            emulator = Emulator(dol, disc.resolve() if disc else None, work, args.out, args.region,
                                args.storage, sd, args.cable, args.sd_faults)
            route = Route(emulator, args.out, probe=bool(args.probe), fresh_card=bool(sd) and start is None,
                          cable=args.cable, region=args.region, fragments=args.fragments)
            getattr(route, args.route)()
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
    report.update({
        "passed": status == 0,
        "checks": route.checks if route else [],
        "fatal_log_lines": fatal,
        "probe": route.report if route else None,
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
