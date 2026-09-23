# Changelog

Versions follow [semantic versioning](https://semver.org/); each release is a
tag on `main`.

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
