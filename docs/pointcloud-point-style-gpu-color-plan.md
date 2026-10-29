# Plan: point appearance, size controls and GPU depth coloring

**Status:** implementation plan only. No renderer changes or implementation subagents have been launched for this planning request.

**Repository:** `/home/cory/Work/chiton`. Planning baseline: `461b2f0`; working tree was clean before adding this document. Recheck the actual tree before implementation.

**Requirement:** implement feature **#2, better point appearance and size controls**, and feature **#5, move coloring into a GPU shader**, from [point-cloud-features.md](../point-cloud-features.md). Use bounded implementation subagents, with parent-controlled integration and independent review.

## 1. Scope and non-goals

### Included

- Circular and square point styles, with adjustable screen-space diameter.
- An optional, bounded perspective-scaled size mode; constant screen size remains the default.
- Vertex-shader depth coloring using the existing signed depth component and acknowledged color range.
- Packed signed-int16 XYZD GPU attributes, subject to compatibility and correctness gates.
- Viewer-local controls, lifecycle handling, PNG consistency, focused performance evidence and regression tests.

### Excluded

- **Feature #1, a new palette.** Preserve the current color function, including its black endpoints. Moving it to the GPU must not silently change the palette. A visible-endpoint palette remains a separate improvement.
- Geometry projection, AOI/calibration changes, acquisition smoothing, validity thresholds, filtering, point deletion, sampling density, inverse projection or picking.
- New endpoints, cloud formats, compression/streaming, WebGPU, a Three.js upgrade, adaptive resolution/LOD, meshing, normals, eye-dome lighting or temporal accumulation.
- Physical-camera access, a real SDK/camera-owning service, deployment, default-page promotion, commits or pushes without separate authorization.

All returned records remain represented, including origins, zero/negative coordinates and signed negative depth. Appearance controls affect rendering and viewport PNGs only—not measurements, local Kalman estimates, device OBJ/raw exports or their bytes.

## 2. Current implementation and contracts to preserve

Read before implementation:

- `release/files/console-{cloud,cloud-view,app,view}.js`, `console.html`, `console.css`.
- `tests/console_cloud.js`, `console_cloud_scheduler.js`, `console_cloud_browser.js`, `console_native_browser.js`, `console_layout.js`, `console_viewport.js`, `console_assets.py`.
- `tools/capture_console.js`, `tests/console_fixture.js`, `tests/console_native.py`.
- [Measurement contracts](measurements.md), [HTTP contracts](http-contracts.md), [test safety](../tests/SAFETY.md), [console preview](console-preview.md), [3D checkpoint](console-3d-preview.md), and applicable [3D requirements](console-3d-implementation-plan.md).

Verified planning facts:

| Current behavior | Implementation consequence |
| --- | --- |
| Bundled Three.js is r128; `PointsMaterial`, size 2.5, `sizeAttenuation:false`, no renderer MSAA | Use r128-compatible point shaders; do not assume newer APIs or promise antialiased edges from MSAA that is not enabled. |
| Each distinct snapshot fills reusable Float32 XYZ and RGB arrays | Remove CPU per-point color generation and the RGB attribute; preserve reusable storage and draw-range shrinking. |
| The owned snapshot is an endian-decoded `Int16Array` of XYZD | Upload decoded signed values, not unvalidated wire bytes. The protocol and decoder remain unchanged. |
| `recolor(snapshot, range)` currently changes the color buffer and increments uploads | Replace this with uniform-only range changes; preserve frozen-frame recoloring and matching legend/PNG. |
| Painting is on demand, capped at 30 Hz; duplicate frames do not upload geometry | Slider/range changes request one coalesced repaint, not a new animation loop, fetch or geometry upload. |
| Renderer recreation occurs on representation/context changes and explicit retry | Viewer appearance state must survive recreation and be applied on every construction path, including deterministic capture. |
| Tests directly inspect `.positions`, `.colors` and `.uploads` | Update these tests to inspect packed attributes, actual rendered colors and explicitly defined counters; do not retain wasteful arrays just to satisfy old test internals. |

