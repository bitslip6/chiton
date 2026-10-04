#!/bin/sh
# Desktop gates must pass before copying the separately built target binary.
# These checks never execute bin/chiton and do not replace hardware approval.
set -eu
cd "$(dirname "$0")"
test -f bin/chiton
python3 tests/release_gate.py --all
cp bin/chiton release/chiton
