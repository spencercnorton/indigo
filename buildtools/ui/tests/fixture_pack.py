#!/usr/bin/env python3
"""Build a small real posters.pak for the C harness end-to-end pass.

Usage: fixture_pack.py <output.pak>
Exits 3 (distinct from generator errors) when no encoder is available so
run_tests.sh can skip the real-pak pass loudly instead of failing.
"""

import hashlib
import json
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import poster_pack as pp

from test_poster_pack import flat_image, gradient_image, sha256_file


def main():
    if len(sys.argv) != 2:
        print("usage: fixture_pack.py <output.pak>", file=sys.stderr)
        return 2
    try:
        pp.resolve_gxtexconv(None)
    except pp.PackError as e:
        print(f"fixture_pack: skipping ({e})", file=sys.stderr)
        return 3

    workdir = tempfile.mkdtemp(prefix="fixture_pack_")
    art = os.path.join(workdir, "art")
    os.makedirs(art)
    gradient_image(384, 512).save(os.path.join(art, "GALE01.png"))
    flat_image(256, 342, (60, 60, 140)).save(os.path.join(art, "GC6E01.png"))
    gradient_image(192, 256).save(os.path.join(art, "GM4E01.png"))

    records = []
    for gid, universal in (("GALE01", True), ("GC6E01", False),
                           ("GM4E01", False)):
        records.append({
            "game_id": gid,
            "source": os.path.join("art", f"{gid}.png"),
            "universal": universal,
            "source_sha256": sha256_file(os.path.join(art, f"{gid}.png")),
            "note": "C-harness fixture, synthetic art",
        })
    manifest = os.path.join(workdir, "manifest.json")
    with open(manifest, "w", encoding="utf-8") as f:
        json.dump({"version": 1, "records": records}, f)

    pack = pp.generate(manifest, sys.argv[1], art_root=workdir)
    print(f"fixture_pack: {sys.argv[1]} "
          f"({len(pack)} bytes, sha256 {hashlib.sha256(pack).hexdigest()})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
