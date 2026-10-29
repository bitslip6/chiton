# Implementation plan: allocation-free boundary cleanup at scan-line samples

**Status: plan only.** Sensor mixing is operator-confirmed. This document specializes the boundary-cleanup work in `plan-cleanup.md`; it replaces the proposed full-image candidate/classification passes with classification only at the existing point-cloud sample locations. No implementation, device access, cross-build, deployment, commit or push is authorized by writing this plan.

Repository: `/home/cory/Work/chiton`.

## 1. Objective and hard constraints

Identify mixed-depth samples near foreground/background boundaries without changing their measured depths. Generate an optional display-rejection classification for each point already emitted by the scan-line runs.

1. **Classify only existing emitted sample centers.** Preserve the exact run traversal, stride, order, exclusions and point count. Do not increase point density.
2. **No dynamic allocation after initial application startup in the cleanup implementation.** This includes initialization-on-first-use, enable/disable, mode/context changes, retries, parameter changes and error handling—not merely the inner loop.
3. **Do not copy source data.** Read the existing processed depth frame directly through a read-only pointer under its existing ownership. No copied frame, crop, halo, neighborhood array, staged depth texture or duplicate cloud for classification.
4. **No sorting.** No median, percentile selection, `sort`, `nth_element`, neighborhood ordering, adaptive clustering container or hidden sorting dependency.
5. No filtering, replacement, averaging or mutation of source depth, original XYZD, measurements or existing raw/OBJ/binary exports.
6. No per-frame full-image classification pass, dense depth-derived image, spatial index, worker queue or additional thread in the first implementation.
7. Work and storage must have explicit compile-time/startup bounds. No unbounded neighborhood expansion, convergence loop or accumulated frame history.
8. Ambiguous or unsupported classifications retain the original point in Conservative mode. Do not equate a geometric heuristic with sensor confidence.

### Exact meaning of “sampled points”

Only points selected by the existing emitter receive classifications. A classifier may **read unsampled neighboring pixels directly from the original processed frame** to evaluate an emitted sample. Those neighbors do not receive their own classifications, become new cloud points, or get copied into temporary windows.

Restricting all neighbor reads to the sparse emitted cloud would discard useful sensor-grid evidence and is not the algorithm proposed here.

### Allocation boundary: do not overclaim

The cleanup path must satisfy the startup-only allocation rule end to end. Existing unrelated Chiton paths already allocate at runtime, including legacy HTTP snapshot vectors and response serialization. This plan neither relies on those allocations for cleanup nor claims the whole current application is allocation-free.

Do not introduce a new allocating cleanup transport/helper and call it “outside the hot loop.” If zero allocation is intended for **every existing application subsystem**, stop and scope that broader refactor separately; it is not accomplished by this classifier. Browser-managed JavaScript/WebGL memory is also not a sensor-side allocation guarantee.

## 2. Current contracts to preserve

Source inspection anchors:

- `src/graphics/calibration.cpp::vector_point_cloud()` currently traverses each run as:

  ```cpp
  for (unsigned i = run.srcstart; i < (run.runlen & ~3u); i += 4) {
      // Emit XYZD for source pixel i.
  }
  ```

  `runlen` is an **exclusive end offset**, not a length. Run starts need not be four-aligned. Preserve this exact selection; do not normalize it into a different stride or repair its omissions in this feature.

- Projection uses the same processed `calibrated` frame after existing calibration/smoothing and optional plugin processing. The cleanup must analyze that exact frame, not a separately acquired image or a copied SDK buffer.
- Calibration/smoothing updates pixels in the run domain. Neighbor pixels outside that domain can be stale or differently processed and must not be used as evidence merely because their address is in bounds.
- The producer currently allocates `640 * 480 * 3` signed components for its cloud, not `640 * 480 * 4`. Derive permitted record capacity from actual storage: components divided by four. The browser's 307,200-record ceiling is not proof of producer capacity.
- `tests/pointcloud_geometry.cpp` already audits the exact source indices, irregular runs, tails, capacity and scalar output. Extend it or a closely scoped production-path fixture rather than implementing a second emitter approximation.
- Frame processing and cloud publication currently occur under existing camera/state ownership. Cleanup must not extend ownership into network writes, retain an unowned source pointer or race a mutable source buffer.
- `make_calibration_data()` allocates/reallocates tables. Do not call it to enable cleanup or rebuild cleanup metadata after startup. Geometry topology changes require a separately established allocation-free update policy or restart.

