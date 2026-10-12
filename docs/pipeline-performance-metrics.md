# Phase 0 diagnostics (measurement only)

No algorithm, preview gate, register schedule, JPEG ownership, or SDK call-order
change is included. No build, deployment, target execution, or hardware qualification
was performed by the agent. Building and target comparisons remain operator work.

## Server and browser

`GET /stream/metrics` retains existing fields and adds `pipeline.stages` and
`pipeline.system`. Both browser copies poll this endpoint once/second, including
snapshot fallback and cloud mode. Only one diagnostics request is outstanding,
with a 1.5-second deadline. Hidden/page-away clients abort collection. Diagnostics
failures never change image transport; the UI labels results stale after five
seconds. Latest diagnostics are available as `window.CHITON_METRICS`.

WebSocket status retains its small, existing pipeline fields; detailed diagnostics
are only fetched through the independent endpoint (not appended to JPEG messages).
The UI also shows existing browser-local displayed FPS, not a server estimate.

### Image stage histograms

The stage summaries describe the **latest configuration epoch**, accumulating
until mode, depth/IR, smoothing, calibration/sample state, or configuration revision
changes. Each change clears the fixed histograms and increments `epoch`. There is
no frame history. Compare epochs with identical settings and durations.

- `processing`: existing acquired-to-processed interval, including miscellaneous
  processing, publication and camera ownership work; stages are not exhaustive.
- `calibration_smoothing`: averaging/calibration block, including calibration
  sample completion and its processing/save work when applicable.
- `preview_copy`: preview mutex wait plus unchanged crop-copy/publication block.
- `conversion`: existing mode-dispatch block, including colour/intensity, planar,
  cloud, gain and SLAM output generation as appropriate.
- `register_io`: existing eligible illumination write call only (including SDK
  device lookup), **mean/count only**, no change in values, ordering or cadence.
- `timer_probe`: empty timer pair measured per frame, **mean/count only**.
- `record_overhead`: camera-thread histogram recording, lock wait and thread-ID
  publication, **mean/count only**. Recording occurs after camera ownership is
  released and after encoding, and does not enter the processing/encoding totals.

The first four expose `count`, `mean_ms`, `p95_ms_upper_bound` and
`p99_ms_upper_bound`. There are 96 fixed bins: 95 finite upper bounds starting at
0.001 ms and growing by 1.2, then overflow. Percentiles are approximate conservative
upper bounds, not exact samples; no samples or an overflow percentile is `null`.
Stream output, acquisition and JPEG encoding gain **no percentile buckets**.
Existing acquisition/encoding rolling means and transport counters remain intact.

The timer pair and record overhead are diagnostic overhead components, **not an
exact measurement of total instrumentation overhead**: stage boundary clocks,
frame initialization, and UI/endpoint work add cost. `system.collection_ms` measures
OS collection time separately. Keep all instrumentation and client polling cadence
unchanged for A/B comparisons; qualify total overhead with a separate measured
uninstrumented comparison before accepting the baseline.

### Automatically sampled Linux observations

OS sampling runs only on diagnostics reads, never on the camera thread and never
while holding JPEG hub ownership. A mutex-protected cache limits sampling to once
per second regardless of browser count. Sampling uses `/proc/stat`,
`/proc/self/stat`, `/proc/self/task/<camera_tid>/stat`, CPU frequency sysfs, and
thermal/cooling sysfs. CPU counters are interval deltas, not lifetime averages.

- Process and camera-thread CPU: **100% means one full core**; process usage may
  exceed 100%. First samples, missing tasks/interfaces and counter resets are null.
- Aggregate and per-core busy CPU: percentages of machine/core capacity, with
  idle and iowait treated as idle and guest ticks not double-counted.
- `camera_last_core` is the last observed scheduled core, not affinity or proof
  the thread remained on that core throughout the interval.
