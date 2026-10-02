#!/bin/sh
# Assemble the drag-and-drop download for one release. The zip's root IS the
# root of the SD card: unzip it, select everything, drag it onto the card.
#   Indigo-<version>.zip
#     Indigo-README.txt              what goes where, in plain words
#     ipl.dol                        Indigo; PicoBoot and other modchips boot it
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

INSTALL: DRAG AND DROP
New to Indigo:
1. First, before you copy anything: if your SD card already has an ipl.dol
   in its root, that is your current Swiss. Rename it to z.dol to keep it;
   with PicoBoot or PicoLoader, holding Z while you power on starts it.
2. Select everything in this folder and drag it onto the root of the card.
   Let it replace files of the same name. On a Mac, hold Option as you drop
   and choose Merge: Replace deletes what is already in the card's swiss
   folder (your settings, cheats and saves).
3. Put your games in /games (make the folder if the card has none), as
   GAMES below shows, put the card back and power on.
Your Swiss settings carry over.

PicoBoot and PicoLoader boot ipl.dol. FlippyDrive boots boot.dol, and with
GC Loader or another loader that boots a disc image you start ipl.dol from
Swiss's file browser. The install guide covers every loader:
https://norvitech.com/indigo/guide/install/

UPDATING FROM INDIGO 1.x
ipl.dol is already Indigo, so don't rename it: copy these files over the
old ones, choosing Merge on a Mac. If 1.25.0 made you rename stock Swiss to
swiss.dol, you can rename it back to z.dol: since 2.0, Indigo doesn't start
it by itself.

WHAT EACH FILE IS FOR
  ipl.dol                      Indigo itself
  swiss/patches/apploader.img  In-Game Reset returns to Indigo (see below)
  swiss/ui/                    the poster pack goes here, if you add it
  swiss/indigo/                Indigo's licence (GPL-2.0-or-later) and notice

GAMES
Put your games in /games, either one folder per game or the disc images
directly:
    /games/Super Mario Sunshine [GMSE01]/game.iso
    /games/Super Mario Sunshine.iso
Disc images end in .iso, .gcm, .tgc or .fdi. The Library skips anything
else there (a text file, a cover image, an empty folder). If /games holds
no disc images (and, with Library Folders on, no folders), you get Swiss's
plain file list instead.
On Home, turn the cube to Library and press A.
To sort your games into folders, turn on Settings > Setup > Library >
Library Folders: the Library then shows the folders in /games, two levels
deep, empty ones included. A opens a folder and B goes back. Only a folder
named like Super Mario Sunshine [GMSE01] is one game; a game in a folder
with any other name shows as a folder you open. Keep a game's discs
together. A PNG beside a folder with its name (Nintendo.png for Nintendo)
is its poster.

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
