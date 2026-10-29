# Viewer-only cloud clipping and depth slices

Implemented and locally validated against `plan-point-cloud-clipping.md`. Independent read-only critic found no actionable defects; a parent-discovered deterministic-capture rewind fix was separately re-reviewed without findings. This is not deployment, operator visual approval, or physical-GPU/device qualification.

## Operator controls

In depth-cloud mode, open **Display controls**, then expand **Clipping / slices — this viewer only**:

- Depth slice compares the returned **signed D**, not XYZ Z, camera distance, absolute depth or inferred millimeters.
- XYZ box compares returned API coordinates before camera transforms. Both enabled tools select their inclusive intersection. Equal bounds select an integer plane.
- Tools start **off**, with full signed-int16 limits. Edit checkboxes/number fields as a draft, then **Apply local clipping** or Enter. Invalid/unapplied drafts never change applied pixels, counts or PNG provenance.
- **Use current cloud bounds** explicitly seeds the draft from retained XYZ and D extrema; it neither enables tools nor applies anything. Disabled for empty/unavailable clouds.
- **Reset clipping** immediately disables both tools and discards invalid drafts. It does not change acquisition, camera, color range, point appearance or EDL.

Applied clipping has an indicator, applied intervals, separate matching/submitted counts and Reset outside the expandable group. Zero matches say so; axes may remain. Unit labels use acknowledged cloud units/scope: mm only in calibration AOI. These are logical predicate counts, not validity classifications, pixels, frustum-visible records or GPU-memory measurements. Returned/submitted telemetry retains its original totals; all returned signed records remain stored and submitted.

Clipping never refits. Fit all and Fit main cloud still use all-record geometry/distribution. Reset view retains clipping. Viewport PNGs contain the applied clipped view and a wrapped appended provenance footer, including clipping off or all enabled intervals/counts even when overlays are off. Appending the footer preserves the entire viewport; PNG height is therefore larger than drawing-buffer height. Device OBJ/raw/binary bytes, units, filenames and independent-frame semantics are unchanged; active clipping shows that reminder.

## State and rendering contracts

`console-cloud.js` exports `defaultClipSettings()`, `validateClipSettings(input)`, `summarizeCloudClipping(records, settings)`, explicit `cloudClipBounds(records)` seeding, `cloudClipContext(snapshot)` and `clipDescription(settings)`. Settings require a complete shape, actual booleans, three-element vectors and ordered finite signed-int16 integer intervals, including disabled intervals. Validation returns deep copies and never partially applies malformed settings.

Controller-owned applied settings and drafts survive same-context pause/hide/resume, mode roundtrips, fullscreen and WebGL retry. The coordinate key is source, frame type, configuration revision, units and units scope. Changes reset both applied state and drafts with a notice, even if a revision did not actually change coordinates. Confirmed restart resets too; accepted lower status IDs mark the tracker reconnecting so the lower cloud sequence can recover. Newer/duplicate IDs, empty clouds, transport errors, extents, style/camera/EDL and same-revision color changes do not reset clipping. No storage writes. Demo capture's intentional timeline rewind is not a process restart: it clears transport tracking but preserves same-coordinate applied settings/drafts, including invalid drafts. Optional complete capture settings are validated before stopping/mutating anything.

One existing point material adds distinct high-precision D/XYZ uniforms. Each rejected vertex receives finite clip-space position `(0,0,2,1)` beyond the far plane and safely initialized size, producing no fragments/depth writes in tested WebGL1/2. No fragment-mask fill penalty, alpha trick, geometry compaction, selection attribute, material/program variant, RGB array, global clipping planes or GPU readback in production. EDL uses the same selected scene for color and depth.

`CloudView.setClipSettings()` validates/copies, ignores equivalent settings, updates uniforms and cached logical counts, and uses the existing inactive-aware dirty mechanism. `clipUpdates` counts changed complete applied sets, including disabled-limit edits; `uniformUpdates` also increments for these sets. Counts recompute only on accepted snapshot updates/applied changes, with an O(1) all-match path when both tools are disabled. Counters deep-copy settings and expose matching/clipped/submitted totals plus retained frame/context provenance. Zero matches never hide nonempty Points or suppress a new frame's upload. Buffer stride/capacity, all-record bounds, pan anchor, camera reference distance, refresh default2Hz and paint cap30Hz are unchanged. The subsequent [display-first change](console-display-first.md) raises the selectable cloud-read maximum from10Hz to30Hz; the historical evidence below remains the original10Hz qualification.

## Local validation and evidence

Ignored evidence root: `_build/pointcloud-clipping/`. Baseline includes the original tracked patch, untracked archive, hashes, full runtime copy, existing tests and16 captures; pre-existing user work was preserved. Feature-only review patch and critic reports are under `review/`.

Passed commands (individual logs under `validation/`):

