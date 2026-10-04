# Changelog

Versions follow [semantic versioning](https://semver.org/). Each release is a
`vX.Y.Z` tag on `main`, and a release candidate before it is a `vX.Y.Z-rc.N`
tag on `beta`. The newest changes are at the top until their release is named.

## Unreleased

### New

- Settings › Setup › Library › **Library Folders** lets you sort your games
  into folders. On, the Library shows the folders in `/games` as cards beside
  the games, two levels deep: A opens one, B goes back up, and the heading
  names the folder you are in. A folder in a folder shows every game below
  it. Off (the default), the Library is one list of every game, as before.
  In `global.ini` it is `Library Folders`.
- A folder can have a picture: a PNG beside it with its name, such as
  `Nintendo.png` for the Nintendo folder, becomes its poster, as an app's
  picture does. A folder without one shows a poster of its name. A picture
  over 2 MB is never read, and posters are made one at a time on a thread of
  their own, so a large picture neither holds up a button nor runs the
  console short of memory.
- Settings › Setup › Console › **Wave Speed** sets how fast the waves drift:
  Slow (half as fast), Normal (the default) or Fast (three times as fast). A
  new speed picks up from where the waves are. In `global.ini` it is
  `Wave Speed`.
- Settings › Setup › Console › **Temperature** places the temperature dial on
  its own: Right, Left or Off, as Clock places the time. Clock moves only the
  time now. In the same corner the time sits beside the dial as before; apart,
  each sits in its own corner. In `global.ini` it is `Temperature`; settings
  saved by 2.2, which has no `Temperature`, keep the dial with the clock.
- Settings › Setup › Console › **Cube** chooses how Home's cube turns.
  Infinite (the default) turns round every face in a ring, as before. Classic
  lays the faces out like the GameCube's own menu: Library in front, Settings
  on the left, System on the right, Source on top and Apps underneath. From
  Library a direction turns to that side; from a side, only the way back or
  B returns to Library. The icons stay on their sides as the cube turns, and
  a Classic turn arrives in about a quarter of a second. In `global.ini` it
  is `Cube`.

### Changes

- Game folders skip Mac `._` metadata files, so copying a game from a Mac
  no longer makes its folder report an invalid disc image. Hidden files in
  a game's folder follow Show hidden files, as they do in the file list.
- Every face of the Home cube shows its icon as the cube turns, animated, the
  side faces at rest included. An icon on a face turned away from the screen
  is drawn four times as wide and twice as tall, then scaled down, so its
  thinnest lines keep their light, without breaking into fragments, fading
  out or flickering as the cube sways. Mostly horizontal icons, such as
  Settings' sliders, still look narrow on a face seen nearly edge-on.
- A slanted line on a face seen nearly edge-on, such as the clock's minute
  hand at ten to the hour, no longer disappears at some angles as the cube
  sways. Its corners were too sharp for its soft edge, and it was skipped;
  it is drawn plain now.
- The halo around the Home cube is gone, as the glow spot under it went in
  2.2.
- The Home cube's icons stand a little off the glass instead of lying in it:
  the face you are on lifts its icon, which shifts against the glass as the
  cube turns and sways. Under it a soft shadow falls on the glass, down and
  to the left, away from the light; the icon itself stays sharp. Faces turned
  away keep their icons down, so none hangs past the cube's edge.
- A Jet Black backdrop is darker still. Its bottom right corner, the
  brightest, read as a grey cloud.
- The CPU temperature holds steady. The console's sensor reads in 4 degree
  steps, so a CPU sitting on a step showed one side and then the other every
  second (40 °C, 44 °C, 40 °C...). Indigo now shows an average of the last few
  seconds' readings, which moves a degree at a time, at the top of the screen
  and in System Information alike.
- With Clock at Left, the loading spinner at the top right ran its word
  "Loading" from the wheel out to the screen's edge, where a TV can crop it.
  It reads inward from the wheel now, in either corner.
- The menus' animated background costs the console far less each frame: the
  cube works out its square roots without newlib's slow loop, keeps its
  outline's lengths, skips icons that can't be seen, copies only the part of
  the frame its glass shows, and isn't drawn at all under a full-screen page
  or when Grid and Spotlight park it beside the screen. Upstream's backdrop
  picture, which Indigo's background always covered, is no longer drawn under
  it. In Dolphin the background's time a frame went from 8.75 ms to 4.25 ms
  on Home and from 9.00 ms to 4.50 ms on the Settings face.
- Scrolling back through the Library shows the covers at once: a cover
  scrolled past keeps its place in memory until a new one needs it, instead
  of being read from the card again.
- Starting a two-disc game no longer reads every disc image in its folder to
  find the other disc, only those whose details don't already rule them out.
- A settings save that loses power (or a card pulled mid-save) keeps the
  settings from before it or the new ones, never none.
- Moving between games, the old title fades out before the new one fades
  in, rather than the two showing over each other.
- With UI Motion on Reduced, the cube no longer moves during the boot and
  then stops short.
- The Loading spinner and the progress bar's sweep keep the same speed at
  50 Hz as at 60 Hz.
- The Home controller's idle animation no longer stops after Indigo has been
  running for an hour and three quarters.
- Holding the stick in the Library's Horizontal, Vertical and Spotlight
  layouts, the games glide past without a jump. The strip could fall only
  one game behind the selection, so at the stick's pace it skipped forward
  two fifths of a card every step; with nine games or more it can fall two
  behind now.
- The waves behind the menus ease between Home's quieter strength and the
  other screens' as the cube moves, instead of brightening or dimming in one
  frame as you leave or come back to Home.
- Coming back to Home, the face's name and the hint under the cube fade in
  as the cube grows back to its place, instead of appearing at once over a
  cube still on its way.
- On Home, Source's and System's rows and the restart question fade in when
  they open, and the highlight fades from row to row as you move instead of
  jumping.
- As the boot's cube lands on Home, the icon on the face beside the front
  one fades in with the glass's light instead of appearing in one frame.
- A cover, a gameplay still or an app's or folder's poster fades in over a
  fifth of a second as it arrives, over the card that stood in for it,
  rather than replacing it in one frame. One read while it was off screen
  shows at once when you scroll to it, as before.
- The highlight in Game Details slides from row to row, and so do the current
  tab's cell in Settings and its underline in Memory Cards, as the focus in
  the cheat list already did, instead of jumping.
- A moving cover in the Library slides one way to rest, its edges never
  stepping back as it settles, and Settings' focus card no longer nudges an
  edge back the other way as it moves to a row of another width.
- In Spotlight, moving from one game with a gameplay still to another, the
  new still fades in over the old one, instead of both fading through a
  darker panel half way.
- The Home controller lets go of its idle play over a moment when you touch
  the pad, instead of its sticks jumping to your hand in one frame.

### Fixes

- Since 2.3.0-rc.1: a loader that starts Indigo with settings as arguments no
  longer turns Library Folders off and loses the FlattenDir it keeps, and a
  Clock argument no longer moves the temperature dial. Turning Library
  Folders on over a FlattenDir that was empty or already its own pattern now
  comes back to `*/games` when it goes off.
- Apps: a picture with a long block of data in it, more than 256 KB in one
  piece (some programs save a PNG that way, or put a large block of
  metadata in it), crashed Indigo while it made the app's poster. The
  checksum zlib-ng takes of a block that long needs more stack than the
  poster thread has; Indigo now takes it 8 KB at a time. Folder pictures
  are made the same way.
- A game or app could freeze as it started, until the console was switched
  off. Stopping the menu's music and sounds could catch the audio DSP with an
  answer the CPU had not read yet; the DSP then never took the stop, and the
  CPU waited for it with interrupts off. Indigo now builds libogc2's audio
  library from its own copy, with the stop fixed to let the DSP finish first.
- After a game failed to launch, the menu stayed in the video mode the launch
  had switched to for the game. A PAL game left an NTSC console's menu at
  50 Hz, which a TV that only takes 60 Hz shows as a black screen, and an
  NTSC game left a PAL console's at 60 Hz. The menu goes back to its own mode.
- Migrating a settings file from Swiss before 2019 (a single swiss.ini) no
  longer overruns the menu's stack, and a cheats file of 4 GiB, or the
  console running out of memory while reading one, no longer crashes it.

### For developers

- `test_frame_budget.py` runs the real cube renderer against counting GX
  stubs and holds each scene's per-frame cost (square roots, trig, vertices,
  GX_Begin calls, EFB pixels copied) to the ceilings in
  `frame_budget.json`; `--update` locks in a gain.
