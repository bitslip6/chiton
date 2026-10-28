# Sampled boundary cleanup: server producer

Implemented producer and approved live-console integration of [the plan](../plan-sampled-boundary-cleanup.md).
See [console/API/ownership documentation](console-boundary-cleanup.md) for live use.
**Off by default; proposed parameters await operator approval and real saved-depth
qualification.** This is a geometric display classification, not sensor confidence.
The viewer and bounded transport are implemented; no device deployment or target-CPU qualification is included.
Original processed depth, XYZD, measurements and existing exports are unchanged.

## Selection and ownership

`calibration::vector_point_cloud(const uint16_t*, Context)` invokes the fixed
classifier inside the original `i = srcstart; i < (runlen & ~3u); i += 4` loop.
`runlen` remains an exclusive end offset, including its existing tail omissions.
One flag is written at the corresponding XYZD record index, without a second
cloud traversal or an index list. Neighbor reads may use unsampled sensor pixels.

The static producer owns a 640×480 byte membership table, a 2,048-entry endpoint
snapshot, and fixed POD configuration/publication/counters. The server binds its
flag output directly to three startup publication slots, each with original XYZD
capacity and 230,400 flag bytes (actual 921,600-component capacity / 4). These
replace the original cloud/single flag buffers, adding about 3.96 MiB of payload
reservation. Standalone kernel fixtures retain their internal-buffer specialization.
The shared `data::point_cloud_components` constant sizes actual producer capacity.
No source depth or completed XYZD copy is added; all cleanup storage is static.

The main server initializes membership after calibration geometry creation and
before capture/workers start. Every run and the exact selected count are checked
before populating membership. Initialization failure is **fatal**, including when
Off. Retry/reinitialization is rejected. Domain membership is the union of actual
processed runs, not an idealized AOI. Cross-row runs remain supported.
At startup, bit 1 of each membership byte marks centers whose entire 5×5 stencil
is inside the image and processed domain (bit 0). Such centers use direct offsets
without repeated bounds/membership checks; all other centers use the checked
path. This geometric erosion reads no depth and adds no domain storage. Topology
invalidation still prevents use of obsolete metadata. Exact-mode flags are unchanged.

All initialization, configuration, production, and borrowed flag/publication/
metrics access must use existing camera/state ownership. Main capture already
holds it across processing and publication. Source pointers are never retained.
Publication is the last completed cloud generation, not a mask for arbitrary
newer raw/JPEG frames. `Publication.context` supplies frame/revision/time,
depth/units/scope/smoothing/plugin/sampling context; the publication also includes
algorithm version, exact configuration, configuration generation and counts.
A consumer must match this identity and use only `[0, publication.flags)`.
Zero/shrinking clouds cannot expose stale tail flags through that valid prefix.

Runtime topology endpoints/count are compared with startup metadata before any
classification. Any change permanently disables cleanup until process restart;
safe changed runs still emit their original XYZD unfiltered. Unsafe runs (bounds,
overlap/order, excessive count/capacity) publish zero geometry/flags with
`UnsafeTopology`, rather than executing an out-of-bounds legacy emitter. Restoring
old runs does not recover cleanup. There is no live domain rebuild.

## Proposed deterministic predicates: experimental-mm-v1

These are **proposals, not approved universal millimeter thresholds**:

| Parameter | Proposed value |
| --- | --- |
| Evidence interval (inclusive) | 250–6000 mm |
| Boundary gap (strictly greater) | 200 mm |
| Anchor/same-depth tolerance (inclusive) | ±40 mm |
| Intermediate center margin (strictly greater from both anchors) | 60 mm |
| Minimum support on each surface | 4 neighbors |
| Same-depth neighbors sufficient to retain an intermediate center | 3 |
| Ramp residual (inclusive) | 30 mm |

The interval limits experimental **evidence**, not SDK validity. Zero, saturation,
IR and unsupported depth values are not silently removed from the cloud. Values
outside the interval cannot establish rejection evidence. Interval semantics and
threshold suitability need confirmation on saved organized-depth frames.