Preserve the current single-flight cloud scheduler, transport deadlines, contextual frame handling, measurement independence, stable user camera, bounded DPR/drawing-buffer dimensions, context-loss behavior and disposal. Do not refit the camera when appearance or color range changes.

## 3. Product behavior

### Controls

Place a compact, expandable **Point appearance — this viewer only** group in the existing 3D controls. It must work at 320 px width and in fullscreen without crowding the primary navigation/download actions. The current fullscreen root is `#viewport`, while `#cloud-navigation` sits outside it: explicitly make the one appearance-control group available inside that root during fullscreen (for example, reparent and restore it on fullscreen changes). Do not create duplicate control IDs or silently change which element becomes fullscreen.

| Control | Initial contract |
| --- | --- |
| Shape | `Circle` / `Square`; proposed default **Circle**. Square is available for baseline comparison. |
| Diameter | **1–10 CSS px**, step **0.5**, default **2.5**. Slider plus a fixed-width numeric readout; keyboard operable. |
| Sizing | `Constant screen size` / `Perspective scaled`; default **Constant screen size**. |
| Reset appearance | Restores these defaults without resetting camera pose, source context or acquisition. |

These are in-memory, per-page settings; no new persistent device or browser defaults. Retain them across mode switches, pause/resume, context changes and renderer retry; reload returns to defaults. Keep the controls clearly local and disable irrelevant actions when 3D is unavailable. Do not intercept viewport navigation keys while a slider/select has focus.

The default shape is a proposed presentation choice, not user visual approval. Capture circle-versus-square comparisons before final acceptance.

### Point size semantics

- Diameter is in **CSS pixels**, not millimeters or source units.
- Convert CSS diameter to drawing-buffer pixels using the renderer's **effective** pixel ratio, not raw `devicePixelRatio`. Existing resolution caps can reduce effective DPR.
- Query and cache `ALIASED_POINT_SIZE_RANGE` for each actual context. Clamp to the supported physical-pixel range, and expose hardware limiting rather than claiming the requested diameter was achieved.
- Constant-size mode preserves apparent diameter while orbiting/zooming.
- Perspective-scaled mode uses a declared inverse **view-depth** scale, not raw returned depth as a proxy for camera distance. Capture a positive reference distance on initial Fit/Reset; refresh it only on explicit Fit/Reset or a new geometry context, not every frame.
- Bound the scale multiplier to **0.5–2.0** before hardware clamping. This limits the largest requested diameter to 20 CSS px at the UI maximum and prevents huge near-camera sprites.
- Behind-camera, near-plane and degenerate cases must never produce negative, infinite or NaN point sizes. Normal camera clipping remains authoritative.

### Shape, depth and edges

- Square mode draws the complete point sprite; circle mode discards fragments outside a circular footprint using `gl_PointCoord`.
- Keep points **opaque with depth test and depth write enabled**. No blanket transparency, blending-based haze or per-frame point sorting.
- Crisp opaque circles are required. Edge antialiasing is optional only if it demonstrably preserves occlusion and works on the supported WebGL path. Do not introduce a mandatory derivatives/WebGL2 extension or falsely claim hard-discard edges are antialiased.
- Point count means submitted records, not covered pixels; changing shape/diameter does not change returned/rendered record counts.

## 4. Renderer and GPU design

### One material owns both features

Point shape, size and coloring share one shader/material seam. **Do not assign #2's shader and #5's shader to competing writers.** One renderer subagent owns them together; a second subagent owns UI/controller integration.

Prefer one small flat asset, `release/files/console-cloud-material.js`, following the existing namespace/CommonJS test pattern. It can contain pure point-style validation, shader sources and material construction. Load it before `console-cloud-view.js`. No runtime import, CDN or dependency upgrade.

Freeze this integration contract before either component handoff:

