#!/usr/bin/env python3
"""poster_pack.py -- offline generator for Swiss UI poster packs.

Builds /swiss/ui/posters.pak from a local JSON manifest and user-supplied
source art. Fully offline: never scrapes, downloads, or calls any network
service. Output is byte-for-byte deterministic for the same inputs and
toolchain (gxtexconv + Pillow versions are recorded in the provenance
sidecar; they are inputs to that determinism contract).

Binary format: docs/ui-redesign/POSTER_PACK_FORMAT.md (version 1).

Texture geometry: GX hardware (and gxtexconv) require power-of-two
dimensions for mipmapped textures, so the 192x256 poster content is
composited onto a 256x256 canvas (right band edge-extended to keep deep
mip levels from bleeding) and encoded as GX_TF_CMPR with a 5-level mip
chain (256..16). The runtime samples s in [0, 192/256].

Usage:
  poster_pack.py --covers DIR --out posters.pak
  poster_pack.py --manifest manifest.json --out posters.pak [--art-root DIR]
                 [--gxtexconv CMD] [--staging DIR]

--covers takes a folder of front-cover images named by game ID (GMSE01.png,
GALE01.jpg; at least 192x256, cropped to 3:4) and needs Pillow plus either
gxtexconv or Docker.

gxtexconv resolution order: --gxtexconv / $GXTEXCONV, `gxtexconv` on PATH,
then the repo's libogc2 Docker image (one container run for the whole batch).
"""

import argparse
import hashlib
import json
import os
import re
import shlex
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

MAGIC = b"SWPK"
VERSION = 1
HEADER_SIZE = 64
RECORD_SIZE = 32
INDEX_OFFSET = HEADER_SIZE
MAX_RECORDS = 1024

CANVAS_W, CANVAS_H = 256, 256
CONTENT_W, CONTENT_H = 192, 256
MIP_LEVELS = 5          # LOD0..LOD4: 256,128,64,32,16
TEX_FORMAT = 14         # GX_TF_CMPR
POSTER_BYTES = 43648    # sum of CMPR mip levels 256^2/2 + 128^2/2 + ... + 16^2/2

FLAG_UNIVERSAL = 0x01

TPL_MAGIC = 0x0020AF30
# Immutable digest pin: the generator must be byte-deterministic and fully
# offline, so the docker fallback never resolves a mutable tag and never
# pulls (docker run --pull=never). The digest is recorded in provenance.
DOCKER_IMAGE = ("ghcr.io/extremscorner/libogc2@sha256:"
                "903b442dfd18cab00b5958726f70b17d95b0cf40c15d01b11e841825489dbe3d")

GAME_ID_RE = re.compile(r"[A-Z0-9]{6}")
COVER_NAME_RE = re.compile(r"([A-Za-z0-9]{6})\.(?:png|jpe?g)", re.IGNORECASE)
SHA256_RE = re.compile(r"[0-9a-f]{64}")
MANIFEST_RECORD_KEYS = {
    "game_id", "source", "universal", "focal", "dominant_rgb",
    "source_sha256", "note",
}


class PackError(Exception):
    pass


def _require(cond, msg):
    if not cond:
        raise PackError(msg)


