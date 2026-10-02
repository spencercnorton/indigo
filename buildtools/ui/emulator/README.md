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

`card.py` also makes the SD card (`build_card`): see [An SD card](#an-sd-card).
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
quiet.

CI runs four jobs, the first of them the required **Emulator** check, each in
a video mode of its own:

| Job | Console | Video | Storage |
| --- | --- | --- | --- |
| Emulator (smoke) | PAL, composite | 576i | disc |
| game, PAL, component | PAL, component | 480p | disc |
| smoke, NTSC, component, SD2SP2 | NTSC, component | 480p | SD2SP2 |
| game, NTSC, SD2SP2 | NTSC, composite | 480i | SD2SP2 |

`--region pal|pal60|ntsc` is the console: the region Dolphin starts the video
hardware in, and an SRAM (`GC/SRAM.raw`) with the same video format, as a
real console's IPL leaves it; Dolphin's own SRAM is an NTSC console's
whatever the region. `--cable component` makes Dolphin report a component
cable, and Swiss's Auto video mode is then 480p. At the end of the smoke
route the probe reports the mode the menu was in, which must be the one the
console and cable ask for. That route's failed launch is of a game from the
console's other region, which switches to that region's video mode first, so
the check catches a menu that doesn't switch back.

Every check compares the screen with itself earlier in the same run (the
text under the cube, a game's title), never with stored pictures, so a
redesign does not break the test and a crash, a hang, a black screen or a
dead control does. Only where that text sits is fixed (`LABEL_BOX`,
`TITLE_BOX` and `DETAIL_TITLE_BOX` in `run.py`): a change that moves it
updates them. The test waits for what it expects to see, not for a fixed
time, so a busy machine makes it slower, not flaky. A failed step says why,
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

## An SD card

With `--storage sd2sp2 --card-zip Indigo-<version>.zip` there is no disc.
`card.build_card` unpacks the release zip onto an 8 GB FAT32 card image, as
someone sets up a card, and puts the same games, packs and apps beside it.
Indigo boots from the zip's own `ipl.dol`, and Dolphin serves the card as an
SD2SP2 in Serial Port 2: an SD card adapter the emulator runner's Dolphin
adds (`buildtools/ci/runner/dolphin/`), which speaks the SD protocol a byte
at a time, so libogc2's driver, FatFs and Swiss's device code run as they do
on a console. `--storage sdgecko-b` puts the card in Memory Card Slot B
instead.

A new card has no settings, so the route starts in Settings: it must open on
its own, and Save & Exit must write the file and go Home. With `--settings
<name>` the card starts with [`settings/<name>.ini`](settings/) as its
`global.ini` instead and Indigo goes straight Home; at the end every line of
it must still be on the card, through Indigo's own saves. CI's SD2SP2 smoke
job starts with [`non-default.ini`](settings/non-default.ini): colours, icons,
the clock on the left, no menu music or sounds, reduced motion, In-Game Reset. At the end the
test reads the card back: the settings Indigo saved (with Apps Face as the
route left it), and after the game route the launched game first in the
recent list and in Indigo's play history.

## The probe

[`probe/probe.c`](probe/probe.c) is a small libogc2 program, built with the
toolchain (`make -C buildtools/ui/emulator/probe`) and put on the
demonstration disc twice: as a game image whose boot program it is
(`card.probe_image`), and as `/apps/Probe.dol`. It reads what the hand-off
left before libogc touches anything, in `__SYS_PreInit`:

- the disc ID, memory size, video mode, bus and core clocks, arena and top
  of memory in low memory;
- the video mode the screen was left in (the display configuration and
  the 27/54 MHz clock with the component cable bit);
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

The emulator runner builds Dolphin from source with three patches
(`buildtools/ci/runner/dolphin/`): the SD card adapter, its writes flushed,
and a controller that answers like one. The disc routes' menus run in any
Dolphin; the launches and the SD card need the runner's.

- **A controller answers like one.** A console's controller ignores serial
  commands it doesn't know, and the hardware reports no response. Dolphin
  answered nothing, and the transfer then never ended: libogc's steering
  wheel probe at startup and the GameID packet Swiss sends before every
  launch both hung with a controller connected. The patched Dolphin answers
  no response, so the controller stays connected through a launch. It is
  still plugged in only once Home is up, which works in any Dolphin; that is
  why the controller is a DSU pad, which Dolphin notices arriving.
- **The DSP runs its microcode, in step with the CPU** (`DSPHLE = False`,
  `DSPThread = False`). Dolphin's high-level stand-ins know libogc's audio
  library but not libogc2's (it logs "Unknown ucode (CRC = 8d527c50) -
  forcing AX"), so the menu music's stop before a launch was never answered.
  On a thread of its own the DSP sometimes missed the mail that stop sends
  (`AESND_Reset` waits for it with interrupts off) when the machine was busy.
- **Games need a file table.** The Library reads a game's banner through the
  file table its disc header points to, so each game on the demonstration
  disc has one, with an `opening.bnr`, as a real game does. Two more images
  are damaged on purpose, a header-only dump and a table that counts more
  entries than it holds, because the Library must list both without
  crashing.
