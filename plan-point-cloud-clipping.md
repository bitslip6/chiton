# Implementation plan: viewer-only point-cloud clipping and depth slices

**Status:** plan only. This request authorizes writing this handoff, not implementation, device access or deployment. No implementation agents were launched.

**Repository:** `/home/cory/Work/chiton`.

**Planning baseline:** HEAD `461b2f0b0d7f4388cca1150c13e34c1baea1ad36`, **plus the existing uncommitted/untracked point-style, GPU-color, framing and EDL implementation**. HEAD alone is not a usable implementation baseline. Recheck status and preserve the complete working state before starting. Do not reset, clean, stash away user work or make an unapproved commit.

## 1. Requirement and boundaries

Implement viewer-only tools for inspecting selected surfaces:

1. An inclusive interval on the returned signed **depth component D**: a depth slice.
2. An inclusive, axis-aligned clipping box in returned **API XYZ coordinates**.
3. Clear active-clipping indicators, accurate record counts, and clipped viewport PNGs.

Both tools are optional, **off by default**, and may be enabled together. Together they select the **intersection** of the depth interval and XYZ box. Disabling both immediately restores the unfiltered display without fetching another cloud.

Clipping is a local display operation, **not sensor validity classification, acquisition filtering, data deletion or a device setting**. Preserve the decoded snapshot and renderer-owned XYZD records, including signed extrema, origins, zero and negative values. All returned records remain stored and submitted; only their rasterization is suppressed.

Operator context: below approximately 600 mm the sensor suffers flooding/blooming; beyond approximately 5.6 m there is no useful returned IR signal. **Do not introduce or change validity thresholds, make those numbers clipping defaults, change the palette, reveal data the producer omitted, or call viewer-selected points “valid.”** Those sensor limits and the acknowledged device color range are separate from this feature.

### Non-goals

- New endpoints, cloud formats, compression, streaming or sampling changes.
- Geometry projection, calibration/AOI, sensor processing, measurements/history/Kalman changes.
- A new palette, EDL redesign, transparency, normals, meshing or temporal accumulation.
- Oriented boxes, multiple boxes, arbitrary planes, inverted/exclusion volumes, picking, draggable 3D gizmos or a box-overlay renderer in the first version.
- Screen/view-depth clipping or a camera-relative slab. D is not camera distance or necessarily XYZ Z.
- Changing existing Fit all / Fit main cloud semantics, or adding Fit matching in this delivery.
- Persistent browser/device settings, clipped OBJ/raw exports, physical camera/real SDK access, deployment, default-page promotion, commits or pushes.

## 2. Read before implementation

Current runtime:

- `release/files/console-cloud.js`: signed XYZD decoder/bounds, robust main bounds, navigation and the separate fitted `panAnchor`.
- `release/files/console-cloud-material.js`: shared point vertex/fragment material and optional EDL fullscreen shader.
- `release/files/console-cloud-view.js`: packed buffer, setters, camera/framing, EDL target, counters, PNG and disposal.
- `release/files/console-app.js`, `console-view.js`, `console.html`, `console.css`.
- `tools/capture_console.js`, `tools/benchmark_console_points.js`, `tools/benchmark_console_edl.js`.

Contracts/checkpoints:

- [Framing and EDL](docs/console-framing-edl.md).
- [Point-style/GPU-color checkpoint](docs/pointcloud-point-style-gpu-color-checkpoint.md).
- [Measurements](docs/measurements.md), [HTTP](docs/http-contracts.md), [test safety](tests/SAFETY.md).
- Applicable [3D requirements](docs/console-3d-implementation-plan.md).

Tests: `tests/console_cloud*.js`, `console_point_appearance_browser.js`, `console_capture_provenance_browser.js`, `console_native.py`, `console_native_browser.js`, `console_layout.js`, `console_viewport.js`, `console_assets.py`, and `console_fixture.js`.

The older point-style plan describes the **pre-migration** renderer. Its Float32 XYZ/RGB / PointsMaterial table is historical, not the current architecture.

### Current invariants

