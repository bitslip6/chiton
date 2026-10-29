# Phased plan: depth-edge and point-cloud cleanup

**Status: plan only.** This document does not authorize implementation, device-setting changes, camera access, deployment, a cross-build, commits or pushes. Each implementation phase needs approval. Repository: `/home/cory/Work/chiton`.

## 1. Goal and governing principles

Improve comprehension of the point cloud by reducing apparent “floating” points between foreground and background objects, especially near depth discontinuities.

The reported symptom is consistent with **flying pixels / mixed-depth edge artifacts**, but the root cause is not yet established. Sensor mixing, multipath, temporal averaging, spatial processing and projection faults can produce superficially similar artifacts.

Distinguish three outcomes:

1. **Clearer display:** unchanged measurements become easier to interpret.
2. **Conservative rejection:** suspect measurements are marked or omitted from the display.
3. **Reconstruction:** measurements are moved, filled or replaced by inferred surfaces.

Prefer the first two. **A small, honestly labeled gap is preferable to a false surface connecting two objects.** Removing points is not proof of improved measurement accuracy.

### Scope and safety

- Target the new `/console.html` system, not legacy `/app.html` or homepage routing.
- Preserve the measurement inspector, histories, Kalman functionality, clipping, point appearance, EDL, framing, source selection, exports and lifecycle behavior.
- Preserve original depth/XYZD values and existing device export contracts. Initial cleanup is viewer-only and opt-in.
- Never silently delete all zero-depth, negative-D or Y=0 records; these require explicit policies and provenance, not guessed validity rules.
- Never treat adjacent cloud records as adjacent sensor pixels, or matching record indices across frames as established correspondence.
- Use acknowledged units and scope. Do not apply millimeter thresholds to unknown or sensor-unit values.
- Keep an immediate unfiltered reference. Prefer **Off / Highlight suspect / Hide suspect** over an unexplained “enhance” switch.
- No quality degradation, record decimation or rate changes merely to claim successful cleanup or 30 FPS.
- Use local saved data, explicit `?source=demo`, loopback fixtures and verified SDK-free stubs for automated work. Physical-camera experiments require separate operator approval.

## 2. Current implementation facts to reverify before coding

| Finding | Consequence |
| --- | --- |
| `console-cloud-view.js` uses opaque, depth-tested points with antialiasing disabled; the point material uses hard sprite boundaries and no blending. | The renderer can visually merge large sprites, but it does not interpolate near/far XYZ into new intermediate points. |
| `calibration::average_frame()` uses 80% previous / 20% current depth when smoothing is enabled. | A change from a 4 m background to a 1 m foreground initially yields 3.4 m. Temporal smoothing can manufacture floating points during motion or edge jitter. |
| The smoothed calibrated path uses 70% previous / 30% corrected current depth. | The corresponding first result is 3.1 m. Investigate this separately from stable sensor-generated mixed returns. |
| `src/main.cpp` projects the processed depth after averaging/calibration and an optional plugin. | `/raw/camera` is processed depth, not necessarily untouched SDK output. A cloud-only fix must not silently alter measurements. |
| The browser receives packed signed XYZD, without source-pixel IDs or per-point confidence. The producer samples sparse runs. | Generic 3D support filtering can be client-only; reliable sensor-grid boundary filtering needs producer-side access or an explicit mapping contract. |
| SDK headers expose depth/IR/raw types and some noise-reduction facilities; startup sets `noise_reduction_threshold`. | These declarations do not establish available confidence data, effective filtering, successful application or live reconfiguration. Capability and acknowledgment need verification. |
| Existing analysis reproduced a projection-model/AOI mismatch capable of producing a zero-elevation band. | Keep that investigation separate. Do not use cleanup to conceal incorrect projection or assume it explains this edge symptom. |

Read before implementing:

- `release/files/console-{app,cloud,cloud-view,cloud-material,view,live,demo,scheduler}.js`, `console.html`, `console.css`.
- `src/graphics/calibration.cpp`, relevant SIMD kernels, `src/main.cpp`, `src/webserver/settings.cpp`, relevant SDK interfaces.
- `docs/pointcloud-artifact-analysis.md`, `docs/pointcloud-artifact-analysis-plan.md`.
- `docs/measurements.md`, `docs/http-contracts.md`, `docs/console-pointcloud-clipping.md`, `docs/console-display-first.md`, `tests/SAFETY.md`.
- Existing clipping/material/framing/lifecycle/source/capture/native tests and offline cloud analyzer.

