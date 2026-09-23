# Contributing to Indigo

Thanks for your interest. Indigo is a small project with one maintainer, so
the process is deliberately light — but a few things are fixed.

## How changes land

This GitHub repository is a **release mirror**: every commit on `main` is a
tagged release built from a private development tree, and `main` only ever
moves forward by a release. That has two consequences for contributors:

- Pull requests are reviewed **here**, but they are not merged here. An
  accepted change is applied to the development tree and ships in the next
  tagged release; the pull request is then closed with a reference to that
  release, and you keep the credit in the release notes.
- Please do not rebase your pull request onto anything but `main`.

## Before you start

- **Bugs** — open a [bug report](https://github.com/spencercnorton/indigo/issues/new/choose).
  A report with reproduction steps, versions and a scrubbed log excerpt is
  usually fixed faster than a pull request that arrives without one.
- **Features** — open a feature request first. Indigo has strong opinions
  about staying faithful to the GameCube's own interface language
  (see the README); an idea that cuts across them needs a conversation before
  code.
- **Security** — never in a public issue. Use
  [private vulnerability reporting](https://github.com/spencercnorton/indigo/security/advisories/new);
  see [SECURITY.md](SECURITY.md).

## Working on the code

```bash
docker run --rm -v "$PWD:/work" -w /work ghcr.io/extremscorner/libogc2 make dev
make dev        # what CI runs
buildtools/check_whitespace.sh        # lint; CI enforces it
```

- the interface is drawn with the console's own GX pipeline at a fixed budget - a change that adds a per-frame allocation or a blocking read to the draw path will be sent back
- Keep a change to one concern. A pull request that fixes a bug and
  reformats a file is two pull requests.
- Tests: a bug fix carries a regression test; a feature carries the smallest
  test that fails without it.
- Commits carry a `Signed-off-by:` line (`git commit -s`, the Developer
  Certificate of Origin). There is no CLA.
- No secrets, hostnames, personal data or screenshots of a real desktop in
  the diff — the export gate rejects them and the pull request will be sent
  back.

## Out of scope

So nobody wastes an evening on it, Indigo will not accept:

- anything that requires a network service or an account
- piracy-adjacent features: disc dumping conveniences, region-bypass shortcuts for retail media you do not own
- porting the interface to other consoles

## Pull request checklist

The template asks for what changed, why, and how it was tested, plus a
confirmation that the diff carries no secrets, machine names or personal
paths. Fill it in — it is what the reviewer reads first.

## Licence

By contributing you agree that your contribution is licensed under the
[GPL-2.0-or-later](LICENSE) that covers the project.
