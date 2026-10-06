# Camera rendering pipeline: minimal-change performance phases

## Goal and boundaries

The operator reports consistent **29–30 browser FPS with 87% aggregate CPU free**
on the target. We are already at the sensor limit. Optimize CPU work, memory
traffic, power and latency without claiming higher physical capture FPS.
Aggregate idle CPU does not establish the load on the camera thread/core.

This is a plan, not authorization to deploy, execute ARM code, change camera
registers or write the SD card. Hardware comparisons are operator-run. Preserve
native NEON, ARMv8-A and the recovered Yocto runtime; do not guess FP16 extensions
or replace target libraries. Keep device calibration/configuration outside
versioned bundles and back up state before tests.

Each numbered phase is a separate reviewed change and versioned artifact. The
small substeps in phases 2 and 4 must also be separate changes where indicated.
Do not combine optimization with cleanup, new transports, codec replacement,
calibration algorithm changes or different smoothing/colour-range semantics.
Retain the existing WebSocket protocol, snapshot fallback, immutable JPEG
ownership, explicit global device mode and observational export contracts.

## Common acceptance gate

Before advancing from any phase:

1. Compare its output with the previous accepted implementation using deterministic
   fixtures. Preserve processed uint16 values, pixel mapping, frame metadata,
   calibration/smoothing results and SLAM buckets. Correctness fixes discovered
   during optimization are separate changes with separately approved semantics.
2. Run focused tests and the simulator/UI regression suite; use ASan, UBSan and
   TSan as appropriate. No cross-built program is executed by these host tests.
3. Cross-build and audit target architecture, strong symbols, dependency closure,
   runtime versions and absence of distribution runtime/RPATH leakage.
4. Produce a new full deployment bundle and checksums. Never overwrite an accepted
   artifact, `bin/chiton`, the working release or shared device state.
5. Operator compares the prior/new bundles on the same target with the same mode,
   smoothing/calibration, quality, client count and network. Warm up for 30 seconds;
   collect at least 120 seconds per run, ideally repeated A/B/A comparisons.
6. Accept only a repeatable benefit larger than run-to-run variation, with no
   regression in 29–30 FPS, tail latency, image correctness or control response.
   If a change is neutral/worse, revert it rather than keep theoretical speedups.

Rollback: stop the new camera owner and run the previous accepted release; never
run both concurrently. Transport-only rollback remains `?transport=poll`.
Hardware register/calibration changes may require a documented device recovery
procedure; reverting code alone is not guaranteed to restore hardware state.

## Phase 0 — Establish a useful baseline (measurement only)

**Minimal change:** extend existing pipeline timing with separately named preview
copy, calibration/smoothing, colour/intensity conversion and register-I/O timings.
Use bounded counters/buckets and report summaries through `/stream/metrics`; no
per-frame logging, unbounded histories or heavyweight tracing. Record timing
instrumentation overhead and keep it unchanged during A/B comparisons.

Record:

- Acquisition/JPEG/display FPS, JPEG bytes and stream send/credit timings.
- Mean and p95/p99 stage timings where supported by bounded timing buckets.
- Camera-thread/per-core CPU, process CPU, temperature/throttling and client count.
- Mode 1 greyscale, mode 3 colour, planar modes 5/6 and cloud mode 4 separately;
  smoothing/calibration on/off where valid. Register tests are deferred to phase 5.

Add small host benchmarks that run existing functions on deterministic buffers,
without SDK/device access. Use them to detect correctness/performance regressions,
not to infer ARM timings from AMD results.

**Exit gate:** establish where the CPU time is actually spent. If all stages are
already negligible, deeper phases can be deferred. Instrumentation must not
change output, SDK call ordering or capture rate.

Phase 0 instrumentation and controlled diagnostic program details are documented
in [pipeline-performance-metrics.md](pipeline-performance-metrics.md). Build and
hardware acceptance remain external operator work; the baseline is not qualified
until those gates complete.

## Phase 1 — Skip unused local preview snapshots in headless mode