## 3. Ranked option inventory

Rank reflects expected benefit for the reported symptom, effort and risk—not a guarantee or an obligation to implement every option.

| Rank | Option | Expected quality improvement | Difficulty / dependency | Main risk or limitation |
| --- | --- | --- | --- | --- |
| 1 | Sensor-grid depth-boundary rejection | Very high for mixed-depth edges | Medium algorithm; medium–high integration | Thin objects and legitimate boundaries can be rejected |
| 2 | Discontinuity-aware temporal smoothing | Very high **if existing EMA contributes** | Low–medium backend | Less smoothing during changes; needs reliable reset/gating |
| 3 | Adaptive 3D neighborhood/support filtering | High for isolated points; moderate for dense bridges | Medium; current client stream is sufficient | Sparse distant surfaces and small objects resemble outliers |
| 4 | Confidence / return-strength / saturation filtering | High when useful quality data exists | Medium–high; SDK and same-frame metadata | Strong mixed returns can still look confident |
| 5 | Narrow depth-edge exclusion band | High visual cleanup | Low–medium once organized depth is available | Shrinks silhouettes and deliberately creates holes |
| 6 | SDK filtering and acquisition/optical tuning | Potentially high at source | Medium qualification; hardware-dependent | Trades noise, range, detail and motion response |
| 7 | Temporal median / persistence filtering | Moderate–high for flickering artifacts | Medium; correspondence required | Latency, motion trails; stable mixed pixels survive |
| 8 | Robust local surface / plane-consistency filtering | High on broad surfaces | Medium–high | Can erase corners, curved surfaces and thin objects |
| 9 | Smaller points, EDL, clipping, orthographic views, clearer colors | Moderate interpretability benefit | Mostly already available | Does not correct erroneous depths |
| 10 | Edge-preserving spatial denoising | Moderate for noise; limited for mixed-depth bridges | Medium | Can preserve false ramps or blur boundaries |
| 11 | Depth-aware surfels / discontinuity-safe meshing | High apparent surface clarity | High | Can invent connections or conceal erroneous geometry |
| 12 | Multi-frame/multi-view reconstruction | Potentially very high for static scenes | Very high; tracking and registration | Motion, latency and false accumulated surfaces |
| 13 | IR/RGB-guided or learned restoration | Potentially high apparent quality | High–very high; aligned data/model validation | Plausible inferred geometry is not measured geometry |

Ordinary blur, unconditional EMA, arbitrary stride decimation and transparency are **not recommended primary corrections**. Screen-space smoothing is view-dependent and cannot establish which 3D measurements are wrong. More cloud refreshes or a renderer rewrite do not resolve mixed returns.

## 4. Phase 0 — Preserve baseline and define acceptance

### Work

1. Record repository status, tracked patches and relevant untracked copies/hashes. Preserve the entire working tree, not HEAD alone. No reset, clean, stash or unapproved commit.
2. Record renderer settings, cloud rate, pose, DPR/buffer size, acquisition context, units and known deployed/source provenance.
3. Separate the reported edge artifact from the previously investigated zero-elevation/projection issue.
4. Define representative scenes and the acceptable tradeoff between clutter removal and lost detail before selecting thresholds.

### Required scene set

- Sharp foreground/background boundary with a substantial depth gap.
- Flat surface and smooth slanted/curved surfaces.
- Thin pole/wire, narrow foreground feature and small isolated object.
- Sparse distant surface and dense nearby surface.
- Stationary scene, moving foreground and camera motion.
- Reflective/dark/translucent material where available in approved captures.
- Known invalid/saturated/out-of-range samples only where their meaning is established.

### Evidence and exit gate

Store ignored artifacts under `_build/pointcloud-cleanup/{baseline,diagnostics,validation,screenshots,review,handoffs}/`.

Agree on measurable acceptance: reduction of known artificial bridge points, retention of known real geometry, edge loss, temporal lag, stable behavior across density/range, processing cost and unchanged measurement/export behavior. Use synthetic ground truth for numeric assertions; do not claim real-scene accuracy without suitable reference evidence.