1. At each emitted center, read up to four opposite probe pairs at radius two:
   horizontal, vertical, then the two diagonals. Check X/Y bounds and membership
   separately before every load. Ignore incomplete/out-of-interval pairs. If no
   usable pair exists, return Unclassified; if the largest gap is ≤200, Retained.
   Largest-gap ties select the first direction.
2. The lower/higher probe values anchor near/far groups. In a direct-read 5×5 pass
   **excluding the center**, accumulate support counts and integer sums for values
   within ±40 of either anchor. Require four per group, then refine by truncated
   integer arithmetic mean. Require refined separation >200 and >2×tolerance.
3. In a second direct-read pass over the same 24 positions, recompute group
   support, same-center-depth support, and spatial evidence. Near/far tolerance
   bands must be disjoint. At least four pixels of each group must be strictly on
   its side of the selected direction; at least 75% of each group must be in its
   corresponding **closed** half-plane. The center line may support either group
   but cannot alone establish opposing surfaces. Failure is Unclassified.
4. Read the two radius-one pixels on that direction. Missing/unsupported profile
   values return Unclassified. For profile t=-1,0,+1 and refined endpoints N,F,
   veto rejection if all `abs(4*d[t] - ((2-t)*N + (2+t)*F)) <= 4*30`.
5. If the center is not more than 60 from both refined endpoints, BoundaryRisk.
   Otherwise ≥3 other pixels within ±40 of center returns Unclassified (possible
   third surface); fewer returns MixedCandidate.

Flags are mutually exclusive bytes: Off=0, Retained=1, BoundaryRisk=2,
Unclassified=4, MixedCandidate=8. Display policy may hide **only**
MixedCandidate. Explicit algorithm 3, [Aggressive boundary bands](aggressive-boundary-bands.md),
uses a different radius-six checked rule, without ramp/third-surface retention.
Algorithms 1/2 below remain conservative and unchanged. No dilation is implemented. `gated` counts
samples passing the boundary gate, including subsequent vetoes; `detailed`
counts those entering support passes, and `early_retained` counts revision-2
proof-based exits (`gated = detailed + early_retained`). Other counters count
final classes. Off writes zeros to the active prefix and has zero class
counters. Unsupported/invalidated contexts write Unclassified, never old flags.

Tuning is unrestricted within the existing unsigned storage/wire types: all six
depth/threshold values accept 0–65535 and both support counts accept 0–255. No
ordering, cross-field or empirically chosen range is enforced by the UI/API or
native configuration. Zero/equal/reversed evidence intervals, zero tolerance,
large residuals and any support counts are accepted unchanged. Reversed intervals
provide no evidence; support larger than the available neighborhood cannot pass.
An empty anchor group returns Unclassified rather than dividing by zero, even
when requested support is zero. The classifier's evidence/support/ramp predicates
still determine labels; accepting a configuration does not promise suspects.
Unknown algorithm IDs remain invalid. API malformed/noninteger/overflow values
are rejected without mutation; each accepted update increments the generation.
No map/string/container is involved.

**Arithmetic/work bounds:** at most 59 scalar source loads per sample: center +
8 gate + 24 first support + 24 second support + 2 radius-one profile. This counts
rereads, not unique pixels. Each pass has fixed loop bounds. Sums are uint32
(≤24×65535 = 1,572,840); signed differences and profile products fit int32
(maximum product 4×65535). Counts/side products are ≤96. The kernel accepts the full uint16 tuning/evidence range safely, but production
projection's existing signed narrowing
is neither changed nor exercised with unsupported >32767 emitted values in tests.
There is no sorting, median, staged neighborhood, dense classifier, worker,
spatial index, temporal history or new source copy in the runtime call graph.

### Opt-in conservative early exit (algorithm revision 2)

The exact interior fast path remains algorithm 1 and is the server preset's
unchanged default. For evaluation, native configuration may select
`Config.algorithm = Algorithm::ConservativeEarlyExit`; publication then reports
version 2. The live console/API now expose this explicit choice as well; it is not
enabled implicitly and the existing server CLI preset remains exact.