The earlier projection-model/AOI issue remains separate. A cleanup mask is not authorization to remove all Y=0 points or conceal unsupported geometry.

## 3. Proposed architecture

```text
Existing processed depth frame — read-only during this operation
                │
                ▼
Existing run traversal and exact sampled source index i
                │
       ┌────────┴─────────┐
       ▼                  ▼
Existing XYZD         classify_sample(src, i,
emission             immutable domain, fixed config)
       │                  │
       ▼                  ▼
Original cloud       One preallocated flag byte
record k             at the SAME record index k
       └────────┬─────────┘
                ▼
Coherent publication: record count + flags count + frame/context
```

No separate pass over the cloud and no sample-index list built per frame. Classification is invoked from the existing emission loop, so correspondence cannot drift through independent traversal logic.

Conceptual seam, not production-ready code:

```cpp
// All storage/configuration already exists; caller owns the source frame.
for (each existing run) {
    for (i = run.srcstart; i < (run.runlen & ~3u); i += 4) {
        // Capacity/topology were validated without changing sample selection.
        const auto flags = enabled
            ? classify_sample(src, i, domain, config)
            : CleanupFlags::Off;
        emit_original_xyzd_exactly_as_before(src, i, record_index);
        cleanup_flags[record_index] = flags;
        ++record_index;
    }
}
// Publish matching valid lengths and metadata together, never stale tail flags.
```

The source argument to the classifier is `const uint16_t*`. Ordinary scalar loads used in arithmetic are necessary; buffering a neighborhood's depth values is prohibited. Fixed counters, sums, extrema and bit flags are derived state, not copied source windows.

## 4. Startup-only storage and initialization

Allocate or statically reserve everything before acquisition starts:

| Item | Purpose | Bound |
| --- | --- | --- |
| Processed-domain membership table | Check whether a neighbor belongs to the established, currently processed run domain | Fixed 640×480 byte table: 307,200 B; a fixed bitset is an optional measured optimization |
| Cleanup flags | One byte per possible emitted record, aligned by record index | Actual cloud component capacity / 4; currently 230,400 B maximum, not a fabricated full-density producer |
| Configuration and status | Thresholds, preset IDs, algorithm version, context, counts | Fixed-size POD structures |
| Performance counters | Candidate/ambiguous/rejected counts and timing histograms | Fixed counters/bins; reuse existing metrics conventions |
| Kernel scratch | A few scalar accumulators and fixed-size derived counters | Compile-time bounded automatic storage; no variable-length arrays or heap |

Approximate initial additional persistent storage with the byte domain table: **525 KiB**, excluding an independently approved transport pool. The active flag prefix for ~40,000 points is only ~40 KB, but capacity is reserved at startup. No depth-frame storage is added.

Startup sequence:

1. Preserve/validate the actual run topology and cloud capacity.
2. Initialize domain membership from the actual processing runs, not an assumed circular AOI or a rendered mask.
3. Validate the exact emitter's maximum selected count against output capacity, without changing its index sequence.
4. Initialize fixed configuration, output storage, counters and any optional transport resources.
5. Perform any required first-use initialization before the allocation boundary. Do not hide lazy library initialization in the first live frame.
6. Set an explicit initialized state. Startup allocation failure disables cleanup or fails startup according to an approved policy; never retry allocation during capture.

The domain table contains geometry membership only, not depth samples, and is built once—not cleared/rebuilt every frame. Validate all runs before populating it. Do not assume a run is confined to a single image row.

For a runtime topology change, invalidate cleanup immediately and retain unfiltered output. Restart is the initial recovery policy. Supporting in-place topology rebuilds without allocation is a separate extension; never silently use the old domain table.

## 5. Bounded, no-sort classification algorithm

### 5.1 Eligibility and border checks

For source index `i`, derive actual `(x, y)` from the fixed sensor width. Check every neighbor's X and Y independently before reading its linear offset. Do not let `i - 1` wrap into the previous row or an offset cross an excluded AOI gap.

Use only neighbors known to be in the processed domain and compatible with the current source context. Do not invent invalid-depth or mm policies. Values without established interpretation cannot provide strong rejection evidence.