- `make -C cube/swiss BUILD=build-perf TARGET=swiss-perf UI_PERF=1` builds the
  performance overlay, now with the GPU's own counters; CI builds it beside
  every DOL.
- One list of C tests drives both the test Makefile and `run_tests.sh`, and
  the contracts lane builds every fuzzer, so a broken one fails CI.
- The DOL is the same bytes in any locale, any folder and whatever tags the
  clone has: the build links its files in byte order, names no build path,
  and `tags.h` (read only by upstream's startup autoload, which Indigo drops)
  is empty. CI's second build runs in another folder.
- New fuzzers and harnesses: `fuzz_cheats` (with allocations failing on
  purpose), the settings fuzzer now also merges files and migrates a legacy
  swiss.ini, and `test_config_save.py` cuts the power at every step of a
  settings save.
- Fuzz runs after "CI passed", so it never holds a runner a required job is
  waiting for, and the weekly run fits its hour. Build runs `make -j8 dev`,
  30 s faster; Reproducible build still builds one file at a time, as the
  README does, and proves the two give the same DOL.
- The toolchain's digest moves by hand, every copy at once: Dependabot leaves
  libogc2 alone and `check_workflows.py` holds all 13 copies equal.
- The menu music is `menu_music.mp3`, embedded with `#embed`, instead of a
  5 MB header of numbers.
- The DOL is 106 KB smaller: Swiss is built without unwind tables, which
  nothing in it reads.
- A thread whose stack overflows now crashes in the emulator test as it does on
  a console: the runner's Dolphin emulates the data address breakpoint libogc
  guards each thread's stack with, where it used to let the overrun corrupt
  memory unseen. A stack overflow that crashed a console, but passed CI, now
  fails there too.
- The emulator test presses a cube turn or a step along a row again when the
  menu was too busy to see the press and the screen did not change, as a
  person would; the report lists each such press.
- AESND, libogc2's audio library, is built from `cube/swiss/aesnd`: a copy
  byte-identical to the toolchain's libogc2, with Indigo's fix as a patch
  applied at build time. CI checks the copy against that commit and the
  toolchain, and stops AESND 300 times in the emulator test.
- CI saves settings to a card whose writes fail, boots it again and checks
  the settings still load in their colours, and its smoke job with every
  setting changed now runs on a GC Loader.
- The emulator test can put the probe's game in pieces on the SD card
  (`run.py --fragments N`), as a copy onto a used card can leave a game: in
  40 it must launch, the most Swiss and a GC Loader can serve, and in more it
  must be refused with a message. CI's GC Loader job launches it in 40.
- The emulator test can put the SD card in a GC Loader (`run.py --storage
  gcloader`): the runner's Dolphin answers as one, so Indigo finds it, keeps
  its settings on its card and launches a game by the game file's fragments,
  as on a console with one. CI launches the probe's game that way, in 480p.
- The emulator test counts the console's own seconds, which the runner's
  Dolphin now reports, for its waits and presses, so a busy machine slows a
  run instead of failing it; a failed step says where the console's CPU was.
- The emulated SD card can fail as a test asks (`run.py --sd-faults`), and
  `--route save` checks that settings saved to a failing card are still there
  on the next boot.
- The emulator test runs each job in a video mode of its own: PAL composite
  576i, NTSC composite 480i and a component cable's 480p, in both regions. A
  console's SRAM now matches its region, and the probe reports the mode the
  menu was in, which the smoke route checks after a failed launch of a game
  from the other region. Until now every job ran interlaced: 480p never ran.
- An SD card can start with settings (`run.py --settings <name>`, from
  `buildtools/ui/emulator/settings/`), which must all survive Indigo's own
  saves. CI's GC Loader smoke job starts with every setting of Indigo's own
  away from its default, but Menu Widescreen.
- The emulator test launches a game and an app all the way. A small program,
  the probe ([`buildtools/ui/emulator/probe/`](buildtools/ui/emulator/probe/probe.c)),
  sits on the demonstration disc as a game and as an app. Launched, it
  reports what the hand-off left it, drawn as blocks the test reads back
  from the screen: the menu music stopped, nothing still writing to memory,
  a game's own disc ID and its 24 MB, an app's own path. Launches used to
  stop at the launch screen in Dolphin: its stand-ins for the DSP don't know
  libogc2's audio library, so the DSP now runs its own microcode, in step
  with the CPU, and its controller answered nothing to commands a real one
  ignores, which hung Swiss's GameID packet before every launch (see below).
- CI runs the emulator test four times: every menu, and a game's launch from
  the Library, each from the demonstration disc in PAL and from an SD card in
  NTSC. Dolphin's own default region had been PAL.
- The emulator test boots from an SD card, as Indigo is installed: the
  build's zip unpacked onto a FAT32 card image with the demonstration games
  beside it, served by an SD2SP2 adapter that CI's Dolphin gains from a patch
  ([`buildtools/ci/runner/dolphin/`](buildtools/ci/runner/dolphin/README.md)),
  so libogc2's SD driver, FatFs and Swiss's device code run as on a console.
  A new card starts in Settings, and the test checks what Indigo writes to
  the card: its settings, the recent list and its play history.
- CI's Dolphin is built from source, a pinned March 2026 commit, in place of
  Ubuntu's 2512 package, with a fix of ours: a controller answers the serial
  commands a real one ignores (a steering wheel's probe, Swiss's GameID
  packet) with no response, where Dolphin's answer of nothing hung the
  transfer for good. That was why Swiss in Dolphin stalled starting up with a
  controller connected, and why a launch stalled.

## v2.2.0 — Backdrop and Wave Color, a Clock setting and a cleaner cube

The backdrop and the waves can each have a color of their own, the clock can
move to the top left or go, and the cube draws cleaner on a console: no glow
spot under it, smoother edges, and side faces that show clear glass at rest
instead of broken icons. A Jet Black backdrop is nearly black now.

### New

- Settings › Setup › Console › **Clock** puts the time and the temperature
  dial in the top right corner (the default), the top left, or nowhere. The
  loading spinner and the Source screen's label move to the other corner.
- Settings › Setup › Console has **Backdrop Color** and **Wave Color** under
  Menu Color. The backdrop behind the cube and the waves in front of it can
  each keep a color of their own, one of Menu Color's eight, or follow Menu
  Color as before (the default). Like Menu Color, A lists the colors and the
  screen shows each one as you move through the list. In `global.ini` they are
  `Backdrop Color` and `Wave Color`.

### Changes

- A Jet Black backdrop is nearly black now. Gray at Indigo's brightness read
  light on a TV.
- The glow spot on the floor under the Home cube is gone; the halo behind the
  cube stays.
- The cube's edges are smoother on a console. The lights mirrored in the glass
  no longer break into a dashed line along a bevel seen nearly edge-on, and the
  rim of light on the top right corner no longer stair-steps.
- The icons on the cube's side faces no longer break into fragments, such as
  the Settings face's sliders showing as crosses. An icon fades as its face
  turns edge-on and comes back as the face turns round, so at rest the faces
  to either side show clear glass.

## v2.1.1 — ipl.dol uncompressed again

2.1.0's compressed `ipl.dol` could leave the console restarting over and over
at power-on instead of starting Indigo. 2.1.1 is 2.1.0 with `ipl.dol`
uncompressed again, as it was in 2.0.1; everything else is the same.

### Fixes

- `ipl.dol` is the uncompressed DOL again (5.5 MB). The compressed one in
  2.1.0 did not start on a GameCube booted from a GC Loader: it failed to
  unpack, the console restarted, the loader read it again and it looped. If
  you installed 2.1.0, copy 2.1.1's files over it the same way.

### For developers

- The zip's `ipl.dol` is `cube/swiss/swiss.dol` again, and the packing from
  #57 (`verify_dol.py --packed`, `dolphin_ipl.py`, the compressed file in
  Reproducible build and the Emulator job) is reverted. The compressed DOL
  passed CI and Dolphin but not a console, so it ships again only after a
  console test.