After selecting raw anchors N/F but **before** either support pass, let
`m = center_margin - anchor_tolerance` (20 mm for the proposed preset).
If `center-N <= m` or `F-center <= m`, return **Unclassified** immediately.
This uses at most nine source loads and does not claim that either anchor has
surface support. It may replace a previous BoundaryRisk/Retained flag with
Unclassified, but preserves the **exact set of MixedCandidate points**.

Proof: the first-pass refined near anchor N' stays in `[N-tolerance,N+tolerance]`.
If `center-N <= margin-tolerance`, then `center-N' <= margin`, so the original
strict intermediate-depth predicate cannot pass. The far-anchor bound is
symmetric. Integer-mean truncation stays inside the supporting-value bounds.
All remaining points execute the unchanged predicates. This is not a heuristic
shortcut and does not trade away mixed-point rejection to achieve faster timing.
The 40 mm anchor tolerance alone would not be a safe early-exit threshold.

Exact versus checked dispatch and revision-2 rejection-set parity are tested
across over 1.5 million center/configuration/domain cases, including structured
and pseudo-random depths, borders, holes and spatial gaps, plus explicit 20 mm
threshold boundaries. Lifecycle/allocation tests include revision changes and
publication identity; the timing fixture compares each enabled rejection byte
with a separately generated exact-mode reference outside measured spans.
Persistent storage remains startup-reserved. The later console integration moves
server geometry/flags into leased slots; see its ownership/memory documentation.

### Expected limitations

A coherent narrow intermediate strip can equally represent a real third surface
or a mixed-depth band. This conservative proposal retains supported strips,
thin lines, smooth ramps and wider bands rather than pretending to disambiguate
them. It targets **unsupported sparse intermediate points**, not every bridge.
Bands wider than radius two and surface supports absent from the stencil can be
missed. A genuinely isolated middle-depth object may still be marked suspect;
synthetic success is not real-world effectiveness qualification.

## Enabling/configuring the server

Normal startup is Off. After approving the experiment, an operator may select:

```sh
chiton --sampled-boundary-cleanup experimental-mm-v1
# Explicit aggressive ID 3, fixed radius 6, existing default thresholds:
chiton --sampled-boundary-cleanup aggressive-mm-v1
# Explicit default:
chiton --sampled-boundary-cleanup off
```

This selects server classification. Legacy HTTP export bodies remain unchanged;
the console's new versioned stream carries flags, and its local Highlight/Hide
controls can now display the result. No setting is persisted. Unknown
presets fail startup. The fixed-POD native API
`calibration::configure_sampled_cleanup(Config)` supports allocation-free toggles
and threshold changes under camera/state ownership. The new bounded
`/api/cleanup/v1/config` endpoint and console expose complete atomic updates. Failed initialization must never be retried during capture.

Classification is available only for calibrated-mm, calibration-domain depth
frames with no plugin and no calibration sampling. Sensor-unit/IR/plugin/sampling
contexts are unclassified. Normal context changes never trigger table allocation.
Off bypasses all source-neighborhood reads.

## Validation and remaining acceptance gates

SDK-free native tests include the actual production emitter TU, following the
existing `pointcloud_geometry` fixture pattern. They cover oriented steps with
isolated intermediate points, coherent third layers/thin lines, ramps/curves,
noise, unsupported ranges, isolated extremes, sparse domain support, every border,
row-wrap exclusion, irregular starts/tails/empty/cross-row/adjacent/gap runs,
capacity rejection, exact source-index/XYZD equality, flags/counts/context,
zero/shrinking output, topology invalidation, configuration and context changes.

The first emitter/kernel/clock call, repeated frames, toggles, threshold changes,
unsupported contexts, failure paths and near-all-candidate clutter execute inside
an unsanitized allocator guard. The test overrides C++ scalar/array/aligned new
and delete, wraps malloc/calloc/realloc/free/aligned_alloc/posix_memalign, and
wraps memcpy/memmove to detect source-frame copies. These cover allocator entries
linked by this path, not arbitrary untested shared-library internals. Runtime
source is additionally read-only mmap protected and hashed. Static inspection of
the new call graph establishes no scalar/window source staging; immutability
alone does not prove no copying. ASan/UBSan are separate builds with allocator
interposition disabled.