def load_manifest(path):
    """Parse and strictly validate the manifest. Returns records sorted by ID."""
    try:
        with open(path, "r", encoding="utf-8") as f:
            doc = json.load(f)
    except (OSError, json.JSONDecodeError) as e:
        raise PackError(f"manifest unreadable: {e}")

    _require(isinstance(doc, dict), "manifest root must be an object")
    _require(doc.get("version") == 1, "manifest version must be 1")
    _require(set(doc) <= {"version", "records"},
             f"unknown manifest keys: {sorted(set(doc) - {'version', 'records'})}")
    records = doc.get("records")
    _require(isinstance(records, list) and records, "manifest needs a non-empty records list")
    _require(len(records) <= MAX_RECORDS, f"pack overflow: {len(records)} records > {MAX_RECORDS}")

    seen = set()
    universal_prefixes = {}
    out = []
    for i, rec in enumerate(records):
        where = f"records[{i}]"
        _require(isinstance(rec, dict), f"{where}: must be an object")
        unknown = set(rec) - MANIFEST_RECORD_KEYS
        _require(not unknown, f"{where}: unknown keys {sorted(unknown)}")
        for key in ("game_id", "source", "universal", "source_sha256", "note"):
            _require(key in rec, f"{where}: missing required key '{key}'")

        gid = rec["game_id"]
        _require(isinstance(gid, str) and GAME_ID_RE.fullmatch(gid),
                 f"{where}: invalid game_id {gid!r} (need exactly six of A-Z0-9)")
        _require(gid not in seen, f"duplicate game_id {gid}")
        seen.add(gid)

        _require(isinstance(rec["universal"], bool), f"{gid}: universal must be true/false")
        if rec["universal"]:
            prefix = gid[:4]
            if prefix in universal_prefixes:
                raise PackError(
                    f"{gid}: universal 4-char prefix {prefix!r} already "
                    f"claimed by {universal_prefixes[prefix]} -- "
                    f"ambiguous fallback")
            universal_prefixes[prefix] = gid

        _require(isinstance(rec["source"], str) and rec["source"], f"{gid}: source must be a path")
        sha = rec["source_sha256"]
        _require(isinstance(sha, str) and SHA256_RE.fullmatch(sha.lower()),
                 f"{gid}: source_sha256 must be 64 hex chars")
        _require(isinstance(rec["note"], str) and rec["note"].strip(),
                 f"{gid}: provenance note is required")

        focal = rec.get("focal", [0.5, 0.5])
        _require(isinstance(focal, list) and len(focal) == 2
                 and all(isinstance(v, (int, float)) and not isinstance(v, bool)
                         and 0.0 <= v <= 1.0 for v in focal),
                 f"{gid}: focal must be [x, y] in 0..1")

        dom = rec.get("dominant_rgb")
        if dom is not None:
            _require(isinstance(dom, list) and len(dom) == 3
                     and all(isinstance(v, int) and not isinstance(v, bool)
                             and 0 <= v <= 255 for v in dom),
                     f"{gid}: dominant_rgb must be [r, g, b] 0..255")

        out.append({
            "game_id": gid,
            "source": rec["source"],
            "universal": rec["universal"],
            "focal": [float(focal[0]), float(focal[1])],
            "dominant_rgb": dom,
            "source_sha256": sha.lower(),
            "note": rec["note"],
        })

    out.sort(key=lambda r: r["game_id"])
    return out


def covers_manifest(covers_dir):
    """--covers: a version-1 manifest with one record per GAMEID.png/.jpg in
    covers_dir. Returns (manifest, skipped) where skipped lists the visible
    files whose names are not a game ID."""
    records, skipped = [], []
    for name in sorted(os.listdir(covers_dir)):
        m = COVER_NAME_RE.fullmatch(name)
        if not m:
            if not name.startswith("."):
                skipped.append(name)
            continue
        with open(os.path.join(covers_dir, name), "rb") as f:
            digest = hashlib.sha256(f.read()).hexdigest()
        records.append({"game_id": m.group(1).upper(), "source": name,
                        "universal": False, "source_sha256": digest,
                        "note": f"covers folder: {name}"})
    return {"version": 1, "records": records}, skipped


def focal_crop(im, focal):
    """Largest 3:4 window inside im, positioned so the focal point stays
    proportionally placed, then resized to CONTENT_W x CONTENT_H."""
    w, h = im.size
    if w * CONTENT_H > h * CONTENT_W:      # wider than 3:4 -- full height
        crop_h = h
        crop_w = h * CONTENT_W // CONTENT_H
    else:                                   # taller than 3:4 -- full width
        crop_w = w
        crop_h = w * CONTENT_H // CONTENT_W
    left = min(max(round(focal[0] * w - crop_w / 2), 0), w - crop_w)
    top = min(max(round(focal[1] * h - crop_h / 2), 0), h - crop_h)
    from PIL import Image
    return im.crop((left, top, left + crop_w, top + crop_h)).resize(
        (CONTENT_W, CONTENT_H), Image.LANCZOS)


