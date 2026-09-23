# Security policy

## Reporting a vulnerability

Please report vulnerabilities privately through GitHub:
**[Report a vulnerability](https://github.com/spencercnorton/indigo/security/advisories/new)**.
Do not open a public issue, and do not include real credentials, disc images, save files
or personal paths in the report — a description and a minimal reproduction
are enough.

There is no e-mail address for security reports; the advisory form is the
only channel, and it is the one that is monitored. You will get an
acknowledgement within a week. Fixes ship as a tagged release; the advisory
is published once the release is out, and credits you unless you ask
otherwise.

## Supported versions

Only the latest tagged release is supported. Indigo has no LTS line.

## Scope

In scope: this repository's code and the artefacts it ships.
Out of scope: upstream Swiss (report those to the
[upstream project](https://github.com/emukidid/swiss-gc)), the homebrew
toolchain and libraries this builds against, and hardware faults.

## What Indigo does with credentials and data

Understanding the trust model helps you judge what is and is not a finding:

- **There are no credentials:** Indigo runs on a GameCube with no accounts and no key material; network settings you enter (SMB, FTP, FSP) are stored in the configuration file on your own storage device.
- **What leaves the console:** nothing, unless you use a network device handler you configured yourself. There is no telemetry and no update check.
- **Game patching is not a security boundary:** Indigo loads and patches code you supply from your own media; a malformed image can crash the console, and that is a bug rather than a vulnerability unless it escapes what the loader is meant to do.
- **Local state** lives under your SD card or other storage device (swiss.ini, cheats and saved selections). No telemetry is
  sent anywhere.