```text
pointStyle = {
  shape: 'circle' | 'square',
  diameterCssPx: finite number in [1, 10],
  sizing: 'screen' | 'perspective'
}

CloudView.setPointStyle(pointStyle)
CloudView.setColorRange(rangeOrNull)
CloudView.update(snapshot, rangeOrNull)    // unchanged incoming snapshot contract
CloudView.counters(now)                  // adds explicit upload/uniform/style evidence
```

Setters validate and copy state, update uniforms and mark the view dirty. They do not mutate a snapshot, request network work, recreate geometry/materials or change the camera. Material shader variants should not recompile when a control changes; prefer uniforms for these small switches.

### Packed attributes: preferred representation

Use a reusable renderer-owned `Int16Array(CLOUD_MAX * 4)`, one r128 `InterleavedBuffer` with stride four, and attributes:

- position: three signed components, offset zero, **not normalized**;
- depth: one signed component, offset three, **not normalized**.

A distinct frame copies the owned snapshot's decoded records into this bounded buffer, marks the populated interleaved range once (`offset=0`, `count=recordCount*4` components, not bytes), and updates draw range. The renderer must not mutate or retain an alias to mutable source storage. Continue computing bounds from authoritative CPU records for navigation; GPU coloring does not eliminate all O(n) CPU work.

Prove r128 binds signed-short attributes correctly to float shader inputs on the actual supported WebGL context. Empty and shrinking clouds must clear stale geometry without uploading unnecessary tails. Keep one Points/geometry/material, not one per response or style.

Expected attribute storage/upload change:

| Representation | Bytes/record | Capacity at 307,200 records |
| --- | --- | --- |
| Current Float32 XYZ + RGB | 24 | 7,372,800 B |
| Preferred packed int16 XYZD | 8 | 2,457,600 B |
| Conditional fallback: Float32 XYZ + depth | 16 | 4,915,200 B |

These are renderer attribute capacities, **not total browser/GPU memory or HTTP bandwidth**. The separate owned snapshot remains. Prefix uploads, driver behavior and browser timings must be measured rather than inferred from these counts alone.

If packed r128 attributes fail an evidenced compatibility/correctness gate, stop for parent disposition. A reusable Float32 XYZ + depth fallback may be accepted, still with GPU coloring and no RGB array. Do not silently ship mixed paths or claim the 3x attribute reduction when using the fallback.

### Depth coloring in the vertex shader

- Calculate color once per point in the vertex shader; pass RGB to the fragment shader. Do not repeat the depth calculation for every covered fragment.
- Use high-precision vertex arithmetic for signed depths, range bounds and normalization. Do not pass large unnormalized depths through a low-precision fragment varying.
- Preserve `cloudColor`'s current clamped piecewise formula and neutral color `[0.6, 0.67, 0.73]` when the range is absent/invalid. Keep the CPU helper as a test/reference function, not a production per-point coloring loop.
- Missing, nonfinite or non-increasing ranges select neutral coloring without division by zero. Negative signed D stays negative and follows the same clamping as today.
- Match the existing renderer's color-space/output behavior. Adding tone mapping, gamma conversion or a different palette is not part of this migration.
- A range change updates uniforms only, including while IDs are frozen. Synchronize rendered range, on-screen legend and PNG provenance.

### Counters and performance invariants

Separate geometry uploads from uniform changes. Retain or migrate existing `uploads` consumers deliberately; do not label a range/shape change as an XYZD upload. Expose at least geometry-upload count, uploaded attribute bytes, uniform-update count, actual attribute capacity, selected style and hardware size limits.

Count bytes accurately for the chosen path and distinguish application-requested uploads from directly observed WebGL calls. Do not claim uniform updates cost no GPU work: they still cause a repaint.

## 5. Subagent implementation topology

### Execution policy

This is **multi-seam work**: renderer/material, UI/controller and integrated qualification are independently testable boundaries. The parent is the integration-only owner and final acceptance authority.

