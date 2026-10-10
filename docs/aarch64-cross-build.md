# AMD-hosted AArch64 cross build

## SD-card sysroot build (target-compatible dependencies)

The SD card's ext4 rootfs was mounted read-only with journal recovery disabled
before copying development files. Its libraries provide **glibc 2.27** and
**GLIBCXX_3.4.24**. An AMD-hosted **GCC 16.1** build using the recovered GCC 7.3
C++ headers, startup objects and runtime libraries succeeded with native NEON,
the real camera SDK, and SDL disabled.

The resulting binary requires only **GLIBC_2.27**, **GLIBCXX_3.4.22**, and
**CXXABI_1.3.9** at most. A read-only ELF audit checked all **15** files in its
dependency closure, AArch64 architecture, required strong symbols, absence of
embedded RPATH, and absence of distribution runtime objects in the linker map.
This establishes library/architecture compatibility with the card, **not** a
hardware or functional runtime qualification. No ARM executable or hardware
test was run, and no application files were copied onto the card.

Persistent local build workspace (outside the repository):

```text
/home/cory/Work/chiton-cross/yocto-sd/build/chiton
/home/cory/Work/chiton-cross/yocto-sd/sysroot/
/home/cory/Work/chiton-cross/yocto-sd/stage/
/home/cory/Work/chiton-cross/yocto-sd/rebuild.sh
/home/cory/Work/chiton-cross/yocto-sd/audit-runtime.py
/home/cory/Work/chiton-cross/yocto-sd/runtime-audit.txt
/home/cory/Work/chiton-cross/yocto-sd/chiton-link.map
/home/cory/Work/chiton-cross/yocto-sd/elf-report.txt
```

To rebuild this workspace on AMD, without the card connected:

```sh
bash /home/cory/Work/chiton-cross/yocto-sd/rebuild.sh
```

The local helper uses a persistent native CMake distribution under
`/home/cory/Work/chiton-cross/tools/`. Alternatively set `CMAKE` to a native
CMake executable. It does not install, deploy, execute ARM programs or run
CTest. Its absolute workspace/source paths are specific to this machine.

For a coherent application release rather than a binary-only copy, use
[`tools/package_aarch64.py`](../tools/package_aarch64.py). See
[deployment-bundle.md](deployment-bundle.md) for packaging, external device
configuration, operator-controlled activation/rollback and the distinction
between an application bundle and a bootable Yocto image.

### Recovering and configuring this sysroot

Copy the card's `/usr/include`, `/usr/lib`, `/lib` and
`/usr/local/include/simde` into the same relative locations under a local
sysroot, preserving symlinks. Normalize absolute symlinks **in the copy only**
to relative links within that copy; otherwise they can escape into AMD's root.
Do not use `rsync --safe-links` for the final copy: valid development symlinks
can refer outside a single copied subtree. The local recovery also copied the
card's current `libaditof.so.1.5.0` into a staging prefix; its SHA-256 matches
`release/libaditof.so.1.5.0` exactly.

Select `cmake/toolchains/aarch64-yocto-sd.cmake` with:

```sh
-DCMAKE_SYSROOT=/home/cory/Work/chiton-cross/yocto-sd/sysroot
-DCHITON_TARGET_PREFIX=/home/cory/Work/chiton-cross/yocto-sd/stage
```

The complete dependency/configuration arguments are recorded in the local
`rebuild.sh`. In contrast to the rolling-distribution experiment below, PNG,
JPEG, zlib and I2C link to the **card's shared libraries**, not freshly built
libraries. The required zlib development link is `sysroot/usr/lib/libz.so`.

The toolchain deliberately selects:

- Modern compiler intrinsic headers (including `arm_neon.h`).
- Card libc headers and C++ 7.3.0 headers under
  `usr/include/c++/7.3.0/aarch64-poky-linux`.
- Card `Scrt1.o`, `crti.o`, `crtn.o`, GCC 7.3 CRT objects and libgcc.
- Card libstdc++, libm and other runtime libraries.
- Baseline ARMv8-A and `-mno-outline-atomics`; GCC 7's runtime lacks the newer
  outlined atomic helpers.

It suppresses implicit distribution headers with `-nostdinc` and selects the
old startup/library paths with `-B`/`-L`. This installed cross compiler would
otherwise retain distribution header/runtime paths despite `--sysroot`.
A small linker script `usr/lib/libatomic_asneeded.so` was added **only to the
copied sysroot** for the modern driver's optional atomic-library lookup:

```text
INPUT ( AS_NEEDED ( -latomic ) )
```

It selects the card's existing libatomic and contains no executable code.
Inspect the linker map and ELF requirements after compiler/toolchain changes;
this recovered setup is tested for this image, not every Yocto release.

