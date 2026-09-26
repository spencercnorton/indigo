# Releasing Indigo

Two channels, one direction: changes land on `beta`, betas are tested, and a
beta that has held up is promoted to `main` unchanged. Tags are immutable; a
mistake is fixed by the next version, never by moving a tag.

| Channel | Branch | Tag | GitHub Release |
| --- | --- | --- | --- |
| Beta | `beta` | `vX.Y.Z-beta.N` | Pre-release |
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
