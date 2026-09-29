#!/usr/bin/env python3
"""The supervisor's decisions, without Docker or GitHub."""

import os
import tempfile
import unittest
from pathlib import Path

os.environ["INDIGO_CI_NAME"] = "ci-host"
import indigo_ci  # noqa: E402


class Pools(unittest.TestCase):
    def test_default_and_custom(self):
        self.assertEqual(indigo_ci.parse_pools("build:3,emulator:1"), {"build": 3, "emulator": 1})
        self.assertEqual(indigo_ci.parse_pools("build:4,emulator:2,site:1"),
                         {"build": 4, "emulator": 2, "site": 1})

    def test_the_site_pool_serves_the_site_on_the_build_image(self):
        site = indigo_ci.POOLS["site"]
        self.assertEqual((site.repo, site.dockerfile, site.label),
                         ("spencercnorton/norvitech-site", "build.Dockerfile", "norvitech-site"))
        self.assertEqual(indigo_ci.POOLS["build"].repo, indigo_ci.REPO)
        self.assertEqual(indigo_ci.parse_pools(" build:0 "), {"build": 0})

    def test_bad_entries_stop_the_supervisor(self):
        for spec in ("", "build", "build:x", "build:17", "mystery:1", "build:1,build:2"):
            with self.assertRaises(SystemExit, msg=spec):
                indigo_ci.parse_pools(spec)


class RunnerRelease(unittest.TestCase):
    body = ("- actions-runner-linux-arm64-2.337.0.tar.gz <!-- BEGIN SHA linux-arm64 -->"
            + "b" * 64 + "<!-- END SHA linux-arm64 -->\n"
            "- actions-runner-linux-x64-2.337.0.tar.gz <!-- BEGIN SHA linux-x64 -->"
            + "a" * 64 + "<!-- END SHA linux-x64 -->\n")

    def test_version_and_linux_x64_checksum(self):
        self.assertEqual(indigo_ci.parse_runner_release({"tag_name": "v2.337.0", "body": self.body}),
                         ("2.337.0", "a" * 64))

    def test_refuses_a_release_it_cannot_verify(self):
        for release in ({"tag_name": "v2.337.0", "body": "no checksums"},
                        {"tag_name": "latest", "body": self.body}, {}):
            with self.assertRaises(RuntimeError):
                indigo_ci.parse_runner_release(release)


class ImageTag(unittest.TestCase):
    def test_changes_with_its_inputs_and_the_runner_only(self):
        with tempfile.TemporaryDirectory() as directory:
            context = Path(directory)
            for name in ("build.Dockerfile", "emulator.Dockerfile", "entrypoint.sh", "egress_proxy.py"):
                (context / name).write_text(name)
            build, emulator = indigo_ci.POOLS["build"], indigo_ci.POOLS["emulator"]
            runner = ("2.337.0", "a" * 64)
            first = indigo_ci.image_tag(build, context, runner)
            self.assertRegex(first, r"^indigo-ci/build:[0-9a-f]{16}$")
            self.assertEqual(first, indigo_ci.image_tag(build, context, runner))
            self.assertNotEqual(first, indigo_ci.image_tag(build, context, ("2.338.0", "c" * 64)))
            (context / "README.md").write_text("docs")  # not an input: no rebuild
            self.assertEqual(first, indigo_ci.image_tag(build, context, runner))
            emulator_tag = indigo_ci.image_tag(emulator, context, runner)
            (context / "egress_proxy.py").write_text("changed")  # the build image copies it
            self.assertNotEqual(first, indigo_ci.image_tag(build, context, runner))
            self.assertEqual(emulator_tag, indigo_ci.image_tag(emulator, context, runner))


class Dockerfiles(unittest.TestCase):
    def test_each_pool_declares_what_its_dockerfile_copies(self):
        here = Path(__file__).resolve().parent
        for pool in indigo_ci.POOLS.values():
            copied = set()
            for line in (here / pool.dockerfile).read_text().splitlines():
                if line.startswith("COPY ") and "--from=" not in line:
                    words = [w for w in line.split()[1:] if not w.startswith("--")]
                    copied.update(words[:-1])
            self.assertEqual(copied, set(pool.copies), pool.dockerfile)
            for name in pool.copies:
                self.assertTrue((here / name).is_file(), name)


class Cleanup(unittest.TestCase):
    def test_only_this_machines_offline_runners_without_a_container(self):
        live = {"ci-host-build-1-aaaaaa"}
        self.assertTrue(indigo_ci.ours({"name": "ci-host-build-2-bbbbbb", "status": "offline"}, live))
        self.assertFalse(indigo_ci.ours({"name": "ci-host-build-1-aaaaaa", "status": "offline"}, live))
        self.assertFalse(indigo_ci.ours({"name": "ci-host-build-2-bbbbbb", "status": "online"}, live))
        self.assertFalse(indigo_ci.ours({"name": "other-host-build-1-cccccc", "status": "offline"}, live))
        self.assertFalse(indigo_ci.ours({"name": "ci-host2-build-1", "status": "offline"}, live))

    def test_names(self):
        self.assertRegex(indigo_ci.runner_name("build", 2), r"^ci-host-build-2-[0-9a-f]{6}$")
        self.assertEqual(indigo_ci.container_name("emulator", 1), "indigo-ci-emulator-1")


class Timestamps(unittest.TestCase):
    def test_docker_started_at_is_utc(self):
        self.assertEqual(indigo_ci._epoch("1970-01-01T00:01:40.123456789Z"), 100.0)
        self.assertEqual(indigo_ci._epoch("2026-07-01T00:00:00Z"), 1782864000.0)


if __name__ == "__main__":
    unittest.main()
