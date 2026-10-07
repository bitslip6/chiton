#!/bin/sh
# SDK-free diagnostics only; does not use the device's saved configuration.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
if [ "$#" -lt 1 ]; then
    echo "Usage: $0 image|calibration KERNEL [ITERATIONS [SMOOTHING]] | display|spi [OPTIONS]" >&2
    exit 2
fi
case "$1" in
    image) PROGRAM=image_pipeline_benchmark ;;
    calibration) PROGRAM=calibration_pipeline_benchmark ;;
    display) PROGRAM=display_refresh_benchmark ;;
    spi) PROGRAM=spi_display_benchmark ;;
    *) echo "Expected image, calibration, display or spi" >&2; exit 2 ;;
esac
shift
export LD_LIBRARY_PATH="$HERE/../lib"
exec "$HERE/$PROGRAM" "$@"