def dominant_color(im):
    """Deterministic dominant color: BOX-filter average of the poster content."""
    from PIL import Image
    px = im.resize((1, 1), Image.BOX).getpixel((0, 0))
    return [px[0], px[1], px[2]]


def compose_canvas(content):
    """Paste 192x256 content onto a 256x256 canvas; edge-extend the right band
    so deep mip levels don't bleed a foreign color into the poster edge."""
    from PIL import Image
    canvas = Image.new("RGB", (CANVAS_W, CANVAS_H))
    canvas.paste(content, (0, 0))
    band = content.crop((CONTENT_W - 1, 0, CONTENT_W, CONTENT_H)).resize(
        (CANVAS_W - CONTENT_W, CANVAS_H), Image.NEAREST)
    canvas.paste(band, (CONTENT_W, 0))
    return canvas


def prepare_images(records, art_root, staging):
    """Validate art, crop/compose, write staging PNG + .scf per record.
    Fills in computed dominant_rgb. Hard-errors on any bad input."""
    from PIL import Image, UnidentifiedImageError
    for rec in records:
        gid = rec["game_id"]
        src = os.path.join(art_root, rec["source"])
        _require(os.path.isfile(src), f"{gid}: missing art {src}")
        digest = hashlib.sha256()
        with open(src, "rb") as f:
            for chunk in iter(lambda: f.read(1 << 20), b""):
                digest.update(chunk)
        _require(digest.hexdigest() == rec["source_sha256"],
                 f"{gid}: source SHA-256 mismatch for {src} "
                 f"(manifest {rec['source_sha256'][:12]}…, file {digest.hexdigest()[:12]}…)")
        try:
            im = Image.open(src)
            im.load()
        except (UnidentifiedImageError, OSError) as e:
            raise PackError(f"{gid}: corrupt source image {src}: {e}")
        im = im.convert("RGB")
        _require(im.width >= CONTENT_W and im.height >= CONTENT_H,
                 f"{gid}: bad dimensions {im.width}x{im.height} "
                 f"(need at least {CONTENT_W}x{CONTENT_H}, no upscaling)")
        content = focal_crop(im, rec["focal"])
        if rec["dominant_rgb"] is None:
            rec["dominant_rgb"] = dominant_color(content)
        compose_canvas(content).save(os.path.join(staging, f"{gid}.png"))
        with open(os.path.join(staging, f"{gid}.scf"), "w", encoding="utf-8",
                  newline="\n") as f:
            f.write(f'<filepath="{gid}.png" id="poster" colfmt={TEX_FORMAT} '
                    f'mipmap=yes minlod=0 maxlod={MIP_LEVELS - 1} />\n')


def resolve_gxtexconv(explicit):
    """Return ('native', cmd) or ('docker', image). Never touches the network
    beyond whatever a locally-cached docker image implies; the image is the
    same one the repo build already requires."""
    cmd = explicit or os.environ.get("GXTEXCONV")
    if cmd:
        return ("native", cmd)
    if shutil.which("gxtexconv"):
        return ("native", "gxtexconv")
    if shutil.which("docker"):
        return ("docker", DOCKER_IMAGE)
    raise PackError("gxtexconv not found: install devkitPro, set $GXTEXCONV, "
                    "or make docker available for the libogc2 image")


def run_gxtexconv(records, staging, resolver):
    """Convert every staged PNG to TPL. One docker container for the whole
    batch when dockerized; plain loop when native (using the RESOLVED
    command, so --gxtexconv/$GXTEXCONV overrides are honored)."""
    mode, target = resolver
    cmd = "gxtexconv" if mode == "docker" else shlex.quote(target)
    script = os.path.join(staging, "convert.sh")
    with open(script, "w", encoding="utf-8", newline="\n") as f:
        f.write("set -e\n")
        for rec in records:
            gid = rec["game_id"]
            f.write(f"{cmd} -s {gid}.scf -o {gid}.tpl >/dev/null\n")
    if mode == "docker":
        present = subprocess.run(["docker", "image", "inspect", target],
                                 capture_output=True)
        if present.returncode != 0:
            raise PackError("the libogc2 Docker image is not downloaded yet "
                            f"(this tool never pulls); run once: docker pull {target}")
        argv = ["docker", "run", "--rm", "--pull=never",
                "-v", f"{staging}:/work", "-w", "/work",
                target, "sh", "convert.sh"]
    else:
        argv = ["sh", "convert.sh"]
    proc = subprocess.run(argv, cwd=staging, capture_output=True, text=True)
    if proc.returncode != 0:
        raise PackError(f"gxtexconv batch failed:\n{proc.stdout}\n{proc.stderr}")


