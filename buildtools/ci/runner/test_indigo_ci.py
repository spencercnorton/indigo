#!/usr/bin/env python3
"""The supervisor's decisions, without Docker or GitHub."""

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

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
            names = {name for pool in indigo_ci.POOLS.values() for name in (pool.dockerfile, *pool.copies)}
            for name in names:
                (context / name).parent.mkdir(parents=True, exist_ok=True)
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
            (context / "dolphin/0002-flush-sd-writes.patch").write_text("changed")  # Dolphin's patches
            self.assertNotEqual(emulator_tag, indigo_ci.image_tag(emulator, context, runner))


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

    def test_unnamed_machine_is_not_named_after_its_host(self):
        """Runner names are public in job logs, so without INDIGO_CI_NAME they say indigo."""
        env = {k: v for k, v in os.environ.items() if k != "INDIGO_CI_NAME"}
        name = subprocess.run([sys.executable, "-c", "import indigo_ci; print(indigo_ci.NAME)"],
                              cwd=Path(__file__).parent, env=env, capture_output=True, text=True,
                              check=True).stdout.strip()
        self.assertEqual(name, "indigo")


class Alerts(unittest.TestCase):
    def setUp(self):
        quiet = mock.patch.object(indigo_ci, "log", lambda *_: None)
        quiet.start()
        self.addCleanup(quiet.stop)

    def test_github_timing_out_is_retried_then_alerted_once_and_resolved(self):
        supervisor = indigo_ci.Supervisor({"build": 1})
        outcomes = [TimeoutError("The read operation timed out")] * (indigo_ci.ALERT_AFTER + 1) + [None, None]

        def tick(now):
            outcome = outcomes.pop(0)
            if outcome:
                raise outcome
        supervisor.tick = tick
        alerts = []
        with mock.patch.object(indigo_ci, "alert", lambda message, key, resolved=False: alerts.append((key, resolved))):
            for _ in range(indigo_ci.ALERT_AFTER + 3):
                supervisor.guarded_tick(0)
        self.assertEqual(alerts, [("supervisor", False), ("supervisor", True)])
        self.assertEqual(supervisor.tick_failures, 0)

    def test_a_bug_still_stops_the_supervisor(self):
        supervisor = indigo_ci.Supervisor({"build": 1})
        supervisor.tick = lambda now: None + 1
        with self.assertRaises(TypeError):
            supervisor.guarded_tick(0)

    def test_the_command_hears_what_and_whether_and_a_broken_one_is_survived(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory, "alert")
            command = f"sh -c 'printf \"%s|%s|%s\" \"$INDIGO_CI_ALERT_KEY\" \"$INDIGO_CI_ALERT_STATE\" \"$0\" > {out}'"
            with mock.patch.dict(os.environ, {"INDIGO_CI_ALERT": command}):
                indigo_ci.alert("the build pool starts runners again", "build", resolved=True)
            self.assertEqual(out.read_text(), "build|resolved|Indigo CI on ci-host: the build pool starts runners again")
        with mock.patch.dict(os.environ, {"INDIGO_CI_ALERT": "/nonexistent/indigo-ci-alert"}):
            indigo_ci.alert("the supervisor keeps failing", "supervisor")


class Timestamps(unittest.TestCase):
    def test_docker_started_at_is_utc(self):
        self.assertEqual(indigo_ci._epoch("1970-01-01T00:01:40.123456789Z"), 100.0)
        self.assertEqual(indigo_ci._epoch("2026-07-01T00:00:00Z"), 1782864000.0)


if __name__ == "__main__":
    unittest.main()