- Up to 256 cores with observed frequency (kHz), 32 thermal zones (Celsius), and
  32 cooling devices (current state). Missing interfaces remain unavailable.
- `throttled` is **null/unknown**: no universal Linux/i.MX8 throttle flag has been
  established. Cooling states and frequency reductions are observations, **not
  proof of thermal throttling**. Hardware-specific qualification is still needed.
- `sample_interval_ms` describes the CPU delta window, `collection_ms` the monitor
  cost. The endpoint also retains active/max stream client counts.

## SDK-free controlled benchmark programs

For a full **cross-compiled distribution** including these programs, launchers,
and instructions, run `./cross-build.sh --name rel_perf_phase0_v1` externally from
the repository root. See [deployment-bundle.md](deployment-bundle.md). It builds
and audits all three executables but never executes target code or deploys. On
the compatible target, run `./benchmarks/run-all.sh 1000` from the extracted
release after stopping Chiton for controlled CPU comparisons.

Opt-in CMake targets `image_pipeline_benchmark` and
`calibration_pipeline_benchmark` use the existing processing functions, local
buffers and geometry initialization. They do not link the camera SDK or access
hardware, registers, saved configuration/calibration, network, or captures.
Logger and SLAM storage are benchmark-only fixtures. They are not deployment
applications and must not replace `bin/chiton`.

Build externally in a **new host-only build directory**, with
`USE_ADITOF_STUB=ON`, `USE_SDL2=OFF`, and `BUILD_PIPELINE_BENCHMARKS=ON`.
Build only the desired benchmark/test targets; use separate build directories for
ASan, UBSan and TSan. Do not execute cross-built targets on the host.

Example operator-run host commands, after external building:

```sh
/path/to/host-build/image_pipeline_benchmark grey16 1000
/path/to/host-build/image_pipeline_benchmark grey26 1000
/path/to/host-build/image_pipeline_benchmark grey24 1000
/path/to/host-build/image_pipeline_benchmark colour 1000
/path/to/host-build/image_pipeline_benchmark planar16 1000
/path/to/host-build/image_pipeline_benchmark planar26 1000
/path/to/host-build/image_pipeline_benchmark planar-colour 1000
/path/to/host-build/calibration_pipeline_benchmark average 1000 0
/path/to/host-build/calibration_pipeline_benchmark average 1000 1
/path/to/host-build/calibration_pipeline_benchmark calibrate 1000 0
/path/to/host-build/calibration_pipeline_benchmark calibrate 1000 1
```

Arguments: kernel, measured iterations (1–1,000,000), smoothing (0/1).
Thirty warm-up iterations precede measurements. Fixed deterministic buffers include
all uint16 values and irregular planar gathers. Histories, destinations and SLAM
buckets reset **outside** each timed invocation: these diagnose isolated kernels,
not capture cadence or long-running smoothing convergence. Entire outputs,
untouched pixels and SLAM buffers are compared with scalar fixture references;
output guards are checked. Existing colour destination offsets are deliberately
preserved, not corrected. Results include output/SLAM FNV-1a checksums, mean,
exact sample p95/p99 and the same bounded-histogram estimates as instrumentation.
Geometry initialization prints existing diagnostics before the final JSON line.

Compare checksums and timings with the previous accepted implementation, using
the same compiler, optimization flags, hardware and fixture. Host timings do
**not** predict ARM timings. These small diagnostics do not replace exhaustive
conversion tests, full simulator/UI gates, smoothing-history tests or operator
30-second warmup / 120-second repeated target A/B/A comparisons.

## Validation status

Added `performance_test` source for bounded histograms, missing data, configuration
resets and CPU-delta behavior. Added `tests/web_performance.js` for diagnostic
polling bounds/cancellation and null formatting. JavaScript diagnostics and the
existing `tests/web_ui.js` suite passed. C++ sources and benchmarks remain **unbuilt
and unexecuted**, as requested; full simulator/sanitizer/ABI/bundle/hardware gates
are pending external validation.
