# Releasing Indigo

Two channels, one direction: changes land on `beta`, betas and release
candidates are tested, and the last candidate's code is promoted to `main`.
Tags are immutable; a mistake is fixed by the next version, never by moving a
tag.

| Channel | Branch | Tag | GitHub Release |
| --- | --- | --- | --- |
| Beta | `beta` | `vX.Y.Z-beta.N` | Pre-release |
| Release candidate | `beta` | `vX.Y.Z-rc.N` | Pre-release |
| Stable | `main` | `vX.Y.Z` | Latest |

Pushing a tag runs `.github/workflows/release.yml`. It checks that the tag
sits on its branch, builds the DOL from the tagged commit in the pinned
image, checks that the DOL names that commit, packages the drag-and-drop zip
(`buildtools/sd_package.sh`), and publishes the release with the zip,
`SHA256SUMS.txt`, a build provenance attestation and notes from
`CHANGELOG.md` (`buildtools/release_notes.sh`). To check the pipeline without
publishing, run the workflow by hand with an existing tag.

## A beta

1. `beta` is green in CI and `CHANGELOG.md` has the changes under
   `## Unreleased`.
2. Tag the tip of `beta` and push the tag:
   ```bash
   git fetch origin
   git tag v1.26.0-beta.1 origin/beta
   git push origin v1.26.0-beta.1
   ```
3. The pre-release appears with the `## Unreleased` notes. Later betas of
   the same version count up: `-beta.2`, `-beta.3`.

## A release candidate

When a version is complete and has held up in the betas before it (they
need not carry its version number), tag the tip of `beta` `vX.Y.Z-rc.1` the
same way. It is a pre-release like a beta, with the same notes, opening with
"Release candidate" instead of "Beta". A fix found in it lands on `beta` as
usual and ships as `-rc.2`. The release's code is the last candidate's,
unchanged: after that candidate only documentation may change on `beta`, the
release notes and the zip's README included (step 1 of a release). A change
to the code needs another candidate.

## A release

1. On `beta`, by pull request: rename `## Unreleased` to
   `## vX.Y.Z — <what this release is about>`, and make sure the README and
   the guide describe what ships.
2. Open a pull request from `beta` into `main` titled `Release vX.Y.Z`. CI runs
   on it; merge it with **Create a merge commit** once the maintainer says go.
3. Tag the merge commit and push the tag:
   ```bash
   git fetch origin
   git tag vX.Y.Z origin/main
   git push origin vX.Y.Z
   ```
4. The release appears as Latest, titled from its changelog heading.
5. Refresh [norvitech.com/indigo](https://norvitech.com/indigo/): its guide,
   pictures and download button are regenerated from the new tag.

## A fix to a release

A fix that can't wait for the next beta: branch from `main`, open a pull
request into `main`, merge it, tag `vX.Y.(Z+1)` on the result, then open a
pull request from `main` into `beta` so the fix is in the next beta too.

## Versions

[Semantic versioning](https://semver.org/): a patch release fixes, a minor
release adds, a major release changes something a user relied on (a settings
key, a folder on the card). The version is the tag; nothing in the tree needs
bumping, and System › System Information shows the commit a build came from.
