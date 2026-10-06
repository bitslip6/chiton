#!/bin/sh
# SDK-free diagnostics only; does not use the device's saved configuration.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
if [ "$#" -lt 1 ]; then
    echo "Usage: $0 image|calibration KERNEL [ITERATIONS [SMOOTHING]]" >&2
    exit 2
fi
case "$1" in
    image) PROGRAM=image_pipeline_benchmark ;;
    calibration) PROGRAM=calibration_pipeline_benchmark ;;
    *) echo "Expected image or calibration" >&2; exit 2 ;;
esac
shift
export LD_LIBRARY_PATH="$HERE/../lib"
exec "$HERE/$PROGRAM" "$@"
