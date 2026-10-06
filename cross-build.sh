#!/usr/bin/env bash
# Build/package only. Never execute ARM code, deploy, or write device state.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
if [[ ${1:-} == --help || ${1:-} == -h ]]; then
  printf '%s\n' \
    'Usage: ./cross-build.sh [--workspace PATH] [--output PATH] [--name rel_VERSION]' \
    '                      [--simd-greyscale] [--simd-calibration]'  \
    '' \
    'Creates a full audited AArch64 application bundle including benchmark programs/tools.' \
    'Defaults: ../chiton-cross/yocto-sd, ../chiton-cross/dist, unique UTC release name.' \
    'Requires the recovered Yocto workspace, AArch64 compiler/readelf, Python 3.12+.' \
    'JOBS controls build parallelism (default 2); CMAKE may override workspace CMake.' \
    'SIMD paths are experimental/default-off; qualify greyscale before calibration.' \
    'Does not run ARM programs, deploy, flash, or alter device configuration.'
  exit 0
fi
if [[ ! ${JOBS:-2} =~ ^[1-9][0-9]*$ ]]; then
  printf 'JOBS must be a positive integer\n' >&2
  exit 2
fi
# Reject arbitrary positional/unknown arguments; the Python packager validates
# release names, prerequisites and existing artifacts. Forward paths verbatim.
args=()
while (($#)); do
  case "$1" in
    --simd-greyscale|--simd-calibration)
      args+=("$1"); shift ;;
    --workspace|--output|--name)
      if (($# < 2)) || [[ -z $2 || $2 == --* ]]; then
        printf 'Missing value for %s\n' "$1" >&2; exit 2
      fi
      args+=("$1" "$2"); shift 2 ;;
    *) printf 'Unknown argument: %s (use --help)\n' "$1" >&2; exit 2 ;;
  esac
done
exec python3 "$ROOT/tools/package_aarch64.py" --include-benchmarks "${args[@]}"
