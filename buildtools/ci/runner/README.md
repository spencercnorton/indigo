# Indigo's CI runners

Indigo's CI runs on the maintainer's own machines, not GitHub's. Every job
gets a fresh container that registers with GitHub just before it is needed,
takes that one job, and is thrown away. Nothing a job does survives it.

| Pool | `runs-on` | Image | What runs there |
| --- | --- | --- | --- |
| build | `[self-hosted, indigo-build]` | [`build.Dockerfile`](build.Dockerfile): the pinned devkitPPC/libogc2 image, GCC and Clang with their sanitizers, Python with Pillow and NumPy | the DOL, the SD card zip, host tests, source checks, releases |
| site | `[self-hosted, norvitech-site]` | the build image | norvitech.com's checks, for [spencercnorton/norvitech-site](https://github.com/spencercnorton/norvitech-site) |
| emulator | `[self-hosted, indigo-emulator]` | [`emulator.Dockerfile`](emulator.Dockerfile): Dolphin (pinned), a virtual X server, FFmpeg, gxtexconv | the emulator test ([buildtools/ui/emulator/](../../ui/emulator/README.md)) |

## How a job is contained

A public repository's pull requests run code nobody has reviewed yet, so a
job is treated as untrusted:

- **One job, then gone.** [`indigo_ci.py`](indigo_ci.py) asks GitHub for a
  just-in-time runner registration, starts a container with it, and removes
  the container when the job ends. The next job gets a new one.
- **No privileges.** The job runs as an unprivileged user with every Linux
  capability dropped and `no-new-privileges`, under CPU, memory and process
  limits. There is no Docker socket, no host directory and no GPU in the
  container, which is why workflows use no `container:` or `services:` (the
  build runner already is the toolchain image).
- **GitHub and nothing else.** Each slot's container sits on its own Docker
  network with no route out. The only way out is
  [`egress_proxy.py`](egress_proxy.py): it opens HTTPS tunnels to GitHub and
  Sigstore (build provenance) and refuses every other host, port and address,
  so a job cannot reach the machine's own network.
- **Approval first.** A pull request from someone who has not contributed
  before waits for the maintainer to approve its workflow run
  (the repository's fork pull request setting), and `pull_request_target`
  is never used. [`check_workflows.py`](../check_workflows.py) fails CI if a
  workflow breaks any of these rules.

The toolchain is the same one every Indigo DOL has been built with. A build
job checks that its runner carries the exact image its workflow names
([`toolchain.sh`](../toolchain.sh)), and records it in the DOL's provenance.

## Images

`indigo_ci.py` builds the images itself, from the `beta` branch, never from a
pull request. Every ten minutes it fetches `beta`; when a file in this
directory changed, or GitHub published a new runner, it builds the new image,
then replaces idle runners with ones on the new image. Runners in the middle
of a job finish it first. A pull request that changes an image is therefore
tested on the current image, and the change applies from the first run after
it merges. A change to `indigo_ci.py` restarts the supervisor the same way.

## Running a runner host

On a Linux machine with Docker, as the user that will own the runners:

```bash
curl -fsSLO https://raw.githubusercontent.com/spencercnorton/indigo/beta/buildtools/ci/runner/install.sh
sh install.sh
```

It clones the repository, installs the systemd user service `indigo-ci`, and
writes `~/.config/indigo-ci.env`. The supervisor needs `GH_TOKEN`, a token
with admin rights on the repository (it mints runner registrations): put it
in that file (it is mode 0600), or give the unit a drop-in that fetches it
from a secret store at start. `INDIGO_CI_POOLS` sets the slots per pool
(default `build:3,emulator:1`; `site:1` also serves norvitech.com's checks);
two machines can serve the same pools.

```bash
systemctl --user status indigo-ci     # the supervisor
journalctl --user -u indigo-ci -f     # what it is doing
python3 ~/.local/share/indigo-ci/src/buildtools/ci/runner/indigo_ci.py status
docker logs indigo-ci-proxy           # every host a job asked for, allowed or refused
```

`indigo_ci.py stop` removes this machine's runners and containers; systemd
restarts it unless you `systemctl --user disable --now indigo-ci` first.

## When CI waits

A job that sits in "Queued" is waiting for a runner with its labels. Check
the service on the runner host, then `indigo_ci.py status`: it lists the
containers and what GitHub thinks of each runner. The supervisor retries a
runner that fails to start with a growing delay, and can run a command of
yours (`INDIGO_CI_ALERT`) when a pool cannot start runners and again when it
recovers.
