#!/bin/sh
# Prove this job runs on the toolchain its workflow names, then show it.
# usage: buildtools/ci/toolchain.sh <image@sha256:...>
set -eu
want=${1:?usage: buildtools/ci/toolchain.sh <image@sha256:...>}
have=$(cat /etc/indigo-ci/toolchain 2>/dev/null || echo "none (not an Indigo build runner)")
if [ "$have" != "$want" ]; then
	echo "::error::This runner's toolchain is $have; the workflow builds with $want. Runner images are built from beta, so a toolchain change merges first and applies from the next run (buildtools/ci/runner/README.md)." >&2
	exit 1
fi
echo "toolchain  $have"
echo "target     $("$DEVKITPPC/bin/powerpc-eabi-gcc" --version | head -n 1)"
echo "host       $(gcc --version | head -n 1); $(clang --version | head -n 1)"
echo "python     $(python3 --version 2>&1)"
echo "runner     $(/home/runner/actions-runner/bin/Runner.Listener --version 2>/dev/null || echo unknown)"
