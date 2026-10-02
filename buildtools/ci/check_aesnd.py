#!/usr/bin/env python3
"""Indigo builds libogc2's AESND from a copy, and cube/swiss/aesnd/UPSTREAM
names the libogc2 commit it is.

Every file in cube/swiss/aesnd/libogc2/ is byte-identical to that commit:
Indigo's changes are the patches beside it, which the Swiss build applies.
The commit is the toolchain's own libogc2, which its ogc/libversion.h names:
a toolchain that moves to another fails here until the copy follows it.

usage: buildtools/ci/check_aesnd.py [libogc2 repository; default: UPSTREAM's]
"""

import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from check_upstream import tree

COPY = "cube/swiss/aesnd"
LIBVERSION = "libogc2/gamecube/include/ogc/libversion.h"  # under $DEVKITPRO


def parse(text: str) -> tuple[str, str, str]:
    """UPSTREAM's repository, its commit, and the toolchain's libogc2 version."""
    fields = {}
    for line in text.splitlines():
        key, *rest = line.split(None, 1) or [""]
        if key and not key.startswith("#"):
            fields[key] = rest[0].strip() if rest else ""
    return fields.get("upstream", ""), fields.get("commit", ""), fields.get("toolchain", "")


def problems(root: Path, source: str = "", libversion: Path | None = None) -> list[str]:
    url, commit, version = parse((root / COPY / "UPSTREAM").read_text(encoding="utf-8"))
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        return [f"{COPY}/UPSTREAM names no commit: it needs a line `commit <40-character SHA>`"]
    found = []
    if libversion is not None and libversion.is_file():
        named = re.search(r'_V_STRING\s+"([^"]*)"', libversion.read_text(encoding="utf-8"))
        if not named or named.group(1) != version:
            found.append(f"the toolchain's libogc2 is {named.group(1) if named else 'unnamed'}, but {COPY} "
                         f"is {version}: bring the copy to the toolchain's commit (and its patches with it)")
    copy = root / COPY / "libogc2"
    with tempfile.TemporaryDirectory() as up:
        subprocess.run(["git", "init", "-q", "--bare", up], check=True)
        fetch = subprocess.run(["git", "-C", up, "fetch", "-q", "--depth=1", "--no-tags", source or url, commit],
                               capture_output=True, text=True)
        if fetch.returncode:
            return found + [f"cannot fetch libogc2 {commit} from {source or url}: {fetch.stderr.strip()}"]
        theirs = tree(up, commit)
        for path in sorted(p for p in copy.rglob("*") if p.is_file()):
            name = path.relative_to(copy).as_posix()
            if name not in theirs:
                found.append(f"{COPY}/libogc2/{name} is not in libogc2 {commit[:8]}")
                continue
            blob = subprocess.run(["git", "-C", up, "cat-file", "blob", theirs[name][1]],
                                  capture_output=True, check=True).stdout
            if blob != path.read_bytes():
                found.append(f"{COPY}/libogc2/{name} differs from libogc2 {commit[:8]}: "
                             f"change it with a patch in {COPY}")
    return found


def main() -> int:
    root = Path(subprocess.run(["git", "rev-parse", "--show-toplevel"],
                               capture_output=True, text=True, check=True).stdout.strip())
    devkitpro = os.environ.get("DEVKITPRO")
    libversion = Path(devkitpro, LIBVERSION) if devkitpro else None
    found = problems(root, sys.argv[1] if len(sys.argv) > 1 else "", libversion)
    if found:
        print("\n".join(found), file=sys.stderr)
        return 1
    url, commit, version = parse((root / COPY / "UPSTREAM").read_text(encoding="utf-8"))
    checked = "the toolchain's too" if libversion and libversion.is_file() else "no toolchain here to compare"
    print(f"AESND OK: {COPY}/libogc2 is libogc2 {commit[:8]} ({version}; {checked})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
