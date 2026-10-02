#!/usr/bin/env python3
"""Stop the menu's audio hundreds of times in Dolphin and check it never freezes.

aesnd/aesnd.c starts AESND with a playing voice and resets it as a launch
does, round after round, and logs every 50th round on the debug UART. A
reset that waits on the DSP for ever leaves the count still: the console
seconds that pass without a new round decide.

  aesnd_test.py build/aesnd.dol --out emulator/aesnd
"""
from __future__ import annotations

import argparse
import re
import sys
import tempfile
import time
from pathlib import Path

import run

STALL_SECONDS = 20  # of the console's time with no new round: 50 rounds take about 3


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("dol", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args(argv)
    args.out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as directory:
        emulator = run.Emulator(args.dol.resolve(), None, Path(directory), args.out, "ntsc")
        try:
            last, stall = None, run.Deadline(emulator, STALL_SECONDS)
            while emulator.dolphin.poll() is None:
                time.sleep(1)
                found = re.findall(r"aesnd: (round \d+|done)", emulator.log.read_bytes().decode(errors="replace"))
                now = found[-1] if found else None
                if now == "done":
                    print(f"PASS the menu's audio stopped every time ({found[-2] if len(found) > 1 else ''})")
                    return 0
                if now != last:
                    last, stall = now, run.Deadline(emulator, STALL_SECONDS)
                elif stall.expired():
                    print(f"FAIL the menu's audio stop froze after {last or 'round 0'}: {emulator.where()}")
                    return 1
            print(f"FAIL Dolphin exited ({emulator.dolphin.returncode}); see {emulator.log}")
            return 1
        finally:
            emulator.close()


if __name__ == "__main__":
    sys.exit(main())