**Files:** `src/main.cpp`; focused simulator/display tests.

**Minimal change:** compute an immutable `local_preview_enabled` flag from the
actual selected backend's `visible()` result on the main thread, before camera
worker startup. Pass that flag to the worker. Gate only the preview-lock/crop-copy/
`preview_ready` block; do not call backend methods from the camera thread.

The current copy is 320 × 190 × 2 = **121,600 bytes/frame**, roughly **3.65 MB/s at
30 FPS**, even in headless mode. Skip that work when nobody can display it.

**Do not change:** JPEG generation, calibration, SDK reads, HTTP exports or visible
framebuffer/desktop previews. Requested `auto` is not sufficient to decide:
use the actual resolved backend, which may be headless.

**Tests:** headless performs no preview copies; visible presentation produces the
same crop; fallback backend selection is covered. Compare raw/measurement/JPEG
output before/after and test shutdown/thread ownership.

**Measure:** preview-copy time/counter falls to zero in headless; camera-thread CPU
and processing time do not regress. Rollback is the single preview gate.

## Phase 2 — Reduce colour-conversion loop overhead, not duplicate the LUT

**Files:** `include/color/hsv.h`, `src/color/hsv.cpp`, `src/graphics/images.cpp`;
focused colour/reference tests.

**Important correction:** `depth_to_hsv_raw()` already performs three reads from
`hsv_cache[*value]`. A new depth-to-HSV lookup table would duplicate existing work.
Determine whether GCC already inlines/coalesces these reads before changing code.

**Phase 2a, one small change:** hoist invariant runline counts/table pointers and
use a single local cached RGB lookup per pixel if generated code shows repeated
lookup work. Preserve RGB byte layout, runline endpoints and destination offsets
exactly. Do not fix suspected mapping/off-by-one issues in the same change.

**Phase 2b, only if measured worthwhile:** apply the accepted loop transformation
to the planar colour path as a separate change. Scalar table gathers may remain
better than forced SIMD; do not assume NEON accelerates arbitrary LUT access.

**Tests:** exhaustive RGB equality for all 65,536 uint16 inputs across selected
valid range settings; full-frame equality for regular/planar modes, irregular
runlines, untouched pixels and destination guard bytes. Verify range changes
refresh the existing cache and do not introduce races or extra per-frame rebuilds.

**Measure:** colour-conversion time/CPU. If the compiler already produces equally
efficient code, record that result and skip the implementation change.

## Phase 3 — Reuse JPEG compressor state, preserve encoded-buffer ownership

**Files:** `src/graphics/images.cpp`, `include/image.h`, camera encoder call site;
codec/lifetime tests.

**Minimal change:** introduce one camera-worker-owned RAII compressor context,
created once and destroyed on that same worker. Keep the error-manager storage
alive for the compressor's full lifetime. Replace only repeated create/destroy
setup; preserve existing library, settings and fatal-error policy initially.

Reconfigure dimensions/components/colour space and quality correctly when they
change. Use a fresh destination for each encoded frame and transfer its allocation
to the existing immutable `JpegHub`; **never reuse/free storage still pinned by a
viewer**. Do not combine this phase with buffer pooling or libjpeg-turbo migration.
Emit complete, independently decodable JPEGs with their required tables each time
(e.g. retain `jpeg_start_compress(..., TRUE)` behavior).

**Tests:** byte equality with the old encoder where achievable; decoded-pixel
and independent-image equivalence are mandatory. Cover repeated frames, quality
changes, greyscale/RGB/planar size changes, cloud/no-JPEG transitions, capture
stop/resume and shutdown. Pin old snapshots while encoding new ones; test
allocation/lifetime behavior under sanitizers. No encode/settings data races.

**Measure:** encode time, allocation/setup count, RSS and JPEG bytes. Reject memory
growth or latency regressions even if compressor initialization gets cheaper.

## Phase 4 — Vectorize remaining loops one kernel at a time

**Files:** `src/graphics/images.cpp`, `src/graphics/calibration.cpp`;
scalar-reference/vectorization-report tests.