Before a later manual hardware test, stop the existing Chiton instance and use
an alternate binary name so the working release can be restored. Do not run
two camera-owning instances simultaneously. Deployment/starting the camera
is a separate action; this cross build did neither.

### Legacy SDK startup correction

The first target headless test exited after `LocalDevice::start` reported
`device already started`. SDK setup had already started streaming, while
Chiton's acquisition tracker was initially false; its extra `start()` returned
BUSY and caused an orderly `EXIT_ADI_ERROR` shutdown.

The first correction normalized SDK acquisition by stopping/restarting it.
The target then reported a select timeout, failure to receive a frame, and exit
251 (`EXIT_ADI_ERROR`, -5 modulo 256). The artificial stream cycle may disrupt
this legacy driver; it was removed rather than assuming the driver is healthy.

`cam::initialize_capture` now attempts start without a preliminary stop. If it
returns BUSY, acquisition is adopted **only after a real first-frame request
returns OK**. Missing verification, frame timeout, and other SDK errors remain
failures. A disabled startup still explicitly stops implicit SDK acquisition.
Ordinary API start requests still require OK. Startup failure responses appear
on stderr instead of being discarded. No blind BUSY acceptance or unbounded
retry was introduced.

The latest binary was rebuilt at the same persistent path and passed the
15-file dependency audit. Native simulator/focused gates passed 13/13, including
a fake-camera regression proving adoption performs no stop/restart and that
BUSY without a frame is rejected. That regression also passed explicit ASan,
UBSan and TSan instrumentation. The operator subsequently reported that the
camera stays running, but the web UI shows no image and floods legacy point
requests. See [image-transport-debug.md](image-transport-debug.md) for the reduced
manual JPEG diagnostic and independent one-second measurement polling. This is
not full hardware/image qualification; a dependency audit alone does not prove
that the sensor/driver delivers correct image content.

## Earlier distribution-sysroot experiment (not deployable to this card)

An experimental hardware-camera build succeeded with the installed Arch cross
compiler, **GCC/G++ 16.1.0**, GNU AArch64 sysroot **glibc 2.44**, and SIMDe **0.8.2**.
The complete application compiled and linked with native NEON enabled; no
`SIMDE_NO_NATIVE`, `SIMDE_NO_SHUFFLE_VECTOR` or Eigen vectorization workaround was
needed with this compiler. This confirms this combination compiles; it is not
hardware qualification or proof that GCC 12.5 fixes every older-toolchain issue.

- CPU baseline: ARMv8-A, no forced FP16 or i.MX8-specific CPU extension.
- Hardware camera SDK (`USE_ADITOF_STUB=OFF`), SDL disabled.
- Repository ARM `release/libaditof.so.1.5.0` and `release/libi2c.so` reused.
- PNG/JPEG/zlib are statically linked; camera/I2C/system C/C++ libraries remain
  dynamic. libunwind was staged to resolve the camera SDK's dependency.
- No ARM program, cross-built test, device action, install or remote copy was run.
  The normal `bin/chiton`, host packages and existing user edits were preserved.

The output requires symbol versions **GLIBC_2.43** and **GLIBCXX_3.4.32**. In
particular, this sysroot's `sqrtf`/`atan2f` bind to GLIBC_2.43. **Do not deploy this
binary to the 2020 Yocto image as-is or replace the device's libc to make it run.**
The CPU architecture being correct is not sufficient for runtime compatibility.

The tested output and build evidence are local temporary artifacts:

```text
/tmp/chiton-aarch64-C2NSef/build-reusable-toolchain/chiton
/tmp/chiton-aarch64-C2NSef/stage/
/tmp/chiton-aarch64-C2NSef/include/simde/
/tmp/chiton-aarch64-C2NSef/downloads/SHA256SUMS
/tmp/chiton-aarch64-C2NSef/logs/
/tmp/chiton-aarch64-C2NSef/build-experiment.sh
```

These paths are evidence from this machine, not files shipped by Git or a
portable deployment layout. The experiment script records the complete
source-build/staging procedure. Downloaded sources were zlib 1.3.2, libpng 1.6.58,
libjpeg-turbo 3.2.0 and libunwind 1.8.3, fetched over HTTPS from upstream GitHub
archives/releases. Local archive SHA-256 hashes are recorded for repeatability;
they are not independently verified release signatures.

## Build inputs

On the AMD build machine, the required inputs are:

- Native CMake and Make (not AArch64 versions of these tools).
- AArch64 GCC **and G++**, binutils, and a target sysroot with C/C++ development
  headers, runtime libraries and startup objects.
- SIMDe headers. Keep them in a dedicated include root containing `simde/`;
  do not expose the entire AMD `/usr/include` to the cross compiler.
- AArch64 PNG, JPEG, zlib and I2C headers/libraries, and the matching ADI SDK.
  SDL is not required for the embedded framebuffer/headless build.
