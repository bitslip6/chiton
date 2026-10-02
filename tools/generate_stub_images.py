#!/usr/bin/env python3
"""Generate headerless 640x480 IR/depth files for Chiton's camera stub.

Samples are unsigned 12-bit values (0..4095), stored right-aligned in
little-endian 16-bit words. The upper four bits are always zero.
Uses only the Python standard library.
"""

import argparse
from pathlib import Path
import struct

WIDTH = 640
HEIGHT = 480
MAX_SAMPLE = (1 << 12) - 1
DEFAULT_SAMPLE = 320
PATTERNS = ("constant", "gradient", "checkerboard")


def sample_argument(text):
    """Accept decimal or hexadecimal samples without truncation or wrapping."""
    try:
        value = int(text, 16) if text.lower().startswith("0x") else int(text, 10)
    except ValueError as error:
        raise argparse.ArgumentTypeError("sample must be an integer from 0 to 4095") from error
    if not 0 <= value <= MAX_SAMPLE:
        raise argparse.ArgumentTypeError("12-bit sample must be between 0 and 4095")
    return value


def generate_image(channel, pattern="constant", value=DEFAULT_SAMPLE):
    """Return row-major uint16-le bytes for one IR or depth image."""
    if channel not in ("ir", "depth"):
        raise ValueError("channel must be ir or depth")
    if pattern not in PATTERNS:
        raise ValueError("unknown pattern: " + pattern)
    if not isinstance(value, int) or not 0 <= value <= MAX_SAMPLE:
        raise ValueError("12-bit sample must be between 0 and 4095")
    if pattern == "constant":
        return struct.pack("<H", value) * (WIDTH * HEIGHT)

    pixels = bytearray(WIDTH * HEIGHT * 2)
    for y in range(HEIGHT):
        for x in range(WIDTH):
            if pattern == "gradient":
                # Different axes make it easy to distinguish the two sources.
                sample = (x * MAX_SAMPLE // (WIDTH - 1) if channel == "ir"
                          else y * MAX_SAMPLE // (HEIGHT - 1))
            else:
                white = ((x // 40) + (y // 40)) % 2
                if channel == "depth":
                    white = 1 - white
                sample = MAX_SAMPLE if white else 0
            struct.pack_into("<H", pixels, (y * WIDTH + x) * 2, sample)
    return bytes(pixels)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=Path("."),
                        help="directory for aditof-ir.bin and aditof-depth.bin (default: current directory)")
    parser.add_argument("--pattern", choices=PATTERNS, default="constant",
                        help="constant (default), IR horizontal/depth vertical gradient, or inverse checkerboards")
    parser.add_argument("--ir-value", type=sample_argument, default=DEFAULT_SAMPLE,
                        help="constant IR sample, 0..4095 (default: 320)")
    parser.add_argument("--depth-value", type=sample_argument, default=DEFAULT_SAMPLE,
                        help="constant depth sample, 0..4095 (default: 320)")
    parser.add_argument("--force", action="store_true", help="overwrite existing output files")
    args = parser.parse_args(argv)

    outputs = [(channel, args.output_dir / ("aditof-" + channel + ".bin"), value)
               for channel, value in (("ir", args.ir_value), ("depth", args.depth_value))]
    # Check both files before writing either, avoiding a partial pair on reruns.
    if not args.force:
        for _, path, _ in outputs:
            if path.exists() or path.is_symlink():
                parser.error(str(path) + " already exists; use --force to overwrite")
    try:
        args.output_dir.mkdir(parents=True, exist_ok=True)
        for channel, path, value in outputs:
            with path.open("wb" if args.force else "xb") as output:
                output.write(generate_image(channel, args.pattern, value))
            print(f"Wrote {path}: {WIDTH}x{HEIGHT}, {WIDTH * HEIGHT * 2} bytes, "
                  f"12-bit samples in uint16-le ({args.pattern})")
    except OSError as error:
        parser.exit(1, f"Cannot write camera images: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