Insufficient valid support returns **Unclassified**, which remains displayed. Invalid topology/configuration disables cleanup for the frame rather than allowing out-of-bounds reads or partial masks.

### 5.2 Cheap candidate gate at every emitted sample

Use a fixed set of opposite-direction probes, initially at a maximum radius of two sensor pixels: horizontal, vertical and both diagonals. Up to eight source reads per sample.

- Track the largest supported opposite-side depth separation using scalar comparisons; do not sort probe values.
- If no separation exceeds the configured boundary threshold, retain the point immediately.
- Missing probes do not become zero depths or artificial edges.
- The gate runs only at emitted sample centers; there is no dense gradient image.

A wider probe radius is a later measured variant, not an automatic retry. Mixed bands wider than the chosen stencil may be missed; report that limitation rather than expanding until a result is found.

### 5.3 Detailed support test only for candidates

Use a fixed 5×5 neighborhood, reading values directly from `src` on each pass. Two bounded passes, no stored depth window:

1. Use the selected opposite-side probes as provisional near/far anchors.
2. First pass accumulates counts, sums and support-location evidence for values within tolerance of each anchor. Require multiple supporting pixels; an isolated extreme is not a surface.
3. Refine supported anchors using their bounded integer sums/counts. Do not compute a median or sort. This is an estimate of the classification reference, **not a replacement source depth**.
4. Second pass verifies support around refined anchors, tests support near the center depth and checks whether the two groups occupy coherent opposing parts of the stencil. Avoid double-counting overlapping anchor bands; if groups cannot be separated reliably, return Unclassified.
5. Reject a center only when it is meaningfully between two supported groups, lacks adequate same-surface support, and the spatial pattern is consistent with a transition rather than a third supported surface.
6. Apply a cheap smooth-ramp veto using fixed directional-profile comparisons and scalar residuals. A gradual, coherent slope must not be rejected merely because its end depths differ. No general plane fitting or iterative solver in v1.

Use widened arithmetic for depth differences, sums and products. Specify and test overflow bounds. Avoid signed/unsigned underflow and do not reinterpret source uint16 values as signed XYZD. Existing unsafe source-to-cloud narrowing outside the supported representation must not be exercised or “repaired” implicitly by cleanup.

All support thresholds, spatial tests and ramp-veto arithmetic must be written as explicit deterministic predicates before implementation acceptance. Tune them on saved organized-depth fixtures; this plan does not declare arbitrary universal millimeter values.

### 5.4 Result classes

Use one byte of flags, for example:

- `MixedCandidate`: strong evidence of unsupported intermediate depth.
- `BoundaryRisk`: supported depth discontinuity nearby; usable by an explicitly aggressive display policy.
- `Unclassified`: insufficient or ambiguous evidence; Conservative mode retains it.

Do not call these “valid/invalid” sensor readings or probability/confidence scores. If flags overlap, document precedence and count semantics.

An optional aggressive policy can hide `BoundaryRisk` samples, but **no full-frame mask dilation** is allowed. A wider exclusion band must be evaluated at emitted centers with a fixed, approved stencil and separate cost/edge-loss validation. Keep it out of the first kernel unless the conservative result demonstrably needs it.

### 5.5 Work bound

Starting design: at most eight gate reads plus two 25-position support traversals per candidate. Include center/probe rereads explicitly in the final code's load budget; do not advertise unique pixels as total loads.

For approximately 40,000 emitted points:

- Gate: up to ~320,000 neighbor reads/frame.
- All-candidate support path: up to ~2,000,000 additional neighbor reads/frame.
- No 307,200-center classifier and no 9×9 window at every image pixel.

These counts are **not timing predictions**. Branches, membership tests, arithmetic and cache behavior also cost CPU. Benchmark the all-candidate case, not merely a wall scene with almost no candidate points.

## 6. Lifetime, context and runtime behavior

