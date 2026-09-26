#!/usr/bin/env python3
"""Host tests for buildtools/ui/poster_pack.py.

Manifest/validation tests run everywhere. Tests that need the real encoder
(gxtexconv natively or via the libogc2 Docker image) are skipped loudly
when neither is available.
"""

import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import poster_pack as pp

from PIL import Image

try:
    pp.resolve_gxtexconv(None)
    HAVE_ENCODER = True
except pp.PackError:
    HAVE_ENCODER = False

NEEDS_ENCODER = unittest.skipUnless(
    HAVE_ENCODER, "gxtexconv unavailable (no devkitPro and no docker)")


def sha256_file(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def gradient_image(w, h):
    """Left-dark to right-bright horizontal gradient (deterministic)."""
    im = Image.new("RGB", (w, h))
    px = im.load()
    for y in range(h):
        for x in range(w):
            v = x * 255 // max(w - 1, 1)
            px[x, y] = (v, v // 2, 255 - v)
    return im


def flat_image(w, h, rgb):
    return Image.new("RGB", (w, h), rgb)


class Fixtures:
    """Class-level art fixtures shared by every test."""

    def __init__(self):
        self.dir = tempfile.mkdtemp(prefix="poster_pack_test_")
        self.art = os.path.join(self.dir, "art")
        os.makedirs(self.art)
        gradient_image(384, 512).save(self.path("GALE01.png"))
        flat_image(512, 512, (200, 40, 40)).save(self.path("GC6E01.png"))
        gradient_image(192, 256).save(self.path("GM4E01.png"))
        flat_image(100, 100, (0, 0, 0)).save(self.path("small.png"))
        with open(self.path("corrupt.png"), "wb") as f:
            f.write(b"this is not a png at all")

    def path(self, name):
        return os.path.join(self.art, name)

    def record(self, gid, source=None, **overrides):
        source = source or f"{gid}.png"
        rec = {
            "game_id": gid,
            "source": os.path.join("art", source),
            "universal": False,
            "source_sha256": sha256_file(self.path(source))
            if os.path.exists(self.path(source)) else "0" * 64,
            "note": "test fixture, synthetic art",
        }
        rec.update(overrides)
        return rec

    def write_manifest(self, records, version=1):
        path = os.path.join(self.dir, "manifest.json")
        with open(path, "w", encoding="utf-8") as f:
            json.dump({"version": version, "records": records}, f)
        return path

    def cleanup(self):
        shutil.rmtree(self.dir, ignore_errors=True)


FX = None


def setUpModule():
    global FX
    FX = Fixtures()


def tearDownModule():
    FX.cleanup()


class ManifestValidation(unittest.TestCase):
    def expect_error(self, records, fragment, version=1):
        manifest = FX.write_manifest(records, version=version)
        with self.assertRaises(pp.PackError) as ctx:
            pp.load_manifest(manifest)
        self.assertIn(fragment, str(ctx.exception))

    def test_valid_manifest_loads_sorted(self):
        manifest = FX.write_manifest([
            FX.record("GM4E01"),
            FX.record("GALE01", universal=True),
        ])
        records = pp.load_manifest(manifest)
        self.assertEqual([r["game_id"] for r in records], ["GALE01", "GM4E01"])

    def test_invalid_ids(self):
        for bad in ("gale01", "GALE1", "GALE012", "GAL-01", "GALÉ01"):
            self.expect_error([FX.record("GALE01") | {"game_id": bad}],
                              "invalid game_id")

    def test_duplicate_ids(self):
        self.expect_error([FX.record("GALE01"), FX.record("GALE01")],
                          "duplicate game_id")

    def test_ambiguous_universal_prefix(self):
        self.expect_error(
            [FX.record("GALE01", universal=True),
             FX.record("GALE69", source="GALE01.png", universal=True)],
            "ambiguous fallback")

    def test_universal_must_be_explicit_boolean(self):
        self.expect_error([FX.record("GALE01") | {"universal": "yes"}],
                          "universal must be true/false")

    def test_missing_required_keys(self):
        rec = FX.record("GALE01")
        del rec["note"]
        self.expect_error([rec], "missing required key 'note'")

    def test_unknown_keys_rejected(self):
        self.expect_error([FX.record("GALE01") | {"univresal": True}],
                          "unknown keys")

    def test_bad_focal(self):
        self.expect_error([FX.record("GALE01", focal=[1.5, 0.5])], "focal")

    def test_boolean_focal_rejected(self):
        # JSON true parses as Python True, which is an int subclass; the
        # validator must not silently coerce it to a hard-edge crop.
        self.expect_error([FX.record("GALE01", focal=[True, 0.5])], "focal")

    def test_boolean_dominant_rejected(self):
        self.expect_error(
            [FX.record("GALE01", dominant_rgb=[True, True, True])],
            "dominant_rgb")

    def test_bad_sha(self):
        self.expect_error([FX.record("GALE01") | {"source_sha256": "zz"}],
                          "source_sha256")

    def test_overflow_record_count(self):
        records = [FX.record("GALE01") | {"game_id": f"G{i:04d}X"[:6]}
                   for i in range(pp.MAX_RECORDS + 1)]
        # unique synthetic IDs: A-Z0-9 only
        for i, rec in enumerate(records):
            rec["game_id"] = f"G{i:05d}"[:6]
        self.expect_error(records, "pack overflow")

    def test_empty_records(self):
        self.expect_error([], "non-empty records")

    def test_wrong_version(self):
        self.expect_error([FX.record("GALE01")], "version", version=2)


class CoversFolder(unittest.TestCase):
    def test_covers_folder_becomes_valid_manifest(self):
        d = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, d)
        for name in ("gmse01.PNG", "GALE01.jpg", "Mario Sunshine.png",
                     "GZLE01.gif", ".DS_Store", "._GALE01.jpg"):
            with open(os.path.join(d, name), "wb") as f:
                f.write(name.encode())
        manifest, skipped = pp.covers_manifest(d)
        self.assertEqual([r["game_id"] for r in manifest["records"]],
                         ["GALE01", "GMSE01"])
        self.assertEqual(skipped, ["GZLE01.gif", "Mario Sunshine.png"])
        path = os.path.join(d, "manifest.json")
        with open(path, "w", encoding="utf-8") as f:
            json.dump(manifest, f)
        loaded = pp.load_manifest(path)
        self.assertEqual(loaded[1]["source"], "gmse01.PNG")
        self.assertEqual(loaded[1]["source_sha256"],
                         sha256_file(os.path.join(d, "gmse01.PNG")))

    def test_duplicate_id_across_extensions_is_rejected(self):
        d = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, d)
        for name in ("GALE01.png", "GALE01.jpg"):
            open(os.path.join(d, name), "wb").close()
        manifest, _ = pp.covers_manifest(d)
        path = os.path.join(d, "manifest.json")
        with open(path, "w", encoding="utf-8") as f:
            json.dump(manifest, f)
        with self.assertRaisesRegex(pp.PackError, "duplicate game_id GALE01"):
            pp.load_manifest(path)


