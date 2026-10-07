#!/bin/sh
# Runs real framebuffer writes on the target; never run as part of a build.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
# User arguments select device, duration, method, pattern and optional controls.
# Sweep options come last so every run has a well-defined delay and region.
for delay in 110 50 33 16 0; do
    echo "Display full-frame test: delay=${delay}ms" >&2
    sh "$HERE/run.sh" display "$@" --region 320x240 --delay-ms "$delay"
done
for region in 320x120 320x24; do
    echo "Display partial-update test: region=$region, delay=0ms" >&2
    sh "$HERE/run.sh" display "$@" --region "$region" --delay-ms 0
done
