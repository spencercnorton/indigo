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
    starts one: the launch screen comes up (Dolphin can't take a launch
    further, see start_an_app);
  - nothing crashes: Dolphin emulates the MMU, so an invalid memory access
    stops Indigo on its exception screen as it would on a console, and
    Dolphin's own log reports no exception or invalid access.

usage: run.py DOL --out DIR [--route smoke|tour] [--disc ISO] [--keep-going]
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
DETAIL_TITLE_BOX = (264, 106, 600, 134)
TEXT_LEVEL = 160          # label text is bright; the waves behind it are not
SAME, DIFFERENT = 0.85, 0.5  # intersection over union of two label masks
BOOT_SECONDS = 120
SETTLE_SECONDS = 10
FATAL = re.compile("|".join((
    r"(?:DSI|ISI|Program|Machine Check|Alignment) Exception", r"Unhandled exception",
    r"Segmentation fault", r"core dumped", r"\bPANIC\b", r"ASSERT(?:ION)? FAILED",
    r"Invalid (?:read|write) (?:from|to)", r"Unknown (?:opcode|instruction)",
    r"FIFO (?:is )?(?:overflowed|desync)", r"failed to compile shader", r"device lost",
)), re.I)
DOLPHIN_INI = """[Core]
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


class Emulator:
    def __init__(self, dol: Path, disc: Path, work: Path, out: Path) -> None:
        self.out = out
        self.user = work / "dolphin"
        (self.user / "Config").mkdir(parents=True)
        (self.user / "Config/Dolphin.ini").write_text(DOLPHIN_INI)
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
        self.log = out / "dolphin.log"
        self.dolphin = subprocess.Popen(
            ["dolphin-emu-nogui", "-u", str(self.user), "-p", "x11", "-v", "OGL",
             "-C", f"Dolphin.Core.DefaultISO={disc}", "-e", str(dol)],
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

    def __init__(self, emulator: Emulator, out: Path) -> None:
        self.emulator = emulator
        self.out = out
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
        if not passed and getattr(self, "last_rgb", None) is not None:
            if why := diagnose(self.last_rgb):
                detail["screen"] = why
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
        deadline = time.monotonic() + seconds
        previous, steady = None, 0
        while time.monotonic() < deadline:
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

    def press(self, button: str) -> None:
        self.pad.press(button, 0.1)

    # -- the route
    def boot(self) -> np.ndarray:
        mask, now = self.settled_label(BOOT_SECONDS)
        boot = round(now - self.emulator.started, 1)
        self.shot("home", self.last_rgb)
        self.check("Home appears after boot", mask is not None, seconds=boot)
        self.pad.plug_in()
        deadline = time.monotonic() + 15
        while not self.pad.streaming and time.monotonic() < deadline:
            time.sleep(0.1)
        if not self.pad.streaming:
            raise Broken("Dolphin never asked for the controller")
        time.sleep(0.5)
        return mask

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
        name and the ring. The route stops there. Dolphin as CI runs it
        cannot take a program's launch further: its HLE DSP never answers
        AESND when Swiss stops the menu audio before the hand-off, and a
        hand-off it does reach (with the LLE DSP) never runs the program,
        from Swiss's own file list either.

        The A that opens Apps is held longer than Apps takes to read /apps:
        on a console an SD card is read before a thumb lets go, and that A
        once started the first app with no chance to choose."""
        self.pad.press("A", 2.5)
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
        apps = float(self.emulator.frame().mean())
        self.press("A")
        launched, deadline = False, time.monotonic() + SETTLE_SECONDS
        while not launched and time.monotonic() < deadline:
            time.sleep(0.3)
            rgb = self.emulator.frame()
            self.last_rgb = rgb
            launched = 0.02 < float(rgb.mean()) < 0.6 * apps
        self.shot("app-launch", self.last_rgb)
        self.check("A starts the app: the launch screen dims the Apps screen", launched,
                   apps=round(apps, 1), now=round(float(self.last_rgb.mean()), 1))

    def flip_apps_face(self, settings: np.ndarray, tag: str) -> None:
        """From the Settings face: R and R to Setup, DOWN and A into Console,
        five DOWNs to Apps Face and RIGHT to flip it. B goes back to Setup and
        B again saves and exits (the demo disc can't keep the file; the
        setting holds until Indigo restarts), back to the Settings face."""
        self.press("A")
        opened = self.covered(settings)
        self.shot(f"settings-apps-face-{tag}", self.last_rgb)
        self.check("A opens Settings", opened, apps_face=tag)
        for button, pause in (("R", 1.0), ("R", 1.0), ("DOWN", 0.6), ("A", 1.5)):
            self.press(button)
            time.sleep(pause)
        for _ in range(5):
            self.press("DOWN")
            time.sleep(0.4)
        self.press("RIGHT")
        time.sleep(1.0)
        self.shot(f"apps-face-{tag}", self.emulator.frame())
        self.press("B")
        time.sleep(1.0)
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
        deadline = time.monotonic() + SETTLE_SECONDS
        while time.monotonic() < deadline:
            time.sleep(0.3)
            if overlap(text_mask(self.gray(), box), reference) < DIFFERENT:
                time.sleep(1.0)  # let the screen finish arriving before the picture
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
            time.sleep(0.3)
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
        It used to free a pointer it had never set and crash."""
        self.press("A")
        self.check("A opens the game's details again", self.covered(title, TITLE_BOX))
        self.press("A")
        back, deadline = None, time.monotonic() + BOOT_SECONDS / 2
        while back is None and time.monotonic() < deadline:
            time.sleep(2.0)
            self.gray()
            self.shot("launch-failure", self.last_rgb)
            if diagnose(self.last_rgb):
                break  # a crash or a black screen: check() reports which
            self.press("A")  # dismisses the failure message once it is up
            back, _ = self.settled_label(4, like=title, box=TITLE_BOX)
        self.shot("launch-failed-back", self.last_rgb)
        self.check("a launch that cannot read BS2 comes back to the Library", back is not None)

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
    parser.add_argument("--route", choices=("smoke", "tour"), default="smoke")
    parser.add_argument("--disc", type=Path, help="a disc to use instead of building card.py's")
    args = parser.parse_args(argv)
    args.out.mkdir(parents=True, exist_ok=True)
    report: dict[str, object] = {"schema": "indigo.emulator-test.v1", "route": args.route,
                                 "dol": str(args.dol), "dolphin": _version()}
    status, emulator, route = 0, None, None
    with tempfile.TemporaryDirectory() as directory:
        work = Path(directory)
        try:
            disc = args.disc
            if disc is None:
                disc = work / "demo.iso"
                report["disc"] = card.build(disc)
            emulator = Emulator(args.dol.resolve(), disc.resolve(), work, args.out)
            route = Route(emulator, args.out)
            getattr(route, args.route)()
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
        "pictures": [path.name for _, path in (route.shots if route else [])],
    })
    (args.out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    sheet(route.shots if route else [], args.out / "sheet.png")
    (args.out / "summary.md").write_text(summary(report))
    print(f"emulator test {'passed' if status == 0 else 'FAILED'}: {args.out / 'report.json'}")
    return status


def summary(report: dict[str, object]) -> str:
    lines = [f"### Emulator ({report['route']}): {'passed' if report['passed'] else 'failed'}", "",
             f"{report['dolphin']}, the demonstration disc, a controller plugged in once Home is up.", "",
             "| Check | Result | Detail |", "| --- | --- | --- |"]
    for check in report["checks"]:
        detail = ", ".join(f"{k} {v}" for k, v in check.items() if k not in ("check", "passed"))
        lines.append(f"| {check['check']} | {'✅' if check['passed'] else '❌'} | {detail} |")
    for key in ("failure", "error"):
        if key in report:
            lines.append(f"\n**{key.capitalize()}:** {report[key]}")
    if report["fatal_log_lines"]:
        lines.append("\n**Dolphin reported:**\n```\n" + "\n".join(report["fatal_log_lines"]) + "\n```")
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
