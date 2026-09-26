#!/bin/sh
# Print the GitHub Release notes for one tag: its CHANGELOG.md section, then how
# to install it. A beta (vX.Y.Z-beta.N) uses the vX.Y.Z section when the
# changelog has one yet, else the "## Unreleased" section.
# Usage: buildtools/release_notes.sh <tag>   (run from the repository root)
set -eu

tag=${1:?usage: buildtools/release_notes.sh <tag>}
version=${tag%%-*}

section() { # section <heading word>: the lines under "## <word> ..." up to the next "## "
	awk -v v="$1" '/^## / { if (found) exit; if ($2 == v) { found = 1; next } } found { print }' CHANGELOG.md
}

notes=$(section "$version")
if [ -z "$notes" ] && [ "$tag" != "$version" ]; then
	notes=$(section Unreleased)
fi
[ -n "$notes" ] || { echo "release_notes: CHANGELOG.md has no section for $version" >&2; exit 1; }

if [ "$tag" != "$version" ]; then
	printf '%s\n\n' "> **Beta.** A pre-release for testing what comes next. The current stable release is linked from https://norvitech.com/indigo/."
fi
printf '%s\n' "$notes" | sed -e '/./,$!d'
cat <<NOTES

## Install

1. Download \`Indigo-$tag.zip\` below.
2. Unzip it, select everything inside and drag it onto the root of your SD card. If the card already has an \`ipl.dol\`, that is your current Swiss: rename it to \`z.dol\` first to keep it.
3. Put your games in \`/games\` and power on.

The [install guide](https://norvitech.com/indigo/guide/install/) covers every loader, updating and going back to stock Swiss.

**Posters and cheats** (optional, also drag and drop): [Posters: USA & Japan](https://indigo.norvitech.com/indigo-posters-ntsc.zip) · [Posters: Europe & Australia](https://indigo.norvitech.com/indigo-posters-pal.zip) · [Cheats](https://indigo.norvitech.com/indigo-cheats.zip)

**Verify:** \`SHA256SUMS.txt\` lists the zip's SHA-256, and the zip carries a build provenance attestation: \`gh attestation verify Indigo-$tag.zip --repo spencercnorton/indigo\`.
NOTES
