# AGENTS.md

Guidance for AI coding agents (and a quick orientation for people) working in
this repository. [CONTRIBUTING.md](CONTRIBUTING.md) is the contract; this file
is the working detail behind it.

## What this is

Indigo is a fork of [Swiss](https://github.com/emukidid/swiss-gc) for the
Nintendo GameCube with the interface rebuilt. The fork changes what you look
at and nothing underneath: device handlers, the patch engine and the loader
are upstream's, and a change that reaches them is out of scope unless a
maintainer asked for it.

- The interface: `cube/swiss/source/gui/` (Home cube, Library, Game Detail,
  cheats, Settings), wired in through `swiss.c`, `main.c` and `config/`.
- Host tests: `buildtools/ui/tests/` (C unit tests, GX vertex-stream checks
  and source audits; no console needed).
- User documentation: `README.md`, `docs/guide/` (every screen and setting,
  with pictures), `docs/SETTINGS.md` (the settings files), `CHANGELOG.md`.

## Build and test

```bash
# The DOL, in the pinned image CI uses (writes cube/swiss/swiss.dol):
docker run --rm -u "$(id -u):$(id -g)" -v "$PWD:/work" -w /work \
  ghcr.io/extremscorner/libogc2@sha256:84dcb9aa7c9ee716d4953a3985a9551996cdb7a24c2d95e32153ad0da83da575 make dev

# The SD card zip for that build (after make dev):
buildtools/sd_package.sh dev cube/swiss/swiss.dol .

# Host tests: lanes plain, sanitized, contracts, or all. Needs Python 3 with
# Pillow and NumPy, a C compiler and zlib; the poster tests also need
# gxtexconv (in the image above). CI runs all three lanes in that image.
buildtools/ui/tests/run_tests.sh all

# Source checks CI runs on a pull request into beta:
buildtools/check_whitespace.sh origin/beta
buildtools/check_ui_isolation.sh origin/beta
```

Run tests from a git clone with tags: some checks compare today's renderer
with a tagged release.

## Branches, versions and releases

- `beta` is where work lands. Branch from `beta`, open a pull request into
  `beta`, and squash-merge it once CI is green. Never push to `beta` or
  `main` directly; both are protected.
- `main` holds releases only. A release is a pull request from `beta` into
  `main`, merged with a merge commit, then tagged. Promotion to `main` needs
  the maintainer's explicit go.
- Tags: `vX.Y.Z-beta.N` on `beta` publishes a pre-release; `vX.Y.Z` on `main`
  publishes a release. `.github/workflows/release.yml` builds the zip from the
  tag, attaches it with its SHA-256 and a build provenance attestation, and
  writes the notes from `CHANGELOG.md`. Tags are immutable.
- Versions follow [semantic versioning](https://semver.org/). Every pull
  request adds its user-facing change to `## Unreleased` at the top of
  `CHANGELOG.md`; the release renames that heading.
- The full procedure, including hotfixes, is [docs/RELEASING.md](docs/RELEASING.md).

## Rules

- **Documentation moves with the code.** A change that alters a screen, a
  control or a setting updates its page in `docs/guide/` (and `docs/SETTINGS.md`
  for a settings key) in the same pull request. Pictures are recorded in the
  Dolphin emulator from a real build; say in the pull request which ones are
  stale if you cannot record them.
- **Stay inside the interface.** `buildtools/check_ui_isolation.sh` fails a
  change outside its allowlist. Widening it is a deliberate, reviewed edit
  with an exact-hunk exception and a reason, never a shortcut.
- **Keep the draw path cheap.** The interface is drawn with the console's GX
  pipeline at a fixed per-frame budget: no per-frame allocation, no blocking
  read in a draw function.
- **Tests with every change.** A bug fix carries a regression test; a feature
  carries the smallest test that fails without it. Pinned hashes in the audits
  (for example `SHOW_ACTIONS_SHA256`) are updated on purpose, never to make a
  red test go green.
- **Public-repository hygiene.** Nothing in a commit, a file or a pull request
  may name a private host, an internal tracker or ticket, a personal path, a
  credential, or a real person other than the maintainer. Screenshots come
  from Dolphin with demonstration data. GitHub push protection scans for
  secrets; do not rely on it.
- **Never contact upstream.** Indigo sends the Swiss project nothing: no pull
  requests, issues, comments or forum posts. Indigo problems are Indigo's.
- Commits are signed off (`git commit -s`, the Developer Certificate of
  Origin) under a GitHub noreply address (`<id>+<login>@users.noreply.github.com`),
  never a work or internal one: author, committer and sign-off lines are public.