- Three.js r128; one reusable signed `Int16Array(CLOUD_MAX * 4)` / interleaved XYZD buffer, stride four, non-normalized attributes, 8 B/record.
- One shared point material owns GPU depth color, circle/square and bounded CSS sizing. No RGB attribute or production CPU color loop.
- Ordinary frames upload only their populated prefix; duplicate IDs do not update geometry. Uniform controls must not create geometry/material variants.
- Rendering is on demand, capped at 30 Hz. Cloud refresh is separately configurable to 10 Hz; default is 2 Hz. Do not change those limits/defaults.
- Controller-owned style/projection/lighting survives renderer construction/retry. `createCloudView()` centralizes restoration; `prepareCapture()` is demo-only.
- EDL uses the normal point scene's color/depth target, then a fullscreen shading pass. Plain mode works without EDL extensions.
- `#cloud-navigation` and `#point-appearance` move into `#cloud-fullscreen-controls` inside the existing `#viewport` fullscreen root, then restore. Extend this mechanism, not a second control tree.
- All-record bounds still drive axes and safe clipping planes. Main fit has a separate pan anchor; do not reintroduce the reviewed pan-jump defect.

## 3. Fixed coordinate and selection semantics

Use a complete state object:

```js
clipSettings = {
  depth: { enabled: false, min: -32768, max: 32767 },
  box: {
    enabled: false,
    min: [-32768, -32768, -32768],
    max: [32767, 32767, 32767]
  }
};
```

- Booleans must be booleans. All limits must be finite **integers within the signed-int16 domain**. Require the complete shape, correct vector lengths and `min <= max` on every enabled or disabled interval. Validate and deep-copy; reject malformed state without partial application.
- Bounds are inclusive: equality passes. Equal bounds select an exact integer plane/depth. A finite-thickness slice uses a wider interval. Do not swap reversed bounds silently.
- Depth compares **decoded record D**, not `abs(D)`, Euclidean radius, XYZ Z, color normalization, camera/view Z or physical millimeters inferred from uncalibrated units.
- Box compares decoded API X/Y/Z before model-view/projection transforms. Rotating, panning, zooming, perspective/orthographic changes and API view presets do not move the clipping volume relative to the cloud.
- Use snapshot `units` / `unitsScope`: label mm only for acknowledged mm, and explain calibration-AOI scope. Otherwise show source units, not mm. No OBJ `/6000` normalization.
- Keep D/XYZ clipping bounds completely separate from color-range uniforms and device “Apply range” controls. Changing display clipping never writes device settings.

Reference predicate:

```text
matches = (!depth.enabled || depth.min <= D <= depth.max)
       && (!box.enabled || (box.min[X] <= X <= box.max[X]
                         && box.min[Y] <= Y <= box.max[Y]
                         && box.min[Z] <= Z <= box.max[Z]))
```

Provide one pure CPU reference predicate/summary helper for validation and exact match counts. It must agree with GPU comparisons, including all signed boundary cases.

## 4. UI and state behavior

Add one compact expandable **“Clipping / slices — this viewer only”** group in the local 3D controls. Keep primary navigation and OBJ actions usable at 320 px. Include:

- Independent **Depth slice (returned D)** and **XYZ box** enable checkboxes.
- Labeled depth min/max and X/Y/Z min/max integer numeric fields, step 1, with units/scope explanation.
- **Apply local clipping**: atomically commits the complete valid draft. Enter submits locally. Numeric editing alone does not mutate the applied state; invalid drafts show an accessible inline error and leave the previous applied settings/pixels intact.
- **Use current cloud bounds**: explicitly seeds draft D/XYZ min/max from the current retained snapshot, without enabling tools or applying changes. Compute D extrema from D, not Z. Do not continuously rescale fields from live frames. Disable this action for an empty/unavailable cloud.
- **Reset clipping**: immediately restores the default disabled/full-domain state, discards invalid drafts, and restores all points without changing camera, point style, EDL, color range, source or acquisition.

Checkboxes are part of the draft until Apply. Distinguish **unapplied changes** from the active applied state. Native number fields/spinners and keyboard submission are sufficient for this version; range sliders, scrubbing and 3D manipulation are not required.

### Always-visible truthfulness

