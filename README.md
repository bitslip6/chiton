# Chiton Project

Toolchain | GCC | Devops Status
-- | -- | --
Yocto 2.5 | 7.3.0 |![ubuntu1804_yocto_25](https://github.com/bitslip6/chiton/workflows/ubuntu1804_yocto25/badge.svg)
Yocto 3.1 | 9.3.0 |![ubuntu1804_yocto_31](https://github.com/bitslip6/chiton/workflows/ubuntu1804_yocto31/badge.svg)
Ubuntu 18.04 | 7.5.0 | ![ubuntu1804_intel](https://github.com/bitslip6/chiton/workflows/ubuntu1804_intel/badge.svg)



**NOTE:** This is ported from older code bases and could use some code cleanup.

* [Dependencies]
    - [Submodules]
* [Building the Project and Application]
    - [LVGL and Drivers]
    - [Build Output Structure]
    - [Binary Execution]

<br>
---
<br>

### Compile
* git submodule init ; git submodule update
* git fetch -r
* make lvgl
* make

## Dependencies
[Dependencies]: #dependencies

* `make`
* `libpng-dev`
* zlib development headers (`zlib1g-dev` on Debian/Ubuntu, `zlib` on Arch Linux)
* SIMDe headers (`libsimde-dev` on Debian/Ubuntu, `simde` on Arch Linux)
* SDL2 development files for desktop presentation (`libsdl2-dev` on Debian/Ubuntu,
  `sdl2` on Arch Linux), plus `pkg-config`; optional for a no-SDL build

Install them through the package manager.

### Portable SIMD (ARM and x86)

The NEON code uses `include/neon_compat.h` and SIMDe's native aliases.
SIMDe uses ARM NEON on supported ARM targets, x86 SIMD on supported x86
targets, and portable fallbacks elsewhere. No extra library needs linking.

If SIMDe is not installed system-wide, download a
[SIMDe checkout](https://github.com/simd-everywhere/simde) and point the build
at its root (the directory containing `simde/`):

```sh
make SIMDE_INCLUDE_DIR=/path/to/simde
# Or, for the CMake build:
cmake -S . -B _build/cmake -DSIMDE_INCLUDE_DIR=/path/to/simde
```

Rebuild existing graphics objects after changing SIMD headers or flags.
This removes the ARM-only SIMD requirement; camera and other application
library dependencies are still required for a full build.

### Submodules
[Submodules]: #submodules

This project uses submodules for bringing in some 3rd party source code:

* [FunctionalPlus](https://github.com/Dobiasd/FunctionalPlus)
* [lvgl](https://github.com/lvgl/lvgl)
* [lv_drivers](https://github.com/lvgl/lv_drivers)

Make sure that submodules are up to date before attempting to compile any code:

```
git submodule init
git submodule update
```

**NOTE:** If there are strange build errors about header files not being found,
update the submodules again to make sure they were checked out after switching
branches, etc.

## Building the Project and Application
[Building the Project and Application]: #building-the-project-and-application

Building the project and application binaries can be run with a simple `make`
call. The Makefile detects the Linux host architecture and uses native
`/usr/bin/gcc` and `/usr/bin/g++` by default, even if a cross-toolchain was
previously loaded into the shell. Objects are isolated under
`_build/<OS>-<architecture>/sdl<0|1>-stub<0|1>-<debug>/`, preventing stale
objects when switching camera/display build options. Run `make print` to inspect the configuration.
The output executable is `bin/chiton` (also with `DEBUG_MODE=ON`), not
`bin/chiton-debug`.

Compiler and architecture overrides must be explicit make arguments:

```sh
make DEBUG_MODE=ON                  # Native debug build
make CXX=clang++ CC=clang           # Native build with Clang
make ARCH=aarch64 CC=aarch64-linux-gnu-gcc CPP=aarch64-linux-gnu-g++
```

The cross-build still needs matching target headers and libraries. Native
builds require native versions of those dependencies. Non-Linux hosts are
currently unsupported because the application depends on Linux APIs.

A `make clean` call will wipe the `chiton` source code object files while
keeping built dependency libraries around for faster recompile.

## File-backed camera stub

Desktop Make builds use `src/stubs/aditof.cpp` instead of the hardware SDK.
It builds a static `libaditof.a` and exposes one simulated camera. On camera
initialization it reads `aditof-ir.bin` and `aditof-depth.bin` from the current
working directory. Missing default files are created with every pixel set to
320; existing files are never overwritten.

Each file must contain exactly 640×480 unsigned 16-bit little-endian pixels
(614,400 bytes), row-major, with no header. Camera samples are 12-bit values
(0–4095), right-aligned in each 16-bit word; the upper four bits are zero.
Files are loaded at initialization
and the same images are returned for every frame, without calibration or gain
adjustments. To select other files:

```sh
make
ADITOF_STUB_IR=/path/to/ir.bin ADITOF_STUB_DEPTH=/path/to/depth.bin ./bin/chiton
make test-aditof-stub
```

Generate both default image files with the dependency-free Python tool:

```sh
python3 tools/generate_stub_images.py                  # Both constant 320
python3 tools/generate_stub_images.py --pattern gradient --force
python3 tools/generate_stub_images.py --pattern checkerboard --output-dir /tmp/chiton-images
python3 tools/generate_stub_images.py --ir-value 0 --depth-value 4095 --force
python3 -m unittest discover -s tests -p test_generate_stub_images.py
```

The tool writes `aditof-ir.bin` and `aditof-depth.bin` to the current directory
(or `--output-dir`) and refuses to overwrite either file unless `--force` is
specified. Gradients span 0–4095 horizontally for IR and vertically for depth;
checkerboards use opposite black/white 40-pixel tiles. Constant values may also
be given in hexadecimal, such as `--depth-value 0xFFF`. Run Chiton from the
output directory or select the generated files with `ADITOF_STUB_IR` and
`ADITOF_STUB_DEPTH`.

Missing explicitly selected files and incorrectly sized files cause camera
initialization to fail. Controls and AFE register writes are simulated; EEPROM,
remote cameras, low-level device capture and SDK calibration are unavailable.
Frame callbacks are synchronous. Stub builds skip the hardware temperature
monitor and debug-time system core configuration, and discover desktop network
interface names without requiring `ifconfig`. Hardware-only HTTP actions (I2C,
Wi-Fi setup, software upgrades) remain intended for embedded targets.

Use `make USE_ADITOF_STUB=0` for the real SDK or `make USE_ADITOF_STUB=1` to
force the simulator on ARM. CMake also supports `-DUSE_ADITOF_STUB=ON/OFF`.

## Display backends

The default `auto` mode keeps `/dev/fb1` when present, otherwise tries an SDL2
window. Without a graphical session (or if SDL fails), it logs the reason and
continues headless with camera capture and HTTP running. Permission errors,
invalid framebuffer geometry/format, and ioctl/mapping failures are reported
as errors rather than silently falling back. Framebuffers must be packed
truecolor 16/24/32-bit, at least 320×240; stride and virtual offsets are respected.

For TFT throughput qualification, `cross-build.sh` includes a standalone display
benchmark and delay/region sweep. See [TFT refresh qualification](docs/display-refresh-benchmark.md)
for driver controls, SPI versus parallel transport, and measuring actual panel FPS.
For kernel-side transfer timing, see [SPI driver diagnostics](docs/spi-display-diagnostics.md).
For software-only spidev word-size/transfer comparisons, see
[direct SPI benchmark](docs/direct-spi-benchmark.md); stop flexfb first.

```sh
make USE_SDL2=1                    # Require SDL2 via pkg-config
./bin/chiton --display desktop     # Explicit failures return nonzero
./bin/chiton --display framebuffer
./bin/chiton --display headless
CHITON_DISPLAY=auto ./bin/chiton    # Command-line option overrides this default
make USE_SDL2=0                    # Framebuffer/headless build, no SDL dependency
cmake -S . -B _build/cmake -DUSE_SDL2=ON -DUSE_ADITOF_STUB=ON
cmake --build _build/cmake
# Use -DUSE_SDL2=OFF for a no-SDL CMake build.
```

Make enables SDL automatically when `pkg-config` finds it. CMake enables it by
default and requires the dependency unless explicitly disabled. CMake requires
version 3.10 or newer and builds the current application without legacy LVGL.
The resizable desktop window preserves the 4:3 preview aspect ratio. Escape,
window-close, SIGINT, and SIGTERM stop capture/HTTP workers and join them before
releasing display resources. HTTP tries port 80, then 8080; failure on both ports
logs the final port/error and exits cleanly.

Fonts, HTTP assets, startup animation, configuration, and optional `api.so` are
resolved from the installed resource root, then executable-relative locations
(including the repository's `release/`), with a final current-directory fallback.
Make's installed root defaults to `/usr/local/chiton/current` and can be changed
with `CHITON_INSTALL_DIR=/path`; CMake uses its install prefix's `bin` directory
and offers `-DCHITON_INSTALL_DIR=/path`. `CHITON_ASSET_DIR=/path/to/files` overrides
font/HTTP assets. Missing or corrupt PNGs are nonfatal: status text falls back to
a built-in 5×7 font, and unavailable startup animation is skipped. Missing or
unloadable optional plugins are diagnosed without stopping capture.
`CHITON_RESOURCE_DIR=/path/to/release` strictly scopes general resources to that
release; `CHITON_CONFIG_FILE=/path/to/chiton.json` explicitly selects device
configuration. For a versioned AArch64 application artifact including assets and
private libraries, see [deployment bundles](docs/deployment-bundle.md).
The browser now prefers a bounded persistent JPEG WebSocket stream, with a
30-FPS paint target and snapshot fallback. See [image streaming](docs/image-streaming.md)
for metrics, protocol details and transport/application rollback.

```sh
make test-display test-aditof-stub
python3 tests/desktop_runtime.py    # SDL stub build; ports 80/8080 must be free
ctest --test-dir _build/cmake --output-on-failure
```

The display tests cover sample clamping (including 0/65535), framebuffer stride
and format validation, asset fallback, and repeated SDL close/Escape handling.
Runtime tests use temporary IR/depth files, SDL's dummy driver, blocked ports,
and idle HTTP clients. Successful privileged port-80 binding is checked only
when the test process has permission. Real embedded display and camera hardware
still require on-target validation.

## HTTP and calibration correctness

HTTP handlers now return one owned response through a common CRLF/length/status
serializer. Errors use JSON `{ "error": "message", "status": 400 }` (with the
actual HTTP status). Legacy GET routes remain supported; HEAD is supported for
static assets, and unsupported methods return 405 without executing actions.

`/depth/setting/depth` selects DEPTH and returns `value: true`;
`/depth/setting/ir` selects IR and returns `value: false`. Both include a
`frame_type` string. `/depth/setting` reads the current selection. The checkbox
labeled IR is checked only for IR. `/range/setting` reads the color limits;
`/range/setting/<min>/<max>` validates and applies both atomically. The old
individual min/max routes remain available, with `0 <= min < max <= 9000`.

Startup, HTTP save/load, and automatic calibration use the same file:
`CHITON_CALIBRATION_FILE`, or `calibration.bin` in the working directory. New
saves are atomic and include a version/shape/ABI header plus CRC32. Complete
legacy native-layout files are still accepted; new files cannot be read by old
Chiton binaries. Invalid loads preserve live data; invalid startup files are
reported and ignored in favor of default calibration. This is not a portable
cross-ABI file format. See [tests/SAFETY.md](tests/SAFETY.md) for regression commands.

Simulator builds simulate register reads/writes through the SDK and reject
embedded-only Wi-Fi, gateway, upgrade, and diagnostics actions with HTTP 501.
Real hardware behavior still requires target testing.

### Browser live-view recovery

Both shipped UI copies bound frame, depth, and temperature reads to 10 seconds,
including response bodies and image decoding. Each stream permits one in-flight
read and schedules its next poll after completion, with failure backoff capped
at eight seconds (never faster than its normal interval). The status strip shows
connecting/live/stale/disconnected/paused state and time since the last successful
UI update; this is not a sensor capture timestamp. Failed reads preserve the
previous display and recover without a page reload.

Hidden/page-away views stop reads; returning resumes them. Mode/selection changes
cancel obsolete reads, and temporary image URLs are always released. Color-mode
settings get one bounded attempt and are not automatically retried. Run
`node tests/web_ui.js` for deterministic regression checks and optionally
`node tests/web_stream_browser.js` for a local mock-sensor Chromium test.

### Measurement correctness and compatibility

Legacy `/get/value/<x>/<y>` requests are now stateless single-pixel measurements;
rectangle requests report spatial population statistics of one frame. Explicit
`/get/value/client/<id>/<x>/<y>` requests opt into isolated temporal histories of
up to 60 distinct observed frames. The UI creates a new identity per tab and
shows units, sample scope/count, and actual values without a browser low-pass
blend. Repeated polls of the same frame do not manufacture new samples.

Statistics use wide sums and double squared deviations. Frame IDs, host receipt
times, units, and processing state are returned in JSON and snapshot-bound export
headers, never in measurement pixels. `/frame/camera` reads this metadata;
`/raw/camera` remains the legacy processed uint16 frame, not necessarily untouched
SDK data. Legacy plugins retain their final-element counter on a private copy,
while public last-pixel measurements are preserved.

See [docs/measurements.md](docs/measurements.md) for history limits, invalid-sample
policy, RMS/unit semantics, plugin compatibility, and regression commands.

## Desktop release gates

```sh
python3 tests/release_gate.py --all --output /tmp/chiton-release-checks --jobs 2
```

Requires a fresh output directory, CMake/build dependencies, Node and Chromium.
Runs separate normal/ASan/UBSan/TSan headless simulator CTest/HTTP/UI gates and
native browser recovery/downloads for both shipped UI copies. Missing tools or
failed checks stop the gate; logs survive fixture cleanup. `mkrelease.sh` requires
these checks before copying the target binary, without executing that binary.
CI runs the same gates on pushes/PRs and has passed in GitHub. `master` now
requires the GitHub Actions `Web release gate` with up-to-date checks and
administrator enforcement; no review approvals are required. Use a feature
branch and pull request for new changes. The operator reports successful
application operation on hardware (smoke test); full hardware qualification
remains manual/outstanding. See [docs/release-gates.md](docs/release-gates.md)
for scope, evidence, protection settings and the hardware checklist.

### Global modes and frame freshness (F09)

Presentation mode remains device-global. Homepage and frame/export reads never
change it; both UIs follow the acknowledged global mode automatically. Select
cloud mode (4) explicitly before binary-cloud or OBJ downloads. Incompatible
outputs return JSON 409 with `current_mode`; disabled/not-ready capture and empty
JPEGs return 503. Both capture-control route families now use checked SDK
start/stop, including startup acquisition.

`/frame/camera`, `/raw/camera`, `/image/camera`, `/pointcloud/camera` and
`/pointcloud_obj/camera` accept `?after=<frame_id>&timeout_ms=<0..1000>`.
They return a snapshot newer than that ID or 504; the default wait budget is
1,000 ms, including frame-lock contention. Ordinary reads return the latest
available compatible snapshot without requesting a new capture. Cloud capture
now publishes continuously. Shutdown cancels waits; the server remains
sequential until F08, so this is not a concurrent-service or whole-transfer
latency guarantee.

Frame JSON and export headers include monotonic host-receipt age. Both UIs show
that age separately from their last successful UI update, and recover when a
server restart resets frame IDs. These are not hardware exposure timestamps.
See [docs/http-contracts.md](docs/http-contracts.md) for exact fields and limits.

**Recommended next:** F08's bounded connection/job architecture. F09 is
implemented; hardware lifecycle qualification remains manual. See
[docs/web-server-review.md](docs/web-server-review.md) for current status.

## LVGL and Drivers
[LVGL and Drivers]: #lvgl-and-drivers

LVGL is legacy code and is not needed by the current preview/status UI.

To explicitly build the LVGL library:

```
make lvgl
```

Similarly, to clean up the built library for a recompile:

```
make cleaner
```

This will also remove built `chiton` source objects so that they will be
rebuilt and linked with the new library code.

### Build Output Structure
[Build Output Structure]: #build-output-structure

Build outputs for all dependency libraries and core project files under kept
under the same build directory, `_build`, for tidiness and ease of cleaning:

```
chiton
    |
    +--> _build/
    |       |
    |       +--> lvgl/              (1)
    |       |       |
    |       |       +-->lib/        (2)
    |       |       +-->obj/        (3)
    |       +--> obj/               (4)
    |       +--> ...
    |
    +--> bin/                       (5)
    +--> lib/                       (6)
```

As referenced above:


1. Build folder for a dependency library, such as `lvgl`.
2. Subdirectory containing built `.so` files for the library
3. Built object files for the library
4. Built object files for the core project, such as those in `chiton/src`
5. Binary outputs from `Makefile`
6. Common library directory. Dependency libraries, such as `liblvgl.so` are
    symbolically linked here to the library built by their associated
    `Makefile.<library>`. The main `Makfile` then only has to link against a
    single folder for linking all library files into the project build.

### Binary Execution
[Binary Execution]: #binary-execution

The main application no longer requires `liblvgl.so`. Hardware SDK or legacy
binaries using local shared libraries may still need `LD_LIBRARY_PATH`:

```
# From the repo's root directory
export LD_LIBRARY_PATH=$PWD/lib:$LD_LIBRARY_PATH
```
