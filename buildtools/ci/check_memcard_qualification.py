#!/usr/bin/env python3
"""Inspect recorded MemCard PRO test coverage without inferring a hardware pass."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
RECORD = "docs/validation/memcard-pro.json"
# What a real card ran: the slot code whole, and swiss.c's launch around a
# switch (the wait for an ID that waited, then the cards' set-up before the
# game starts). The rest of swiss.c is the release's own: Game Detail only
# reads a memory card emulator, as the tested build did (gameflowSaveChoosable,
# held by audit_game_detail_safety.py).
SOURCES = (
    "cube/swiss/source/gui/card_slots.c", "cube/swiss/source/gui/card_slots.h",
    "cube/swiss/source/gui/ui_card_slots.c", "cube/swiss/source/gui/ui_card_slots.h",
)
LAUNCH = "cube/swiss/source/swiss.c"
LAUNCH_PARTS = (
    re.compile(r"(?ms)^static void gameflowWaitForCardIds\(void\)\n\{\n.*?^\}\n"),
    re.compile(r"(?m)^\telse \{\n\t\tinitialize_card\(CARD_SLOTA\);\n"
               r"\t\tinitialize_card\(CARD_SLOTB\);\n"),
)
CHECKS = ("existing_card_updates", "new_card_finishes", "rapid_gameid_switches")


def fingerprint(root: Path, revision: str | None = None) -> str:
    digest = hashlib.sha256()
    def content(relative: str) -> bytes:
        if revision is None:
            return (root / relative).read_bytes()
        return subprocess.run(["git", "-C", str(root), "show", f"{revision}:{relative}"],
                              check=True, capture_output=True).stdout
    for relative in SOURCES:
        digest.update(relative.encode() + b"\0" + content(relative) + b"\0")
    launch = content(LAUNCH).decode()
    for part in LAUNCH_PARTS:
        found = part.findall(launch)
        if len(found) != 1:
            raise ValueError(f"the launch around a card switch changed in {LAUNCH}")
        digest.update(found[0].encode() + b"\0")
    # libogc2 supplies the actual MMCE/CARD/EXI implementation. Bind the
    # pinned build image too, without binding unrelated workflow comments.
    for relative in (".github/workflows/ci.yml", ".github/workflows/release.yml"):
        values = re.findall(r"(?m)^\s+TOOLCHAIN:\s*(.+)$", content(relative).decode())
        if len(values) != 1:
            raise ValueError(f"ambiguous toolchain in {relative}")
        digest.update(relative.encode() + b"\0" + values[0].encode() + b"\0")
    return digest.hexdigest()


def problems(root: Path, tag: str) -> list[str]:
    if re.fullmatch(r"v\d+\.\d+\.\d+-(?:beta|rc)\.\d+", tag):
        return []
    if not re.fullmatch(r"v\d+\.\d+\.\d+", tag):
        return ["not a release or prerelease tag"]
    if not (root / SOURCES[0]).exists():
        return []  # A legacy release without this switching implementation.
    try:
        record = json.loads((root / RECORD).read_text())
        current = fingerprint(root)
    except (OSError, ValueError) as error:
        return [f"hardware qualification unavailable: {error}"]
    if not isinstance(record, dict):
        return ["hardware qualification must be an object"]
    found = []
    if record.get("status") != "passed" or record.get("environment") != "real-hardware":
        found.append("MemCard PRO switching remains unqualified on real hardware")
    if record.get("source_fingerprint") != current:
        found.append("hardware qualification does not match the current switching code")
    revision = record.get("tested_revision")
    if not isinstance(revision, str) or not re.fullmatch(r"[0-9a-f]{40}", revision):
        found.append("the tested build's full revision is required")
    else:
        try:
            if fingerprint(root, revision) != current:
                found.append("the tested build contains different switching code")
        except (subprocess.CalledProcessError, ValueError):
            found.append("the tested build's switching source cannot be read")
    hardware = record.get("hardware")
    if (not isinstance(hardware, dict) or hardware.get("device") != "MemCard PRO GC"
            or not isinstance(hardware.get("firmware"), str) or not hardware["firmware"].strip()
            or not isinstance(hardware.get("setup"), str) or not hardware["setup"].strip()):
        found.append("the real card, firmware and console/loader setup are required")
    evidence = record.get("evidence")
    if not isinstance(evidence, str) or not evidence.startswith("https://"):
        found.append("a reviewed hardware-test evidence link is required")
    results = record.get("results")
    if not isinstance(results, dict) or any(results.get(check) is not True for check in CHECKS):
        found.append("every real-card switching check must pass")
    return found


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tag", nargs="?")
    parser.add_argument("--fingerprint", action="store_true")
    parser.add_argument("--report", action="store_true",
                        help="report coverage without making it a publication gate")
    args = parser.parse_args()
    if args.fingerprint:
        print(fingerprint(ROOT))
        return 0
    if not args.tag:
        parser.error("a tag or --fingerprint is required")
    if args.report:
        if not re.fullmatch(r"v\d+\.\d+\.\d+(?:-(?:beta|rc)\.\d+)?", args.tag):
            parser.error("not a release or prerelease tag")
        # Report the same missing coverage on candidates and stable releases.
        found = problems(ROOT, args.tag.split("-", 1)[0])
        print("## MemCard PRO GC test coverage")
        if found:
            print("Current source/SDK: **pending real-card qualification**.")
            print("This coverage gap does not block publication.")
            for problem in found:
                print(f"- {problem}")
        elif not (ROOT / SOURCES[0]).exists():
            print("This source does not contain the switching implementation.")
        else:
            print("The recorded real-card pass matches the current source/SDK.")
        return 0
    found = problems(ROOT, args.tag)
    for problem in found:
        print(f"Qualification audit: {problem}", file=sys.stderr)
    if not found:
        print("Qualification audit completed for this release channel.")
    return bool(found)


if __name__ == "__main__":
    raise SystemExit(main())
