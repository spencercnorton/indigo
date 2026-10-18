#!/usr/bin/env python3
"""Watch upstream Swiss and email only the changes Indigo should take.

Indigo is built on the one Swiss commit UPSTREAM names, and moving to a
newer one is manual (buildtools/upstream_merge.sh). This weekly check
(.github/workflows/swiss-watch.yml) looks for new Swiss commits and judges
each one for Indigo, in gates cheapest first so most weeks cost nothing:

1. One `git ls-remote`: upstream still at the pinned commit -> exit silent.
2. A deterministic filter (no model): every changed file is classified with
   check_upstream's OWN and UPSTREAM's listed paths. A week that touched
   only markdown or upstream's own CI exits silent too.
3. One model call, only on a real diff: per commit, take / review / skip,
   with why it matters to Indigo. Anything the model does not assess counts
   as review; a reply that assesses nothing fails the run instead of
   emailing a guess.
4. Email, only when at least one commit is take or review. The email is
   plain text: what changed, why Indigo should take it, the risks, and the
   exact upstream_merge.sh command. It never merges anything itself.

Configuration is environment only (GitHub secrets/vars; see the workflow):
  SWISS_WATCH_TO        recipient (required to send)
  SWISS_WATCH_AI_KEY    API key for an OpenAI-compatible chat endpoint
  SWISS_WATCH_AI_BASE   default https://api.openai.com/v1
  SWISS_WATCH_AI_MODEL  default gpt-4o-mini; set BASE+MODEL together for
                        another provider (e.g. OpenRouter)
  SMTP_HOST, SMTP_PORT  submission server; port defaults to 587 (465 = TLS)
  SMTP_USER, SMTP_PASSWORD, SMTP_FROM (FROM defaults to USER)

usage: swiss_watch.py [--dry-run] [--no-ai] [--report FILE] [upstream repo]
  --dry-run  print the email instead of sending it (missing mail/AI config
             is a warning, so a first manual run works before any secret)
  --no-ai    stop after gate 2 and print the classified diff
Exit codes: 0 silent/sent/printed, 1 model or mail failure, 2 bad config.
"""

import argparse
import json
import os
import re
import smtplib
import ssl
import subprocess
import sys
import tempfile
import urllib.request
from email.message import EmailMessage
from pathlib import Path

import check_upstream

BRANCH = "master"  # upstream Swiss's default branch; UPSTREAM pins by commit
MAX_COMMITS = 50
MAX_DIFF_CHARS = 60_000
NOISE = re.compile(r"^(\.github/|\.gitignore$|.*\.md$)")
SHA = re.compile(r"[0-9a-f]{40}")
VERDICTS = ("take", "review", "skip")


class WatchError(Exception):
    pass


def git(*args: str, cwd: str = "") -> str:
    done = subprocess.run(["git", *(["-C", cwd] if cwd else []), *args],
                          capture_output=True, text=True)
    if done.returncode:
        raise WatchError(f"git {' '.join(args[:2])} failed: {done.stderr.strip()[:300]}")
    return done.stdout


def classify(path: str, listed: dict[str, str]) -> str:
    """listed: Indigo modified it, a merge needs care. noise: never worth a
    model call alone. own: Indigo replaced this area. upstream: clean take."""
    if any(check_upstream.covers(entry, path) for entry in listed):
        return "listed"
    if NOISE.match(path):
        return "noise"
    if check_upstream.OWN.match(path):
        return "own"
    return "upstream"


def upstream_head(url: str) -> str:
    out = git("ls-remote", url, f"refs/heads/{BRANCH}")
    found = SHA.search(out)
    if not found:
        raise WatchError(f"no {BRANCH} head at {url}")
    return found.group(0)