def extract_tpl_payload(path, gid):
    """Parse a single-texture TPL and return the raw CMPR mip payload,
    validating geometry and exact payload length."""
    try:
        with open(path, "rb") as f:
            data = f.read()
    except OSError as e:
        raise PackError(f"{gid}: TPL unreadable: {e}")
    _require(len(data) > HEADER_SIZE, f"{gid}: TPL truncated ({len(data)} bytes)")
    magic, ntex, _ = struct.unpack(">III", data[:12])
    _require(magic == TPL_MAGIC and ntex == 1, f"{gid}: unexpected TPL structure")
    tex_off = struct.unpack(">I", data[12:16])[0]
    _require(tex_off + 12 <= len(data),
             f"{gid}: TPL texture header offset {tex_off} out of bounds")
    height, width, fmt, data_off = struct.unpack(">HHII", data[tex_off:tex_off + 12])
    _require((width, height, fmt) == (CANVAS_W, CANVAS_H, TEX_FORMAT),
             f"{gid}: TPL is {width}x{height} fmt {fmt}, expected "
             f"{CANVAS_W}x{CANVAS_H} fmt {TEX_FORMAT}")
    payload = data[data_off:]
    _require(len(payload) == POSTER_BYTES,
             f"{gid}: TPL payload {len(payload)} bytes, expected {POSTER_BYTES} "
             f"(mip chain missing? gxtexconv needs power-of-two input)")
    return payload


def build_pack(records, staging):
    """Assemble the deterministic pak bytes from converted payloads."""
    count = len(records)
    data_offset = HEADER_SIZE + count * RECORD_SIZE   # both 32-multiples
    file_length = data_offset + count * POSTER_BYTES
    _require(file_length < 2**32, "pack overflow: file length exceeds 32 bits")

    index = bytearray()
    payloads = []
    for i, rec in enumerate(records):
        payload = extract_tpl_payload(
            os.path.join(staging, f"{rec['game_id']}.tpl"), rec["game_id"])
        payloads.append(payload)
        crc = zlib.crc32(payload)
        rec["poster_crc32"] = f"{crc:08x}"
        dom = rec["dominant_rgb"]
        index += struct.pack(
            ">6sBBIII4sHHI",
            rec["game_id"].encode("ascii"),
            FLAG_UNIVERSAL if rec["universal"] else 0,
            0,
            data_offset + i * POSTER_BYTES,
            POSTER_BYTES,
            crc,
            bytes([dom[0], dom[1], dom[2], 0xFF]),
            round(rec["focal"][0] * 65535),
            round(rec["focal"][1] * 65535),
            0,
        )

    def header(crc_field):
        return struct.pack(
            ">4sIIIIIIIIHHHHBBH", MAGIC, VERSION, crc_field, count,
            INDEX_OFFSET, count * RECORD_SIZE, data_offset, file_length,
            POSTER_BYTES, CANVAS_W, CANVAS_H, CONTENT_W, CONTENT_H,
            MIP_LEVELS, TEX_FORMAT, 0,
        ) + bytes(16)

    crc = zlib.crc32(header(0) + index)
    return header(crc) + bytes(index) + b"".join(payloads)