Example host commands (no camera):

```sh
cmake -S . -B _build/sampled-boundary-cleanup/host \
  -DUSE_ADITOF_STUB=ON -DUSE_SDL2=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build _build/sampled-boundary-cleanup/host -j4
ctest --test-dir _build/sampled-boundary-cleanup/host --output-on-failure
_build/sampled-boundary-cleanup/host/sampled_boundary_cleanup_test --benchmark
# Separate ASan/UBSan trees: -DCHITON_SANITIZER=address / undefined
```

The optional benchmark stores 100 durations before offline reporting and runs
Off/On/Off for flat and adversarial detailed-support scenes, both full-image
legal sampling (76,800 records) and generated production AOI (~34,532 for this
fixture). Wall timings include XYZD emission and are **not** device CPU timing.
Production metrics use ten fixed duration histogram bins for the entire emitter,
not per-point clocks or purported isolated cleanup cost. No exact percentiles or
sorting are computed inside the runtime path.

The operator's 65%-of-one-core baseline is the **whole process**. Sensor-CPU
A/B/A (typical added ≤3 ms, p95 ≤5 ms), cadence/responsiveness/lock-time and thermal
checks remain unrun and require separate authorization. Host adversarial maxima
can exceed those targets; do not infer target acceptance. Real saved organized
frames and operator-labeled retention/effectiveness validation are also pending.
Do not deploy or report a qualified cleanup feature on synthetic tests alone.

### Production-emitter timing integration test

`tests/pointcloud_cleanup_timing.cpp` times the actual `vector_point_cloud()`
with classification Off/On/Off. Unlike the earlier optional diagnostic, it
measures **thread CPU time as well as wall time**, uses real production projection
factors for the generated AOI, warms every phase, repeats the comparison, and
checks byte-identical XYZD, unchanged source, output canaries, publication identity
and enabled/candidate-path status outside the timed spans. The second layout
uses the maximum full-domain traversal with explicitly initialized safe synthetic
projection factors; it is not a physical-camera configuration.

CTest runs a short correctness smoke test without machine-dependent timing
assertions. For a longer reproducible comparison, use an **unsanitized Release**
build, optionally pinning the process to an allowed logical CPU:

```sh
cmake --build _build/sampled-boundary-cleanup/host --target pointcloud_cleanup_timing_test
_build/sampled-boundary-cleanup/host/pointcloud_cleanup_timing_test \
  --samples 1000 --rounds 3 --warmup 30 > timing.log
```

The final line is JSON (legacy geometry setup prints preceding diagnostic lines).
It reports each layout/scene's mean, p95, p99 and maximum CPU/wall time, the
pooled Off baseline, separate before/after Off means, added mean time, ratio,
point count and detailed-classification percentage. All timing storage is fixed
before emission; statistical sorting and serialization happen only after all
measured cases finish. Timing includes XYZD emission, classification, flag/count
publication and existing emitter metrics. It excludes fixture creation,
configuration changes, warmup, validation and reporting. This comparison is
against classification Off in the current producer, not an older binary without
cleanup integration.

Before the optimizations below, one host run (AMD Ryzen 5 5600X, GCC 16.2.1,
Release, pinned logical CPU 0,
3×1000 On and 6×1000 pooled Off frames per case) measured:

| Layout / scene | Points | Off CPU ms/frame | On CPU ms/frame | Added mean CPU ms |
| --- | ---: | ---: | ---: | ---: |
| Production AOI / flat | 34,532 | 0.109 | 0.567 | 0.458 |
| Production AOI / single boundary | 34,532 | 0.108 | 0.582 | 0.474 |
| Production AOI / boundary clutter | 34,532 | 0.108 | 1.276 | 1.168 |
| Production AOI / near-all-candidate | 34,532 | 0.108 | 3.454 | 3.346 |
| Synthetic full domain / near-all-candidate | 76,800 | 0.215 | 7.704 | 7.488 |