def collect(root: Path, source: str = "") -> dict | None:
    """The evidence bundle for pinned..upstream-head, or None when gate 1
    already knows there is nothing new."""
    url, pinned, listed = check_upstream.parse((root / "UPSTREAM").read_text(encoding="utf-8"))
    if not SHA.fullmatch(pinned):
        raise WatchError("UPSTREAM names no 40-character commit")
    url = source or url
    head = upstream_head(url)
    if head == pinned:
        return None
    with tempfile.TemporaryDirectory() as bare:
        git("init", "-q", "--bare", bare)
        git("fetch", "-q", "--no-tags", url, pinned, head, cwd=bare)
        ancestor = subprocess.run(["git", "-C", bare, "merge-base", "--is-ancestor", pinned, head],
                                  capture_output=True).returncode == 0
        log = git("log", "--reverse", "--format=%H%x1f%ad%x1f%s", "--date=short",
                  f"{pinned}..{head}", cwd=bare).splitlines()
        commits = [{"sha": sha, "date": date, "subject": subject}
                   for sha, date, subject in (line.split("\x1f", 2) for line in log if line)]
        names = git("diff", "--name-status", pinned, head, cwd=bare).splitlines()
        files = []
        for line in names:
            status, _, path = line.partition("\t")
            path = path.rsplit("\t", 1)[-1]  # a rename names old and new; judge the new
            files.append({"status": status, "path": path, "class": classify(path, listed)})
        stat = git("diff", "--stat", pinned, head, cwd=bare).strip()
        diff = git("diff", pinned, head, cwd=bare)
        def links(rev: str) -> list[str]:  # submodule pins, as upstream_merge.sh compares them
            return [l for l in git("ls-tree", "-r", rev, cwd=bare).splitlines()
                    if l.startswith("160000")]
        moved = links(pinned) != links(head)
    return {"url": url, "pinned": pinned, "head": head, "ancestor": ancestor,
            "listed": listed, "commits": commits[:MAX_COMMITS], "total_commits": len(commits),
            "files": files, "stat": stat, "submodule_moved": bool(moved),
            "diff": diff[:MAX_DIFF_CHARS], "diff_truncated": len(diff) > MAX_DIFF_CHARS}


def needs_ai(bundle: dict) -> bool:
    """Gate 2: a model call only when a non-noise file changed."""
    return any(f["class"] != "noise" for f in bundle["files"])


PROMPT = """You judge upstream Swiss (emukidid/swiss-gc, a GameCube homebrew \
loader) commits for Indigo, a fork that rebuilt only the interface: device \
handlers, the patch engine and the loader are upstream's and Indigo takes \
them by moving UPSTREAM to a newer Swiss commit (upstream_merge.sh). \
For each commit give one verdict: "take" (Indigo should move to include it: \
a fix, device/game compatibility, data updates), "review" (probably take, \
but it touches files Indigo modified or its interface, or is risky/unclear), \
or "skip" (upstream interface work Indigo replaced, formatting, docs or CI \
with no effect on the console). File classes: upstream = clean take, \
listed = Indigo carries its own change to this file, own = Indigo's \
interface replaced this area, noise = markdown/CI. Judge from the evidence \
only; if the diff shown is truncated or unclear, say review. \
Reply with one JSON object and nothing else: \
{"summary": "2-4 sentences: what changed upstream and whether Indigo should move", \
"commits": [{"sha": "<full sha>", "verdict": "take|review|skip", \
"why": "one or two sentences: what it does and why Indigo should or should not take it"}], \
"risks": ["merge risks for Indigo, or empty"]}"""


def ask_ai(bundle: dict, base: str, model: str, key: str, opener=None) -> str:
    """One OpenAI-compatible chat call. Returns the raw reply text."""
    evidence = {
        "pinned": bundle["pinned"], "head": bundle["head"],
        "pinned_is_ancestor": bundle["ancestor"],
        "submodule_moved": bundle["submodule_moved"],
        "indigo_listed_paths": bundle["listed"],
        "commits": bundle["commits"], "files": bundle["files"],
        "diffstat": bundle["stat"],
        "diff_truncated": bundle["diff_truncated"], "diff": bundle["diff"],
    }
    body = json.dumps({"model": model, "temperature": 0, "messages": [
        {"role": "system", "content": PROMPT},
        {"role": "user", "content": json.dumps(evidence, indent=1)},
    ]}).encode()
    request = urllib.request.Request(
        base.rstrip("/") + "/chat/completions", data=body, method="POST",
        headers={"Authorization": f"Bearer {key}", "Content-Type": "application/json"})
    open_url = opener or (lambda req: urllib.request.urlopen(req, timeout=120))
    try:
        with open_url(request) as reply:
            payload = json.loads(reply.read().decode("utf-8", "replace"))
        return payload["choices"][0]["message"]["content"]
    except Exception as problem:  # network, HTTP, shape: all fail closed below
        raise WatchError(f"model call failed: {problem}") from problem