Outside the collapsed group, show **“Display clipped”** whenever either applied tool is enabled, even if every record happens to match. Show applied intervals and **M / N records match clipping**; zero matches must say **“No records match clipping”**, not “cloud unavailable” or “no sensor returns.” Keep Reset clipping readily accessible from that status.

Rename the existing “Returned / rendered” telemetry label to **“Returned / submitted”**, keeping both underlying totals compatible, and report matching count separately. Do not silently replace the existing submitted total with M.

Do not call M “visible pixels,” “valid points” or an exact viewport-visible count: matching points may still be behind the camera, occluded, outside the frustum or black under the existing palette. While a draft is invalid/unapplied, status and PNGs must describe the last **applied** state.

### Lifecycle / coordinate-context policy

Clipping settings are unit-bound, unlike shape/diameter:

- Preserve applied settings and drafts through pause/hide/resume, same-context WebGL retry/recreation, fullscreen and mode roundtrips **when the coordinate-context key remains identical**.
- Define the key from acknowledged/accepted cloud source, frame type, configuration revision, units and units scope. Do not include palette bounds, camera, style, EDL, frame ID or record count.
- If that key changes, conservatively reset applied clipping **and drafts** to off/defaults and show a nonfatal “Clipping reset: coordinate context changed” notice. Do not reinterpret old numeric limits in new units. A revision change may conservatively reset even if coordinates happen to be unchanged; document this behavior.
- A confirmed source/process restart also resets clipping. Ordinary newer IDs, duplicate frames, transport errors/stale data, empty clouds and changing observed extents do not.
- A color-range-only change with the same context key leaves clipping unchanged. If it also changes configuration revision, the explicit context-reset policy applies.
- No rendering/acquisition restart while paused/hidden. Applying valid local settings may update cached uniforms/counts, but must not schedule renderer paints while inactive.
- Disable irrelevant actions when 3D is unavailable; retain state only according to the context rules above. Reload returns to defaults. Do not add storage writes.

Extend the existing fullscreen reparent/restore logic so this **single** group and its status remain available inside `#viewport`. No duplicate IDs, cloned inputs or new fullscreen root. Slider/number/select keys must not trigger viewport navigation.

## 5. Renderer and controller integration

Freeze this API before implementation:

```text
validateClipSettings(input) -> deep copied complete state, or throws
summarizeCloudClipping(records, settings) -> exact counts (and explicit bounds seeding helper)
CloudView.setClipSettings(settings)
CloudView.update(snapshot, range)          // incoming contract unchanged
CloudView.counters(now)                    // extended, cheap cached reads
```

Prefer small pure helpers in the existing `console-cloud.js` namespace/CommonJS pattern; use the existing material seam. No new dependency or flat asset is necessary.

`setClipSettings` validates/copies, updates uniforms, recomputes cached matching counts for the retained populated records, increments explicitly documented counters for real changes, and calls the existing dirty mechanism. No-op equivalent settings are no-ops. It must not mutate snapshots/records, alter draw range, request data, recreate buffers/materials/program variants, reset navigation/pan anchor, refit, change size reference distance or upload attributes.

Controller owns the durable applied settings/draft/context policy and applies settings on **every** constructor path through `createCloudView()`, including retry and deterministic capture. Update context/reset ordering before rendering a cloud in a new context. Do not let independent renderer/controller state disagree about what is applied.

### GPU selection

Add distinct uniforms to the existing **point** vertex shader: depth enabled/min/max; box enabled/XYZ min/max. Use high-precision comparisons against the signed, non-normalized attributes. Do not enable Three.js global clipping planes or introduce a second point material/compacted geometry.

Preferred path: calculate the predicate once per point; for a nonmatch, set a finite clip-space position definitely outside the clip volume and finish that vertex (initialize point size safely). Prove on real WebGL1/2 that rejected points produce **no fragments or depth writes**, including large sprites and all camera directions. If a fragment-discard marker is needed instead for correctness, keep large D/XYZ comparisons in the vertex shader, pass only a small selection marker, and disclose its fill-cost tradeoff. Do not use alpha=0, blending, black color, zero point size, NaN positions or moving rejected points to the origin as a hiding mechanism.

The existing square/circle footprint, opaque depth behavior, palette and size semantics remain unchanged for matches. Shader failure must reach the existing unavailable/retry path, not silently show unclipped data under an active-clipping badge.

