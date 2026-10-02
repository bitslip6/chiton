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
`_build/<OS>-<architecture>/`. Run `make print` to inspect the configuration.
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
(614,400 bytes), row-major, with no header. Files are loaded at initialization
and the same images are returned for every frame, without calibration or gain
adjustments. To select other files:

```sh
make
ADITOF_STUB_IR=/path/to/ir.bin ADITOF_STUB_DEPTH=/path/to/depth.bin ./bin/chiton
make test-aditof-stub
```

Missing explicitly selected files and incorrectly sized files cause camera
initialization to fail. Controls and AFE register writes are simulated; EEPROM,
remote cameras, low-level device capture and SDK calibration are unavailable.
Frame callbacks are synchronous. This replaces only the camera SDK, not other
hardware-specific application operations.

Use `make USE_ADITOF_STUB=0` for the real SDK or `make USE_ADITOF_STUB=1` to
force the simulator on ARM. CMake also supports `-DUSE_ADITOF_STUB=ON/OFF`.

## LVGL and Drivers
[LVGL and Drivers]: #lvgl-and-drivers

LVGL source code is compiled into a shared library that is ultimately linked
into `chiton` source code.
This library is a prerequisite to the `chiton` code and is always built ahead
of it when calling `make`.

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

Currently, executing a built binary like `./bin/test` will fail because it
can't see the `liblvgl.so` library.
A present workaround is adding the output path to the `LD_LINKER_PATH`
environment variable:

```
# From the repo's root directory
export LD_LIBRARY_PATH=$PWD/lib:$LD_LIBRARY_PATH
```
