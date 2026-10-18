#!/bin/sh
# Verified portlib overlay for unprivileged, reproducible builds. The runner
# caches the archive while its image is built; jobs never need package egress.
# Usage: libiconv-lite.sh --cache DIR | libiconv-lite.sh DESTINATION
set -eu
package=ppc-libiconv-lite-1.19-4-any.pkg.tar.zst
sha=5ab3303642f9cc2145cf4caed9c42bf35b49c3484ff2815a07c772c9255e64d7
url=https://packages.libogc2.org/devkitpro/linux/x86_64/$package
verify() { printf '%s  %s\n' "$sha" "$1" | sha256sum -c - >/dev/null; }
fetch() {
	if [ ! -f "$1" ]; then
		curl -fLsS --retry 3 "$url" -o "$1.tmp"
		verify "$1.tmp"
		mv "$1.tmp" "$1"
	fi
	verify "$1"
}
if [ "${1:-}" = --cache ]; then
	mkdir -p "${2:?cache directory}"
	fetch "$2/$package"
	exit 0
fi
dest=${1:?destination directory}
if [ -f "$dest/.sha256" ] && [ "$(cat "$dest/.sha256")" = "$sha" ] &&
	[ -f "$dest/lib/libiconv.a" ]; then exit 0; fi
mkdir -p "$(dirname "$dest")"
cache=/usr/local/share/indigo
if [ ! -f "$cache/$package" ]; then
	cache=$(dirname "$dest")
	fetch "$cache/$package"
fi
verify "$cache/$package"
staging=$(mktemp -d "$dest.XXXXXX")
trap 'rm -rf "$staging"' EXIT HUP INT TERM
bsdtar -xf "$cache/$package" -C "$staging" --strip-components 4 opt/devkitpro/portlibs/ppc
printf '%s\n' "$sha" > "$staging/.sha256"
rm -rf "$dest"
mv "$staging" "$dest"
echo "ppc-libiconv-lite 1.19-4 ($sha)"