Default to **sequential component subagents**, not parallel writers. Both requested features share the material seam, and the UI benefits from a working renderer API. Sequential sole-writer ownership also avoids requiring a commit or losing working-tree additions when creating a HEAD-only worktree.

Before execution, enable subagents and discover executable agents with `subagent({action:'list', capabilities:true})`. Use native `worker` roles for implementation/qualification and the configured fresh-context read-only `critic` for the final checkpoint. Do not assume custom agent names or external CLI availability.

Compose coordinated work using an async workflow and labeled, stable-key `runs.run` stages. Consume durable handoffs and parent decisions at dependency barriers; do not blindly advance after an error. If using separate coordinated waves around parent integration, retain the mission/handoffs. No child may launch other agents.

Before each writer starts, record its explicit repo/cwd/ref, file claims, authority, next gate and handoff path. Only that writer may mutate its cwd during its run; parent work is read-only preparation. All component writers must be terminal before parent integration. A read-only critic can inspect the integrated checkout without owning mutations.

If isolation is needed, seed each isolated checkout from the **complete approved working state**, including required untracked assets and this plan. Managed `worktree:true` requires a clean baseline; do not reset, clean, stash away user work or make an unapproved commit to satisfy it. No parallel implementation is required by this plan.

### Lane board

All normal lanes target `/home/cory/Work/chiton`, at a baseline/ref recorded immediately before launch. Alternative isolation paths must be recorded before mutation.

| Stage / owner | Exclusive edit scope while active | Concrete outcome and handoff gate |
| --- | --- | --- |
| **0 — Parent: preserve and freeze** | Evidence directory only | Hash/patch/status baseline, baseline tests/captures, frozen UI/material/counter contract, exact lane packets. No renderer implementation. |
| **1 — Renderer worker: Implement GPU point material** | New `console-cloud-material.js`; `console-cloud-view.js`; narrowly necessary pure helpers in `console-cloud.js`; new focused material tests | Implement packed attributes, GPU color parity, shape/size uniforms, size caps, stable camera behavior and disposal. Return scoped patch, contract, tests, buffer accounting and unsupported cases. No HTML/controller edits. |
| **2 — UI worker: Integrate local point controls** | `console.html`, `console.css`, `console-app.js`, `console-view.js`; focused UI tests | Implement controls/state, all renderer creation/retry/capture paths, uniform-only range integration and accurate telemetry. Consume stage 1's API. No material/shader changes or transport edits. |
| **3 — Parent: integrate components** | Integration-only glue, test registration, asset references if needed | Review both handoffs and resolve only boundary mismatches. No independent feature redesign. Return a single integrated diff; route substantive defects back to their sole component owner. |
| **4 — Qualification worker: Prove rendering and budgets** | Relevant tests, `tools/capture_console.js` or a focused new capture/benchmark tool, evidence/checkpoint documentation; narrowly needed CMake test registration | Adapt tests that assumed CPU arrays, add GPU framebuffer/style/performance assertions, run regression/native-stub gates, capture actual PNGs and document results. Do not repair runtime code without returning a finding to the parent. |
| **5 — Critic: Review exact implementation** | Read-only source/artifact inspection | Review requirement, current scoped diff including new files, contracts, actual logs, screenshots and measured claims. No edits or nested agents. Parent classifies findings, applies evidence-backed corrections through the appropriate owner and reruns affected gates. |

Each child packet must include: original request and this plan, actual cwd/ref/baseline, allowed files, fixed contract, safety boundaries, acceptance criteria, test commands, expected durable output and stop/escalation rules. Bind output artifacts through the subagent tool, not only filenames in task prose.

Setup/provider/tooling failures are infrastructure blockers. Preserve partial diffs and exact run/status, then report or make a clear same-protocol retry; do not silently switch to an external CLI or foreground runner. When only async work remains, yield for native completion rather than polling or waiting in a loop.

## 6. Implementation order and gates

