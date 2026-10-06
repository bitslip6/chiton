# On-target pipeline diagnostics

These programs are cross-compiled for the bundle's AArch64 Yocto runtime. Run
only on the compatible target, not on the AMD build host. They use deterministic
in-memory fixtures and do not access the camera SDK, hardware registers, network,
or saved device configuration/calibration. The launcher selects this release's
private libraries; it does not require shared/chiton.json.

Stop Chiton first for controlled CPU comparisons. Fix CPU/power policy and thermal
conditions, avoid other workloads, and compare previous/new bundles repeatedly
with identical iteration counts, compiler flags and fixtures. No script stops
services, changes CPU policy or deploys anything automatically.

From the extracted release directory:

```sh
./benchmarks/run.sh image grey16 1000
./benchmarks/run.sh image colour 1000
./benchmarks/run.sh calibration average 1000 1
./benchmarks/run.sh calibration calibrate 1000 0
./benchmarks/run-all.sh 1000
```

To retain a report, redirect stdout to a new operator-chosen file outside the
release (avoid overwriting an earlier report). Each successful run ends with a
JSON line after existing geometry-initialization messages. Scalar fixture or
guard failures return nonzero and stop run-all. Thirty warm-up iterations precede
measured work; buffer/history resets and result validation are outside the timed
spans. Reported percentiles/checksums describe isolated kernels, not live camera
FPS. A matching fixture checksum is necessary but not a substitute for full
image/calibration/smoothing regression and hardware acceptance tests.

Image kernels: grey16, grey26, grey24, colour, planar16, planar26, planar-colour.
Calibration kernels: average, calibrate; smoothing argument is 0 or 1.
See pipeline-performance-metrics.md for exact boundaries and limitations.