## 5. Phase 1 — Diagnose which stage introduces the artifact

### Work

1. Inspect saved XYZD captures and same-frame organized depth when available. Do not diagnose from JPEG softness alone.
2. Re-render one frozen cloud with small opaque points, EDL off, fixed pose and no clipping. Compare an independent point plot.
3. Compare a stationary edge with an edge moving across sensor pixels.
4. If separately approved, compare current smoothing on/off while holding all other settings fixed. Record acknowledgments and restore the original setting.
5. Compare SDK depth, processed depth and projected samples only when the same-frame relationship is established. The current processed-raw endpoint is insufficient to label data “untouched SDK.”
6. Inspect a depth cross-section through the boundary: distinguish isolated spikes, stable intermediate bands, temporal trails and projection-induced distortions.
7. Verify effective SDK filtering and optional plugin involvement rather than assuming a named control is active.

### Capture contracts

- Preserve each binary body's own frame ID, revision, units and headers.
- Separate HTTP reads may represent different frames; do not pair them by proximity in time alone.
- Source-pixel maps must correspond to the exact emitter/configuration. Do not attach an example sidecar to an unrelated cloud by record order.
- No automatic mode, calibration, illumination or register experiments.

### Exit gate / branch decision

- Strong EMA dependence: prioritize Phase 4 before adding expensive cleanup.
- Stable mixed-depth boundary: prioritize Phase 5; Phase 3 remains a useful reversible prototype.
- Mostly isolated noise: prioritize Phase 3.
- Confirmed projection defect: stop that branch and propose a separate geometry correction. Do not filter away evidence of the defect.
- Inconclusive evidence: retain uncertainty and request the smallest missing capture, not unrestricted device access.

## 6. Phase 2 — Establish the best unchanged-data display

### Work

Evaluate existing controls on matched captures:

- Smaller point diameter to avoid merging sparse artifacts into a curtain.
- EDL off/on for depth perception, recognizing it may emphasize erroneous points.
- Depth slices and XYZ clipping to isolate relevant objects.
- Orthographic/front/top/side inspection.
- A separate palette improvement only if necessary and approved; visible endpoints must not imply validity.

### Exit gate

Retain a documented reference preset and matched screenshots. Report this as **presentation improvement**, not artifact correction. Do not automatically change the operator's appearance, clipping or camera settings.

## 7. Phase 3 — Reversible client-only support-filter prototype

**Recommended first implementation experiment when diagnosis does not identify a simpler upstream fix.** No acquisition or wire-format change is required.

### Algorithm

1. Build a bounded spatial index over the current immutable XYZD snapshot.
2. Evaluate radius-neighbor support and/or statistical neighbor-distance outliers.
3. Account for changing point density and distance; do not assume one fixed metric radius works for every surface and range.
4. Produce a separate suspect mask/score. Do not move points or overwrite original depths.
5. Start conservatively. Dense false ribbons may survive; record that limitation rather than adding aggressive heuristics without evidence.

Avoid O(N²) all-pairs scans. Measure typical ~40k and maximum supported 307,200-record cases. Choose CPU/worker/GPU placement based on measured cost, not fashion. If asynchronous processing is used, retain bounded latest-frame work, generation cancellation and no backlog.

### UI and data contracts

- Add cleanup under the existing single Display controls tree: **Off / Highlight suspect / Hide suspect**.
- Initially default Off. Show algorithm, scope and relevant parameters; do not call a geometric heuristic “sensor confidence.”
- Highlight mode retains all points and makes classifications inspectable. Avoid transparency as the only distinction.
- Show returned count, cleanup-retained/rejected count and final displayed count after clipping, with explicit predicate composition. Do not conflate rejection with clipping or validity.
- Keep original frame ID/context tied to the mask. Never display a mask belonging to another snapshot.
- Filter toggles do not refit, alter pan anchor/reference distance, restart acquisition, write device settings or change measurements/history/Kalman.
- Preserve pause/hide suppression of network and actual renderer paints. Local settings may be retained without resuming work.
- Do not add browser persistence unless explicitly approved.
- Fit all/main retain their established semantics. Any “Fit displayed points” action is separate and explicit.
- PNG provenance identifies cleanup mode/parameters/counts and clipping. Device raw/OBJ/binary remain unchanged; a cleaned geometry export would be a separate approved feature.
- A display mask does not inherently reduce response bytes, decoding or submitted vertices. Report actual implementation behavior honestly.