**Phase 4a, compiler-first:** collect target GCC optimization/vectorization reports
and inspect assembly. Hoist invariant branches such as smoothing outside runline
loops where safe. Make only one kernel change at a time. Existing scalar-looking
code may already be vectorized; reports are evidence, not a reason to rewrite it.

**Phase 4b, first explicit SIMD candidate:** implement a native-NEON/SIMDe
uint16-to-greyscale conversion for one common path (start with division by 16).
Retain scalar tails and a reference fallback. Preserve unsigned conversion,
inversion/wrapping and untouched AOI pixels; do not substitute saturation,
rounding or change invalid-sample policy. Extend /24 and /26 paths only as separate
changes with exhaustively verified integer arithmetic.

**Phase 4c, calibration only after greyscale acceptance:** consider separating
calibration/smoothing arithmetic from the order-dependent SLAM-bucket updates.
Keep the latter scalar initially. That extra memory pass must earn its cost in
measurements; do not casually vectorize scatter updates with repeated buckets.

Preserve scalar/reference results with existing float rounding/truncation and
smoothing behavior. No new FMA contraction, fast-math, FP16, gain/offset change or
new numeric saturation policy. If vector results differ, resolve semantics as a
separate correctness decision rather than silently changing measurements.

**Tests:** exhaustive greyscale inputs, unaligned buffers, short/odd runlines,
scalar tails, guard regions, full-frame/AOI equivalence, smoothing history,
calibration boundary fixtures and exact SLAM bucket outputs. Build both native
NEON and portable fallback paths. Run calibration/measurement/sanitizer gates.

**Measure:** conversion/calibration time, camera CPU and memory traffic for each
substep. Revert kernels independently when no benefit is demonstrated.

## Phase 5 — Suppress redundant illumination writes (hardware-dependent, last)

**Files:** a small private register-write/cache helper and camera lifecycle hooks;
fake-SDK tests first. No changes to illumination calculations or desired values.

**Phase 5a, prerequisites only:** obtain/document the SDK/driver/sensor contract
for registers `0x4011`–`0x4014`. Determine whether repeated writes are a required
heartbeat, refresh, latch or recovery mechanism. Measure current write cost and
write frequency without changing cadence. If periodic writes are required or the
contract remains unknown, stop here and retain current behavior.

**Phase 5b, default-off implementation:** cache the last successfully acknowledged
four-register tuple. At the existing update opportunities, skip only a tuple
known to match current hardware state. The first eligible write must still occur;
initial values in software are not proof of what the hardware contains.

Update the cache only after SDK success. Invalidate it on acknowledged capture
restart, device reinitialization, failed/uncertain writes and any alternate writer
of these registers. If all writers cannot be accounted for, do not enable caching.
Retain the original write policy as the default and a rollback switch; do not add
unbounded retries, invented refresh intervals or new register values.

**Tests:** fake SDK verifies first-write, unchanged skip, changed write, failure
not caching, lifecycle invalidation and disabled optimization preserving the old
schedule. Host tests never write actual registers.

**Phase 5c, separately authorized operator qualification:** controlled A/B test
on hardware, including dynamic/static illumination, depth/IR, capture restart,
reinitialization and recovery. Check image/depth stability as well as FPS/CPU.
Document any required recovery/power-cycle procedure before enabling broadly.

**Exit gate:** verified hardware contract and measurable benefit without image,
measurement or recovery regressions. Otherwise leave the feature off/remove it.

## Suggested execution order

**0 → 1 → 2a → optional 2b → 3 → 4a → optional 4b/4c → 5a → conditional 5b/5c.**

At each checkpoint record the source change, artifact SHA, fixture/reference
results, ABI audit, operator A/B timings and rollback release. Preserve the
accepted 30-FPS streaming baseline described in [image-streaming.md](image-streaming.md).
Do not treat completion of all phases as necessary: a measured no-op or unsafe
hardware assumption is a reason to skip a phase, not to add more complexity.
