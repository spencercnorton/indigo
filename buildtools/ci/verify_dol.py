#!/usr/bin/env python3
"""Check a built DOL and record where it came from.

The DOL must be well formed (every section inside the file, the entry point
inside a text section), must name the commit it was built from exactly once
(System > System Information shows it), and must fit the size budget. The
provenance record (--out) names the commit, the toolchain image, the DOL's
SHA-256 and the CI run, so a zip can be traced back to how it was made.

--packed checks ipl.dol, the file the card gets: the same DOL compressed by
cube/packer. It must unpack to exactly the DOL checked above, in the one .xz
format the console's decoder reads, without the unpacker overwriting itself.

usage: verify_dol.py DOL --revision <sha> --short <short sha> --toolchain <image@sha256:...> [--packed IPL] [--out JSON]
"""

from __future__ import annotations

import argparse
import hashlib
import json
import lzma
import os
import re
import struct
import sys
from pathlib import Path

# A tripwire, not a hardware limit: the DOL is about 5.2 MiB, 1.3 MiB of it
# the menu music's MP3 and 1.1 MiB the libiconv and uchardet tables upstream
# Swiss links since r2119 for file names in other encodings. Growth past this
# should be a decision someone made, so raise it on purpose when that happens.
MAX_DOL_BYTES = 6 * 1024 * 1024

# cube/packer: main.c unpacks to EXECUTABLE_ADDR and jumps there, copying the
# loader's arguments into the payload's "_arg" and "_env" slots; crt0.S runs
# it on a stack at 16 MiB. Its XZ Embedded (xz/xz_config.h) reads one stream,
# a CRC32 check and PowerPC BCJ + LZMA2, nothing else.
UNPACK_TO = 0x80003100
UNPACKER_STACK = 0x81000000
XZ_MAGIC = b"\xfd7zXZ\0"
XZ_CRC32 = b"\0\x01"
XZ_FILTERS = [0x05, 0x21]  # PowerPC BCJ, LZMA2


class Invalid(Exception):
    pass


def sections(data: bytes) -> list[dict[str, int | str]]:
    if len(data) < 0x100:
        raise Invalid(f"shorter than the 0x100-byte DOL header ({len(data)} bytes)")
    offsets = struct.unpack_from(">18I", data, 0x00)
    addresses = struct.unpack_from(">18I", data, 0x48)
    sizes = struct.unpack_from(">18I", data, 0x90)
    found: list[dict[str, int | str]] = []
    for index, (offset, address, size) in enumerate(zip(offsets, addresses, sizes)):
        if size == 0:
            continue
        kind, number = ("text", index) if index < 7 else ("data", index - 7)
        if offset < 0x100 or offset + size > len(data):
            raise Invalid(f"{kind} section {number} lies outside the file (offset {offset:#x}, {size} bytes)")
        if address == 0:
            raise Invalid(f"{kind} section {number} has no load address")
        found.append({"kind": kind, "index": number, "offset": offset, "address": address, "bytes": size})
    if not found:
        raise Invalid("no loadable sections")
    return found


def check(data: bytes, short: str) -> dict[str, object]:
    loaded = sections(data)
    bss_address, bss_size, entry = struct.unpack_from(">3I", data, 0xD8)
    if not any(s["kind"] == "text" and s["address"] <= entry < s["address"] + s["bytes"] for s in loaded):
        raise Invalid(f"entry point {entry:#010x} is outside every text section")
    if bss_size and not bss_address:
        raise Invalid("BSS has a size but no address")
    if len(data) > MAX_DOL_BYTES:
        raise Invalid(f"{len(data)} bytes is over the {MAX_DOL_BYTES}-byte budget (verify_dol.py)")
    if not re.fullmatch(r"[0-9a-f]{7,40}", short):
        raise Invalid(f"not a short commit hash: {short!r}")
    needle = b"\0" + short.encode() + b"\0"
    hits = [s["offset"] + i + 1 for s in loaded
            for i in _find_all(data[s["offset"]:s["offset"] + s["bytes"]], needle)]
    if len(hits) != 1:
        raise Invalid(f"the DOL names commit {short} {len(hits)} times inside its sections; it must be exactly once")
    return {"entry_point": f"{entry:#010x}", "sections": len(loaded), "bss_bytes": bss_size,
            "revision_offset": hits[0]}


def _find_all(haystack: bytes, needle: bytes):
    start = haystack.find(needle)
    while start >= 0:
        yield start
        start = haystack.find(needle, start + 1)


def memory_image(data: bytes) -> tuple[int, bytes]:
    """The memory a loader fills from a DOL: its lowest section to its highest end, gaps zero."""
    loaded = sections(data)
    low = min(s["address"] for s in loaded)
    image = bytearray(max(s["address"] + s["bytes"] for s in loaded) - low)
    for s in loaded:
        image[s["address"] - low:s["address"] - low + s["bytes"]] = data[s["offset"]:s["offset"] + s["bytes"]]
    return low, bytes(image)


def _vli(data: bytes, at: int) -> tuple[int, int]:
    value = 0
    for shift in range(0, 63, 7):
        value |= (data[at] & 0x7F) << shift
        at += 1
        if not data[at - 1] & 0x80:
            return value, at
    raise Invalid("a malformed number in the .xz block header")