### Preserve upload semantics, including zero matches

- Keep `count`, geometry draw range and submitted records based on **returned** count.
- Do **not** set `Points.visible=false` merely because matching count is zero. That could defer a new frame's attribute upload until a later clipping toggle, breaking the uniform-only interaction invariant. Keep normal nonempty-frame submission even when every vertex is rejected. Existing truly empty-cloud handling remains.
- Do not create a per-point selection attribute, matching-record array, RGB array or GPU readback loop.
- Compute count summaries only on distinct snapshot update or applied-setting change, not on orbit, render, telemetry polling or `counters()`. Counts may require an additional O(n) CPU pass; fuse with the existing bounds pass only if the implementation remains simple and tested. Disabled clipping can use `matchingCount = count` without a predicate scan.
- At maximum 307,200 records and 10 Hz, measure that CPU cost. Clipping can reduce fragment work but does **not** reduce HTTP bytes, decoded records, submitted vertices or normal geometry-upload bytes.

### Cached counters and provenance

Keep existing `points` / `uploads` consumers compatible: `points` remains submitted records and `uploads` remains geometry requests. Add explicit copied/cached fields such as:

```text
clipSettings, clippingActive,
matchingCount, clippedCount, submittedCount,
clipStatsFrameId, clipStatsContext,
clipUpdates                         // changed applied clipping sets
```

`matchingCount + clippedCount == submittedCount`, including empty clouds. Associate counts with the actual retained/rendered snapshot, not a separately polled source frame. Renderer counters and telemetry must not scan records on demand. Explain that these are logical predicate counts, not raster/GPU-memory/validity measurements.

## 6. EDL, framing and exports

### EDL

Rejected points must be absent from **both color and depth** in the existing EDL scene target. A rejected foreground point must not occlude or shade a retained surface behind it. Do not apply a post-EDL black mask: that leaves ghost depth/halos. Test EDL enabled/disabled and its unsupported/fallback path. Preserve bounded/reused targets, clearing on every paint, resize and disposal.

Axes remain orientation helpers, not clipped records, and remain outside match counts. When zero records match, axes/overlays may still be visible; say so rather than asserting that the canvas is entirely blank.

### Framing

- Clipping edits never change pose, reference distance, pan anchor or near/far policy.
- Fit all still uses all returned XYZ; Fit main cloud still uses its existing robust all-record distribution. Label help accordingly; do not quietly make either action fit only matches.
- Reset view retains clipping just as it retains projection/lighting; only Reset clipping clears it.
- Keep safe all-record bounds for clipping planes/axes, especially with EDL. Do not reuse matching bounds as authoritative geometry bounds.

### PNG and device exports

Viewport PNGs show the applied clipped view, current EDL/projection/style and matching/submitted counts. Always record **clipping off**, or the enabled D/XYZ intervals and logical counts, in PNG provenance **even when overlays are unchecked**. Use a bounded multiline footer with readable layout; do not silently truncate away active-axis intervals or replace the existing frame/source/units/range/style information. An all-clipped but nonempty cloud may still export a PNG with explicit zero-match provenance; truly empty/unavailable behavior remains as currently defined.

Device OBJ/raw/binary export bodies, units, independent-frame semantics and filenames remain untouched. Keep “device export is not clipped” help near download controls when local clipping is active. No new clipped-geometry download in this scope.

Extend demo-only `prepareCapture()` with an optional complete `clipSettings`; validate before mutating/stopping the viewer. Capture manifests retain structured rendered cloud ID/count/range, applied clip state/counts/context, final renderer pose, style/projection/EDL and effective DPR. **Do not overwrite `evidence.cloud` with renderer counters**—that was a previously fixed provenance bug.

## 7. Implementation stages and ownership

A new implementing agent should proceed through these gates; this document does not require implementation delegation. Keep one writer per checkout. If delegation is separately authorized, assign renderer/material+pure selection to one owner and UI/controller to another, sequentially, with a fixed API and durable handoffs; do not create competing shader writers. The parent/integrator retains acceptance. Follow applicable project instructions, including the fresh-context read-only critic checkpoint after implementation and focused validation.

