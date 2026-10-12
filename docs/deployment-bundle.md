# AArch64 application deployment bundle

A cross-compiled executable is **not** a release. Deploy the executable, matching
web UI/assets, SDK/application libraries and launcher as one versioned unit.
This bundle updates the Chiton **application on the existing Yocto SD image**;
it is not an OS installer or a bootable disk image.

## Build on AMD

Requirements: the recovered `chiton-cross/yocto-sd` workspace, installed AMD-hosted
AArch64 tools, Python 3.12+ and the source checkout. No mounted card is needed.

```sh
cd /home/cory/Work/chiton
python3 tools/package_aarch64.py \
  --workspace /home/cory/Work/chiton-cross/yocto-sd \
  --output /home/cory/Work/chiton-cross/dist \
  --name rel_cross_MY_VERSION
```

To include the cross-compiled diagnostic programs and tools in the **full**
application bundle, use the repository-root wrapper instead:

```sh
cd /home/cory/Work/chiton
./cross-build.sh --name rel_perf_phase0_v1
```

It accepts the same `--workspace`, `--output`, and `--name` paths/names shown above.
`JOBS=4 ./cross-build.sh ...` selects parallelism; default is two jobs. Existing
artifacts are rejected. Run `./cross-build.sh --help` for usage. The wrapper calls
`tools/package_aarch64.py --include-benchmarks`; ordinary packager calls remain
application-only.

The recovered workspace helper builds/audits Chiton. The packager then enables
`BUILD_PIPELINE_BENCHMARKS` in that same cross-build tree and explicitly builds
both diagnostic targets. Each benchmark gets its own linker map; architecture,
RPATH, dependency closure, strong symbols/runtime versions and link-input
provenance are checked before packaging. No benchmark is executed on the host.
Their non-system dependencies join the release's private `lib/` directory;
program hashes, runtime version requirements and map hashes enter the manifest.

Additional layout when benchmarks are included:

```text
benchmarks/
  image_pipeline_benchmark
  calibration_pipeline_benchmark
  run.sh
  run-all.sh
  README.md
  pipeline-performance-metrics.md
```

Experimental SIMD paths are independently **default-off**. To qualify a separate
variant, add `--simd-greyscale`, then only after its acceptance separately add
`--simd-calibration`. The packager explicitly selects both options for every
bundle (no stale cache inheritance), rebuilds Chiton when needed and records the
choices in `manifest.json`. Benchmarks compile with those same choices. See
[pipeline-simd.md](pipeline-simd.md) for exactness gates and external tests.

The experimental illumination controller is separately **default-off**. An external
`--experimental-illumination` build flag compiles availability; runtime remains
legacy unless both `--illumination-policy experimental-depth-proxy` and an explicit
`--illumination-initial p0,p1,p2,p3` are supplied. The manifest records availability
and assumptions; instructions are packaged under `docs/`. Omitting the build flag
explicitly resets availability OFF, even in a reused workspace. See
[illumination-controller-integration.md](illumination-controller-integration.md)
for proxy limitations, external tests and hardware/rollback gates. A new archive
can contain an unchanged executable after an incremental no-op build; compare its
binary hash and manifest, not just the release name.

All these files enter `SHA256SUMS`. Run the same archive validation command below;
it additionally checks benchmark files/hashes and launcher syntax without ARM
execution. On the compatible target, after stopping Chiton for controlled CPU
comparisons, run from the extracted release:

```sh
./benchmarks/run.sh image grey16 1000
./benchmarks/run.sh calibration calibrate 1000 1
./benchmarks/run-all.sh 1000
```

These launchers set private library paths but do not load saved configuration,
stop services, or access the camera/SDK. See the bundled benchmark README for
reporting and [pipeline-performance-metrics.md](pipeline-performance-metrics.md)
for fixture/timing limitations. Do not run these AArch64 programs on AMD.

Use a new name per build; existing artifacts are not overwritten. The command:

1. Runs the matching-old-runtime cross rebuild, keeping native NEON/ARMv8-A.
2. Audits the AArch64 dependency closure, required strong symbols, library
   provenance and absence of distribution runtime/RPATH leakage.
3. Packages the freshly built executable, current assets and non-system
   libraries from the audited stage/SD sysroot, under their exact SONAMEs.
4. Adds a launcher, build/runtime manifest and file/archive SHA-256 checksums.

It does **not** execute ARM code, copy to a device, alter services or write the
card. It never packages the stale prebuilt `release/chiton-debug`, `dhcpd`, or
blindly archives the historical `release/` provisioning tree.

Check a generated artifact locally without executing ARM code:

```sh
python3 tests/test_aarch64_package.py \
  /home/cory/Work/chiton-cross/dist/rel_cross_MY_VERSION.tar.gz
```

The initial verified artifact is:

```text
/home/cory/Work/chiton-cross/dist/rel_cross_bundle_20261005_v2.tar.gz
/home/cory/Work/chiton-cross/dist/rel_cross_bundle_20261005_v2.tar.gz.sha256
```

For the persistent JPEG transport, the new tested artifact is
`/home/cory/Work/chiton-cross/dist/rel_stream_20261005.tar.gz` (with its `.sha256`
sidecar). Keep the earlier v2 bundle intact for rollback. See
[image-streaming.md](image-streaming.md) for the new protocol, metrics and
operator smoke-test/rollback instructions.

## Layout