Detailed support ran for 0%, 0.36%, 24.50% and 98.15% of production-AOI points,
respectively. Near-all-candidate **enabled total** CPU p95/p99 was 4.073/5.166 ms
for the AOI and 8.226/9.195 ms for the full domain. These are not percentiles of
added cost or claims about the sensor CPU. CPU scaling was not controlled; these
are synthetic scenes with no acquisition, client load or real-device cadence.
Raw results/environment and validation logs are under
`_build/sampled-boundary-cleanup/timing/{benchmarks,validation,review}/`.

### Optimization evaluation and stronger stress fixture

The integration benchmark now also includes `dense_mixed_samples`: overwrite
only the exact emitted centers with 1300–1700 mm depths cycling across five rows;
unsampled pixels still provide the striped 1000/2000 mm surface evidence.
This prevents a supported same-depth middle strip and produces actual
MixedCandidate flags on 61.16% of AOI points and 99.38% of full-domain points.
The original stripe scene produced **zero** MixedCandidate flags and is not a
sufficient worst-case test for revision 2. CTest asserts both gate density and
real rejection density, so bypassing detailed classification cannot silently
make the dense mixed benchmark look faster.

Run the same test with `--early-exit` to evaluate revision 2. JSON now includes
algorithm, full-flag and rejection-prefix hashes, mixed counts, detailed support
counts and early exits. Both exact and early-exit quick tests are registered.
Classification remains Off by default outside these test runs.

A same-session comparison on the host above used 3×500 On frames per case,
30 warmup frames per phase, pinned CPU 0 and unchanged Release compiler options:

| Layout / scene | Original classifier | Exact interior | Opt-in early exit |
| --- | ---: | ---: | ---: |
| AOI / flat | 0.559 ms | 0.517 ms | 0.514 ms |
| AOI / single boundary | 0.569 ms | 0.521 ms | 0.516 ms |
| AOI / boundary clutter | 1.263 ms | 1.030 ms | 0.524 ms |
| AOI / surface stripes | 3.334 ms | 2.574 ms | 0.556 ms |
| AOI / **dense mixed points** | **3.202 ms** | **2.470 ms** | **2.480 ms** |
| Full domain / surface stripes | 7.585 ms | 5.770 ms | 1.184 ms |
| Full domain / **dense mixed points** | **7.519 ms** | **5.767 ms** | **5.787 ms** |

Values are **total emitter mean thread CPU time**, not isolated added cost.
Current Off means were about 0.090 ms (AOI) / 0.179 ms (full domain). Algorithm 2
still executes detailed support on 97.91% / 99.50% of dense-mixed samples.
Dense-mixed enabled CPU p95 was 2.572 / 5.927 ms for exact and 2.592 / 5.946 ms
for early exit (AOI / full domain). It does **not** solve the true worst case.

A saved pre-optimization binary with the stronger fixture was replayed before
and after the final optimized runs; its means differed by ≤1.1%. The table pools
those two baseline replays. An earlier run in the same session was substantially
slower during uncontrolled CPU scaling, so it is retained in evidence but not
used to claim speedups. All ten original/exact flag-prefix hashes match; all ten
exact/early rejection-prefix hashes match. No record counts changed.

Evidence and environment: `_build/sampled-boundary-cleanup/optimization/`, with
`benchmarks/final-comparison.txt`, raw JSON/logs, saved baseline executable,
allocation logs and Release/ASan/UBSan validation. These are **host-only synthetic
results**, not sensor acceptance. The exact path saves roughly 20–25% on heavy
scenes; the early exit principally saves unnecessary work on supported surfaces.
Dense mixed-depth work still needs a separately measured device budget before
shipping, and no sample skipping, source copying, neighborhood staging, runtime
allocation or reduced point density was introduced.

Evidence: ignored `_build/sampled-boundary-cleanup/{baseline,validation,benchmarks,review,handoffs}/`.
Rollback: restart without the experimental option (Off), or revert the phase
changes; startup storage remains reserved when Off. The approved transport/UI is
implemented with direct startup-owned publication and bounded framing, not the
existing allocating HTTP snapshot helpers; see console-boundary-cleanup.md.