### Validation / exit gate

- Known artificial outliers rejected; known thin/distant objects retained within approved tolerances.
- Empty/single/duplicate/max clouds, signed limits, context changes, retries and out-of-order asynchronous results handled safely.
- Unfiltered restoration is exact; original records/hashes unchanged.
- Existing clipping, EDL, appearance, fullscreen, PNG and lifecycle contracts pass.
- Processing time, responsiveness, memory and actual GL upload behavior measured at target2/10/20/30, without promising unattainable hardware rates.
- Stop if meaningful cleanup requires unacceptable loss of real geometry. Do not promote the prototype merely because screenshots look smoother.

## 8. Phase 4 — Conditional temporal-processing correction

Implement only if Phase 1 demonstrates a meaningful temporal contribution. This is a device-processing change and requires separate approval from viewer-only work.

### Work

- Small, same-surface changes: retain appropriate smoothing.
- Large credible depth transition: reset history or rapidly follow the new depth instead of averaging foreground/background.
- Ambiguous boundary measurements: preserve uncertainty rather than manufacturing intermediate values.
- Define initialization, dropout, invalid-value and acquisition/context-reset behavior.
- Evaluate a short temporal median or persistence rule only if flickering artifacts remain. Quantify latency and moving-object loss.
- Maintain scalar/SIMD parity and avoid unconditional comparisons by cloud-record index.

### Scope decision required

Choose explicitly between:

1. Correcting the device-wide processed-depth path, which affects measurements and exports; or
2. A separate cloud-display processing path that leaves measurement depth unchanged.

Do not change one while labeling it as the other. Revisit metadata/revision semantics and rollback accordingly.

### Exit gate

Production-path tests cover step transitions, static noise, motion, edge jitter, invalid samples and context changes. Validate changes to numeric histories and exports where applicable. Retain an unsmoothed baseline and document the remaining stable mixed-return limitation.

## 9. Phase 5 — Sensor-grid boundary rejection and quality mask

**Preferred targeted solution for persistent mixed-depth boundaries.** Requires original sensor-grid access or an explicit, trustworthy source-pixel mapping.

### Algorithm

For each sensor pixel:

1. Examine a small neighborhood in the original depth grid.
2. Detect a significant depth discontinuity using unit/range-appropriate thresholds.
3. Identify supported foreground/background groups.
4. Mark intermediate measurements lacking adequate surface support as suspect.
5. Distinguish ordinary slopes/curvature from discontinuities; preserve thin-object evidence where possible.

Do not merely reject every large gradient. Do not replace suspect depths with the nearer/farther surface by default.

Optional aggressive mode: expand the detected depth-edge mask by a small configurable band. Clearly label its silhouette erosion and thin-object risk. Default it off until separately accepted.

### Integration

- Prefer analysis before sparse point emission, while pixel neighborhoods are available.
- Keep depth and XYZD originals intact; carry a separate mask/quality value.
- Define a versioned or explicitly optional metadata/data extension if the browser needs pixel IDs or quality. Existing binary/OBJ routes must not silently change shape or order.
- Quality and geometry must share frame/context identity and bounded lifetime. Separate unsynchronized requests are not sufficient.
- Do not assume XYZ direction reliably reconstructs the exact original pixel map, especially with the existing projection-domain questions.
- Reuse Phase 3's UI and provenance rather than creating another competing filter panel.

### Exit gate

Demonstrate improved bridge suppression versus Phase 3 at comparable real-point retention. Test foreground/background contrast, slopes, corners, thin objects, sparse sampling, unit changes, maximum clouds and moving scenes. Report boundary pixels lost and residual false ribbons, not just total rejection percentage.

## 10. Phase 6 — Sensor quality signals and acquisition/optical qualification

This phase is conditional on hardware support and explicit operator-controlled device testing.

### Work

