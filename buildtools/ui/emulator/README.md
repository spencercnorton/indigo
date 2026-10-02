# The emulator test

CI boots every build in [Dolphin](https://dolphin-emu.org/) and walks its
menus with a controller, the way someone would on a console. The host tests
prove the interface's parts; this proves the DOL they end up in starts, draws
and answers the controller.

| File | What it is |
| --- | --- |
| [`run.py`](run.py) | The test: starts Dolphin, plugs a controller in, walks the route, checks every step |
| [`card.py`](card.py) | The demonstration disc it boots with: fictitious games with banners, two damaged images, posters, and apps with pictures |
| [`dsu_pad.py`](dsu_pad.py) | The controller: a pad served to Dolphin over its DSU protocol |
| [`test_emulator.py`](test_emulator.py) | Tests for those three, without Dolphin |

## What it checks

Dolphin runs headless, drawing with software OpenGL on a virtual X server,
and emulates the MMU, so an invalid memory access stops Indigo on its
exception screen as it would on a console. The route:

1. **Boot.** Home appears, with a face's name under the cube.
2. **Turn.** RIGHT five times: each turn shows another name, the fifth
   comes back to the first, and the five names differ (the disc has apps, so
   Apps is a fifth face). LEFT turns back.
3. **Every face.** A opens it and B comes back to the same face. In the
   Library, RIGHT twice and LEFT twice move between games and back, and A
   opens a game's details. There UP moves to Settings and A opens them, B
   comes back to the details, and B again to the same game. A launch of that
   game then fails (Dolphin has no IPL ROM, so BS2 cannot be read): Indigo
   must say so and come back to the Library once A dismisses it. On the Source
   face, A on Change Source opens the device picker, RIGHT shows another
   device and B leaves the picker. On the Settings face, Setup › Console › Apps
   Face Off takes Apps off the cube and On puts it back, and Setup › Library
   › Library Folders On shows the disc's folders in the Library: an empty
   folder opens with only its way back, A opens a folder and a folder in it,
   which also lists the game a level further down, and B goes back up a
   folder at a time to the same card, then Home.
4. **Apps, last.** RIGHT and LEFT move between the disc's apps and back, and
   A on one brings up the launch screen. The route stops there: Dolphin as
   CI runs it can't take any program's launch further (its HLE DSP never
   answers when Swiss stops the menu audio, and with the LLE DSP the
   hand-off runs no program, from Swiss's own file list either). A console
   proves the rest.
5. **Nothing crashed.** No step lands on the exception screen or a black
   screen, and Dolphin's log reports no exception or invalid access.

Every check compares the screen with itself earlier in the same run (the
text under the cube, a game's title), never with stored pictures, so a
redesign does not break the test and a crash, a hang, a black screen or a
dead control does. Only where that text sits is fixed (`LABEL_BOX`,
`TITLE_BOX` and `DETAIL_TITLE_BOX` in `run.py`): a change that moves it
updates them. The test waits for what it expects to see, not for a fixed
time, so a busy machine makes it slower, not flaky. A failed step says why,
and names the crash when there is one.

## Running it

In CI it is the **Emulator** job. Its summary lists every check, and the
`emulator-<commit>` artifact holds a picture of every step (`sheet.png` has
them all), Dolphin's output and `report.json`.

On a machine with Docker, in the emulator runner's image
([`buildtools/ci/runner/emulator.Dockerfile`](../../ci/runner/emulator.Dockerfile)),
after `make dev`:

```bash
docker build -t indigo-emulator --build-arg RUNNER_VERSION=2.337.0 \
  --build-arg RUNNER_SHA256=<its linux-x64 checksum> \
  -f buildtools/ci/runner/emulator.Dockerfile buildtools/ci/runner
docker run --rm -u "$(id -u):$(id -g)" -v "$PWD:/work" -w /work --entrypoint python3 \
  indigo-emulator buildtools/ui/emulator/run.py cube/swiss/swiss.dol --out emulator
```

Or anywhere with Dolphin (`dolphin-emu-nogui`), Xvfb, FFmpeg, genisoimage and
Python 3 with Pillow and NumPy. The posters need gxtexconv; without it the
disc has none and every card shows its banner.

## Two things Dolphin needs

- **The controller is plugged in after startup.** Swiss's startup stalls in
  Dolphin when a controller is already connected (its pad and steering-wheel
  setup; a console is fine), so the pad reports itself unplugged until Home
  is up. That is why the controller is a DSU pad: Dolphin notices one being
  connected while it runs.
- **Games need a file table.** The Library reads a game's banner through the
  file table its disc header points to, so each game on the demonstration
  disc has one, with an `opening.bnr`, as a real game does. Two more images
  are damaged on purpose, a header-only dump and a table that counts more
  entries than it holds, because the Library must list both without
  crashing.
