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

Download **\`Indigo-$tag.zip\`** from Assets below, rather than GitHub's Source code ZIP. Back up your working card and **extract the ZIP on your computer**. The **card root** is the first level you see when you open it; copy files from inside the extracted folder, rather than the ZIP or its enclosing Indigo folder.

| Your current setup | Download → card destination | Launch |
| --- | --- | --- |
| PicoBoot / PicoLoader using gekkoboot | \`ipl.dol\` → \`/ipl.dol\` on its SD adapter | Power on normally |
| FlippyDrive | \`ipl.dol\` → \`/boot.dol\` on FlippyDrive's microSD | Normal boot mode |
| GC Loader, Swiss in flash, cubeboot, or unsure | \`ipl.dol\` → \`/apps/indigo.dol\` | Start Swiss, browse to Apps, launch \`indigo.dol\` |

Identify and back up your existing boot program before replacing it: an \`ipl.dol\` is not always Swiss. With gekkoboot, preserve stock Swiss as \`z.dol\` only if that shortcut is unused; keep existing shortcuts. The Launch from Swiss route keeps your original boot files and needs no firmware change. The Indigo ZIP contains no \`boot.iso\`; keep GC Loader's current boot image.

**Merge the download's \`swiss\` files into your existing folder.** On a Mac, hold Option while dragging and choose **Merge**, never Replace for the whole folder. If Merge is unavailable, copy the individual files into the matching subfolders. Keep settings, posters, cheats, saves and games. The new \`swiss/patches/apploader.img\` makes Apploader In-Game Reset return to Indigo, including for games started in stock Swiss.

Safely eject, launch by your route, and check **System › System Information › About Indigo**. Choose the source holding your games; the Library looks for \`/games\` there by default. SD2SP2 and SD Gecko are adapters, not boot methods, and your boot card and game-storage card may differ.

**Updating Indigo 1.x or 2.x:** replace the executable you actually launch (\`/ipl.dol\`, \`/boot.dol\` or \`/apps/indigo.dol\`), renaming the new copy as needed, and merge the shared files again. Keep your recovery copy.

The [step-by-step install guide](https://norvitech.com/indigo/guide/install/) has exact card trees, first-launch checks, upgrades and recovery; the [guide included with this release]($repo/blob/$tag/docs/guide/install.md) is also available on GitHub.

**Posters and cheats** (optional; unzip one and drag its \`swiss\` folder onto the card the same way): [Posters](https://indigo.norvitech.com/indigo-posters-all.zip) (every region, with Spotlight's stills and descriptions) · [Cheats](https://indigo.norvitech.com/indigo-cheats.zip)

**Verify:** \`SHA256SUMS.txt\` lists the zip's SHA-256, and the zip carries a build provenance attestation: \`gh attestation verify Indigo-$tag.zip --repo spencercnorton/indigo\`.
NOTES
