# Security policy

## Reporting a vulnerability

Please report vulnerabilities privately through GitHub:
**[Report a vulnerability](https://github.com/spencercnorton/indigo/security/advisories/new)**.
Do not open a public issue, and do not include real credentials, disc images, save files
or personal paths in the report — a description and a minimal reproduction
are enough.

There is no e-mail address for security reports; the advisory form is the
only channel, and it is the one that is monitored. You will get an
acknowledgement within a week. Fixes ship as a stable release; the advisory
is published once the release is out, and credits you unless you ask
otherwise.

## Supported versions

The latest stable release (`vX.Y.Z`, marked Latest on GitHub) is supported
and gets security fixes. Release candidates (`vX.Y.Z-rc.N`) and betas
(`vX.Y.Z-beta.N`) are pre-releases for testing; a problem found in one is
fixed in the next pre-release. Indigo has no LTS line.

## Scope

In scope: this repository's code and the artefacts it ships.
Out of scope: upstream Swiss (report those to the
[upstream project](https://github.com/emukidid/swiss-gc)), the homebrew
toolchain and libraries this builds against, and hardware faults.

## What Indigo does with credentials and data

Understanding the trust model helps you judge what is and is not a finding:

- **There are no credentials:** Indigo runs on a GameCube with no accounts and no key material; network settings you enter (SMB, FTP, FSP), passwords included, are stored as plain text in `swiss/settings/global.ini` on your own storage device.
- **What leaves the console:** nothing, unless you use a network device handler you configured yourself. There is no telemetry and no update check.
- **Game patching is not a security boundary:** Indigo loads and patches code you supply from your own media; a malformed image can crash the console, and that is a bug rather than a vulnerability unless it escapes what the loader is meant to do.
- **Local state** lives on your SD card or other storage device: settings and play history in `swiss/settings/` (`global.ini`, and one file per game under `game/`), cheat files and the cheats you switched on for each game in `swiss/cheats/`, and saves copied with Memory Cards in its Save Folder (`swiss/saves/` unless you choose another). No telemetry is
  sent anywhere.
