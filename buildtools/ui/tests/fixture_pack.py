#!/usr/bin/env python3
"""Build a small real posters.pak (and optionally a stills.pak) for the C
harness end-to-end pass.

Usage: fixture_pack.py <output.pak> [<stills.pak>]
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
    if len(sys.argv) not in (2, 3):
        print("usage: fixture_pack.py <output.pak> [<stills.pak>]",
              file=sys.stderr)
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
    gradient_image(640, 480).save(os.path.join(art, "still-GALE01.png"))
    flat_image(512, 384, (40, 160, 60)).save(
        os.path.join(art, "still-GC6E01.png"))
    gradient_image(320, 240).save(os.path.join(art, "still-GM4E01.png"))

    outputs = [("posters", "", sys.argv[1])]
    if len(sys.argv) == 3:
        outputs.append(("stills", "still-", sys.argv[2]))
    for kind, prefix, out in outputs:
        records = []
        for gid, universal in (("GALE01", True), ("GC6E01", False),
                               ("GM4E01", False)):
            source = os.path.join(art, f"{prefix}{gid}.png")
            records.append({
                "game_id": gid,
                "source": os.path.relpath(source, workdir),
                "universal": universal,
                "source_sha256": sha256_file(source),
                "note": "C-harness fixture, synthetic art",
            })
        manifest = os.path.join(workdir, f"{kind}.json")
        with open(manifest, "w", encoding="utf-8") as f:
            json.dump({"version": 1, "kind": kind, "records": records}, f)
        pack = pp.generate(manifest, out, art_root=workdir)
        print(f"fixture_pack: {out} "
              f"({len(pack)} bytes, sha256 {hashlib.sha256(pack).hexdigest()})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