def parse_verdict(text: str, bundle: dict) -> dict:
    """The model's reply, validated against the commits that really exist.
    Unknown SHAs are dropped, unassessed commits become review, and a reply
    assessing nothing is an error: no email is ever a model's guess."""
    start, end = text.find("{"), text.rfind("}")
    try:
        raw = json.loads(text[start:end + 1]) if start >= 0 else {}
    except json.JSONDecodeError as problem:
        raise WatchError(f"model reply is not JSON: {problem}") from problem
    known = {c["sha"]: c for c in bundle["commits"]}
    assessed = {}
    for item in raw.get("commits") or []:
        sha = next((s for s in known if s.startswith(str(item.get("sha", ""))[:40]) and
                    len(str(item.get("sha", ""))) >= 7), "")
        if sha and item.get("verdict") in VERDICTS:
            assessed[sha] = {"verdict": item["verdict"],
                             "why": str(item.get("why", "")).strip()[:600]}
    if not assessed:
        raise WatchError("model assessed no commits; refusing to email a guess")
    commits = [{**c, **assessed.get(c["sha"], {"verdict": "review",
                                               "why": "Not assessed by the model; review by hand."})}
               for c in bundle["commits"]]
    risks = [str(r).strip()[:300] for r in (raw.get("risks") or [])][:10]
    if bundle["submodule_moved"]:
        risks.append("Upstream moved a submodule; its vendored copy here needs updating by hand.")
    if not bundle["ancestor"]:
        risks.append("The pinned commit is not an ancestor of upstream head (rewritten history?); merge by hand.")
    if bundle["total_commits"] > len(bundle["commits"]):
        risks.append(f"Only the first {len(bundle['commits'])} of {bundle['total_commits']} "
                     "commits were assessed; check the rest in the compare view.")
    return {"summary": str(raw.get("summary", "")).strip()[:2000] or "No summary given.",
            "commits": commits, "risks": [r for r in risks if r]}


def candidates(verdict: dict) -> list[dict]:
    return [c for c in verdict["commits"] if c["verdict"] in ("take", "review")]


def compare_url(bundle: dict) -> str:
    url = bundle["url"]
    if url.startswith("https://github.com/"):
        return f"{url.removesuffix('.git')}/compare/{bundle['pinned']}...{bundle['head']}"
    return ""


def render(bundle: dict, verdict: dict) -> tuple[str, str]:
    take = candidates(verdict)
    subject = (f"[Indigo] Swiss: {len(take)} candidate change(s) "
               f"({bundle['pinned'][:8]}..{bundle['head'][:8]})")
    lines = [verdict["summary"], "",
             f"Upstream moved {bundle['total_commits']} commit(s) since the "
             f"commit UPSTREAM names ({bundle['pinned'][:8]} -> {bundle['head'][:8]})."]
    if compare_url(bundle):
        lines.append(f"Compare: {compare_url(bundle)}")
    for heading, group in (("Take", [c for c in take if c["verdict"] == "take"]),
                           ("Review first", [c for c in take if c["verdict"] == "review"]),
                           ("Skip", [c for c in verdict["commits"] if c["verdict"] == "skip"])):
        if group:
            lines += ["", f"## {heading}"]
            lines += [f"- `{c['sha'][:8]}` ({c['date']}) {c['subject']}\n  {c['why']}"
                      for c in group]
    if verdict["risks"]:
        lines += ["", "## Risks"] + [f"- {risk}" for risk in verdict["risks"]]
    touched = sorted({f["path"] for f in bundle["files"] if f["class"] == "listed"})
    if touched:
        lines += ["", "Files Indigo carries its own change to (merge with care):"] + \
                 [f"- {path}" for path in touched]
    lines += ["", "## Next step", "",
              "From a branch off beta, in its own pull request (see AGENTS.md):", "",
              f"  buildtools/upstream_merge.sh {bundle['head']}",
              "  python3 buildtools/ci/check_upstream.py",
              "  buildtools/ci/source_checks.sh", "",
              "Then refresh the audit fixtures that copy upstream code, as the",
              "merge script says. Nothing has been merged or opened for you.",
              "", f"Files changed: {len(bundle['files'])} "
              f"({sum(f['class'] == 'upstream' for f in bundle['files'])} clean takes, "
              f"{len(touched)} Indigo-modified, "
              f"{sum(f['class'] == 'own' for f in bundle['files'])} in Indigo's interface, "
              f"{sum(f['class'] == 'noise' for f in bundle['files'])} markdown/CI).",
              "", "--", "swiss-watch (weekly; silent when there is nothing to take)"]
    return subject, "\n".join(lines) + "\n"


