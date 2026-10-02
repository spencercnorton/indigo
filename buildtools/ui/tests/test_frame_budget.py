#!/usr/bin/env python3
"""Frame budget: what one steady frame of each scene costs the console.

frame_budget.c runs the real cube renderer and the hint icons against
counting GX stubs. Its counts are compared with frame_budget.json, whose
numbers are ceilings: a change that adds work to a frame raises one on
purpose, and a change that removes work lowers it, with

    python3 test_frame_budget.py --update

so the diff of frame_budget.json is the change's cost or gain. The hash is
reported, not enforced: it changes when a change moves a visible pixel.
"""

import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

from test_cheats_gx_stream import extract_function

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
GUI = ROOT / "cube/swiss/source/gui"
BUDGET = HERE / "frame_budget.json"
METRICS = ("sqrtf", "trig", "minmax", "vertices", "begins", "copy_pixels")
SOURCES = ("ui_scene.c", "ui_motion.c", "ui_home.c", "ui_cube_motif.c",
           "ui_color.c", "ui_stage.c", "ui_clock.c")


def hint_source() -> str:
    """The hint icons' shapes, out of FrameBufferMagic.c."""
    fbm = (GUI / "FrameBufferMagic.c").read_text(encoding="utf-8")
    parts = re.findall(r"^#define HINT_\w+ .*$", fbm, re.M)
    for marker in ("static void _HintVertex(", "static void _HintShape(",
                   "static void _HintRoundRect("):
        parts.append(extract_function(fbm, marker))
    return "\n\n".join(parts) + "\n"


def measure() -> dict:
    with tempfile.TemporaryDirectory() as tmp:
        Path(tmp, "hint_source.c").write_text(hint_source(), encoding="utf-8")
        binary = Path(tmp, "frame_budget")
        command = shlex.split(os.environ.get("CC", "cc")) + [
            "-std=gnu11", "-O1", "-ffp-contract=off", "-Wall",
            "-I", str(HERE / "fixtures/gx"), "-I", str(GUI), "-I", tmp,
            "-o", str(binary), str(HERE / "frame_budget.c"),
            *(str(GUI / name) for name in SOURCES), "-lm"]
        subprocess.run(command, check=True)
        output = subprocess.run([str(binary)], check=True, capture_output=True,
                                text=True).stdout
    return {row["scene"]: row for row in map(json.loads, output.splitlines())}


def main() -> int:
    measured = measure()
    if "--update" in sys.argv[1:]:
        BUDGET.write_text(json.dumps(measured, indent=1, sort_keys=True) + "\n",
                          encoding="utf-8")
        print(f"wrote {BUDGET.name}: {len(measured)} scenes")
        return 0

    budget = json.loads(BUDGET.read_text(encoding="utf-8"))
    failures = []
    print(f"{'scene':24} " + " ".join(f"{m:>12}" for m in METRICS) + "  hash")
    for name, row in measured.items():
        ceiling = budget.get(name)
        if ceiling is None:
            failures.append(f"{name}: no budget")
            continue
        cells = []
        for metric in METRICS:
            cell = f"{row[metric]:g}"
            if row[metric] > ceiling[metric]:
                failures.append(f"{name}: {metric} {row[metric]:g} > {ceiling[metric]:g}")
                cell += "!"
            elif row[metric] < ceiling[metric]:
                cell += "-"
            cells.append(f"{cell:>12}")
        moved = "" if row["hash"] == ceiling["hash"] else "  (pixels moved)"
        print(f"{name:24} " + " ".join(cells) + f"  {row['hash']}{moved}")
    failures += [f"{name}: scene gone" for name in budget.keys() - measured.keys()]
    if failures:
        print("\nframe budget exceeded:\n  " + "\n  ".join(failures) +
              "\nA change that adds work raises the budget on purpose: "
              "test_frame_budget.py --update", file=sys.stderr)
        return 1
    print("frame budget held ('-' is under budget: --update locks the gain in)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