- Run synchronously on the existing processing owner; no source copy or asynchronous access after its lock/lifetime ends.
- Publish a complete flag prefix with the exact emitted count, frame ID, revision, units/scope and algorithm/configuration generation.
- Zero-point clouds publish zero flags. Shrinking clouds must not expose stale flag tails.
- Do not reuse classifications across frames, even when geometry looks similar. No temporal matching or persistence in this version.
- Numeric threshold changes update a fixed POD configuration under existing ownership and increment cleanup configuration generation. No maps, strings, dynamic lookup tables or storage growth.
- Ordinary unit/context changes select an already defined compatible fixed configuration or mark cleanup unavailable. Do not apply mm thresholds to sensor-unit values.
- IR and unsupported source contexts remain unclassified/unfiltered. No automatic acquisition changes.
- Off bypasses the classifier. Do not add an avoidable full-capacity memset; either write the active flag prefix during emission or publish explicit Off semantics that cannot expose stale classifications.
- Do not log per point/frame or construct error strings in the runtime cleanup path. Use fixed status codes and existing bounded diagnostic counters.

## 7. Display and transport integration boundary

The initial implementation milestone is the allocation-free producer classifier and coherent sampled flags, with offline/native validation. The new console remains the eventual consumer: Off / Highlight suspect / Hide suspect, retaining originals and showing counts/provenance.

**Do not silently alter existing `/pointcloud/camera`, raw or OBJ bodies.** The current browser stream lacks flags, so console integration needs a separately reviewed, versioned same-frame transport design.

Requirements for that design:

1. It must satisfy the same startup-only allocation and no-extra-source-copy constraints for every new sensor-side cleanup path.
2. Do not use a new per-request `std::vector` snapshot, dynamic JSON/stream builder or concatenation buffer for the cleanup response.
3. Do not concatenate/copy the source depth or duplicate XYZD to manufacture the mask response. Geometry and flags should be emitted directly into bounded startup-owned publication storage if a new ownership model is approved.
4. Any publication slots, headers, descriptors and supported-request resources must be reserved at startup. Finite slot exhaustion uses an explicit bounded failure/skip policy—not allocation, capture blocking or reuse while a reader still owns the slot.
5. Do not hold camera ownership across socket writes. A zero-copy borrowed pointer into the mutable current cloud is not safe merely because it avoids allocation.
6. Geometry/flags/count/context must come from the same owned generation. No separate “latest mask” request paired with an unrelated cloud.
7. Audit actual HTTP helpers: existing response maps/strings/snapshots allocate. Reusing them does **not** establish that a new route meets the hard rule.

A preallocated leased-slot/scatter-gather publication path is a possible follow-up, not automatically approved scope. If it requires a substantial server ownership redesign, stop at the validated classifier milestone and present that dependency. Do not weaken the allocation/copy rule to finish the UI.

The shader can consume the per-point flags alongside packed XYZD and reject points through the existing early-clipping mechanism. Keep geometry/measurement originals, camera pose, framing reference and device exports unchanged. PNG provenance must distinguish cleanup from clipping. Browser integration cannot claim zero JavaScript/WebGL allocations; the hard device-processing contract must remain explicit.

## 8. Performance acceptance

Operator baseline: approximately **65% of one CPU core at 30 FPS**. Confirm whether this measures the camera thread, full process or cloud stage before calculating headroom.

At 30 FPS, each extra 1 ms of CPU work/frame costs roughly three percentage points of one core. If 65% represents all relevant serial CPU work, theoretical remaining budget is ~11.7 ms/frame; it is not a safe operating target and does not account for all lock/wall-time effects.

Provisional acceptance targets on the actual sensor CPU:

- Typical added cleanup CPU time **≤3 ms/frame**.
- p95 added cleanup time **≤5 ms/frame**; record p99 and worst observed time too.
- Sustained target cadence with measurement/status/HTTP responsiveness preserved.
- No allocation, growth, hidden queues, silently skipped sample classifications or altered point density under load.
- Baseline comparison includes enabled and disabled overhead, startup memory and extended camera-lock duration.

Measure fixed 40k-ish production sampling plus the largest legally supported run configuration, all-candidate checker/edge clutter, AOI borders and sparse-support cases. Full-image checkerboards may be physically unusual but are useful cost bounds.

Use fixed histogram counters in production; exact percentile calculations may be performed **offline after the benchmark**. The classifier never sorts. Preallocate benchmark sample storage before its allocation boundary if exact samples are retained.

Use same compiler/options, device frequency/thermal conditions, frame inputs, processing settings, target rate and client load for A/B/A comparisons. Host measurements do not predict sensor CPU timings. Initial target sequence: 30-second warmup and 120-second repeated runs, subject to operator approval.

