#!/bin/sh
# Assemble the drag-and-drop download for one release. The zip's root IS the
# shared files retain their paths; the executable name depends on the loader.
#   Indigo-<version>.zip
#     Indigo-README.txt              what goes where, in plain words
#     ipl.dol                        Indigo; rename for your chosen boot route
#     swiss/patches/apploader.img    In-Game Reset (Apploader) returns to Indigo
#     swiss/ui/                      posters.pak, if you add one
#     swiss/indigo/LICENSE.txt, NOTICE.txt
# Usage: buildtools/sd_package.sh <version> [swiss.dol] [out dir]
# Run `make dev` first (see README, Install); run from the repository root.
set -eu

version=${1:?usage: buildtools/sd_package.sh <version> [swiss.dol] [out dir]}
dol=${2:-cube/swiss/swiss.dol}
out=$(cd "${3:-.}" && pwd)
[ -f "$dol" ] || { echo "sd_package: no DOL at $dol; build it first" >&2; exit 1; }
reboot=cube/packer/reboot.dol
[ -f "$reboot" ] || { echo "sd_package: no $reboot; build it first (make dev)" >&2; exit 1; }

card=$(mktemp -d)
trap 'rm -rf "$card"' EXIT
# No games/: a Mac's Replace would swap the card's own games folder for an empty one.
mkdir -p "$card/swiss/ui" "$card/swiss/patches" "$card/swiss/indigo"
cp "$dol" "$card/ipl.dol"
# In-Game Reset set to Apploader restarts whatever this file holds: Indigo.
python3 buildtools/dol2ipl.py "$card/swiss/patches/apploader.img" "$reboot" "*indigo-$version" >/dev/null
cp LICENSE "$card/swiss/indigo/LICENSE.txt"
cp NOTICE "$card/swiss/indigo/NOTICE.txt"
cat > "$card/Indigo-README.txt" <<TXT
Indigo $version - an unofficial fork of Swiss with the interface rebuilt
Guide:  https://norvitech.com/indigo/
Source: https://github.com/spencercnorton/indigo/tree/$version

INSTALL: CHOOSE YOUR LAUNCH ROUTE
Full walkthrough with card trees and recovery:
https://norvitech.com/indigo/guide/install/

1. Back up your working card to your computer. Keep existing boot files,
   games and the whole swiss folder. No formatting is needed.
2. Extract the ZIP on your computer. The card root is the first level you
   see when you open the card, beside its existing swiss or games folders.
   Copy the files INSIDE the extracted folder, not the ZIP or its enclosing
   Indigo folder. Choose the route that matches your current setup:

   PicoBoot/PicoLoader using gekkoboot:
     download's ipl.dol -> SD adapter card root/ipl.dol
     Identify the old ipl.dol first: it could be Swiss, Indigo or cubeboot.
     Preserve stock Swiss as z.dol only if that shortcut name is free.
     Hold Z at power-on for that backup. Keep existing shortcuts.

   FlippyDrive:
     download's ipl.dol -> FlippyDrive microSD root/boot.dol
     Rename the new copy to boot.dol and back up the old boot.dol.

   GC Loader, Swiss in flash, cubeboot, or unsure:
     download's ipl.dol -> card root/apps/indigo.dol
     Keep all root boot files. Start Swiss as usual, browse to apps, and
     launch indigo.dol. The ZIP has no boot.iso; don't rename a DOL to ISO.
     This route needs no firmware change and keeps your usual startup.

   SD2SP2 and SD Gecko are card adapters, not boot methods. Your boot card
   and the card holding your games may be different cards.

3. Merge the shared files into the EXISTING swiss folder:
     swiss/patches/apploader.img -> /swiss/patches/apploader.img
     swiss/indigo/LICENSE.txt   -> /swiss/indigo/LICENSE.txt
     swiss/indigo/NOTICE.txt    -> /swiss/indigo/NOTICE.txt
   Make /swiss/ui if missing; keep any poster pack already in it.
   On a Mac, hold Option while dragging swiss and choose Merge. Never
   Replace the entire swiss folder: that removes settings, cheats and
   saves. If Merge is unavailable, copy the individual files above.
   On Windows/Linux, merge folders and replace only matching files.
   apploader.img changes Apploader In-Game Reset to Indigo, even for
   games launched in stock Swiss. Keep the old file in your backup.
4. Safely eject, return the card and launch using your chosen route.
   Check System > System Information > About Indigo. Choose the Source
   holding your games, then open Library. See GAMES below.

UPDATING INDIGO (1.x OR 2.x)
Back up first. Replace the executable you ACTUALLY launch: /ipl.dol,
/boot.dol or /apps/indigo.dol. Rename the new ipl.dol copy as needed.
Merge the shared files again; keep games, settings, artwork, cheats,
saves and recovery shortcuts. Launch and check About Indigo.

WHAT EACH FILE IS FOR
  ipl.dol                      Indigo itself (rename for your launch route)
  swiss/patches/apploader.img  In-Game Reset returns to Indigo (see below)
  swiss/ui/                    the poster pack goes here, if you add it
  swiss/indigo/                Indigo's licence and notice

GAMES
Put your games in /games, either one folder per game or the disc images
directly:
    /games/Super Mario Sunshine [GMSE01]/game.iso
    /games/Super Mario Sunshine.iso
Disc images end in .iso, .gcm, .tgc or .fdi. The Library skips anything
else there (a text file, a cover image, an empty folder). With Library Folders
off, if /games holds no disc images, you get Swiss's plain file list instead.
With Library Folders on, an empty /games stays in Indigo.
On Home, turn the cube to Library and press A.
To sort your games into folders, turn on Settings > Setup > Library >
Library Folders: the Library then shows the folders in /games, two levels
deep. A opens a folder and B goes back. Keep a game's discs together. A PNG
beside a folder with its name (Nintendo.png for Nintendo) is its poster.

POSTERS AND CHEATS (optional, also drag and drop)
Without posters, each game shows its disc banner and its six-character game
ID (GMSE01). The poster pack and the cheat pack are on https://norvitech.com/indigo/
- unzip one and drag its swiss folder onto the root of the card, choosing
Merge on a Mac.

IN-GAME RESET
With In-Game Reset set to Apploader (Settings > Quick), A + Z + START
during a game returns to Indigo: swiss/patches/apploader.img is Indigo.
It replaces stock Swiss's copy, so a game started from stock Swiss returns
to Indigo too. Reboot resets the console, back to whatever your loader boots.
TXT

rm -f "$out/Indigo-$version.zip"
# Directory entries included, so the empty folders arrive on the card too.
(cd "$card" && zip -qrX "$out/Indigo-$version.zip" Indigo-README.txt ipl.dol swiss)
shasum -a 256 "$out/Indigo-$version.zip"
