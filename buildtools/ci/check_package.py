#!/usr/bin/env python3
"""Check the drag-and-drop zip: exactly the SD card layout, and the DOL just built.

The zip's root is the root of an SD card, so every entry matters: an extra
file lands on someone's card, a missing folder breaks the Library, and an
ipl.dol that is not the DOL CI built and tested is not the release.

usage: check_package.py ZIP --dol cube/swiss/swiss.dol --version <version>
"""

from __future__ import annotations

import argparse
import stat
import sys
import zipfile
from pathlib import Path

LAYOUT = {
    "Indigo-README.txt", "ipl.dol", "games/", "swiss/", "swiss/ui/", "swiss/patches/",
    "swiss/patches/apploader.img", "swiss/indigo/", "swiss/indigo/LICENSE.txt",
    "swiss/indigo/NOTICE.txt",
}
MAX_ZIP_BYTES = 6 * 1024 * 1024  # a tripwire; the zip is under 3 MiB


def problems(zip_path: Path, dol: bytes, version: str) -> list[str]:
    found: list[str] = []
    if zip_path.stat().st_size > MAX_ZIP_BYTES:
        found.append(f"{zip_path.name} is {zip_path.stat().st_size} bytes, over {MAX_ZIP_BYTES}")
    with zipfile.ZipFile(zip_path) as archive:
        if (bad := archive.testzip()) is not None:
            found.append(f"corrupt member: {bad}")
        names = [info.filename for info in archive.infolist()]
        if len(names) != len(set(names)):
            found.append("duplicate entries")
        for name in sorted(set(names) - LAYOUT):
            found.append(f"unexpected entry: {name}")
        for name in sorted(LAYOUT - set(names)):
            found.append(f"missing entry: {name}")
        for info in archive.infolist():
            mode = info.external_attr >> 16
            if stat.S_ISLNK(mode) or mode & (stat.S_ISUID | stat.S_ISGID):
                found.append(f"{info.filename}: symlink or set-id bit")
            if info.filename.startswith("/") or ".." in info.filename.split("/"):
                found.append(f"{info.filename}: escapes the card root")
        if "ipl.dol" in names and archive.read("ipl.dol") != dol:
            found.append("ipl.dol is not the DOL this build produced")
        if "swiss/patches/apploader.img" in names:
            image = archive.read("swiss/patches/apploader.img")
            if f"*indigo-{version}".encode() not in image:
                found.append(f"apploader.img does not carry *indigo-{version}")
        if "Indigo-README.txt" in names:
            readme = archive.read("Indigo-README.txt").decode("utf-8", "replace")
            if not readme.startswith(f"Indigo {version} "):
                found.append(f"Indigo-README.txt does not open with Indigo {version}")
    return found


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("zip", type=Path)
    parser.add_argument("--dol", type=Path, required=True)
    parser.add_argument("--version", required=True)
    args = parser.parse_args(argv)
    found = problems(args.zip, args.dol.read_bytes(), args.version)
    for problem in found:
        print(f"::error::{args.zip.name}: {problem}", file=sys.stderr)
    if not found:
        print(f"{args.zip.name}: the SD card layout, the DOL CI built, version {args.version}")
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
