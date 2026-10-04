#!/usr/bin/env python3
"""Indigo is built on one commit of upstream Swiss, and UPSTREAM names it.

Every file outside Indigo's own paths (OWN) matches upstream at that commit,
line endings aside, unless UPSTREAM lists it as one of Indigo's own changes,
with the reason. A listed path that matches upstream again fails too, so the
list stays true.

usage: buildtools/ci/check_upstream.py [upstream repository; default: UPSTREAM's]
"""

import re
import subprocess
import sys
import tempfile
from pathlib import Path

# Indigo's own paths: the interface, where it is wired into Swiss, and the
# project's own files. Anything else is upstream's.
OWN = re.compile(r"^(cube/swiss/source/gui/|cube/swiss/source/images/|cube/swiss/source/swiss\.[ch]$"
                 r"|cube/swiss/source/main\.c$|cube/swiss/include/swiss\.h$|cube/swiss/include/input\.h$"
                 r"|cube/swiss/include/mp3\.h$|cube/swiss/source/input\.c$|cube/swiss/source/mp3\.c$"
                 r"|cube/swiss/source/wiiload\.c$|cube/swiss/source/cheats/(cheats|cheat_policy)\.[ch]$"
                 r"|cube/swiss/source/config/|docs/|AGENTS/|buildtools/|\.github/|[^/]*\.md$"
                 r"|UPSTREAM$|NOTICE$|\.gitignore$|\.public-release\.toml$)")


def parse(text: str) -> tuple[str, str, dict[str, str]]:
    """UPSTREAM's repository, its commit, and the listed paths with their reasons."""
    url = commit = ""
    listed = {}
    for line in text.splitlines():
        key, *rest = line.split(None, 1) or [""]
        rest = rest[0].strip() if rest else ""
        if not key or key.startswith("#"):
            continue
        if key == "upstream":
            url = rest
        elif key == "commit":
            commit = (rest.split() or [""])[0]
        else:
            listed[key] = rest
    return url, commit, listed


def tree(repo: str, rev: str) -> dict[str, tuple[str, str]]:
    out = subprocess.run(["git", "-C", repo, "ls-tree", "-r", "-z", "--full-tree", rev],
                         capture_output=True, check=True).stdout
    entries = {}
    for record in filter(None, out.split(b"\0")):
        meta, path = record.split(b"\t", 1)
        mode, _, sha = meta.decode().split()
        entries[path.decode()] = (mode, sha)
    return entries


def text(repo: str, sha: str) -> bytes:
    blob = subprocess.run(["git", "-C", repo, "cat-file", "blob", sha], capture_output=True, check=True).stdout
    return blob.replace(b"\r\n", b"\n")


def covers(entry: str, path: str) -> bool:
    """A listed path covers itself; one ending in / covers its directory."""
    return path == entry.rstrip("/") or (entry.endswith("/") and path.startswith(entry))


def problems(root: Path, source: str = "") -> list[str]:
    url, commit, listed = parse((root / "UPSTREAM").read_text(encoding="utf-8"))
    found = [f"UPSTREAM gives no reason for {path}" for path, why in listed.items() if not why]
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        return found + ["UPSTREAM names no commit: it needs a line `commit <40-character SHA>`"]
    with tempfile.TemporaryDirectory() as up:
        subprocess.run(["git", "init", "-q", "--bare", up], check=True)
        fetch = subprocess.run(["git", "-C", up, "fetch", "-q", "--depth=1", "--no-tags", source or url, commit],
                               capture_output=True, text=True)
        if fetch.returncode:
            return found + [f"cannot fetch upstream commit {commit} from {source or url}: {fetch.stderr.strip()}"]
        theirs, ours = tree(up, commit), tree(str(root), "HEAD")
        differ = []
        for path in sorted(set(theirs) | set(ours)):
            a, b = theirs.get(path), ours.get(path)
            if OWN.match(path) or a == b:
                continue
            if a and b and a[0] == b[0] != "160000" and text(up, a[1]) == text(str(root), b[1]):
                continue
            differ.append(path)
    found += [f"{path} differs from upstream {commit[:8]} and UPSTREAM does not list it"
              for path in differ if not any(covers(entry, path) for entry in listed)]
    found += [f"UPSTREAM lists {entry}, which matches upstream {commit[:8]}"
              for entry in listed if not any(covers(entry, path) for path in differ)]
    return found


def main() -> int:
    root = Path(subprocess.run(["git", "rev-parse", "--show-toplevel"],
                               capture_output=True, text=True, check=True).stdout.strip())
    found = problems(root, sys.argv[1] if len(sys.argv) > 1 else "")
    if found:
        print("\n".join(found), file=sys.stderr)
        print("Make the file match upstream, or list it in UPSTREAM with the reason; "
              "remove a line that no longer applies.", file=sys.stderr)
        return 1
    url, commit, listed = parse((root / "UPSTREAM").read_text(encoding="utf-8"))
    print(f"upstream OK: built on {url} {commit[:8]}; {len(listed)} listed paths carry Indigo's own changes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