- A target aditof CMake package providing `aditof::aditof`, or an imported target
  for the existing SDK as shown below.

Do not let native `pkg-config` resolve AMD libraries. Use target-only
`PKG_CONFIG_LIBDIR`, and target-only library/include/package search paths.
The new `cmake/toolchains/aarch64-linux-gnu.cmake` performs those CMake root
restrictions and verifies the compiler target. It uses the compiler's default
sysroot unless `CMAKE_SYSROOT` is explicitly supplied.

## Reproduce the application configuration

After preparing the dependency staging prefix, run from the repository root:

```sh
# Replace this prefix with your prepared AArch64 dependency staging directory.
prefix=/tmp/chiton-aarch64-C2NSef/stage
simde=/tmp/chiton-aarch64-C2NSef/include
build=_build/aarch64-cross

PKG_CONFIG_LIBDIR="$prefix/lib/pkgconfig" cmake -S . -B "$build" \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/aarch64-linux-gnu.cmake" \
  -DCHITON_TARGET_PREFIX="$prefix" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_SKIP_RPATH=ON \
  -DCHITON_INSTALL_DIR=/usr/local/chiton/current \
  -DUSE_ADITOF_STUB=OFF -DUSE_SDL2=OFF -DCHITON_RELEASE_TESTS=OFF \
  -DCHITON_DEFAULT_DISPLAY=framebuffer \
  -DSIMDE_INCLUDE_DIR="$simde" \
  -Daditof_DIR="$prefix/lib/cmake/aditof" \
  -DZLIB_INCLUDE_DIR="$prefix/include" -DZLIB_LIBRARY="$prefix/lib/libz.a" \
  -DPNG_PNG_INCLUDE_DIR="$prefix/include" -DPNG_LIBRARY_RELEASE="$prefix/lib/libpng16.a" \
  -DJPEG_INCLUDE_DIR="$prefix/include" -DJPEG_LIBRARY_RELEASE="$prefix/lib/libjpeg.a" \
  -DI2C_LIBRARY="$prefix/lib/libi2c.so" \
  -DCMAKE_EXE_LINKER_FLAGS="-Wl,-rpath-link,$prefix/lib"

cmake --build "$build" --target chiton --parallel 2
file "$build/chiton"
aarch64-linux-gnu-readelf -d -V "$build/chiton"
```

Build only the application target. Do not run CTest against a real-camera cross
build. Do not use `make clean` to switch architectures; it also overwrites assets.
Use a fresh build directory for compiler, sysroot or option changes.

The tested staging prefix contains these camera/I2C files and aliases:

```text
lib/libaditof.so.1.5.0
lib/libaditof.so.1.0 -> libaditof.so.1.5.0
lib/libaditof.so -> libaditof.so.1.0
lib/libi2c.so.0                 # copied from release/libi2c.so
lib/libi2c.so -> libi2c.so.0
```

The temporary `lib/cmake/aditof/aditofConfig.cmake` imported the existing SDK:

```cmake
add_library(aditof::aditof SHARED IMPORTED)
set_target_properties(aditof::aditof PROPERTIES
  IMPORTED_LOCATION "/absolute/staging/prefix/lib/libaditof.so.1.5.0"
  IMPORTED_SONAME "libaditof.so.1.0"
  INTERFACE_INCLUDE_DIRECTORIES "/absolute/chiton/source/include")
```

`CMAKE_SKIP_RPATH=ON` prevents temporary AMD staging directories from being
embedded in the binary's runtime search path. The deployed system must provide
compatible libraries with the listed SONAMEs via its normal loader setup.

## General compatibility guidance

A matching Yocto SDK is preferable when available. For this card, the target's
development sysroot and runtime version information were recovered as above. Prefer the SDK's own CMake toolchain/environment setup.
Its sysroot must supply **C++ headers and runtime libraries as well as libc**,
plus PNG/JPEG/zlib/I2C and the ADI SDK. Rebuild dependencies and the application
against that environment; do not combine the rolling-distribution dependency
stage above with the old target sysroot.

A newer compiler must be built/configured for the older target runtime, or be
made to use a compatible target C++ library/header/startup configuration.
`--sysroot` alone does not guarantee that the compiler's own libstdc++/libgcc
or default library search paths match the old image.

On the target, useful read-only information is:

```sh
getconf GNU_LIBC_VERSION
uname -m
uname -r
g++ -print-file-name=libstdc++.so.6
```

On AMD, inspect the target runtime libraries with `aarch64-linux-gnu-readelf -V`
and compare them with the final application **and every staged shared library**.
After those checks pass, deployment and a separately approved hardware smoke
check can proceed. Cross compilation alone does not validate acquisition,
calibration, timing, physical display or administrative actions on the device.