1. **Preserve/baseline:** record status, full patch plus relevant untracked files/hashes and matched captures under ignored `_build/pointcloud-clipping/baseline/`. Run focused existing GPU/framing/EDL/UI/asset tests. Preserve pre-feature behavior and original user changes.
2. **Pure contracts:** implement/validate full state, inclusive predicate, count summary and explicit D/XYZ bounds seeding; add boundary and malformed-state tests.
3. **Renderer:** uniform-only GPU selection, cached exact counts, no-data/zero-match transitions, EDL correctness and shader failure flow. Prove actual WebGL before UI integration.
4. **Controller/UI:** atomic drafts/apply/reset, context lifecycle, visible status, units, fullscreen, no-write/no-paint safeguards and capture restoration.
5. **Exports/qualification:** PNG provenance, capture manifests, native-stub byte independence, layout/resources and measured CPU/GPU costs. Update narrowly affected test expectations without weakening old assertions.
6. **Review:** supply exact scoped diff including new files, original requirement, contracts, actual test logs/images/benchmarks to one fresh read-only critic. Classify findings, fix evidence-backed in-scope defects, rerun affected checks and review only the fix blast radius when warranted.

Stop for disposition on ambiguous coordinate/units semantics, packed-attribute correctness failure, new runtime dependency, required backend change, repeatable material performance regression or unavailable infrastructure. Do not silently switch representation or expand scope.

## 8. Required validation

### Pure tests

Add `tests/console_cloud_clipping.js` covering:

- Deep-copy/defaults; complete object/vector/boolean validation; nonfinite/fractional/out-of-domain/reversed limits; disabled fields are still validated; no mutation on rejection/no-op commits.
- Inclusive min/max, equal limits, intersection, independent enable flags, signed extrema, negative/zero D, origins and negative XYZ; D deliberately differs from Z in fixtures.
- Empty/single/maximum clouds; all/none/some matches; count conservation; explicit observed D/XYZ extrema seeding with degenerate extents.
- Cached-count invalidation on frame/settings changes, not camera/style/range/EDL; context policy including confirmed restart.

Register only dependency-free Node tests in the existing opt-in SDK-free release-test CMake block. Chromium stays explicit unless its dependency policy is deliberately changed.

### Actual WebGL framebuffer tests

Use loopback fixture/disposable Chromium and forced WebGL1 plus WebGL2. Extend the focused GPU suites or add `tests/console_cloud_clipping_browser.js`:

- Known signed XYZD points at/inside/outside every interval, separate D-vs-Z cases, combined intersection and exact-plane matches. Match observed pixel coverage to the CPU predicate.
- Keep retained endpoint-black points detectable against a contrasting clear color. Compare unchanged retained colors to the reference with the existing <=2/255 tolerance when EDL is off.
- Circle/square, diameter1/10, bounded perspective sizes, DPR1/2/capped DPR; rejected large sprites cannot leak edge pixels. Both camera projections and front/top/side views; orbit does not alter predicate/counts.
- Overlapping differently colored near/far records in both draw orders. Rejecting near points reveals the retained far points; rejected records write no depth. Repeat with EDL: no stale foreground occlusion or ghost silhouettes from rejected geometry.
- Zero matches -> subset -> all, max -> small -> empty -> nonempty, settings changes on frozen IDs and new frames arriving while zero matches. Inspect actual GL buffer upload calls, attribute versions and program/resource counts—not only self-reported counters. Settings-only changes upload no XYZD; new nonempty frames still do.
- Clipping off restores baseline pixels with identical frame/pose/range/style/projection/lighting. No camera/pan-anchor/reference/count/source-array changes from applying masks.

### UI, lifecycle and safety