def send_mail(subject: str, body: str, env: dict[str, str]) -> None:
    message = EmailMessage()
    message["Subject"], message["To"] = subject, env["SWISS_WATCH_TO"]
    message["From"] = env.get("SMTP_FROM") or env.get("SMTP_USER", "")
    message.set_content(body)
    host, port = env["SMTP_HOST"], int(env.get("SMTP_PORT") or 587)
    try:
        if port == 465:
            server = smtplib.SMTP_SSL(host, port, timeout=60, context=ssl.create_default_context())
        else:
            server = smtplib.SMTP(host, port, timeout=60)
            server.starttls(context=ssl.create_default_context())
        with server:
            if env.get("SMTP_USER"):
                server.login(env["SMTP_USER"], env.get("SMTP_PASSWORD", ""))
            server.send_message(message)
    except Exception as problem:
        raise WatchError(f"sending mail via {host}:{port} failed: {problem}") from problem


def describe(bundle: dict) -> str:
    """Gate 2's output for --no-ai: the classified diff, no verdict."""
    lines = [f"{bundle['total_commits']} new commit(s), "
             f"{bundle['pinned'][:8]}..{bundle['head'][:8]} "
             f"(ancestor: {bundle['ancestor']}, submodule moved: {bundle['submodule_moved']})"]
    lines += [f"  {c['sha'][:8]} {c['date']} {c['subject']}" for c in bundle["commits"]]
    lines += [f"  [{f['class']}] {f['status']} {f['path']}" for f in bundle["files"]]
    lines.append(f"needs_ai={needs_ai(bundle)} diff_chars={len(bundle['diff'])}"
                 f" truncated={bundle['diff_truncated']}")
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--dry-run", action="store_true", help="print the email, send nothing")
    parser.add_argument("--no-ai", action="store_true", help="stop after the deterministic gates")
    parser.add_argument("--report", default="", help="also write the report to this file")
    parser.add_argument("source", nargs="?", default="", help="upstream repository (default: UPSTREAM's)")
    args = parser.parse_args(argv)
    env = {key: os.environ.get(key, "").strip() for key in
           ("SWISS_WATCH_TO", "SWISS_WATCH_AI_KEY", "SWISS_WATCH_AI_BASE", "SWISS_WATCH_AI_MODEL",
            "SMTP_HOST", "SMTP_PORT", "SMTP_USER", "SMTP_PASSWORD", "SMTP_FROM")}
    try:
        if not args.dry_run and not args.no_ai:  # a sending run with no config fails on week one
            missing = [k for k in ("SWISS_WATCH_TO", "SWISS_WATCH_AI_KEY", "SMTP_HOST") if not env[k]]
            if missing:
                print(f"swiss_watch: missing {', '.join(missing)}; see the workflow header",
                      file=sys.stderr)
                return 2
        root = Path(git("rev-parse", "--show-toplevel").strip())
        bundle = collect(root, args.source)
        if bundle is None:
            print("swiss_watch: upstream is at the pinned commit; nothing to do")
            return 0
        if args.no_ai or not needs_ai(bundle):
            report = describe(bundle) + ("\n" if not needs_ai(bundle) else "")
            if not needs_ai(bundle) and not args.no_ai:
                report += "Only markdown/CI changed upstream; no model call, no email.\n"
            print(report)
            if args.report:
                Path(args.report).write_text(report, encoding="utf-8")
            return 0
        if not env["SWISS_WATCH_AI_KEY"]:
            report = describe(bundle) + "\nSWISS_WATCH_AI_KEY is not set: no candidacy verdict, no email.\n"
            print(report)
            if args.report:
                Path(args.report).write_text(report, encoding="utf-8")
            return 0 if args.dry_run else 2
        verdict = parse_verdict(ask_ai(bundle, env["SWISS_WATCH_AI_BASE"] or "https://api.openai.com/v1",
                                        env["SWISS_WATCH_AI_MODEL"] or "gpt-4o-mini",
                                        env["SWISS_WATCH_AI_KEY"]), bundle)
        if not candidates(verdict):
            print(f"swiss_watch: {len(verdict['commits'])} commit(s) assessed, none worth taking; no email")
            return 0
        subject, body = render(bundle, verdict)
        if args.report:
            Path(args.report).write_text(f"# {subject}\n\n{body}", encoding="utf-8")
        summary = os.environ.get("GITHUB_STEP_SUMMARY", "")
        if summary:
            Path(summary).open("a", encoding="utf-8").write(f"## {subject}\n\n```text\n{body}```\n")
        if args.dry_run:
            print(f"Subject: {subject}\n\n{body}")
            return 0
        send_mail(subject, body, env)
        print(f"swiss_watch: emailed {env['SWISS_WATCH_TO']}: {subject}")
        return 0
    except WatchError as problem:
        print(f"swiss_watch: {problem}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