1. **Baseline:** run current cloud/source/layout/browser tests and freeze representative captures with the same seed/frame/pose/viewport/range. Preserve square/2.5 px behavior as the visual/performance comparison.
2. **GPU color migration:** prove color parity and uniform-only recoloring before changing default shape. Remove RGB storage/loops. Prove packed signed attributes; record any fallback decision.
3. **Appearance:** add square/circle and diameter controls to the material, then bounded perspective sizing. Validate opaque occlusion and actual CSS-pixel size.
4. **UI integration:** apply state on first construction, recreation/retry and capture; update telemetry and exports without touching acquisition.
5. **Qualification:** execute framebuffer, lifecycle, layout, native-stub and performance tests; inspect actual images.
6. **Independent critic:** review integrated evidence, classify findings and rerun affected checks after corrections.

## 7. Required validation

### Pure/material tests

Add focused Node tests for validation, defaults, invalid enums/nonfinite values, diameter limits and reset behavior; reference size scaling and hardware clamps; valid/invalid/neutral color-range uniforms; packed stride/offset/signed-value semantics; empty/maximum/populated-prefix bounds; and stable style state across recreated views.

Keep binary endian/protocol tests. The GPU must receive exact decoded XYZD, including the first record, `-32768`, `32767`, zero and negative D. No normalization, axis mirror, depth reinterpretation or hidden subset is allowed.

### Actual GPU framebuffer tests

Use a loopback-only fixture and disposable Chromium profile, not DOM-only mocks:

- Render isolated, known-depth points to a small controlled target. Compare **actual pixel RGB** against `cloudColor` across below/min/intermediate/max/above-range values, signed extrema and neutral mode. Proposed tolerance: at most **2/255 per RGB channel** at sprite centers; document any justified platform adjustment.
- Test black endpoint colors against a contrasting clear color so black points cannot pass as missing draws.
- Test circle center/corner coverage versus square coverage, small/large diameters, and overlapping near/far differently colored points in both draw orders. Far points must not bleed through opaque near-point interiors.
- Test effective DPR 1 and 2, capped DPR, requested sizes near limits, zoom invariance in screen mode and bounded inverse-depth behavior in perspective mode. Do not assert exact edge pixels where rasterization is implementation-dependent.
- For frozen frame IDs, change range/shape/diameter/sizing: pixels must change as appropriate, but geometry-upload count, record count, snapshot values, camera pose and distinct-cloud rate must not.
- Spy on buffer upload calls or use equivalent test-only instrumentation to prove range/style changes do not re-upload attributes. Do not rely solely on self-reported counters.
- Maximum → small → empty → nonempty clouds must not show stale points or retain multiple buffers/materials/program variants.
- Shader compilation/link failures must reach the existing unavailable/retry flow; no CPU-color or demo-data substitution that hides failure.

### UI, lifecycle and exports

- Accessible labels, slider keyboard operation, fixed-width size readout, no unsupported-option assumptions, no added device-setting writes.
- Retain appearance state across 2D ↔ 3D, hide/resume, pause/resume, source-context changes and explicit WebGL retry; no fetch/paint work while hidden or paused. Changing controls while paused may update local state but must not restart painting/acquisition.
- Repeated transitions and style changes must not grow contexts, geometry/material/listener/RAF counts. Preserve the current on-demand <=30-Hz design.
- Verify no camera refit from appearance/range changes and no influence on measurements/Kalman/history.
- Actual downloaded PNG reflects current shape, diameter, sizing and color range with the correct cloud provenance. Export metadata should record the style; deterministic capture manifests must include it and the effective DPR/size limits.
- OBJ/raw bytes, units and independent-frame semantics remain unchanged; no claim that style or PNG changes affect device output.
- Cover 1920×1080, 1280×720, 390×844 and 320 px width; normal/Large/Presentation, fullscreen and 200%-equivalent reflow. Retain existing broader layout matrices.

### Performance evidence

Use one deterministic recorded/synthetic cloud, fixed pose/range and identical drawing-buffer size. Compare the preserved baseline and candidate at approximately 40,000 and 307,200 records, after warm-up, over multiple repetitions. Record median and p95 preparation time, upload bytes/calls, CPU attribute capacity, render/interaction timing and test environment.