def toolchain_info(resolver):
    """Record the exact converter identity: the immutable image digest for
    docker, or the resolved executable path + SHA-256 (+ version banner if
    printed) for a native binary."""
    mode, target = resolver
    try:
        from PIL import __version__ as pillow_version
    except ImportError:
        pillow_version = "unknown"
    info = {"mode": mode}
    if mode == "docker":
        info["image_digest"] = target
        info["pull_policy"] = "never"
    else:
        resolved = shutil.which(target) or target
        info["path"] = resolved
        try:
            with open(resolved, "rb") as f:
                info["sha256"] = hashlib.sha256(f.read()).hexdigest()
        except OSError:
            info["sha256"] = "unresolved"
        try:
            proc = subprocess.run([resolved], capture_output=True, text=True,
                                  timeout=10)
            m = re.search(r"v\d+(?:\.\d+)+", proc.stdout + proc.stderr)
            info["version"] = m.group(0) if m else "unknown"
        except (OSError, subprocess.TimeoutExpired):
            info["version"] = "unknown"
    return {
        "gxtexconv": info,
        "pillow": pillow_version,
    }


def write_provenance(records, out_path, resolver):
    doc = {
        "pack": os.path.basename(out_path),
        "format_version": VERSION,
        "toolchain": toolchain_info(resolver),
        "records": [
            {
                "game_id": r["game_id"],
                "source": r["source"],
                "source_sha256": r["source_sha256"],
                "note": r["note"],
                "universal": r["universal"],
                "focal": r["focal"],
                "dominant_rgb": r["dominant_rgb"],
                "poster_crc32": r["poster_crc32"],
            }
            for r in records
        ],
    }
    with open(out_path + ".provenance.json", "w", encoding="utf-8", newline="\n") as f:
        json.dump(doc, f, indent=2, sort_keys=True)
        f.write("\n")


def generate(manifest_path, out_path, art_root=None, gxtexconv=None, staging=None):
    """Full pipeline. Returns the pak bytes (also written to out_path)."""
    records = load_manifest(manifest_path)
    art_root = art_root or os.path.dirname(os.path.abspath(manifest_path))
    resolver = resolve_gxtexconv(gxtexconv)
    own_staging = staging is None
    staging = staging or tempfile.mkdtemp(prefix="poster_pack_")
    staging = os.path.abspath(staging)  # docker -v needs an absolute path
    os.makedirs(staging, exist_ok=True)
    try:
        prepare_images(records, art_root, staging)
        run_gxtexconv(records, staging, resolver)
        pack = build_pack(records, staging)
    finally:
        if own_staging:
            shutil.rmtree(staging, ignore_errors=True)
    with open(out_path, "wb") as f:
        f.write(pack)
    write_provenance(records, out_path, resolver)
    return pack


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    source = ap.add_mutually_exclusive_group(required=True)
    source.add_argument("--manifest")
    source.add_argument("--covers", help="folder of cover images named by game "
                                         "ID, e.g. GMSE01.png or GALE01.jpg")
    ap.add_argument("--out", required=True)
    ap.add_argument("--art-root", help="base dir for manifest source paths "
                                       "(default: manifest's directory)")
    ap.add_argument("--gxtexconv", help="gxtexconv command (default: $GXTEXCONV, "
                                        "PATH, then libogc2 docker image)")
    ap.add_argument("--staging", help="keep intermediates in this directory")
    args = ap.parse_args(argv)
    try:
        if args.covers:
            manifest, skipped = covers_manifest(args.covers)
            for name in skipped:
                print(f"poster_pack: skipped {name} (name it GAMEID.png or "
                      f"GAMEID.jpg)", file=sys.stderr)
            _require(manifest["records"],
                     f"no GAMEID.png/.jpg cover images in {args.covers}")
            with tempfile.TemporaryDirectory(prefix="poster_covers_") as tmp:
                manifest_path = os.path.join(tmp, "manifest.json")
                with open(manifest_path, "w", encoding="utf-8") as f:
                    json.dump(manifest, f)
                pack = generate(manifest_path, args.out, args.covers,
                                args.gxtexconv, args.staging)
        else:
            pack = generate(args.manifest, args.out, args.art_root,
                            args.gxtexconv, args.staging)
    except PackError as e:
        print(f"poster_pack: error: {e}", file=sys.stderr)
        return 2
    print(f"poster_pack: wrote {args.out} "
          f"({len(pack)} bytes, sha256 {hashlib.sha256(pack).hexdigest()})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