```sh
node tests/console_cloud_clipping.js
node tests/console_cloud_clipping_browser.js
node tests/console_cloud_clipping_ui_browser.js
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

New pure suite is registered as **console_cloud_clipping** in the existing opt-in SDK-free CMake block; Chromium remains explicit. GPU suite proves actual forced WebGL1/2 signed boundary/CPU parity, distinct D/Z, inclusive intersection, circle/square1/10, bounded perspective sizes, both projections/front/top/side, DPR1/2/cap, no fragment/depth leaks, near/far overlap in both orders, EDL equivalence to absent foreground, cached reads, real GL upload calls and zero/max/small/empty recovery. Existing material suite retains signed attribute/color/resource/shader-failure coverage. UI suite covers atomic drafts, native keys/Enter, reset/seeding, applied provenance under invalid drafts, actual downloaded/decoded zero-match and subset PNGs,15 expanded layout variants, shared fullscreen, paused/hidden no network/RAF/paints, same-context mode/retry preservation, context/restart resets (including empty snapshots), no device/storage writes, active clipping with unsupported EDL falling back to plain WebGL1, and capture rewind provenance.

A newly isolated SDK-free/headless build used:

```sh
cmake -S . -B _build/pointcloud-clipping/native \
  -DUSE_ADITOF_STUB=ON -DUSE_SDL2=OFF \
  -DCHITON_DEFAULT_DISPLAY=headless -DCHITON_RELEASE_TESTS=ON \
  -DZLIB_LIBRARY_RELEASE=/usr/lib/libz.so -DI2C_LIBRARY=/usr/lib/libi2c.so
cmake --build _build/pointcloud-clipping/native -j4
CHITON_CONSOLE_NATIVE_OUT="$PWD/_build/pointcloud-clipping/validation/native-runtime" \
  python3 tests/console_native.py _build/pointcloud-clipping/native/chiton
ctest --test-dir _build/pointcloud-clipping/native \
  -R 'console_|web_ui|web_performance' --no-tests=error --output-on-failure
cmake --install _build/pointcloud-clipping/native \
  --prefix "$PWD/_build/pointcloud-clipping/install"
```

The two host library paths were rediscovered, not assumed portable. CMake/binary/startup identity checks preceded simulator setup; linked libraries were recorded. Temporary config/calibration/resources and command tripwires only. Actual HTTP export helper ran, plus native-browser clipping/EDL/orthographic PNG and unchanged raw/OBJ/binary hashes. Focused CTest **12/12** passed. Static/native/package/install asset bytes match (15 unique installed entry/reference/legacy names). No physical camera, deployed binary, real SDK, provisioning or host-policy changes.

Matched23 captures and actual PNG exports: `screenshots/matched/manifest.json` (off/depth/box/intersection/zero, both projections, EDL off/on, mobile/fullscreen). Same frame181 and pose for matched cases:307200/140549/269397/112499/0 matches respectively out of307200 submitted. Structured rendered cloud ID/count/range are retained separately from final nested renderer/pose/style/DPR/clip/context fields. Parent inspected actual off/depth/box/intersection/zero, EDL/orthographic, mobile/fullscreen and native PNG images. `ui.json`/screenshots also retain invalid-draft evidence. Review reports: `review/critic-initial.md`, `review/critic.md`; no critic findings required corrections. Parent's capture rewind correction and test evidence are in `review/capture-fix.patch` and the UI/provenance logs.

## Measured costs and limitations

Reproduce with `node tools/benchmark_console_clipping.js`; requires the preserved runtime baseline. Full final matrix: `validation/benchmark-final.json`, including raw samples, warmup, reversed ordering, actual upload calls/bytes, CPU capacity, matching counts and synthetic10Hz scans. Chrome155 / ANGLE SwiftShader / Ryzen5600X,800×600 DPR1, circle2.5, identical synthetic frame/pose/range. Two reversed case/version-order rounds,4 warmups+10 samples each; GPU completion uses test-only finish+readPixels, separately from CPU submission. Off baseline/candidate framebuffer hashes match exactly. Depth/box/intersection masks retain roughly0/10/50/100%; EDL measured separately.

At307200 records, active-mask CPU reference-scan medians were0.7–2.2ms (worst p95≈2.9ms); applied-set medians at most1.7ms (worst p95≈2.3ms). A synthetic10Hz count-only feed measured depth0.7/1.1ms, box1.1/1.3ms, intersection1.0/1.4ms median/p95. Disabled clipping avoids the record scan. These CPU-only feed measurements do not claim a10Hz completed GPU pipeline.

Representative isolated clipping-off completed-draw median/p95 (ms), final off-only confirmation:

| Records | EDL | Preserved baseline | Candidate |
|---|---|---|---|
|40000|off|11.4 /12.3|11.5 /12.4|
|40000|on|18.7 /20.5|17.9 /19.1|
|307200|off|34.9 /38.1|35.5 /39.7|
|307200|on|40.8 /42.3|41.8 /46.6|

An initial concurrent run was noisy. One round of the final full matrix also showed a40k EDL slowdown (combined median19.8 vs17.7ms); its reversed round was17.2 vs17.7ms. The prior full isolated run and two isolated off-only confirmations did not reproduce a>10% median regression. All evidence is retained (`benchmark.json`, `benchmark-isolated.json`, `benchmark-off-repeat.json`, `benchmark-off-confirmation.json`); no quality/cadence/representation change was used to hide it. Software-WebGL timing variability remains a limitation, not a universal speedup claim.

The existing packed-SHORT upload/software-render cost and optional EDL fill/target cost remain. Clipping does not reduce network bytes, decoded/submitted records, normal geometry upload bytes or CPU attribute capacity (2457600B). Large sprites/high DPR can still be expensive. Physical GPU/other-browser/thermal/network/optical qualification, full sanitizer release gate and operator visual approval remain separate. No commit, push, deployment or default-page promotion was performed.