If the bound fails, optimize the fixed kernel or reduce its approved complexity and revalidate effectiveness. Do not silently lower frame rate, narrow the domain, skip every other sample or allocate a worker queue. Additional cores are not assumed free.

## 9. Validation gates

### Algorithm and geometry

- Sharp two-surface step with inserted intermediate band, multiple orientations and band widths.
- Broad ramps/curves, flat noise, three real depth layers, small isolated objects, thin poles/wires and genuine middle-depth surfaces.
- Isolated extreme probes: must not establish a supported near/far surface.
- Insufficient support, holes, domain boundaries, top/bottom/left/right image borders and no row-wrap reads.
- Exact original emitted indices for aligned/unaligned starts, short/empty runs, cross-row fixtures, adjacent runs, AOI gaps, first/last source pixels and existing tail rules.
- Cleanup disabled/enabled: source bytes unchanged; XYZD bytes/count/order identical to the baseline emitter; only flags/derived counters differ.
- Scalar/SIMD processing input parity where relevant; no changes to calibration, EMA or plugin policy.
- Unit/context changes, unavailable configurations, topology invalidation, zero/shrinking/max output and complete publication identity.

### Allocation and copying proof

- Allocate all fixtures, output, metadata and instrumentation before enabling the runtime allocation guard.
- Trap/count `new`, `new[]`, aligned allocation, `malloc`, `calloc`, `realloc` and other allocator entry points actually linked into the tested path. Test frees/reallocation too; no dynamic lifecycle churn after startup.
- Include the **first call**, repeated frames, enable/disable, all-candidate input, threshold/context changes and failure paths. Do not warm up an allocating first call outside the guard and then claim startup-only behavior.
- Disable test-harness logging/result serialization inside the guarded interval; it must not obscure implementation allocations. Separately audit every newly added integration path, not only the leaf kernel.
- Source hashing/read-only protected fixtures and output comparisons prove no source mutation. Static inspection/instrumentation must additionally prove absence of source-buffer copies; immutability alone does not prove no copying.
- No `sort`, `nth_element`, median library, resizable container or source-window staging in the runtime cleanup call graph.
- Fixed buffer canaries and appropriate ASan/UBSan checks cover overflow and invalid reads. Allocation tracing is a separate unsanitized check because allocator instrumentation can interact with sanitizer runtimes.

### Integration and evidence

Preserve the original working tree, including unrelated uncommitted/untracked work. Store ignored evidence under `_build/sampled-boundary-cleanup/{baseline,validation,benchmarks,screenshots,review,handoffs}/`.

Use SDK-free production-path tests and local captures first. Run affected native/export/UI gates for any approved integration. No physical camera access, target build/deployment or device mutation without explicit authorization.

After nontrivial implementation and focused validation, obtain the required fresh-context read-only critic review of the phase-only diff and actual evidence. Parent resolves evidence-backed in-scope findings and reruns affected checks.

## 10. Implementation phases and completion gates

1. **Contract/baseline:** freeze the sampled-index and capacity contracts, processing-domain policy, allocation boundary, source fixtures and CPU baseline. No broad re-diagnosis of already confirmed sensor mixing.
2. **Pure fixed kernel:** implement deterministic direct-read, no-sort classification with bounded scalar scratch and explicit predicate tests. No transport/UI change.
3. **Run-loop integration:** preallocate domain/flags at startup and classify exactly the emitted centers in the existing loop. Prove unchanged source/XYZD and coherent mask identity.
4. **Allocation/performance gate:** first-call and lifecycle allocation traps, copying audit, stress/sanitizer tests and operator-approved target A/B/A timing. Do not proceed if the constraints or retention/cleanup tradeoff fail.
5. **Optional console delivery:** approve the same-frame, startup-owned transport/ownership design before implementing it; reuse the single controls panel and shader. Existing allocating HTTP paths are an explicit dependency, not an exemption.
6. **Review/handoff:** independent critic, corrected findings, exact evidence and rollback. Keep cleanup opt-in until operator acceptance.

The core classifier milestone is complete only with **zero post-startup cleanup allocations, no source-data copies, no sorting, unchanged sampled XYZD, bounded per-sample work and measured effectiveness/cost**. End-to-end console cleanup is complete only after its transport also meets the stated sensor-side constraints; a producer-only milestone must not be reported as the finished viewer feature.