- Verify actual availability and semantics of confidence, amplitude, saturation, invalid-depth and phase-consistency information.
- Do not equate IR brightness with confidence or assume depth/IR are synchronized.
- Establish whether SDK noise reduction is thresholding, spatial filtering, temporal filtering or something else; verify successful application/read-back.
- Evaluate supported integration/exposure/filter settings one at a time.
- Inspect focus, optics cleanliness, protective-window reflections, crosstalk, multipath and difficult materials.
- Respect qualified sensor range and safe operating limits. No arbitrary illumination increases or register writes.
- If intrinsic resolution/optical mixing dominates, compare hardware alternatives separately; software cannot reliably recover two surfaces from one ambiguous depth value.

### Exit gate

Acknowledge/record settings, preserve original captures and restore known baseline settings. Quantify the tradeoff between fewer mixed returns, missed dark/distant surfaces, saturation, motion response and frame rate. Combine quality evidence with geometry conservatively; a strong return does not prove an unmixed return.

## 11. Phase 7 — Optional advanced processing; separate proposals

Proceed only if earlier phases leave a demonstrated unmet need.

| Branch | Suitable use | Required safeguard |
| --- | --- | --- |
| Robust plane/surface consistency | Broad walls, floors, boxes, machinery | Preserve nonplanar objects and label model-based rejection |
| Bilateral/guided denoising or median replacement | Surface noise after discontinuity handling | Separate changed depths from original measurements; quantify edge movement |
| Surfels / discontinuity-safe meshing | Presentation or reconstruction view | Never connect across unsupported depth jumps; retain point reference |
| Multi-view fusion / TSDF | Static scene reconstruction | Registration, moving-object rejection and explicit accumulated-view provenance |
| RGB/IR-guided or learned restoration | Specialized reconstructed display | Verify alignment; distinguish inferred surfaces from measured geometry |

Do not implement these as silent default cleanup. Screen-space filters and transparency are cosmetic modes, not substitutes for source-space validity evidence.

## 12. Phase 8 — Integration, independent review and release decision

### Validation

- Run focused algorithm tests plus existing console source/scheduler/model/Kalman, clipping/material/framing/EDL, layout/controls/lifecycle and capture suites.
- Validate interactions between cleanup, clipping, zero displayed points, unavailable frames, paused state, hidden tabs, fullscreen and exports.
- Benchmark fixed frame/pose/style/DPR/buffer configurations separately from layout or rate changes. Record latency median/p95, mask/index costs, actual uploads, resource peaks and achieved distinct/render rates.
- Use isolated verified SDK-free native tests for backend changes, appropriate sanitizer gates and static/package/install parity. Follow `tests/SAFETY.md`; do not run a camera-owning/deployed binary by inference.
- Inspect actual before/after screenshots and original/highlight/hidden comparisons. Validate thin-object preservation, not only cleaner empty space.

### Review and handoff

After nontrivial implementation and focused validation, obtain one fresh-context read-only critic review of the approved phase's feature-only diff, including relevant untracked files and actual evidence. Parent classifies findings, fixes evidence-backed in-scope issues and reruns affected checks. Re-review only the necessary fix scope.

Record a durable checkpoint containing changed files, algorithms/parameters, context/unit contracts, exact tests, performance, captures, rejection/retention evidence, review disposition, limitations and rollback instructions.

### Completion boundary

A phase is complete only when its acceptance gate passes and no accepted review findings remain. Local validation is not hardware qualification, operator visual approval or deployment authorization. Keep cleanup opt-in until the operator approves its real-scene tradeoffs.

## Recommended execution order

1. **Phases 0–1:** preserve evidence and determine whether EMA, sensor mixing or geometry is responsible.
2. **Phase 2:** establish a clear unchanged-data reference using existing controls.
3. **Phase 3:** test reversible client-only support filtering, unless diagnosis points to a simpler Phase 4 correction first.
4. **Phase 4, when justified:** remove temporal foreground/background blending with an explicitly approved processing scope.
5. **Phase 5:** implement targeted sensor-grid boundary rejection if generic support filtering is insufficient.
6. **Phase 6, when available/approved:** incorporate real quality signals and improve acquisition conditions.
7. **Phase 7 only by separate decision:** consider reconstruction rather than treating it as obligatory cleanup.
8. **Phase 8 applies to every implementation delivery**, not just the final advanced phase.

The preferred endpoint is **conservative, inspectable rejection of unsupported edge points while preserving original measurements**, not a smooth-looking reconstruction that hides uncertainty.
