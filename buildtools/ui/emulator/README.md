# The emulator test

CI boots every build in [Dolphin](https://dolphin-emu.org/) and walks its
menus with a controller, the way someone would on a console, then launches a
game and an app all the way to a program that reports what the launch handed
it. The host tests prove the interface's parts; this proves the DOL they end
up in starts, draws, answers the controller and hands over cleanly.

| File | What it is |
| --- | --- |
| [`run.py`](run.py) | The test: starts Dolphin, plugs a controller in, walks the route, checks every step |
| [`card.py`](card.py) | The demonstration disc it boots with: fictitious games with banners, two damaged images, posters, and apps with pictures |
| [`dsu_pad.py`](dsu_pad.py) | The controller: a pad served to Dolphin over its DSU protocol |
| [`probe/`](probe/probe.c) | The program the launches reach: it reports what the hand-off left it |
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
   game then fails (a stand-in game has no program of its own, so Swiss falls
   back to the IPL's, and Dolphin has no IPL ROM): Indigo
   must say so and come back to the Library once A dismisses it. On the Source
   face, A on Change Source opens the device picker, RIGHT shows another
   device and B leaves the picker.
4. **Apps, last.** RIGHT and LEFT move between the disc's apps and back, and
   A on the probe brings up the launch screen and then the probe itself,
   which must report that the menu music stopped before the hand-off, that
   nothing still writes to memory after it, and that the app was started
   with its own path (`dvd:/apps/Probe.dol`), which homebrew uses to find
   its files.
5. **Nothing crashed.** No step lands on the exception screen or a black
   screen, and Dolphin's log reports no exception or invalid access.

The **game route** (`--route game`) boots, opens the Library, moves to the
probe's game and launches it from its details: the probe must see the game's
own disc ID, the 24 MB a game is promised, the music stopped and memory
quiet. CI runs both routes in both regions (`--region pal` or `ntsc`, the
video mode the console starts in): four jobs, the first of them the required
**Emulator** check.

Every check compares the screen with itself earlier in the same run (the
text under the cube, a game's title), never with stored pictures, so a
redesign does not break the test and a crash, a hang, a black screen or a
dead control does. Only where that text sits is fixed (`LABEL_BOX`,
`TITLE_BOX` and `DETAIL_TITLE_BOX` in `run.py`): a change that moves it
updates them. Text is the same text a pair of rows at a time: a slow frame
can leave the 480i picture half a line higher. The test waits for what it
expects to see, not for a fixed time, so a busy machine makes it slower, not
flaky. A failed step says why,
and names the crash when there is one.

## Running it

In CI it is the **Emulator** jobs. Each summary lists every check and what
the probe reported, and the `emulator-<commit>` artifacts hold a picture of
every step (`sheet.png` has them all), Dolphin's output and `report.json`.

On a machine with Docker, in the emulator runner's image
([`buildtools/ci/runner/emulator.Dockerfile`](../../ci/runner/emulator.Dockerfile)),
after `make dev`:

```bash
make -C buildtools/ui/emulator/probe        # in the toolchain image, as make dev
docker build -t indigo-emulator --build-arg RUNNER_VERSION=2.337.0 \
  --build-arg RUNNER_SHA256=<its linux-x64 checksum> \
  -f buildtools/ci/runner/emulator.Dockerfile buildtools/ci/runner
docker run --rm -u "$(id -u):$(id -g)" -v "$PWD:/work" -w /work --entrypoint python3 \
  indigo-emulator buildtools/ui/emulator/run.py cube/swiss/swiss.dol \
  --probe buildtools/ui/emulator/probe/probe.dol --route game --region ntsc --out emulator
```

Without `--probe` the smoke route stops at the launch screen, as it did
before the probe.

Or anywhere with Dolphin (`dolphin-emu-nogui`), Xvfb, FFmpeg, genisoimage and
Python 3 with Pillow and NumPy. The posters need gxtexconv; without it the
disc has none and every card shows its banner.

## The probe

[`probe/probe.c`](probe/probe.c) is a small libogc2 program, built with the
toolchain (`make -C buildtools/ui/emulator/probe`) and put on the
demonstration disc twice: as a game image whose boot program it is
(`card.probe_image`), and as `/apps/Probe.dol`. It reads what the hand-off
left before libogc touches anything, in `__SYS_PreInit`:

- the disc ID, memory size, video mode, bus and core clocks, arena and top
  of memory in low memory;
- whether the audio interface's DMA is still running (the menu music);
- the path it was started with (`argv[0]`);
- and, once up, whether any word of an 8 MB block it filled changes in a
  second and a half: a DMA or a thread the hand-off failed to stop.

Then it turns the screen azure and draws its results as a strip of black and
white blocks, 32 to a row, one row per word, ending in a CRC-32. `run.py`'s
`probe_report` finds the strip in a picture of the screen, whatever its scale,
and refuses a misread through the CRC. It writes each step to the debug UART
too ("probe: started", "probe: video", "probe: reported"), which Dolphin's log
shows. On a console it draws the same screen.

Dolphin reports 16 MB to programs started from Apps where a console reports
24 MB: Swiss reads the memory size from a register Dolphin leaves at zero.
Games are given 24 MB by Swiss itself, so the game route checks it.

## What Dolphin needs

- **The controller is plugged in after startup, and pulled at a launch.**
  Swiss's startup stalls in Dolphin when a controller is already connected
  (its pad and steering-wheel setup; a console is fine), and so does its
  shutdown before a launch, in libogc's serial transfer. The pad reports
  itself unplugged until Home is up, and again once the A that launches has
  gone in. That is why the controller is a DSU pad: Dolphin notices one
  coming and going while it runs.
- **The DSP runs its microcode** (`DSPHLE = False`). Dolphin's high-level
  stand-ins know libogc's audio library but not libogc2's (it logs
  "Unknown ucode (CRC = 8d527c50) - forcing AX"), so the menu music's stop
  before a launch was never answered and the launch screen never moved on.
- **Games need a file table.** The Library reads a game's banner through the
  file table its disc header points to, so each game on the demonstration
  disc has one, with an `opening.bnr`, as a real game does. Two more images
  are damaged on purpose, a header-only dump and a table that counts more
  entries than it holds, because the Library must list both without
  crashing.