- Accessible drafts/validation/Enter/apply/reset, visible active/applied status even when collapsed or overlays off, zero-match wording and separate matching/submitted semantics.
- Same-context pause/hide/mode/retry/fullscreen preservation; explicit reset/notice on units/scope/revision/source-context/restart changes. Test both retained stale data and an empty incoming cloud.
- Changing controls while paused/hidden causes no actual paints/renderer RAF/network work; active edits cause one coalesced repaint, not a new loop. No device-setting routes or browser storage writes.
- Measurement selection/history/Kalman/reference values remain unchanged. Same frozen source raw/OBJ hashes remain unchanged through masks and PNG exports.
- Actual downloaded PNG bytes decode, match the clipped view, and contain full applied provenance with overlays on/off, including zero-match export.
- Repeated apply/reset/recreation/EDL toggles do not grow point buffers, materials, programs, targets, listeners, contexts or RAF chains.
- Cover 1920×1080,1280×720,390×844,320px, normal/Large/Presentation, 200%-equivalent reflow and fullscreen with the shared scrolling controls host.

### Regression/native commands

Retain all existing gates, particularly:

```sh
node tests/console_cloud_clipping.js                    # new pure suite
node tests/console_cloud_clipping_browser.js            # new actual GPU/UI suite, if split
node tests/console_cloud.js
node tests/console_cloud_material.js
node tests/console_cloud_material_gpu.js
node tests/console_cloud_framing.js
node tests/console_cloud_framing_edl_browser.js
node tests/console_point_appearance_browser.js
node tests/console_cloud_lifecycle_browser.js
node tests/console_cloud_browser.js
node tests/console_cloud_scheduler.js
node tests/console_model.js
node tests/console_scheduler.js
node tests/console_kalman.js
node tests/console_source_contracts.js
node tests/console_browser.js
node tests/console_layout.js
node tests/console_viewport.js
node tests/console_capture_provenance_browser.js
python3 tests/console_assets.py
node tests/web_ui.js
node tests/web_image_stream.js
node tests/web_performance.js
```

For native integration, read `tests/SAFETY.md` and `tests/console_native.py` first. Build an isolated **SDK-free/headless stub**, verify CMake/binary/startup identity before API setup, use temporary resources/config/calibration and command tripwires. Rediscover host library overrides rather than copying old build assumptions. Run the native browser/export helper with clipping active, focused CTest using `--no-tests=error`, and asset/package/install byte comparisons as applicable. Never run the deployed binary or contact the camera. Record exact commands and named registrations; no empty CTest passes or standalone uninvoked helper claims.

### Performance / captures

At ~40,000 and 307,200 records, compare preserved baseline vs candidate clipping off, then depth-only, box-only and intersected masks retaining roughly 0/10/50/100%. Fix input, pose, range, style, DPR, drawing-buffer size and EDL state; warm up and repeat with reversed case ordering. Include EDL off/on separately.

Record median/p95 validation/count-preparation time, mask-apply latency, new-frame preparation, completed render/interaction time, observed upload calls/bytes and CPU attribute capacity. If using synchronous readback for completed timing, keep it test-only and separate CPU submission from completion. No forced synchronization in production.

Report CPU count-scan cost honestly at a synthetic 10-Hz feed and the existing software packed-SHORT/EDL costs; do not claim faster Wi-Fi, reduced network bytes or automatic FPS gains. Investigate repeatable clipping-off regressions above roughly 10% beyond noise; do not change refresh limits, quality/DPR, representation or sample density to hide them.

Capture matched unfiltered/depth/box/intersection/zero-match views, EDL and projection examples, mobile/fullscreen, invalid draft vs applied state and actual PNG downloads. Manifests must identify rendered frame, applied intervals/context and matching/submitted counts. Inspect the actual images.

## 9. Completion and handoff

Complete only after the tools work in the real console and verified native stub; GPU/CPU predicate parity, no-depth-write rejection, EDL integration, zero-match recovery and uniform-only edits are proven; local-only lifecycle and export contracts pass; and the critic's accepted findings are fixed/revalidated.

Keep ignored artifacts under `_build/pointcloud-clipping/{baseline,handoffs,validation,screenshots,review}/`. Add a concise durable checkpoint documenting changed files, exact tests, images/benchmarks, count semantics, context-reset policy, critic disposition and browser/hardware limits. Preserve user changes and leave no uncertain writer/process ownership. No release or commit is implied.

Final operator handoff should state: controls/defaults, D vs XYZ semantics, matching vs submitted counts, camera/framing behavior, PNG vs unmodified device exports, measured costs/limits, evidence paths, and confirmation of no device access/deployment.
