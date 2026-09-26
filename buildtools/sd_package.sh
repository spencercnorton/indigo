#!/bin/sh
# Assemble the drag-and-drop download for one release. The zip's root IS the
# root of the SD card: unzip it, select everything, drag it onto the card.
#   Indigo-<version>.zip
#     Indigo-README.txt              what goes where, in plain words
#     ipl.dol                        Indigo; PicoBoot and other modchips boot it
#     games/                         your disc images
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
mkdir -p "$card/games" "$card/swiss/ui" "$card/swiss/patches" "$card/swiss/indigo"
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
1. If your SD card already has an ipl.dol in its root, that is your current
   Swiss. Rename it to z.dol first if you want to keep it; holding Z while
   you power on starts it.
2. Select everything in this folder and drag it onto the root of your SD
   card. Let it replace files of the same name.
3. Put the card back and power on. PicoBoot and other modchips boot ipl.dol
   from the root of the card. With GC Loader or another loader that boots a
   disc image, start ipl.dol from Swiss's file browser instead.
Your Swiss settings carry over.

WHAT EACH FILE IS FOR
  ipl.dol                      Indigo itself
  games/                       your games go here (see GAMES)
  swiss/patches/apploader.img  In-Game Reset returns to Indigo (see below)
  swiss/ui/                    posters.pak goes here, if you add one
  swiss/indigo/                Indigo's licence (GPL-2.0-or-later) and notice

GAMES
Put your games in /games, either one folder per game or the disc images
directly, and keep nothing else in that folder:
    /games/Super Mario Sunshine [GMSE01]/game.iso
    /games/Super Mario Sunshine.iso
Disc images end in .iso, .gcm, .tgc or .fdi. Any other file in /games (a
text file, a cover image, an empty folder) turns the Library back into
Swiss's plain file list; "Hide unknown file types" in Settings > Setup >
Library hides stray files. On Home, turn the cube to Library and press A.

POSTERS AND CHEATS (optional, also drag and drop)
Without posters, each game shows its disc banner and its six-character game
ID (GMSE01). Poster packs and a cheat pack are on https://norvitech.com/indigo/
- unzip one and drag its swiss folder onto the root of the card.

IN-GAME RESET
With In-Game Reset set to Apploader (Settings > Quick), A + Z + START
during a game returns to Indigo: swiss/patches/apploader.img is Indigo.
It replaces stock Swiss's copy, so a game started from stock Swiss returns
to Indigo too. Reboot resets the console, back to whatever your loader boots.
TXT

rm -f "$out/Indigo-$version.zip"
# Directory entries included, so the empty folders arrive on the card too.
(cd "$card" && zip -qrX "$out/Indigo-$version.zip" Indigo-README.txt ipl.dol games swiss)
shasum -a 256 "$out/Indigo-$version.zip"