## v2.1.0 — Spotlight, Apps and one poster pack for every region

A fourth Library layout, Spotlight, with a gameplay still and a description
for every game; Apps, a fifth face of the cube for your emulators and other
programs, with your own pictures as posters; one poster pack for every
region; and everything upstream Swiss changed up to r2119. The code is
2.1.0-rc.1's, unchanged.

### Swiss r2119

Indigo is built on upstream Swiss r2119 now, up from a July build between
r2073 and r2092, and has everything upstream changed since:

- Games start with Swiss's current video timings: the timing tables Swiss
  gives every game, and a +0 vertical offset by default. Under GCVideo or
  GCDigital compatibility, the default, every game started at -3 (#42).
- Turning off controller rumble no longer makes games reset after 32
  minutes.
- Neighbours from Hell returns to its menu. Animal Crossing Deluxe starts
  without disc read speed emulation, and discs with Kawasedo's NES emulator
  without the forced anisotropic filter.
- Swiss Video Mode offers 240p and 288p.
- Setup › Library › Load at startup chooses a device and a folder for Indigo
  to open when it starts. Z on a game's details still sets a game.
- A game's name and description from its banner, when written in another
  encoding such as Japanese Shift JIS, show far fewer garbled characters.
- In the file lists, a clap on the DK Bongos jumps to the next game that
  uses them.
- Fixes to the SD card file system (FatFs R0.16-p2), the FTP client,
  copying a file onto a name that already exists, and the stub that
  reloads Swiss.
- An NKit.iso of an ArtX diagnostic disc says it can't be played in that
  format.
- The Redump and [T-En] Collection game databases are current.
- The first start from a GC Loader or PicoLoader boot image says that
  System Boot Mode › Production brings back the GameCube logo screen.

### Library

- A fourth Library Layout, **Spotlight**: the selected game's gameplay still
  fills a 4:3 panel, with its title, publisher, a description and its game
  ID in the column beside it, over a row of disc banners, the selected
  game's in the middle and a little larger. Left and Right move along the
  row, as in Horizontal; the still changes with the title. A game without a still shows its cover there, and leaves from it
  for its details. Choose it in Settings › Setup › Library › Library
  Layout.
- Spotlight's stills come from `/swiss/ui/stills.pak`, a second pack next to
  the posters: one 320×240 screenshot per game. The poster pack carries it
  beside `posters.pak`, made from the libretro-thumbnails project's
  GameCube screenshots; `poster_pack.py --stills` builds one from your own.
- Spotlight describes every game: a few sentences from
  `/swiss/ui/descriptions.txt`, which the poster pack carries too
  (English descriptions from GameTDB), else the description on the game's
  disc banner. A game with neither says so. The file is plain text you can
  edit, one game per line.
- The Spotlight layout's design comes from
  [Gameplay Spotlight](https://github.com/mvizensk/gameplay-spotlight) by mvizensk,
  with its author's permission. System › Credits and `NOTICE` thank them.

### One poster pack

- Posters are one download now, for every region: box art for every
  GameCube game GameTDB has a cover for, with Spotlight's stills and
  descriptions. There is no region to choose, and a card that mixes USA,
  Japanese and European games gets posters for all of them. A pack holds up
  to 2,048 games, up from 1,024. Indigo 2.0 and earlier refuse a pack this
  big; the regional packs their release notes link stay online for them.

### Apps

- New: Apps, for the programs on your card that aren't games (emulators,
  Game Boy Interface, tools). Put their `.dol`, `.dol+cli` or `.elf` files in
  `/apps` at the root of the card, loose or each in a folder of its own, and
  Home gets a fifth face, Apps, one turn left of Library. A on it shows them
  as posters in your Library Layout, moving the same way; A starts one, with
  the launch screen games have, and B goes back to Home. Swiss's `.cli` and
  `.dcp` files beside a program still work. Without apps on the card, Home is
  the four faces it was.
- Setup › Console › Apps Face (`Hide Apps Face` in `global.ini`) turns the
  Apps face off, and Indigo then doesn't look in `/apps` at all. It is On by
  default.
- Each app's poster is your own picture: a PNG beside the program with its
  name (`gbi.png` for `gbi.dol`), or its folder's `icon.png`, up to 2048
  pixels a side. Indigo converts it on the console, so there is no pack to
  build. A poster-shaped picture fills the card; a square icon or a Homebrew
  Channel banner sits in the middle over a backdrop in its own colours.
- An app without a picture gets a poster of its name, in big letters split
  where the name has a `-`, `_` or space, in a colour picked from its first
  word: Game Boy Interface's `gbihf-ossc` and `gbihf-direct-hdmi` share one,
  and its `gbisr` variants have another.
- `boot.dol` is left out of Apps, since the Wii's Homebrew Channel layout uses
  that name for the Wii program, and so are hidden files and names starting
  with a dot.
- The cube's icons fade one by one. When a turn has to move an icon to
  another side, as turning sideways and then tipping at once does, only that
  icon fades out and back in, where every icon used to. With five faces the
  icons you can see never have to move: only ones facing away do.

### Fixes

- `ipl.dol` is compressed, as stock Swiss's own download is: 2.9 MB for a
  loader to read at power-on instead of 5.5 MB, and the console checks it as
  it unpacks. A copy the loader read only in part could stop Indigo on a black
  screen; now the console restarts and the loader reads it again. Unpacking
  adds a moment before the cube appears.
- A file in `/games` that isn't a game no longer hides the Library. A text
  file, a cover picture or an empty folder there, or in a game's folder,
  turned the whole Library into Swiss's plain file list, with no sign of
  which file did it, and Library Layout and posters then seemed to do
  nothing. The Library now skips them and shows the games. Swiss's list
  comes back only when `/games` holds no disc images at all.
- The glass cube no longer sparkles along its edges on a console. Single
  pixels flickered where a face meets a bevel and where a bevel meets a
  corner: the glass's refraction, reflection and sheen cut a face's side and
  the bevel beside it at different points, and the GameCube snaps every
  point to a sixteenth of a pixel, which opened and closed pixels along the
  seam as the cube moved. Both sides are now cut at the same points, made
  the same way. The round ends of the icons' strokes (the controller's
  triggers and X and Y, the disc's glints) had the same fault and are fixed
  too. Dolphin's finer snap hid it.
- System Information shows the whole date: weekdays and months are three
  letters (TUE  SEP 29, 2026). A long one, such as a Tuesday in September,
  ended in an ellipsis.

### For developers

- [`UPSTREAM`](UPSTREAM) names the upstream Swiss commit Indigo is built on,
  now r2119, and lists the upstream files Indigo changes, each with the
  reason. CI checks it both ways: a change to an
  upstream file that the list leaves out fails, and so does a listed file
  that matches upstream again. It replaces the interface-only gate and its
  exact-line exceptions, which are now lines in that list.
  `buildtools/upstream_merge.sh <commit>` moves to another upstream commit:
  it merges upstream's changes over the difference in line endings, updates
  the commit, and lists anything left to resolve.
- The size tripwires are 6 MiB for the DOL (`verify_dol.py`) and 7 MiB for
  the zip (`check_package.py`), up from 5 and 6: the DOL is about 5.2 MiB
  and the zip about 5.5 MiB with the text-encoding libraries r2119 links.
- The zip's `ipl.dol` is `cube/packer/swiss.dol`: the DOL compressed by
  upstream's packer, which `make dev` already built for In-Game Reset's
  `apploader.img`. `verify_dol.py --packed` checks that it unpacks to exactly
  the DOL CI checks, in the one .xz format the console's decoder reads, and
  that the unpacker stays clear of what it unpacks and of its stack. The
  Emulator job boots it through `buildtools/ui/emulator/dolphin_ipl.py`, a copy
  past the unpacker's console-only check, and Reproducible build compares both
  files.
- The pack builder makes gameplay stills as well as posters:
  `poster_pack.py --stills <folder>` writes `stills.pak`, one 320×240
  screenshot per game in the poster pack's format. The Library's art cache
  loads a stills pack next to the poster pack, in three slots of its own.
  [docs/PACKS.md](docs/PACKS.md) describes
  the format, which the source cited but the repository never had.
- Game details read no further than a disc banner's 128-character
  description, which need not end in a NUL; they could read on into the
  memory after it.

## v2.0.1 — The download keeps your games folder

Three fixes to 2.0: the download, where System Information sends you for help, and a
flash between a game's details and its settings.

### Fixes

- The download no longer carries an empty `games` folder. On a Mac,
  choosing Replace when dropping it on the card swapped the card's own
  `games` folder, and every game in it, for the empty one. Make `/games` on
  the card if it has none; the install steps say so.
- System Information › About Indigo sends bug reports and questions to
  Indigo's GitHub issues and discussions. It pointed to upstream Swiss's
  community, which does not support Indigo.
- Opening a game's settings from its details and leaving them no longer
  flashes the empty background in between. The details went away as the
  button went down, but the settings page came up only once it was let go,
  and on the way back the page went first; for as long as the button was
  held, about a tenth of a second on a quick press, only the background and
  the cube showed. The page now comes up on the press and stays until the
  button is let go. Opening and leaving Settings from Home flashed the same
  way and is fixed too.

### For developers

- A CI machine names its runners "indigo" unless `INDIGO_CI_NAME` says
  otherwise, instead of after its hostname: runner names show in public job
  logs.
- The CI runner supervisor keeps running when GitHub or Docker fails, and
  retries on its next pass. It used to exit on one slow GitHub reply. After
  five failed passes in a row it runs `INDIGO_CI_ALERT`, and it runs it again
  when it recovers. The command now also gets `INDIGO_CI_ALERT_KEY` and
  `INDIGO_CI_ALERT_STATE`, so it can close its own notification.
- The build uses libogc2's toolchain image of 2026-09-28
  (`sha256:e6531ec…`), up from 2026-07-05's. CI, releases, both runner
  images, the poster pack builder and the build commands in the README, the
  guide and AGENTS.md all name it. The poster builder had its own older pin.
  The new image has an arm64 build as well, so it runs natively on
  Apple-silicon Macs.

## v2.0.0 — A clear glass cube, a launch screen and new menu music

Indigo 2.0: the Home cube is clear glass all through, a game started from
the Library opens on a launch screen with its cover instead of Swiss's
progress boxes, Change Source shows the devices themselves, and the menus
have new music. Everything below was tested together on a GameCube as
2.0.0-rc.1, and every picture in the README and the guide is recorded again
on it.

### Menu music

- New menu music: "Up in the Sky" by Memoraphile (CC0), given the console's
  sound: the GameCube's 32 kHz, a gentle top end and its own DSP-ADPCM
  compression. The track's quiet intro plays once, then an 85-second section
  loops without a seam.
- The music streams: a decoder thread feeds the audio DSP a frame at a time,
  so the two-minute piece takes less memory than the old 16-second loop,
  which decoded 2 MB of sound up front. The DOL grows by 1.1 MB, the MP3.

### Menu Widescreen

- New setting, Setup › Display › Menu Widescreen, for a TV set to 16:9.
  Indigo's own screens are drawn narrower so the TV's stretch gives them
  back their shape: the background and its waves fill the whole screen, and
  the cube, the text and the menus keep their size in the middle. The clock
  moves into the corner. It changes as soon as you switch it. Games still
  follow Force Widescreen.

### Library

- The Horizontal layout shows two covers either side of the selected game
  instead of one. The second on each side, until now a thin strip seen
  edge-on, is a smaller card turned away with its box art, or its disc banner
  when the poster pack has none, and covers fade in at the edges as you move.

### Game details

- Up and down on the D-pad or the control stick move between Launch Game,
  Cheats and Settings on a game's details, and A opens the one you're on. A
  bright frame shows where you are: it starts on Launch Game, passes over
  Cheats when the game has none, and is still there when you come back from
  Settings or Cheats. X, Y, Z, R, L + A and B work as before, wherever the
  frame is. The hint line reads D-pad Move, A Select, B Library, X Settings,
  Y Cheats.
- Game details play the menu's sounds: moving the frame blips, and every
  action, the shortcuts too, plays the select sound. They made none before.

### Launch screen

- Starting a game from the Library no longer shows Swiss's progress boxes,
  with their file names, sizes and bar that starts over for every file. The
  details make way for the game's cover, centred in a ring that fills as
  Indigo checks, prepares and loads the game, and one line under the title
  says which step it's on. When the ring is full the screen fades to black
  and the game starts. A card that must stay in its slot is still named. It
  is the same with Boot without prompts. With UI Motion set to Off the ring
  fills in steps, without its glint, and the screen goes straight to black.
- The video mode and the number of cheats applied are steps on the ring
  instead of notices that held the start for two seconds and one second.

### Source picker

- Change Source shows the devices themselves. The cube lifts out of the way
  and the devices Indigo found line up under it on glass tiles, each with
  its picture: the one in the middle larger and framed, its neighbours
  smaller and dimmer. The row slides from one device to the next and goes
  round. Under the middle one, the picker says what the device can do
  (Boot + Stream, Boot or Files, where 1.25.0 said Boot Ready and Files
  Ready), where it plugs in (Slot A, Serial Port 2, Disc Drive and so on),
  whether Indigo detected it, and whether it is the current source or holds
  your settings.
- With no device detected that can do the job, the picker lists every one
  that could, as Z does. It used to search for one forever.
- The control stick changes the device too, and holding it keeps going; the
  D-pad, L and R still work. The bottom line shows the buttons, the stick
  and D-pad that change the device among them, and the picker plays the
  menu's sounds.
- Copy and Move choose where to put a file in the same picker.

### Home cube

- The cube's edges no longer look jagged on a large or upscaled TV. Where a
  face met one of its rounded edges the glass changed colour in one step,
  which the picture drew as a staircase; the colour now blends across. The
  cube's outline, its rim of light, the face icons and the waves behind
  fade over one whole pixel, with Menu Widescreen too, which had narrowed
  them to three quarters of one, and the rim keeps one brightness along its
  length.
- The cube is clear glass all through: the solid cube inside it is gone.
  Its far edges show through, bent by the front, and a smoked panel under
  each face's icon keeps the icon easy to read. The glass mirrors soft
  lights around it, a gradient and a brighter window on the faces and thin
  glints along the rounded edges, drawn sharp to the pixel; they slide
  across the glass as the cube turns and drift a little while Home rests
  (not with UI Motion set to Reduced or Off).

### Fixes

- Indigo stays on screen when stock Swiss is kept on the card as `z.dol`, as
  the install guide says. At startup Indigo still ran Swiss's search for a
  newer Swiss in the root of the card (`z.dol`, `a.dol`, `start.dol`,
  `boot.dol` and similar names) and took any stock Swiss it found for one, so
  PicoBoot and PicoLoader started Indigo only for it to start stock Swiss
  straight away. Indigo no longer runs that search, which also made a copy of
  Indigo named `boot.dol` start itself over and over
  ([#3](https://github.com/spencercnorton/indigo/issues/3)).
- Settings › Setup › Storage no longer offers A Edit on Save Folder while it
  is dimmed. Without a Configuration Device there are no folders to list, so
  A does nothing there. Dimmed network settings keep A Edit: A still opens
  their editor.
- The Library no longer crashes on a damaged disc image. It reads each
  listed image's file table to find the game's banner, and a header-only
  dump, a truncated download or a corrupt table could send that read past the
  end of the table and crash the console. Indigo now checks every entry
  against the table's size; such an image shows without its banner.
- Settings' middle tab reads Defaults. "Game Defaults" is wider in the
  console's font than on a computer and showed cut short, as "Game Def…";
  the page it opens is still called Game Defaults.
- Video settings no longer switch the picture at every step. On Swiss Video
  Mode, System Video, AVE Compatibility, Force DTV Status and RetroTINK-4K
  HDMI Input, Left and Right now only choose a value, so holding them steps
  through the values; A switches to the one shown and asks, as before,
  whether to keep it. Moving to another setting or leaving without A puts the
  old value back.
- A game whose launch cannot read the console's BS2 says "Failed to read
  BS2!" and comes back to the Library, as Swiss means it to. It crashed
  instead: the failure freed a pointer the launch had never set.

### Documentation

- The guide's example of a dimmed setting is Save Folder with no
  Configuration Device. IPv4 Address, the old example, doesn't dim while DHCP
  is on.
- The install instructions were checked against the code and against
  PicoBoot's, PicoLoader's, FlippyDrive's and Swiss's own documentation:
  - PicoLoader sits beside PicoBoot. A chip with Swiss in its flash names the
    firmware to flash instead (`picoboot_full_pico.uf2`,
    `picoloader_gekkoboot.uf2`); FlippyDrive boots `boot.dol`.
  - SD2SP2 and SD Gecko are named as what reads the card, not as ways to
    start Swiss, and a Wii needs GameCube ports and an SD Gecko.
  - A stray file or folder inside a game's folder also brings back Swiss's
    file list, and Hide unknown file types hides text files and pictures but
    not folders, programs or music.
  - Home faces Library when Indigo has a device to read from, whether or not
    it found games.
  - Stock Swiss drops Indigo's own settings, such as Menu Color, when it
    saves.
  - The build commands name the image CI uses, pinned by digest; there is no
    `make dist`.
  - Troubleshooting covers stock Swiss taking over at startup in 1.25.0 and
    earlier.
  - The README's release badge links the latest release rather than the list
    of tags.
- Every picture in the guide and the README is recorded again: the clear
  glass cube, the Defaults tab, two covers either side in the Library, the
  frame on a game's details and the new source picker. Game details has a
  picture of the launch screen, and the button icons keep their colors in
  the animations, where the green A had turned teal.
- The install steps start by renaming the card's `ipl.dol`, before anything
  is copied, and say how to update from Indigo 1.x. They warn that on a Mac,
  Finder's Replace deletes what is in the card's `games` and `swiss`
  folders: hold Option and choose Merge. The README, the guide, the release
  notes and the zip's `Indigo-README.txt` say the same.
- The README describes 2.0's screens.
- The guide's Game details page no longer says Indigo can't read memory
  cards.
- SECURITY.md, SUPPORT.md and CONTRIBUTING.md name release candidates, and
  CONTRIBUTING.md the route for a fix to a release.
- The issue forms are brought up to date for 2.0.

### For developers

- Indigo is developed on GitHub. Changes land on the `beta` branch by pull
  request, betas ship as `vX.Y.Z-beta.N` pre-releases, and a release
  candidate that holds up is promoted to `main` as a release.
  [docs/RELEASING.md](docs/RELEASING.md) has the procedure and
  [AGENTS.md](AGENTS.md) the working detail.
- GitHub Actions builds the DOL and the SD card zip for every push and pull
  request, runs the three host-test lanes and the source checks (whitespace
  and the UI isolation guardrail), and keeps the zip as an artifact to try on
  a console. A pushed tag publishes its release from the tagged commit, with
  `SHA256SUMS.txt` and a build provenance attestation.
- The issue forms are the two Indigo ones; upstream's duplicates are gone.
- `make clean` in `buildtools/ui/tests/` removes every test it builds. It
  used to leave the cube-motif tests behind and listed others twice; it now
  removes the Makefile's own target lists, so a new test is cleaned too.
- CI and releases run on the maintainer's own machines: every job gets a
  fresh container that takes that one job and is thrown away, runs without
  privileges, and can reach GitHub and nothing else
  ([buildtools/ci/runner/](buildtools/ci/runner/README.md)).
- CI checks more: the DOL's structure, size and the commit it names; the
  zip's exact layout; a second build that must match the first byte for
  byte; the host tests under Clang's sanitizers as well as GCC's; and a
  policy check on the workflows themselves. One "CI passed" check sums up
  every job.
- CI boots every build in Dolphin and walks its menus with a controller:
  the cube turns through its four faces, each face opens and closes, and the
  Library moves between games and opens one's details, where UP and A open
  the game's settings, on a demonstration disc of fictitious games.
  Dolphin emulates the MMU, so a crash stops the test as it would stop a
  console. Every step's picture is kept with the run
  ([buildtools/ui/emulator/](buildtools/ui/emulator/README.md)).
- CI fuzzes the files Indigo reads from a card: poster packs, the play
  history, saves, settings files and disc images' file tables, each with
  AddressSanitizer and UBSan, starting from real files of each kind. The
  settings and file-table fuzzers run Swiss's own code as the console does:
  with its `strtok_r`, and with its unsigned `char`. The emulator test's disc
  also carries two damaged images the Library must list without crashing. Weekly on `main`, every check runs
  again and the fuzzers run for ten minutes each.
- The emulator test also opens Change Source: the device picker shows a
  device's name, RIGHT shows another, and B leaves it.
- Release candidates: a `vX.Y.Z-rc.N` tag on `beta` publishes a pre-release
  like a beta, its notes opening with "Release candidate"
  ([docs/RELEASING.md](docs/RELEASING.md)).
- Release notes link the repository's files by full URL at the release's
  tag, so the links also work where GitHub doesn't rewrite them: through the
  API and `gh`.
- The test fixtures use neutral machine names and generic addresses.

## v1.25.0 — Memory Cards, a cube that flies in, and a drag-and-drop download

The first formal release of Indigo: everything below, tested together, with the
guide and every picture recorded again on this build.

### Memory Cards

- **Memory Cards**, a new row on the System face between System Information
  and Restart Indigo, shows the saves on the memory cards in Slot A and Slot
  B and in folders on the SD card, much as the GameCube's own Memory Card
  screen does. Each save shows its banner, the game's name for it and its
  size in blocks; the tab says how many blocks the card has free, and the
  line above the buttons shows the rest of the save's comment. L and R move
  between Slot A, Slot B and the SD card; A opens a folder.
- A on a save copies, moves or deletes it. Copy and Move go to the other
  slot, to the Save Folder, or to another folder you choose. A save copied
  off a card becomes a `.gci` named as Dolphin names a GCI folder's saves;
  Action Replay (`.sav`) and GameShark (`.gcs`) saves copy onto a card too.
  Every copy is read back and compared before it counts, and a Move removes
  the original only then. A card that already has the save, or hasn't the
  room for it, says so and nothing changes; a save its game marks as not to
  be moved can be copied but not moved; Delete asks first, with Cancel
  highlighted.
- Settings › Setup › Storage › **Save Folder** chooses where saves copied off
  a card go: a folder on the Configuration Device, `swiss/saves` until you
  choose another (key `Save Folder` in `global.ini`). A lists the folders, X
  chooses the one that's open.
- The System face's three rows sit a little closer together to fit under
  the cube, and Restart Indigo's confirmation returns to its row, now the
  third.

### The cube flies in

- When Indigo starts, the Home cube flies in from the distance. It starts
  as a small spinning cube in the middle of the screen, tumbles toward you,
  and comes to rest on Library half a second later. Home is ready after 0.8
  seconds instead of 1.15, and the cube is in view from the first frames
  instead of fading up out of a dark screen.
- Starting no longer dims the screen before the cube appears. The face name,
  its hint and the glass's light fade in as the cube arrives instead of
  switching on at once.
- With UI Motion set to Reduced, the cube fades in where it rests. With Off
  it is simply there, as before.

### In-Game Reset comes back to Indigo

- With In-Game Reset set to Apploader, A + Z + START during a game now
  returns to Indigo. A reset restarts whatever `swiss/patches/apploader.img`
  holds, and until now that was stock Swiss's copy. The release zip now
  carries Indigo's own. It replaces stock Swiss's, so a game started from
  stock Swiss resets to Indigo too.

### A drag-and-drop download

- The download is drag and drop. `Indigo-vX.Y.Z.zip` is laid out exactly
  as the card is: unzip it, select everything inside and drag it onto the
  root of the SD card. Before, everything sat in an `Indigo-vX.Y.Z/SD card/`
  folder inside the zip. An `Indigo-README.txt` says what each file is for,
  and the licence and notice now travel in `swiss/indigo/`.

### Fixes

- Starting a game from the Library opens its file afresh. A read error while
  browsing, which a slow SD card adapter (SD2SP2, SD Gecko) can cause, no
  longer carries into the launch. A launch that failed after setup no longer
  leaves the next try reading the game from the wrong place on the card.
- Indigo records when you last played a game before it prepares the game, as
  Swiss does with its recent list. It no longer writes to the card after the
  game's data has been mapped.

### Documentation

- Every picture in the README and the guide is recorded again on this
  release, and the README gains two animations: the cube flying in when
  Indigo starts, and Memory Cards copying a save to the SD card. The guide's
  Memory Cards page has pictures of each screen and a card of its own on the
  guide's index, next to System. The README and the guide link the video
  tour and the Indigo page at norvitech.com/indigo; the poster and cheat
  downloads stay at indigo.norvitech.com, with their checksums.
- The guide says Settings help closes with B as well as Y.

### For developers

- `make dev` also builds the packer (`cube/packer/reboot.dol`), compressing
  with `xz` because the build image has no `7z`, and
  `buildtools/sd_package.sh` turns it into that `apploader.img`.
- The host test suite ships with the source: `buildtools/ui/tests/` (C unit
  tests, GX vertex-stream checks and source audits) and the UI isolation
  guardrail `buildtools/check_ui_isolation.sh` are now in the public
  repository, so anyone can run what CI runs with
  `buildtools/ui/tests/run_tests.sh all`.

## v1.24.0 — four icons for every face, sharp on the glass

- Each face of the Home cube has four icons of its own, and no face offers
  another face's. Library has Controller, Books, Covers and Play; Source has
  Hub, Disc, SD Card and Folder; Settings has Sliders, Gear, Toggles and Dial;
  System has Clock, Info, Power and Chip. Covers, Play, SD Card, Folder,
  Toggles, Dial, Info, Power and Chip are new, and None is gone.
- A settings file that names an icon from another face's list, or None,
  leaves that face on its first icon.
- The face icons are drawn after the glass's glow instead of under it, so
  their lines stay sharp and easy to read.
- The sun no longer glints in the corner of the glass, and no band of light
  passes over it while Home rests. The rest of the glass (the bent view
  through it, the prism edges, the glow, the rim and the halo) is unchanged.
- The dot that circled the temperature dial at the top right is gone; the
  time and the temperature stay.

## v1.23.2 — a quieter sun on the glass

- The sun that a corner of the Home cube catches is now a small glint with
  short rays instead of a large starburst. Its core is smaller and softer,
  the streak and rays are shorter and fainter, and the reflections across
  the screen are fainter. The README and guide pictures of Home, Source and
  System are recorded again with it.

## v1.23.1 — pictures of every new screen

- The README's Library animation shows all three layouts, and two new
  animations follow its Settings paragraph: a tour of Settings in its new
  look, and Y opening a game's settings from the Library. Home, Game Detail
  and Menu Color are recorded again on this release.
- The guide's Library page has an animation of the Library Layout setting
  and of the Vertical and Grid layouts, and one of Y and a game's settings.
  Every picture of Settings, Game Detail, the cheat browser, the Library,
  the Source and System screens and the troubleshooting box is new, so each
  shows the current look.
- The guide's Game Detail, Settings and Controls pages describe the
  Settings line on Game Detail, Y in the Library and each layout's buttons.

## v1.23.0 — Library layouts, and a game's settings from the Library

- Library Layout, the first setting in Setup › Library, lays the Library out
  three ways. **Horizontal** is the row of covers you know. **Vertical** is a
  column down the left of the screen, turning like a wheel, with the
  selected game's title, publisher and details beside its cover. **Grid**
  shows rows of five covers, three rows at a time, with a lit frame that
  slides to the selected cover and its title and publisher above the
  controls.
- Every layout wraps round from the last game to the first. In the grid,
  Left and Right run on from one row to the next, Up and Down move a row
  and keep to the column, and L and R move three rows at a time.
- Y on a game in the Library opens that game's own settings, in every
  layout. Leaving them shows the game's details, and B returns to the
  Library with the same game selected. Y never starts a game, even with
  Boot without prompts on.
- The sliders mark that shows a game has settings of its own now appears
  on every cover that shows its art.

## v1.22.1 — glass that catches the light without burning white

- When a face of the Home cube turns through the light as the cube turns or
  tips, it now brightens to a pale lilac with its icon still showing. Before,
  the glow added on top of the reflection turned the whole face white for a
  few frames. The cube at rest looks the same as before.

## v1.22.0 — Settings in the cheat browser's look

- Every Settings page (Quick, Game Defaults, Setup and its six sections, and
  a game's own settings) is drawn like the cheat browser: one full page, the
  tabs as a segmented control between L and R, the settings as cards, and the
  highlighted one lit with an accent bar that slides as you move.
- Settings that are on or off show an ON or OFF switch; settings with more
  choices show their value in a pill, with arrows while highlighted. Text
  settings read like a field, and "Not set" when empty. Menu Color shows a
  swatch of the color.
- A line above the buttons says what the highlighted setting does, or what
  its current value does. The buttons that work are shown as icons, with
  Save & Exit and Discard & Exit beside them.
- The list of choices, the help and every message and progress box are cards
  in the same style; a warning's accent is amber and a failure's red.
- A game's own settings count their Custom rows at the top right ("2
  custom"), and Game Detail gains a SETTINGS line: how many are the game's
  own and the first of them, or "Game Defaults" with X to change them.
- The page stays on screen for the whole of Settings instead of being
  rebuilt after every press, so it never flickers.

## v1.21.0 — glass that bends light

- The Home cube is glass that refracts: through the front you see the cube's
  far edges, its inner cube and the backdrop, bent and slightly magnified,
  and the rounded edges part the light into red, green and blue like a prism.
- Highlights, glints and the face icons glow. A fine rim lights the edges
  that face the light, a halo sits behind the cube, and the glass focuses a
  patch of light on the floor under it.
- A corner that catches the light flares like the sun: a bright point, six
  rays, a long streak and faint reflections strung across the screen.
- While Home rests, a band of light passes over the glass every seven
  seconds. It needs UI Motion set to Full.
- All of it follows Menu Color. The sun's white core and the prism's fringes
  keep their own colors.

## v1.20.0 — see which games have their own settings

- In the Library, a small sliders mark in the corner of a cover means the
  game has settings of its own: at least one that differs from Game
  Defaults, the same count Game Detail shows as "X SETTINGS (2 CUSTOM)". A
  game reset to its defaults loses the mark. It follows Menu Color.
- Indigo reads the games' settings files once when the Library first opens,
  and again after you save settings.
- Game Defaults no longer shows Force Vertical Offset. That default never
  reached a game (each game starts at -3 with GCVideo or GCDigital and +0
  otherwise), so the row did nothing. Set it in a game's own settings; the
  key in global.ini is still read.

## v1.19.1 — the Indigo guide

- New: [the Indigo guide](docs/guide/README.md), thirteen pages with pictures
  from the real interface. It covers installing, the controls, Home, the
  Library, game details, cheats, Settings and a game's own settings, Menu
  Color and the cube icons, sources, System, posters and troubleshooting,
  and ends with every setting explained.
- The README links to it, and its Home animation and System picture are
  re-recorded with v1.19.0's cube, whose icons now share one shade.

## v1.19.0 — cube face icons

- Settings › Setup › Console has a row for each face of the Home cube:
  Library Icon, Source Icon, Settings Icon and System Icon. Each picks the
  picture on its face: Controller, Books, Hub, Disc, Sliders, Gear, Clock or
  None. Any face can show any of them, and a face keeps its name and what
  A does there.
- Disc and Gear are new, and Books is the bookshelf the Library face had in
  v1.4.0. Controller mirrors your controller and Clock tells the time on
  whichever face you put them.
- Every icon now glows in the same lilac, the Library controller's, on every
  face, and the glass behind each face has the same tint. The defaults are
  still the controller, hub, sliders and clock.
- The settings file keys are `Library Icon`, `Source Icon`, `Settings Icon`
  and `System Icon`.

## v1.18.0 — button icons in prompts

- Message boxes show their buttons as icons too. "Press A to continue."
  becomes the A button and CONTINUE, and deleting a file shows L + A to
  continue or B to cancel.
- Settings' two questions end on their buttons: "Reset everything on this
  screen?" shows A RESET or B KEEP, and "Keep this video mode?" shows
  A KEEP or B CHANGE BACK under its countdown.
- The icons also reach these screens:
  - the on-screen keyboard;
  - the DOL parameters screen;
  - the cheat browser when it has nothing to show;
  - the older Swiss screens: the classic file browser's game box, the
    file manager, the folder and DOL pickers, and the MP3 player.

## v1.17.2 — Menu Color in the README

- The README shows Menu Color: the Home cube in each of the eight colors,
  and the Menu Color list previewing each one before you choose.

## v1.17.1 — direct download links

- The README's Posters and Cheats sections link straight to the three
  downloads (posters for USA and Japan, posters for Europe and Australia,
  cheats for every region) instead of only to the page.

## v1.17.0 — Menu Color previews in its list

- The list that A opens on Settings › Setup › Console › Menu Color now shows
  each color on screen as you move through it, the way Left and Right
  already do on the row. B puts your color back; A keeps the one you are on.

## v1.16.1 — posters and cheats to download

- Ready-made poster packs (USA and Japan, or Europe and Australia) and a cheat
  pack for every region are at https://indigo.norvitech.com. The README and
  the release zip's instructions point there; building your own posters.pak
  still works as before.

## v1.16.0 — Menu Color

- Settings › Setup › Console › Menu Color colors the whole interface: the
  cube, its light and the stage behind it, the panels, the highlight and the
  text. Choose Indigo (the default), Azure, Emerald, Gold, Spice, Crimson,
  Rose or Jet Black. Right steps round the color wheel and A lists them all.
  The new color shows at once, and Discard & Exit puts the old one back.
- Every color keeps Indigo's brightness, so text reads as easily and the
  glass cube keeps its depth. Cover art, banners, the controller buttons
  in the hints, warnings and enabled cheats keep their own colors.
- The settings file key is `Menu Color`.

## v1.15.0 — button icons

- Control hints show the controller's buttons instead of spelling them
  out, the way games do. You see a green A, a red B, grey X and Y, a
  purple Z, the L and R triggers, START, the control stick and the D-pad,
  each followed by what it does.
- They cover every hint line: Home, the source picker, the Library, game
  detail, cheats, Settings and System information. On the game detail
  card they also mark the launch button, "Choose cheats" and the
  shortcuts. Hold L and press A shows as L + A.
- A game's own settings now show all of "X DEFAULT  B DONE". The title
  bar used to cut it off.
- The README's pictures are re-recorded with the icons.

## v1.14.1 — Home

- The white frame on the cube is gone. The white line and the dark line
  inside it no longer outline the face you are on; the glass, its reflection
  and each face's emblem are unchanged.
- The README's Home animation and System screenshot are re-recorded without
  it.

## v1.14.0 — pick from a list

- A on a setting with four or more choices lists them all. That covers the
  video modes, languages, polling rates, horizontal scale, camera stick
  options, RetroTINK-4K profiles and a few more. Move with the D-pad or
  stick, press A to pick, or B to keep what you had; the current choice is
  marked. Left and Right still step one choice at a time.
- The list holds exactly what Right steps through, so video modes your
  cable can't show stay out of it, and the polling rates the list would
  otherwise name twice appear once.

## v1.13.1 — the video prompt names the new mode

- "Keep this video mode?" now says what you'd keep, for example "Keep
  Swiss Video Mode: PAL 576p?". The row behind the prompt still showed the
  old value until you answered.

## v1.13.0 — steady stick in the last two old lists; the disc drive is "Game Disc"

- The folder picker (copying or moving a file) and the "Select DOL"
  list now scroll with the stick like every other list: one row, a pause,
  then a steady repeat. Before, a full stick raced through a row every
  24 ms or so.
- The disc drive is called "Game Disc" instead of "DVD" wherever Indigo
  names a source: the source picker, Home's Source screen, the file list
  and System information. Settings about the drive itself keep their names.
- System information says CPU calibration is adjusted in Setup / Console,
  where it moved in v1.9.0, instead of the old "Settings / System".

## v1.12.1 — Home

- The white frame on the cube now marks the face you are on. It stayed on
  the face Library starts on, so after a turn it sat edge-on beside the face
  you had chosen, or on the top or bottom after Up or Down. Now it fades
  across to each face as that face turns to the front.
- The README's Home animation is re-recorded with it, and the System
  screenshot is current: it still showed the rings around the cube removed
  in v1.4.1.

## v1.12.0 — settings read the positive way round

- Settings stored as "Disable …" now read the way you'd say them:
  Controller Recalibration, Alpha Dithering and Hypervisor show On or Off,
  like Controller Rumble already did. The settings file keeps its key
  names, so `Disable Hypervisor=Yes` still works and shows as Hypervisor ›
  Off.
- Their help says what On and Off each do.

## v1.11.0 — Storage says whether your settings file loaded

- Settings › Setup › Storage now tells you, under its title, where your
  settings live and whether that file loaded: "Settings are saved in
  swiss/settings/global.ini.", "No swiss/settings/global.ini yet: using
  defaults." or "No device to save settings to."
- Configuration Device's help names the file and says the choice changes
  only once Save & Exit has written the settings there.

## v1.10.0 — help for every setting; saving keeps your files intact

- Y now explains every setting in Settings. 34 rows had no help, including
  most of Network, Swiss Video Mode, Disable Video Patches, Force
  Widescreen, Field Rendering and Horizontal Scale.
- Game Defaults' NTSC and PAL video modes have their own help instead of
  sharing a game's.
- Saving settings no longer wipes a hand-edited `global.ini` or game file:
  - comments, blank lines and keys Indigo doesn't know stay where they are;
  - known keys get their new value in place;
  - missing keys are added before the End marker.

  Older key names are replaced by their current ones.
- Reset to defaults in a game's settings no longer wipes the Comment and
  Status lines in its file.
- Toggling autoload (Z) saves only the Autoload line. Before, it saved every
  setting in memory, including ones turned off for this session only, such
  as Boot without prompts when B was held at launch.
- A new Configuration Device is written to the console's SRAM only after
  the settings are saved to it. If that save fails, the console keeps the
  old device instead of pointing at one without settings.
- A settings file that can only be read in part now counts as unreadable,
  so it is never half-applied or saved over.
- Fixed in v1.9.0's game settings: opening them with X no longer puts the
  game's Force Video Mode back to its default. The X press that opened the
  screen was also read as "X: use the default" on the first row.

## v1.9.0 — Settings layout and a game's own settings

- Settings is reorganised around when you change things:
  - It opens on **Quick**: nine settings that fit on one screen. They are
    menu music and sounds, UI motion, rumble, In-Game Reset, the GameCube
    main menu, memory-card emulation, auto-loaded cheats, and boot without
    prompts.
  - **Game Defaults** holds what every game starts with.
  - **Setup** holds everything you set once, in six sections: Display,
    Console, Storage, Network, Library and Developer. A opens a section and
    B goes back.
  - L and R move between the three.
- A game's own settings (X on its detail screen) open on their own, titled
  with the game's name and without the global tabs, so changing them can't
  also change the defaults.
- The Save & Exit and Discard & Exit buttons replace Back and Next.
- The old Game tab's "Reset to defaults" is gone, because its settings are
  now spread across Quick and Setup. Game Defaults and a game's own settings
  keep their resets.
- docs/SETTINGS.md now lists each key under the screen that shows it.
- Values that differ from Game Defaults are marked **Custom**. Every other
  row follows Game Defaults, and so changes whenever the defaults do.
- **X** puts the highlighted row back to its Game Defaults value.
- The rows you're most likely to change come first, in the same order on
  Game Defaults: video mode, widescreen, language, polling rate and the
  controls. Compatibility, picture tuning and the RetroTINK-4K profile
  follow.
- "Reset to defaults" asks before it resets a whole screen.
- Game Detail says when a game has its own settings: the hint reads
  "X SETTINGS (2 CUSTOM)".

## v1.8.0 — Settings controls

- Holding the D-pad now repeats, the same way the control stick does: hold
  Down to run through a list.
- A changes the highlighted value, the same as Right.
- B leaves Settings. Anything you changed is saved, as with Save & Exit.
  Discard & Exit is still there to undo.
- L and R wrap around the six tabs.
- After you change Swiss Video Mode, System Video, AVE Compatibility, Force
  DTV Status or RetroTINK-4K HDMI Input, Settings asks whether to keep the new
  picture. Press A within 10 seconds to keep it; otherwise it changes back by
  itself, so a mode your TV can't show never sticks.
- Fixed: if you opened Settings from a game and changed a game default,
  saving wrote that game's old values into its own settings file, so the game
  stopped following the defaults. It now keeps following them.

## v1.7.0 — Home

- The cube is glass, like the GameCube's own menu. A soft reflection lies
  across each face and slides over it as the cube turns and sways, and the
  bevelled edges catch the light as thin glints.
- The light stays put while the cube turns, so every face looks the same when
  it comes to the front. Before, the lighting was painted on the cube and
  turned with it: Source arrived brighter than Library, and Up or Down
  carried the bright top to the front.
- The README's Home animation tips the cube up and down as well as turning
  it left and right, as Up and Down on the D-pad or control stick do.

## v1.6.0 — download

- Each release now has an `Indigo-vX.Y.Z.zip` download. Its `SD card`
  folder holds `ipl.dol` and the `games` and `swiss/ui` folders, ready to
  copy to the root of the card.
- The README's new "Set up your library" section says how `/games` has to
  look for the Library to appear, and how to add posters.
- `buildtools/ui/poster_pack.py` is now published. `--covers DIR` builds
  `posters.pak` from a folder of cover images named by game ID, and a missing
  Docker image produces the exact `docker pull` command instead of Docker's
  error.

## v1.5.2 — settings files

- New: [docs/SETTINGS.md](docs/SETTINGS.md) lists every key Swiss reads from
  `global.ini` and from a game's own settings file, with its values, its
  default and where it appears in Settings, so a card can be set up on a
  computer before it goes in the console. Example files are in
  [docs/examples/](docs/examples/).
- The README no longer says Settings was rebuilt: it keeps Swiss's six tabs,
  restyled.

## v1.5.1 — README

- Install now says where `swiss.dol` goes on the console: in place of the file
  your loader already boots, under that file's name, with PicoBoot spelled
  out. The build path `cube/swiss/swiss.dol` read as a path on the SD card.

## v1.5.0 — Home

- The Library face shows a GameCube controller instead of the bookshelf, and
  it follows the controller in your hand: the control stick and C-stick lean
  with yours, and A, B, X, Y, Start, the D-pad and the L and R triggers light
  while you hold them. Two seconds after you let go it starts playing by
  itself; with reduced motion on it only mirrors you.
- The short marks on the four sides of the cube are gone, on Home, in
  Settings and on the restart prompt.
- The README's Home animation is re-recorded with the controller.

## v1.4.2 — README

- The README's licence section notes that Indigo's interface was built with
  Claude Code.

## v1.4.1 — Home

- The orbit rings around the cube are gone, on Home and on every other
  screen that shows the cube; it now sits on its own over the backdrop.
- The README's Home animation is re-recorded without them.

## v1.4.0 — Indigo

- Swiss UI is now called Indigo, with a new home at
  [spencercnorton/indigo](https://github.com/spencercnorton/indigo). Earlier
  releases stay in the archived spencercnorton/swiss-ui repository. The
  executable is still `swiss.dol` and settings stay where they were, so an
  existing card needs no changes.
- Home names only the face you are on, under the cube. The source name above
  the cube and the neighbouring faces' names beside it are gone; they did not
  turn with the cube.
- The Library face shows books on a shelf instead of three bars that read as
  a chart.
- The README's Home animation is re-recorded with the new labels and emblem.
- System Information and the restart prompt use the new name, and the About
  page's source link points at this repository.

## v1.3.4 — cheats

- Fixed: leaving the cheat browser crashed Swiss when the cheat file came from
  a read-only device such as a data disc. Saving the selection now skips a
  device that cannot write instead of calling a missing write handler.
- The game detail picture is now an animation: the cheat browser opens,
  cheats are switched on and off, and the detail screen shows the new count.

## v1.3.3 — screenshots

- The library animation and game detail still show real GameCube games with
  their box art from a poster pack, instead of fictitious placeholder titles.

## v1.3.2 — screenshots

- The library animation and game detail still now show a poster pack in use:
  nine illustrated covers drawn for the fictitious demonstration titles,
  rendered by the real poster pipeline (focus scaling, depth, retained art),
  instead of the generated fallback cards.

## v1.3.1 — screenshots

- The Home animation is re-cut on the rotations: the loop now walks Library,
  Source, System and back to Library, instead of opening on four idle seconds
  and cutting off mid-turn.

## v1.3.0 — screenshots

- The README now shows the interface: an animated capture of the Home cube
  turning between its four faces, the game library, and the game detail
  screen.
- Captured from a demonstration card built for the purpose — nine fictitious
  titles with generated cover art — so nothing in frame is anyone's real
  library or third-party artwork. The animations are APNG, which GitHub
  renders in a README like any image.

## v1.2.0 — first public release

The first published release of the interface rebuild. Everything below is what
this tag contains rather than a list of changes against an earlier public
version; the upstream Swiss history it is built on is at
[emukidid/swiss-gc](https://github.com/emukidid/swiss-gc).

- **Home**: an animated four-face cube, one face per destination, with
  device state carried through the rotation.
- **Library**: a retained poster grid over the device's games, with a saved
  selection that survives a return to Home.
- **Game details**: a per-title surface with artwork, region and format
  information, and the boot options for that title.
- **Cheats**: a presentation pass over the cheat browser, with clearer
  per-cheat state and safer runtime handling.
- **Settings and System**: rebuilt surfaces with consistent controller
  navigation shared across every screen.
- Interface work only: device handlers, the patch engine and the loader are
  upstream Swiss.