Separate three comparisons:

1. Old square/2.5 px versus new square/2.5 px: isolate the data/color migration.
2. New square versus new circle at the same diameter: measure appearance cost.
3. Diameter/sizing extremes: demonstrate bounded cost and disclose overdraw tradeoffs.

Mandatory structural results: no RGB attribute or CPU color loop; uniform-only recoloring/styles; one reusable geometry/material; 8 B/record attributes on the preferred path, or an explicit approved 16 B fallback. HTTP bytes must remain unchanged.

Do not promise a universal FPS gain. A repeatable regression above roughly 10% beyond measured noise in the equal-style comparison is a parent investigation gate, not an assertion that software-WebGL timing predicts hardware. Do not add forced GPU synchronization to normal rendering just to time it. Report shader/discard fill cost honestly, especially at large diameters.

## 8. Commands and artifact layout

Existing commands to retain:

```sh
node tests/console_cloud.js
node tests/console_cloud_scheduler.js
node tests/console_model.js
node tests/console_scheduler.js
node tests/console_kalman.js
node tests/console_source_contracts.js
node tests/console_cloud_browser.js
node tests/console_browser.js
node tests/console_layout.js
node tests/console_viewport.js
python3 tests/console_assets.py
node tests/web_ui.js
node tests/web_image_stream.js
node tests/web_performance.js
```

Add explicit commands for the new pure/material and GPU framebuffer suites to the implementation report. Register dependency-free Node tests in the existing opt-in simulator release block. Keep Chromium tests explicit unless its dependency/failure policy is deliberately integrated. Use `--no-tests=error` for CTest; empty registrations are not passes.

For verified native integration, follow `tests/SAFETY.md` and `tests/console_native.py`: build an isolated **SDK-free stub**, verify stub identity before runtime setup, use temporary resources/config/calibration and command tripwires. Do not run the deployed hardware binary locally or contact the device. Reuse the documented host-library overrides only after rediscovering them.

Use ignored artifacts under `_build/pointcloud-renderer-style/`:

```text
baseline/       original scoped files/patches, hashes, logs and fixed captures
handoffs/       component contracts, scoped patches and subagent output references
validation/     exact commands, test results and benchmark environment/results
screenshots/    matched square/circle/size views, PNG exports and manifests
review/         exact integrated diff, new-file inventory, critic and disposition
```

Extend capture automation narrowly to select/freeze appearance state. Do not allow its demo-only capture path to become an accidental device collector. Confirm any new flat asset is served, copied and installed byte-for-byte through existing asset/native/package checks.

## 9. Completion, rollback and remaining boundaries

Complete only when:

- Both requested features work through the real console and verified native stub, with truthful local-only controls.
- GPU color parity, packed signed geometry, actual point coverage/size/occlusion and frozen-frame uniform-only updates are proven.
- Existing transport/measurement/navigation/layout/export/lifecycle regressions pass.
- Performance/memory claims are supported by actual structural and measured evidence; any fallback or skipped gate is explicit.
- Parent has inspected actual screenshots; proposed visual design remains subject to operator approval.
- One fresh-context read-only critic has reviewed the exact integrated implementation and evidence; accepted findings are corrected and revalidated.
- Subagent lanes are terminal or explicitly blocked with a named next action. No worker/process is left owning an uncertain tree.

Rollback is a scoped restoration of the recorded pre-feature console assets/test integration, not a reset of unrelated work. Device configuration, calibration, geometry, export contracts and the legacy application require no migration. Retain Square/2.5 px as a useful comparison, but do not keep an untested second CPU-color renderer as a hidden fallback.

Final handoff: changed files, subagent roles/handoffs, exact test evidence, screenshots/benchmark paths, critic disposition, packed/fallback representation, defaults/caps, remaining browser/hardware/visual limits and confirmation that no device was contacted or release deployed.
