#!/bin/sh
# Run after stopping Chiton for controlled CPU comparisons. Output is stdout;
# redirect into a NEW operator-selected file to retain a diagnostic report.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
if [ "$#" -gt 1 ]; then
    echo "Usage: $0 [ITERATIONS]" >&2; exit 2
fi
ITERATIONS=${1:-1000}
case "$ITERATIONS" in
    ''|*[!0-9]*) echo "Iterations must be an integer from 1 to 1000000" >&2; exit 2 ;;
esac
if [ "${#ITERATIONS}" -gt 7 ] || [ "$ITERATIONS" -lt 1 ] || [ "$ITERATIONS" -gt 1000000 ]; then
    echo "Iterations must be an integer from 1 to 1000000" >&2; exit 2
fi
for kernel in grey16 grey26 grey24 colour planar16 planar26 planar-colour; do
    "$HERE/run.sh" image "$kernel" "$ITERATIONS"
done
for kernel in average calibrate; do
    for smoothing in 0 1; do
        "$HERE/run.sh" calibration "$kernel" "$ITERATIONS" "$smoothing"
    done
done
