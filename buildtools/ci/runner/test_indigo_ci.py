#!/usr/bin/env python3
"""The supervisor's decisions, without Docker or GitHub."""

import os
import shutil
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
        self.assertEqual(indigo_ci.parse_pools("build:4,emulator:4"), {"build": 4, "emulator": 4})
        self.assertEqual(indigo_ci.parse_pools("build:4,emulator:4,release:2"),
                         {"build": 4, "emulator": 4, "release": 2})
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


HERE = Path(__file__).resolve().parent
TOOLCHAIN = "ghcr.io/extremscorner/libogc2@sha256:" + "e" * 64


class ImageTag(unittest.TestCase):
    def test_changes_with_its_inputs_and_the_runner_only(self):
        with tempfile.TemporaryDirectory() as directory:
            context = Path(directory, "runner")
            shutil.copytree(HERE, context)
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

    def test_another_branch_s_dockerfile_hashes_what_it_copies(self):
        """main's build.Dockerfile can copy fewer files than beta's: the release
        pool hashes what main's copies, not what beta's does."""
        with tempfile.TemporaryDirectory() as directory:
            context = Path(directory)
            (context / "build.Dockerfile").write_text(f"FROM {TOOLCHAIN}\nCOPY --chmod=0755 entrypoint.sh /opt/\n")
            (context / "entrypoint.sh").write_text("#!/bin/sh\n")
            release, runner = indigo_ci.POOLS["release"], ("2.337.0", "a" * 64)
            first = indigo_ci.image_tag(release, context, runner)
            self.assertRegex(first, r"^indigo-ci/release:[0-9a-f]{16}$")
            (context / "entrypoint.sh").write_text("#!/bin/sh\nexit 1\n")
            self.assertNotEqual(first, indigo_ci.image_tag(release, context, runner))


class Dockerfiles(unittest.TestCase):
    def test_every_file_a_dockerfile_copies_is_here(self):
        self.assertEqual(indigo_ci.copied((HERE / "build.Dockerfile").read_text()),
                         {"entrypoint.sh", "egress_proxy.py", "libiconv-lite.sh"})
        for pool in indigo_ci.POOLS.values():
            copied = indigo_ci.copied((HERE / pool.dockerfile).read_text())
            self.assertIn("entrypoint.sh", copied, pool.dockerfile)
            for name in copied:
                self.assertTrue((HERE / name).is_file(), name)


class Labels(unittest.TestCase):
    def test_the_toolchain_is_the_libogc2_digest_a_dockerfile_builds_from(self):
        self.assertEqual(indigo_ci.pinned_toolchain(f"# x\nFROM {TOOLCHAIN} AS toolchain\nFROM ubuntu\n"), "e" * 12)
        self.assertEqual(indigo_ci.pinned_toolchain("FROM ghcr.io/extremscorner/libogc2:latest\n"), "")
        self.assertRegex(indigo_ci.pinned_toolchain((HERE / "build.Dockerfile").read_text()), r"^[0-9a-f]{12}$")

    def test_jobs_find_their_toolchain_and_old_workflows_stay_on_beta(self):
        build, release = indigo_ci.POOLS["build"], indigo_ci.POOLS["release"]
        self.assertEqual(release.ref, "main")
        self.assertEqual(indigo_ci.runner_labels(build, "f" * 12),
                         ["self-hosted", "linux", "x64", "indigo-build", "indigo-build-" + "f" * 12])
        # No bare indigo-build: a workflow that names no toolchain is beta's.
        self.assertEqual(indigo_ci.runner_labels(release, "e" * 12),
                         ["self-hosted", "linux", "x64", "indigo-build-" + "e" * 12])
        with self.assertRaises(RuntimeError):
            indigo_ci.runner_labels(release, "")


class Checkout(unittest.TestCase):
    def test_another_trusted_branch_is_a_worktree_of_the_source(self):
        with tempfile.TemporaryDirectory() as directory:
            home = Path(directory)
            source = home / "src"

            def git(*args):
                subprocess.run(["git", "-C", str(source), "-c", "user.name=t", "-c", "user.email=t@example.invalid",
                                "-c", "commit.gpgsign=false", *args], check=True, capture_output=True)
            (source / indigo_ci.CONTEXT).mkdir(parents=True)
            git("init", "-q")
            for branch in ("main", "beta"):
                (source / indigo_ci.CONTEXT / "build.Dockerfile").write_text(f"FROM {branch}\n")
                git("add", "-A")
                git("commit", "-qm", branch)
                git("update-ref", f"refs/remotes/origin/{branch}", "HEAD")
            with mock.patch.multiple(indigo_ci, HOME=home, SOURCE=source, REF="beta"):
                self.assertEqual(indigo_ci.checkout("beta"), source / indigo_ci.CONTEXT)
                context = indigo_ci.checkout("main")
                self.assertEqual(context, home / "src-main" / indigo_ci.CONTEXT)
                self.assertEqual((context / "build.Dockerfile").read_text(), "FROM main\n")
                git("update-ref", "refs/remotes/origin/main", "refs/remotes/origin/beta")  # main moves on
                self.assertEqual(indigo_ci.checkout("main"), context)
                self.assertEqual((context / "build.Dockerfile").read_text(), "FROM beta\n")


class Retire(unittest.TestCase):
    def test_an_idle_runner_registered_with_other_labels_is_replaced(self):
        """Labels are fixed at registration: runners from before toolchain labels
        must give way, or jobs that name one queue behind them."""
        supervisor = indigo_ci.Supervisor({"build": 1})
        supervisor.images = {"build": "indigo-ci/build:1"}
        supervisor.labels = {"build": ["self-hosted", "linux", "x64", "indigo-build", "indigo-build-" + "f" * 12]}
        supervisor.next_source = supervisor.next_release = supervisor.next_cleanup = supervisor.next_health = 1e12
        supervisor.runner = ("2.337.0", "a" * 64)
        running = {"State": {"Running": True, "StartedAt": "2026-10-19T00:00:00Z"},
                   "Config": {"Image": "indigo-ci/build:1", "Labels": {"indigo-ci.runner": "ci-host-build-1-aaaaaa"}}}
        removed = []
        with mock.patch.multiple(indigo_ci, log=lambda *_: None, inspect=lambda name: running,
                                 repo_runners=lambda repos: [{"name": "ci-host-build-1-aaaaaa", "busy": False}],
                                 docker=lambda *args, **_: removed.append(args)), \
                mock.patch.object(supervisor, "ensure_proxy"), mock.patch.object(supervisor, "launch") as launch:
            now = indigo_ci._epoch("2026-10-19T00:01:00Z")
            supervisor.tick(now)
            self.assertIn(("rm", "--force", "indigo-ci-build-1"), removed)
            removed.clear()
            running["Config"]["Labels"]["indigo-ci.labels"] = ",".join(supervisor.labels["build"])
            supervisor.tick(now)
            self.assertEqual(removed, [])
            launch.assert_not_called()


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
