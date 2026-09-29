#!/bin/sh
# Start a just-in-time GitHub Actions runner: it registers, takes one job, and
# the container ends with it. indigo_ci.py passes the single-use configuration.
set -eu
config=${INDIGO_RUNNER_JITCONFIG:?no just-in-time runner configuration}
unset INDIGO_RUNNER_JITCONFIG
cd /home/runner/actions-runner
exec ./run.sh --jitconfig "$config"