```text
rel_cross_MY_VERSION/
  chiton.bin                    # Fresh AArch64 ELF, including debug evidence
  run.sh                        # Sets resource/config/calibration/library paths
  chiton -> run.sh               # Compatibility entry point, not another binary
  chiton.sh -> run.sh            # Application-only compatibility start script
  app.html -> files/app.html
  files/                        # Current UI, debug.html, JS/CSS/fonts/images
  jabil/                        # Startup animation frames
  lib/                          # SDK, PNG/JPEG/zlib/I2C/unwind/atomic dependencies
  defaults/chiton.example.json   # Example only, never auto-installed as device config
  manifest.json
  SHA256SUMS
  DEPLOY.txt
```

The existing image must supply its compatible glibc 2.27 loader/system libraries,
GCC 7.3 libstdc++/libgcc, kernel, firmware, device drivers, device permissions and
network/boot integration. **Do not replace these with AMD distribution runtime
files.** The manifest records exact system-library hashes used for the audit.

`api.so` is explicitly omitted: the repository has a legacy AArch64 binary but
no corresponding API source to rebuild. Chiton supports running without this
optional plugin. This bundle is therefore **not** a replacement for a factory
profile that requires its optional callbacks. Qualify such a profile separately
before including an old plugin; an ELF ABI check alone does not prove callback
structure/semantic compatibility. Old alternate UI pages are also excluded.

## Device configuration stays outside releases

Each device keeps its own state under:

```text
/usr/local/chiton/shared/chiton.json
/usr/local/chiton/shared/calibration.bin   # If calibration exists for this device
```

The launcher refuses startup if `chiton.json` is missing. It will not silently
install generic factory measurements/calibration. `CHITON_STATE_DIR` can select
another state directory. The launcher explicitly sets:

- `CHITON_RESOURCE_DIR` to the new release (no fallback to old installed assets/plugins).
- `CHITON_ASSET_DIR` to its `files/` directory.
- `CHITON_CONFIG_FILE` to the shared device configuration.
- `CHITON_CALIBRATION_FILE` to shared calibration, including atomic writes.
- `LD_LIBRARY_PATH` to its private `lib/` only.

## Operator deployment and rollback

The following steps are performed **by the operator**, not by the build command.
Replace `TARGET` and `MY_VERSION`. Use a console/rollback route and first stop
the existing camera-owning Chiton service/process using the device's actual
service manager. Do not run two camera owners concurrently.

Transfer both files from AMD:

```sh
scp /home/cory/Work/chiton-cross/dist/rel_cross_MY_VERSION.tar.gz \
    /home/cory/Work/chiton-cross/dist/rel_cross_MY_VERSION.tar.gz.sha256 \
    root@TARGET:/tmp/
```

On the device, verify and extract as a **new** sibling release, never over
`current` or an existing release:

```sh
cd /tmp
sha256sum -c rel_cross_MY_VERSION.tar.gz.sha256
# Continue only if checksum verification succeeded.
cd /usr/local/chiton
test ! -e rel_cross_MY_VERSION && tar xzf /tmp/rel_cross_MY_VERSION.tar.gz
cd rel_cross_MY_VERSION
sha256sum -c SHA256SUMS
```

Preserve/back up the device's actual current `chiton.json` and active calibration
file before starting. If creating `shared/` for the first time, copy the existing
device files there; do not overwrite existing shared state or use example
configuration unless deliberately commissioning an uncalibrated device. The
active calibration may come from a prior `CHITON_CALIBRATION_FILE` override or
working directory, not necessarily `current/calibration.bin`.

Before changing `current`, run a controlled manual smoke test:

```sh
/usr/local/chiton/rel_cross_MY_VERSION/run.sh --display headless
```

This uses the new UI/resources even while `current` still points at the rollback
release. Close old UI tabs and use `/debug.html?image-debug=1` on the HTTP port
reported by this process. Image delivery/framebuffer qualification remains a
separate hardware test.

Only after stopping that manual test and accepting it, activate the new release
using the device's supported atomic symlink replacement procedure and restart
its service. Keep the previous release intact. The service should invoke
`current/run.sh` or its compatibility `current/chiton`/`current/chiton.sh` entry
point, **not** `chiton.bin` directly.

**Review boot integration before activation:** historical `rc.local` can invoke
`current/tft_device.sh` and `current/mkgpio.sh`; the old `chiton.sh` also started
radio/DHCP services. Those OS/board provisioning hooks are not in this application
bundle and its new launcher does not configure networking. Keep those hooks in
the established OS or pinned old release, or deliberately adapt the boot service
with console access. Do not run the old `setup.sh`/`upgrade.sh` as an application
update installer.

Rollback restores the old `current` link/service command. Back up shared state
as well: rolling back code does not undo calibration/configuration changed during
testing. SHA-256 detects corruption, not artifact authenticity; use a trusted
transfer/build source.

## If you mean a complete bootable SD image

That needs either:

1. The original Yocto build tree/BSP, machine configuration, recipes/layers,
   kernel/device tree, bootloader and firmware, plus a Chiton recipe packaging
   this release; or
2. A clone of the known-good **whole** SD card, with this application release
   applied to a copy and separately qualified on the hardware.

The recovered development sysroot and GCC toolchain cannot recreate these OS
components. A filesystem archive or `/dev/sdd1` partition dump is not necessarily
a bootable whole-card image. Identify the card afresh, stop the device cleanly,
and clone it read-only; do not assume the old `/dev/sdd` name still identifies it.
Boot firmware stored outside the SD card is not captured by a card clone.
Treat clones as confidential: they can contain credentials and per-device keys
and calibration. No OS image creation/flashing is performed by this packager.
