#!/bin/sh
# Print the GitHub Release notes for one tag: its CHANGELOG.md section, then how
# to install it. A pre-release (vX.Y.Z-beta.N or vX.Y.Z-rc.N) uses the vX.Y.Z
# section when the changelog has one yet, else the "## Unreleased" section.
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

case $tag in
	*-rc.*) printf '%s\n\n' "> **Release candidate.** $version as it is meant to ship, for a last round of testing before it does. The current stable release is linked from https://norvitech.com/indigo/." ;;
	*-*) printf '%s\n\n' "> **Beta.** A pre-release for testing what comes next. The current stable release is linked from https://norvitech.com/indigo/." ;;
esac
# The notes are a copy of CHANGELOG.md, whose relative links name files in the
# repository. GitHub's release page and feed rewrite them, but the API and gh
# pass them on as written, so each goes to the file at this tag by full URL
# (a folder, ending in /, to its tree).
repo=https://github.com/spencercnorton/indigo
printf '%s\n' "$notes" | sed -e '/./,$!d' | sed -E \
	-e "s#\]\(([A-Za-z0-9._][^):]*/)\)#]($repo/tree/$tag/\1)#g" \
	-e "s#\]\(([A-Za-z0-9._][^):]*)\)#]($repo/blob/$tag/\1)#g"
cat <<NOTES

## Install

**New to Indigo:**

1. First, before you copy anything: if the card already has an \`ipl.dol\` in its root, that is your current Swiss. Rename it to \`z.dol\` to keep it; with PicoBoot or PicoLoader, holding Z at power-on starts it.
2. Unzip \`Indigo-$tag.zip\` (below), select everything inside and drag it onto the root of the card. Let it replace files of the same name. On a Mac, hold Option as you drop and choose **Merge**: **Replace** deletes what is already in the card's \`swiss\` folder (your settings, cheats and saves).
3. Put your games in \`/games\` (make the folder if the card has none), with nothing else in it or in the game folders, and power on.

**Updating from Indigo 1.x:** \`ipl.dol\` is already Indigo, so don't rename it; copy the new files over the old ones, choosing Merge on a Mac. If 1.25.0 made you rename stock Swiss to \`swiss.dol\`, you can rename it back to \`z.dol\`: since 2.0, Indigo doesn't start it by itself ([#3](https://github.com/spencercnorton/indigo/issues/3)).

PicoBoot and PicoLoader boot \`ipl.dol\`. FlippyDrive boots \`boot.dol\`, and with GC Loader or another loader that boots a disc image you start \`ipl.dol\` from Swiss: the [install guide]($repo/blob/$tag/docs/guide/install.md) covers every loader, updating and going back to stock Swiss.

**Posters and cheats** (optional; unzip one and drag its \`swiss\` folder onto the card the same way): [Posters: every region](https://indigo.norvitech.com/indigo-posters-all.zip) · [Posters: USA & Japan](https://indigo.norvitech.com/indigo-posters-ntsc.zip) · [Posters: Europe & Australia](https://indigo.norvitech.com/indigo-posters-pal.zip) · [Cheats](https://indigo.norvitech.com/indigo-cheats.zip)

**Verify:** \`SHA256SUMS.txt\` lists the zip's SHA-256, and the zip carries a build provenance attestation: \`gh attestation verify Indigo-$tag.zip --repo spencercnorton/indigo\`.
NOTES