class DockerImageMissing(unittest.TestCase):
    def test_missing_image_names_the_pull_command(self):
        calls = []

        def fake_run(argv, **kwargs):
            calls.append(argv)
            return subprocess.CompletedProcess(argv, 1, b"", b"")

        d = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, d)
        real_run = pp.subprocess.run
        pp.subprocess.run = fake_run
        self.addCleanup(setattr, pp.subprocess, "run", real_run)
        with self.assertRaisesRegex(pp.PackError, "docker pull img@sha256:x"):
            pp.run_gxtexconv([{"game_id": "GALE01"}], d, ("docker", "img@sha256:x"))
        self.assertEqual(calls, [["docker", "image", "inspect", "img@sha256:x"]])


class ImageValidation(unittest.TestCase):
    def run_prepare(self, records):
        manifest = FX.write_manifest(records)
        parsed = pp.load_manifest(manifest)
        staging = tempfile.mkdtemp(prefix="pp_staging_")
        try:
            pp.prepare_images(parsed, FX.dir, staging)
        finally:
            shutil.rmtree(staging, ignore_errors=True)
        return parsed

    def test_missing_art(self):
        with self.assertRaises(pp.PackError) as ctx:
            self.run_prepare([FX.record("GALE01", source="nope.png")])
        self.assertIn("missing art", str(ctx.exception))

    def test_sha_mismatch(self):
        rec = FX.record("GALE01")
        rec["source_sha256"] = "0" * 64
        with self.assertRaises(pp.PackError) as ctx:
            self.run_prepare([rec])
        self.assertIn("SHA-256 mismatch", str(ctx.exception))

    def test_bad_dimensions(self):
        with self.assertRaises(pp.PackError) as ctx:
            self.run_prepare([FX.record("GALE01", source="small.png")])
        self.assertIn("bad dimensions", str(ctx.exception))

    def test_corrupt_image(self):
        with self.assertRaises(pp.PackError) as ctx:
            self.run_prepare([FX.record("GALE01", source="corrupt.png")])
        self.assertIn("corrupt source image", str(ctx.exception))

    def test_dominant_color_computed_deterministically(self):
        parsed1 = self.run_prepare([FX.record("GC6E01")])
        parsed2 = self.run_prepare([FX.record("GC6E01")])
        self.assertEqual(parsed1[0]["dominant_rgb"], parsed2[0]["dominant_rgb"])
        r, g, b = parsed1[0]["dominant_rgb"]
        self.assertAlmostEqual(r, 200, delta=4)
        self.assertAlmostEqual(g, 40, delta=4)
        self.assertAlmostEqual(b, 40, delta=4)

    def test_dominant_color_override_respected(self):
        parsed = self.run_prepare([FX.record("GC6E01",
                                             dominant_rgb=[1, 2, 3])])
        self.assertEqual(parsed[0]["dominant_rgb"], [1, 2, 3])

    def test_focal_crop_geometry(self):
        im = gradient_image(640, 480)
        left = pp.focal_crop(im, [0.0, 0.5])
        right = pp.focal_crop(im, [1.0, 0.5])
        self.assertEqual(left.size, (pp.CONTENT_W, pp.CONTENT_H))
        self.assertEqual(right.size, (pp.CONTENT_W, pp.CONTENT_H))
        # focal at the left edge keeps darker columns than focal at the right
        self.assertLess(sum(left.getpixel((0, 128))),
                        sum(right.getpixel((0, 128))))

    def test_truncated_tpl_is_packerror(self):
        # A garbage tex_off in a short TPL must surface as PackError (the
        # tool's error contract), not a raw struct.error traceback.
        path = os.path.join(FX.dir, "evil.tpl")
        with open(path, "wb") as f:
            f.write(struct.pack(">III I", pp.TPL_MAGIC, 1, 12, 0x1000)
                    + bytes(64))
        with self.assertRaises(pp.PackError) as ctx:
            pp.extract_tpl_payload(path, "GALE01")
        self.assertIn("out of bounds", str(ctx.exception))

    def test_docker_command_pinned_and_offline(self):
        # The docker fallback must reference the immutable digest (never a
        # mutable tag) and must never pull -- provable without any network.
        self.assertRegex(pp.DOCKER_IMAGE, r"@sha256:[0-9a-f]{64}$")
        captured = {}

        def fake_run(argv, **kwargs):
            captured["argv"] = argv

            class R:
                returncode = 0
                stdout = ""
                stderr = ""
            return R()

        staging = tempfile.mkdtemp(prefix="pp_docker_")
        orig = pp.subprocess.run
        try:
            pp.subprocess.run = fake_run
            pp.run_gxtexconv([{"game_id": "GALE01"}], staging,
                             ("docker", pp.DOCKER_IMAGE))
        finally:
            pp.subprocess.run = orig
            shutil.rmtree(staging, ignore_errors=True)
        argv = captured["argv"]
        self.assertIn("--pull=never", argv)
        self.assertIn(pp.DOCKER_IMAGE, argv)
        self.assertFalse(any(":latest" in a for a in argv))

    def test_native_override_lands_in_convert_script(self):
        # --gxtexconv/$GXTEXCONV must be the command the batch actually runs.
        staging = tempfile.mkdtemp(prefix="pp_script_")
        try:
            with self.assertRaises(pp.PackError):  # bogus binary fails loudly
                pp.run_gxtexconv([{"game_id": "GALE01"}], staging,
                                 ("native", "/nonexistent/gxtexconv-x"))
            with open(os.path.join(staging, "convert.sh")) as f:
                script = f.read()
            self.assertIn("/nonexistent/gxtexconv-x", script)
        finally:
            shutil.rmtree(staging, ignore_errors=True)

    def test_mip_chain_math(self):
        total = 0
        w, h = pp.CANVAS_W, pp.CANVAS_H
        for _ in range(pp.MIP_LEVELS):
            total += max(w, 8) * max(h, 8) // 2
            w //= 2
            h //= 2
        self.assertEqual(total, pp.POSTER_BYTES)


