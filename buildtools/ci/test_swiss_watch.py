#!/usr/bin/env python3
"""The Swiss watch stays silent for free, and never emails a guess."""

import io
import json
import subprocess
import tempfile
import unittest
from pathlib import Path
from urllib.parse import parse_qs

import swiss_watch


def commit(directory: Path, files: dict[str, bytes], message: str = "c") -> str:
    for name, data in files.items():
        (directory / name).parent.mkdir(parents=True, exist_ok=True)
        (directory / name).write_bytes(data)

    def git(*args: str) -> str:
        return subprocess.run(["git", "-C", str(directory), *args], capture_output=True,
                              text=True, check=True).stdout.strip()

    if not (directory / ".git").exists():
        git("init", "-q", "-b", "master")
    git("add", "-A")
    git("-c", "user.name=t", "-c", "user.email=t@example.invalid", "-c", "commit.gpgsign=false",
        "commit", "-qm", message)
    return git("rev-parse", "HEAD")


class Gates(unittest.TestCase):
    def repos(self, tmp: str, second: dict[str, bytes] | None) -> tuple[Path, Path, str]:
        up, ours = Path(tmp, "up"), Path(tmp, "ours")
        first = commit(up, {"cube/swiss/source/patcher.c": b"a\n", "README.md": b"r\n"})
        if second is not None:
            commit(up, second, "upstream work")
        commit(ours, {"UPSTREAM": f"upstream {up}\ncommit {first}\nMakefile  a tweak\n".encode(),
                      "cube/swiss/source/patcher.c": b"a\n"})
        return up, ours, first

    def test_silent_when_upstream_is_at_the_pinned_commit(self):
        with tempfile.TemporaryDirectory() as tmp:
            up, ours, _ = self.repos(tmp, None)
            self.assertIsNone(swiss_watch.collect(ours, str(up)))

    def test_markdown_and_ci_only_needs_no_model(self):
        with tempfile.TemporaryDirectory() as tmp:
            up, ours, _ = self.repos(tmp, {"README.md": b"r2\n",
                                           ".github/workflows/ci.yml": b"name: x\n"})
            bundle = swiss_watch.collect(ours, str(up))
            self.assertFalse(swiss_watch.needs_ai(bundle))

    def test_code_change_is_classified_and_needs_the_model(self):
        with tempfile.TemporaryDirectory() as tmp:
            up, ours, _ = self.repos(tmp, {"cube/swiss/source/patcher.c": b"b\n",
                                           "cube/swiss/source/gui/menu.c": b"g\n",
                                           "Makefile": b"m\n", "README.md": b"r2\n"})
            bundle = swiss_watch.collect(ours, str(up))
            self.assertTrue(swiss_watch.needs_ai(bundle))
            self.assertEqual({f["path"]: f["class"] for f in bundle["files"]}, {
                "Makefile": "listed", "README.md": "noise",
                "cube/swiss/source/gui/menu.c": "own",
                "cube/swiss/source/patcher.c": "upstream"})
            self.assertEqual(bundle["total_commits"], 1)
            self.assertTrue(bundle["ancestor"])


def bundle() -> dict:
    return {"url": "https://github.com/emukidid/swiss-gc.git",
            "pinned": "a" * 40, "head": "b" * 40, "ancestor": True,
            "listed": {}, "total_commits": 2, "submodule_moved": False,
            "commits": [{"sha": "c" * 40, "date": "2026-10-18", "subject": "Fix a device"},
                        {"sha": "d" * 40, "date": "2026-10-18", "subject": "Repaint a menu"}],
            "files": [{"status": "M", "path": "cube/swiss/source/patcher.c", "class": "upstream"}]}


class Verdict(unittest.TestCase):
    def reply(self, commits: list) -> str:
        return "noise " + json.dumps({"summary": "One fix.", "commits": commits, "risks": []})

    def test_unassessed_commits_become_review_and_unknown_shas_drop(self):
        verdict = swiss_watch.parse_verdict(self.reply([
            {"sha": "c" * 40, "verdict": "take", "why": "A device fix Indigo ships."},
            {"sha": "e" * 40, "verdict": "take", "why": "Invented."}]), bundle())
        self.assertEqual([(c["sha"][0], c["verdict"]) for c in verdict["commits"]],
                         [("c", "take"), ("d", "review")])
        self.assertEqual(len(swiss_watch.candidates(verdict)), 2)

    def test_a_reply_assessing_nothing_fails_closed(self):
        for bad in ("no json here", self.reply([]),
                    self.reply([{"sha": "c" * 40, "verdict": "maybe", "why": "?"}])):
            with self.assertRaises(swiss_watch.WatchError):
                swiss_watch.parse_verdict(bad, bundle())

    def test_the_email_names_the_exact_next_step(self):
        verdict = swiss_watch.parse_verdict(self.reply([
            {"sha": "c" * 40, "verdict": "take", "why": "A device fix Indigo ships."},
            {"sha": "d" * 40, "verdict": "skip", "why": "Upstream menu work Indigo replaced."}]),
            bundle())
        subject, body = swiss_watch.render(bundle(), verdict)
        self.assertIn("1 candidate", subject)
        self.assertIn(f"buildtools/upstream_merge.sh {'b' * 40}", body)
        self.assertIn(f"https://github.com/emukidid/swiss-gc/compare/{'a' * 40}...{'b' * 40}", body)
        self.assertIn("Nothing has been merged", body)


class Mail(unittest.TestCase):
    def test_graph_send_shape(self):
        sent, replies = [], [b'{"access_token": "fake-token"}', b""]

        def opener(request):
            sent.append(request)
            return io.BytesIO(replies.pop(0))

        env = {"SWISS_WATCH_TO": "inbox@example.invalid",
               "SWISS_WATCH_FROM": "sender@example.invalid",
               "M365_TENANT_ID": "tenant", "M365_MAILER_CLIENT_ID": "client",
               "M365_MAILER_CLIENT_SECRET": "not-a-real-secret"}
        swiss_watch.send_mail("Subject", "Body\n", env, opener)
        token_request, send_request = sent
        self.assertIn("login.microsoftonline.com/tenant/oauth2/v2.0/token",
                      token_request.full_url)
        self.assertEqual(parse_qs(token_request.data.decode())["grant_type"],
                         ["client_credentials"])
        self.assertIn("/users/sender%40example.invalid/sendMail", send_request.full_url)
        self.assertEqual(send_request.get_header("Authorization"), "Bearer fake-token")
        payload = json.loads(send_request.data)
        self.assertEqual(payload["message"]["toRecipients"][0]["emailAddress"]["address"],
                         "inbox@example.invalid")
        self.assertEqual(payload["message"]["body"], {"contentType": "Text", "content": "Body\n"})
        self.assertFalse(payload["saveToSentItems"])


if __name__ == "__main__":
    unittest.main()