def check_packed(packed: bytes, dol: bytes) -> dict[str, object]:
    loaded = sections(packed)
    bss_address, bss_size, entry = struct.unpack_from(">3I", packed, 0xD8)
    if not any(s["kind"] == "text" and s["address"] <= entry < s["address"] + s["bytes"] for s in loaded):
        raise Invalid(f"ipl.dol's entry point {entry:#010x} is outside every text section")
    low, payload = memory_image(dol)
    if low != UNPACK_TO or struct.unpack_from(">I", dol, 0xE0)[0] != UNPACK_TO:
        raise Invalid(f"the DOL must start and enter at {UNPACK_TO:#010x}, where the unpacker jumps")
    if payload[4:8] != b"_arg" or payload[36:40] != b"_env":
        raise Invalid("the DOL has no _arg and _env slots for the unpacker to fill")
    start = packed.find(XZ_MAGIC)
    if not any(s["kind"] == "data" and s["offset"] <= start < s["offset"] + s["bytes"] for s in loaded):
        raise Invalid("ipl.dol holds no .xz stream in a data section")
    if packed[start + 6:start + 8] != XZ_CRC32:
        raise Invalid("the .xz check is not CRC32, the only one the console's decoder has")
    flags, at = packed[start + 13], start + 14
    for present in (0x40, 0x80):  # compressed, uncompressed size
        if flags & present:
            _, at = _vli(packed, at)
    filters = []
    for _ in range((flags & 3) + 1):
        filter_id, at = _vli(packed, at)
        size, at = _vli(packed, at)
        filters.append((filter_id, size))
        at += size
    if [filter_id for filter_id, _ in filters] != XZ_FILTERS or filters[0][1] != 0:
        raise Invalid(f"the .xz filters are {filters}: the console's decoder reads PowerPC BCJ, no offset, then LZMA2")
    decoder = lzma.LZMADecompressor(format=lzma.FORMAT_XZ)
    try:
        unpacked = decoder.decompress(packed[start:])
    except lzma.LZMAError as error:
        raise Invalid(f"the .xz stream does not decode: {error}") from None
    if not decoder.eof:
        raise Invalid("the .xz stream is cut short")
    if unpacked != payload:
        raise Invalid("ipl.dol does not unpack to the DOL this build produced")
    lowest = min(s["address"] for s in loaded)
    highest = max([s["address"] + s["bytes"] for s in loaded] + [bss_address + bss_size])
    if lowest < UNPACK_TO + len(payload):
        raise Invalid(f"the unpacker at {lowest:#010x} lies inside the {len(payload)} bytes it unpacks to {UNPACK_TO:#010x}")
    if highest > UNPACKER_STACK - 0x10000:  # 64 KiB of stack is far more than it uses
        raise Invalid(f"the unpacker reaches {highest:#010x}, into its stack below {UNPACKER_STACK:#010x}")
    return {"bytes": len(packed), "sha256": hashlib.sha256(packed).hexdigest(),
            "xz_bytes": len(packed) - start - len(decoder.unused_data)}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("dol", type=Path)
    parser.add_argument("--revision", required=True, help="the full commit hash the DOL was built from")
    parser.add_argument("--short", required=True, help="git rev-parse --short of that commit")
    parser.add_argument("--toolchain", required=True, help="the digest-pinned image it was built in")
    parser.add_argument("--packed", type=Path, help="ipl.dol: the DOL compressed by cube/packer")
    parser.add_argument("--out", type=Path)
    args = parser.parse_args(argv)
    try:
        if not args.revision.startswith(args.short):
            raise Invalid(f"{args.short} is not the start of {args.revision}")
        if "@sha256:" not in args.toolchain:
            raise Invalid(f"toolchain is not pinned by digest: {args.toolchain}")
        for path in filter(None, (args.dol, args.packed)):
            if path.is_symlink() or not path.is_file():
                raise Invalid(f"not a regular file: {path}")
        data = args.dol.read_bytes()
        structure = check(data, args.short)
        packed = check_packed(args.packed.read_bytes(), data) if args.packed else None
    except (Invalid, OSError, struct.error, IndexError) as error:
        print(f"::error::DOL check failed: {error}", file=sys.stderr)
        return 1
    record = {
        "schema": "indigo.dol-provenance.v1",
        "revision": args.revision,
        "toolchain": args.toolchain,
        "dol": {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(), **structure},
        "run": {key.lower(): os.environ.get(key) for key in
                ("GITHUB_REPOSITORY", "GITHUB_RUN_ID", "GITHUB_RUN_ATTEMPT", "GITHUB_EVENT_NAME", "GITHUB_REF")},
    }
    if packed:
        record["ipl_dol"] = packed
    if args.out:
        args.out.write_text(json.dumps(record, indent=2, sort_keys=True) + "\n")
    print(f"DOL OK: {len(data)} bytes, sha256 {record['dol']['sha256']}, names {args.short}")
    if packed:
        print(f"ipl.dol OK: {packed['bytes']} bytes, sha256 {packed['sha256']}, unpacks to that DOL")
    return 0


if __name__ == "__main__":
    sys.exit(main())