def parse_pack(data):
    hdr = struct.unpack(">4sIIIIIIIIHHHHBBH", data[:48])
    fields = {
        "magic": hdr[0], "version": hdr[1], "crc": hdr[2], "count": hdr[3],
        "index_offset": hdr[4], "index_length": hdr[5], "data_offset": hdr[6],
        "file_length": hdr[7], "poster_bytes": hdr[8], "canvas": (hdr[9], hdr[10]),
        "content": (hdr[11], hdr[12]), "mips": hdr[13], "fmt": hdr[14],
    }
    records = []
    for i in range(fields["count"]):
        off = 64 + i * 32
        rec = struct.unpack(">6sBBIII4sHHI", data[off:off + 32])
        records.append({
            "id": rec[0].decode(), "flags": rec[1], "offset": rec[3],
            "length": rec[4], "crc": rec[5], "dom": rec[6],
            "focal": (rec[7], rec[8]),
        })
    return fields, records


def decode_cmpr_average(payload, x0, x1):
    """Average RGB over columns [x0, x1) of the 256x256 LOD0 CMPR level.
    Minimal GX CMPR decode, palette-weighted; good enough for orientation
    and color assertions."""
    def c565(v):
        return ((v >> 11) << 3, ((v >> 5) & 0x3F) << 2, (v & 0x1F) << 3)

    totals = [0, 0, 0]
    count = 0
    tiles_per_row = pp.CANVAS_W // 8
    for ty in range(pp.CANVAS_H // 8):
        for tx in range(tiles_per_row):
            tile = (ty * tiles_per_row + tx) * 32
            for sub in range(4):
                bx = tx * 8 + (sub & 1) * 4
                if not (x0 <= bx < x1):
                    continue
                block = tile + sub * 8
                col0, col1 = struct.unpack(">HH", payload[block:block + 4])
                p0, p1 = c565(col0), c565(col1)
                for ch in range(3):
                    totals[ch] += (p0[ch] + p1[ch]) * 8  # 16 texels, midpoint
                count += 16
    return [t / count for t in totals]


@NEEDS_ENCODER
class PackGeneration(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workdir = tempfile.mkdtemp(prefix="pp_gen_")
        cls.manifest_records = [
            FX.record("GALE01", universal=True, focal=[0.5, 0.3]),
            FX.record("GC6E01"),
            FX.record("GM4E01"),
        ]
        manifest = FX.write_manifest(cls.manifest_records)
        cls.pak_path = os.path.join(cls.workdir, "posters.pak")
        cls.pack = pp.generate(manifest, cls.pak_path, art_root=FX.dir)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.workdir, ignore_errors=True)

    def test_nonexistent_staging_dir_is_created(self):
        staging = os.path.join(self.workdir, "not", "yet", "here")
        out = os.path.join(self.workdir, "staged.pak")
        manifest = FX.write_manifest(self.manifest_records)
        pack = pp.generate(manifest, out, art_root=FX.dir, staging=staging)
        self.assertEqual(pack, self.pack)
        self.assertTrue(os.path.isdir(staging))

    def test_deterministic_rebuild(self):
        out2 = os.path.join(self.workdir, "again.pak")
        manifest = FX.write_manifest(self.manifest_records)
        pack2 = pp.generate(manifest, out2, art_root=FX.dir)
        self.assertEqual(hashlib.sha256(self.pack).hexdigest(),
                         hashlib.sha256(pack2).hexdigest())

    def test_manifest_order_irrelevant(self):
        out3 = os.path.join(self.workdir, "shuffled.pak")
        manifest = FX.write_manifest(list(reversed(self.manifest_records)))
        pack3 = pp.generate(manifest, out3, art_root=FX.dir)
        self.assertEqual(self.pack, pack3)

    def test_header_and_index_structure(self):
        fields, records = parse_pack(self.pack)
        self.assertEqual(fields["magic"], b"SWPK")
        self.assertEqual(fields["version"], 1)
        self.assertEqual(fields["count"], 3)
        self.assertEqual(fields["index_offset"], 64)
        self.assertEqual(fields["index_length"], 96)
        self.assertEqual(fields["data_offset"], 160)
        self.assertEqual(fields["file_length"], len(self.pack))
        self.assertEqual(fields["poster_bytes"], pp.POSTER_BYTES)
        self.assertEqual(fields["canvas"], (256, 256))
        self.assertEqual(fields["content"], (192, 256))
        self.assertEqual(fields["mips"], 5)
        self.assertEqual(fields["fmt"], 14)

        ids = [r["id"] for r in records]
        self.assertEqual(ids, sorted(ids))
        self.assertEqual(ids, ["GALE01", "GC6E01", "GM4E01"])
        self.assertEqual(records[0]["flags"], 1)  # universal
        self.assertEqual(records[1]["flags"], 0)
        self.assertEqual(records[0]["focal"],
                         (round(0.5 * 65535), round(0.3 * 65535)))
        for i, rec in enumerate(records):
            self.assertEqual(rec["offset"], 160 + i * pp.POSTER_BYTES)
            self.assertEqual(rec["length"], pp.POSTER_BYTES)
            self.assertEqual(rec["offset"] % 32, 0)
            payload = self.pack[rec["offset"]:rec["offset"] + rec["length"]]
            self.assertEqual(zlib.crc32(payload), rec["crc"])

        hdr = bytearray(self.pack[:64])
        hdr[8:12] = b"\x00" * 4
        crc = zlib.crc32(bytes(hdr) + self.pack[64:160])
        self.assertEqual(crc, fields["crc"])

    def test_provenance_sidecar(self):
        with open(self.pak_path + ".provenance.json", encoding="utf-8") as f:
            doc = json.load(f)
        self.assertEqual(len(doc["records"]), 3)
        for rec in doc["records"]:
            self.assertTrue(rec["note"])
            self.assertRegex(rec["poster_crc32"], r"^[0-9a-f]{8}$")
            self.assertRegex(rec["source_sha256"], r"^[0-9a-f]{64}$")
        # The exact converter identity must be recorded: immutable image
        # digest for docker, resolved path + binary SHA-256 for native.
        conv = doc["toolchain"]["gxtexconv"]
        if conv["mode"] == "docker":
            self.assertRegex(conv["image_digest"], r"@sha256:[0-9a-f]{64}$")
            self.assertEqual(conv["pull_policy"], "never")
        else:
            self.assertTrue(os.path.isabs(conv["path"]))
            self.assertRegex(conv["sha256"], r"^[0-9a-f]{64}$")

    def test_cmpr_content_orientation_and_color(self):
        fields, records = parse_pack(self.pack)
        # GALE01 fixture is a left-dark -> right-bright red-channel gradient.
        rec = records[0]
        payload = self.pack[rec["offset"]:rec["offset"] + rec["length"]]
        left = decode_cmpr_average(payload, 0, 64)
        right = decode_cmpr_average(payload, 128, 192)
        self.assertLess(left[0] + 40, right[0])
        # GC6E01 is flat (200,40,40); decoded average must sit near it.
        rec = records[1]
        payload = self.pack[rec["offset"]:rec["offset"] + rec["length"]]
        avg = decode_cmpr_average(payload, 0, 192)
        self.assertAlmostEqual(avg[0], 200, delta=16)
        self.assertAlmostEqual(avg[1], 40, delta=16)
        self.assertAlmostEqual(avg[2], 40, delta=16)


if __name__ == "__main__":
    unittest.main(verbosity=2)
