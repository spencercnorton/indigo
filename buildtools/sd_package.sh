#!/bin/sh
# Assemble the drag-and-drop download for one release:
#   Indigo-<version>.zip
#     Indigo-<version>/README.txt, LICENSE.txt, NOTICE.txt
#     Indigo-<version>/SD card/ipl.dol, games/, swiss/ui/
# Usage: buildtools/sd_package.sh <version> [swiss.dol] [out dir]
# Build the DOL first (see README, Install); run from the repository root.
set -eu

version=${1:?usage: buildtools/sd_package.sh <version> [swiss.dol] [out dir]}
dol=${2:-cube/swiss/swiss.dol}
out=$(cd "${3:-.}" && pwd)
[ -f "$dol" ] || { echo "sd_package: no DOL at $dol; build it first" >&2; exit 1; }

stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
root="$stage/Indigo-$version"
mkdir -p "$root/SD card/games" "$root/SD card/swiss/ui"
cp "$dol" "$root/SD card/ipl.dol"
cp LICENSE "$root/LICENSE.txt"
cp NOTICE "$root/NOTICE.txt"
cat > "$root/README.txt" <<EOF
Indigo $version - an unofficial fork of Swiss with the interface rebuilt
Source code: https://github.com/spencercnorton/indigo/tree/$version

INSTALL
1. If your SD card already has an ipl.dol in its root, that is your current
   Swiss. Rename it to z.dol first; holding Z while you power on starts it.
2. Copy everything inside the "SD card" folder to the root of your SD card.
   PicoBoot and other modchips boot ipl.dol from the root. With GC Loader or
   another loader that boots a disc image, start ipl.dol from Swiss instead.
Your Swiss settings carry over.

GAMES
Put your games in /games, either one folder per game or the disc images
directly, and keep nothing else in that folder:
    /games/Super Mario Sunshine [GMSE01]/game.iso
    /games/Super Mario Sunshine.iso
Disc images end in .iso, .gcm, .tgc or .fdi. Any other file in /games (a
text file, a cover image, an empty folder) turns the Library back into
Swiss's plain file list; Settings > Interface > "Hide unknown file types"
hides stray files. On Home, turn the cube to Library and press A.

POSTERS (optional)
Without posters, each game shows its disc banner and its six-character game
ID (GMSE01). For box art, build posters.pak from cover images named by that
ID and copy it to /swiss/ui/posters.pak. See "Set up your library" at
https://github.com/spencercnorton/indigo#set-up-your-library

In-Game Reset set to Apploader returns to the Swiss inside
/swiss/patches/apploader.img, not to Indigo.
EOF

rm -f "$out/Indigo-$version.zip"
(cd "$stage" && zip -qrX "$out/Indigo-$version.zip" "Indigo-$version")
shasum -a 256 "$out/Indigo-$version.zip"
