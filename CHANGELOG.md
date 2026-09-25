# Changelog

Versions follow [semantic versioning](https://semver.org/); each release is a
tag on `main`.

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
