#!/usr/bin/env python3
"""The DOL, package and workflow checks catch what they are for."""

import contextlib
import io
import struct
import tempfile
import unittest
import zipfile
from pathlib import Path

import check_package
import check_workflows
import verify_dol

SHORT = "0a2edf3"
FULL = SHORT + "7" * 33
IMAGE = "example.invalid/libogc2@sha256:" + "1" * 64


def dol(revisions: int = 1, entry: int = 0x80003100, text_offset: int = 0x100) -> bytes:
    data = bytearray(0x200)
    struct.pack_into(">I", data, 0x00, text_offset)
    struct.pack_into(">I", data, 0x48, 0x80003100)
    struct.pack_into(">I", data, 0x90, 0x100)
    struct.pack_into(">I", data, 0xE0, entry)
    for n in range(revisions):
        at = 0x120 + n * 16
        data[at:at + 9] = b"\0" + SHORT.encode() + b"\0"
    return bytes(data)


class Dol(unittest.TestCase):
    def test_a_good_dol(self):
        self.assertEqual(verify_dol.check(dol(), SHORT)["sections"], 1)

    def test_the_commit_must_appear_exactly_once(self):
        for count in (0, 2):
            with self.assertRaises(verify_dol.Invalid):
                verify_dol.check(dol(revisions=count), SHORT)

    def test_structure(self):
        for bad in (dol(entry=0x90000000), dol(text_offset=0x180), dol()[:0x80]):
            with self.assertRaises((verify_dol.Invalid, struct.error)):
                verify_dol.check(bad, SHORT)

    def test_size_budget(self):
        with self.assertRaises(verify_dol.Invalid):
            verify_dol.check(dol() + bytes(verify_dol.MAX_DOL_BYTES), SHORT)

    def test_cli_refuses_a_mismatched_or_unpinned_build(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "swiss.dol"
            path.write_bytes(dol())

            def cli(*args: str) -> int:
                # Quiet: an ::error:: line in a CI log becomes an annotation.
                with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                    return verify_dol.main([str(path), *args])

            self.assertEqual(cli("--revision", FULL, "--short", SHORT, "--toolchain", IMAGE), 0)
            self.assertEqual(cli("--revision", "f" * 40, "--short", SHORT, "--toolchain", IMAGE), 1)
            self.assertEqual(cli("--revision", FULL, "--short", SHORT, "--toolchain", "libogc2:latest"), 1)


class Package(unittest.TestCase):
    def build(self, directory: Path, extra: str | None = None, drop: str | None = None,
              ipl: bytes | None = None) -> Path:
        path = directory / "Indigo-v9.9.9.zip"
        with zipfile.ZipFile(path, "w") as archive:
            for name in sorted(check_package.LAYOUT | ({extra} if extra else set())):
                if name == drop:
                    continue
                if name.endswith("/"):
                    archive.writestr(name, b"")
                elif name == "ipl.dol":
                    archive.writestr(name, ipl if ipl is not None else dol())
                elif name == "swiss/patches/apploader.img":
                    archive.writestr(name, b"\0\0*indigo-v9.9.9\0")
                elif name == "Indigo-README.txt":
                    archive.writestr(name, "Indigo v9.9.9 - an unofficial fork\n")
                else:
                    archive.writestr(name, b"text")
        return path

    def test_the_layout_passes(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual(check_package.problems(self.build(Path(directory)), dol(), "v9.9.9"), [])

    def test_extra_missing_and_foreign_files_fail(self):
        with tempfile.TemporaryDirectory() as directory:
            here = Path(directory)
            for case in ({"extra": ".DS_Store"}, {"extra": "games/demo.iso"}, {"drop": "swiss/ui/"},
                         {"drop": "swiss/patches/apploader.img"}, {"ipl": dol(revisions=2)}):
                self.assertNotEqual(check_package.problems(self.build(here, **case), dol(), "v9.9.9"),
                                    [], case)

    def test_the_version_must_match(self):
        with tempfile.TemporaryDirectory() as directory:
            self.assertEqual(len(check_package.problems(self.build(Path(directory)), dol(), "v1.0.0")), 2)


GOOD_WORKFLOW = """\
on:
  pull_request:
env:
  TOOLCHAIN: example.invalid/libogc2@sha256:{digest}
jobs:
  build:
    runs-on: [self-hosted, indigo-build]
    steps:
      - uses: actions/checkout@{sha} # v7
  emulator:
    runs-on:
      - self-hosted
      - indigo-emulator
    steps:
      - run: true
"""


class Workflows(unittest.TestCase):
    def root(self, directory: Path, workflow: str) -> Path:
        (directory / "buildtools/ci/runner").mkdir(parents=True)
        (directory / ".github/workflows").mkdir(parents=True)
        image = "example.invalid/libogc2@sha256:" + "1" * 64
        (directory / "buildtools/ci/runner/build.Dockerfile").write_text(
            f'FROM {image}\nRUN echo "{image}" > /etc/indigo-ci/toolchain\n')
        (directory / ".github/workflows/ci.yml").write_text(workflow)
        return directory

    def check(self, workflow: str) -> list[str]:
        with tempfile.TemporaryDirectory() as directory:
            return check_workflows.problems(self.root(Path(directory), workflow))

    def good(self) -> str:
        return GOOD_WORKFLOW.format(digest="1" * 64, sha="a" * 40)

    def test_our_runners_pinned_actions_and_the_runner_toolchain_pass(self):
        self.assertEqual(self.check(self.good()), [])

    def test_each_breach_is_caught(self):
        good = self.good()
        for bad in (good.replace("[self-hosted, indigo-build]", "ubuntu-24.04"),
                    good.replace("[self-hosted, indigo-build]", "[self-hosted]"),
                    good.replace("      - indigo-emulator\n", ""),
                    good.replace("@" + "a" * 40, "@v7"),
                    good.replace("  pull_request:", "  pull_request_target:"),
                    good.replace("    steps:\n      - uses", "    container: debian\n    steps:\n      - uses", 1),
                    good.replace("1" * 64, "2" * 64, 1)):
            self.assertNotEqual(self.check(bad), [], bad)


if __name__ == "__main__":
    unittest.main()
