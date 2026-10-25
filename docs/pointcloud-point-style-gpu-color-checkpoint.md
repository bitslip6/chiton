# Point appearance / GPU color: qualification checkpoint

Status: implemented and locally checked; **independent critic completed with no blocking runtime correctness/safety defect; accepted capture-evidence finding fixed and validated, parent final verification completed. Operator visual approval pending; physical GPU/browser/device qualification unrun**. Governing plan: [point-style/GPU-color plan](pointcloud-point-style-gpu-color-plan.md). This is not rollout or default-page approval. No camera/device, real SDK, deployment, staging, commit or push was used.

## Implementation retained

- One signed-int16 XYZD interleaved attribute buffer: **8 B/record**, 2,457,600 B CPU attribute capacity versus 7,372,800 B previously. No RGB attribute/production CPU color loop or fallback renderer. First observed GL allocation uploads full capacity; warmed frames upload the populated prefix.
- Vertex depth coloring preserves the old palette, black endpoints and neutral color. Shape/range/size are uniform-only updates, not geometry uploads or device settings.
- Viewer-local defaults: **Circle / 2.5 CSS px / constant screen size**. Square remains available. Diameter 1–10 CSS px, step 0.5; perspective uses view depth with multiplier bounded 0.5–2 before hardware clamp. Effective capped DPR and actual context size limits are exposed; software contexts reported physical range 1–1023 px, not a universal hardware guarantee.
- One expandable labeled group, including inside the existing fullscreen viewport; state survives controller recreation/retry. Appearance does not refit the camera or alter measurements/history/Kalman, raw or OBJ. PNG footer records current requested style, effective DPR, hardware limits and existing source/frame/range provenance.

## Local checks

All 14 existing plan-section-8 commands passed, plus `console_cloud_material`, real WebGL1/2 `console_cloud_material_gpu`, `console_point_appearance_browser` and new `console_cloud_lifecycle_browser`. Old browser `.colors`/`.positions` checks migrated to actual selected-record framebuffer colors and signed packed attributes without removing their navigation/export/lifecycle assertions. RGB tolerance is ≤2/255/channel for actual 8-bit pixels.

Evidence root: `_build/pointcloud-renderer-style/` (ignored). Exact commands/results and scoped patch: `handoffs/qualification.md`, `handoffs/qualification.patch`, `validation/qualification/`. Notable evidence:

- `validation/renderer-gpu-webgl{1,2}.json`: signed SHORT extrema/endian semantics, palette including drawn black endpoints, coverage, opaque occlusion, DPR/clamps, bounded perspective sizing, frozen uniform-only actual GL uploads, max→small→empty reuse, shader failure and real PNG pixel/provenance checks.
- `validation/ui/report.json`: real keyboard/fullscreen controls, 15 expanded layouts, pause/hide/retry persistence, no local-storage/device writes, unchanged populated measurement/history/Kalman during style change, native square/circle PNG downloads.
- `validation/lifecycle/report.json`: 50 real context transitions, 1,500 style sets and 50 hide/resume cycles. Two geometries/programs, three GL buffers when active; all tracked buffers/programs/shaders/VAOs explicitly deleted on disposal, one live context, stable canvas/document/window listener counts, maximum two outstanding page RAFs, zero idle RAF. Two internal r128 fallback textures per context lack explicit delete calls but are reclaimed by observed forced context loss. Test-only GC/heap samples show stable post-warm-up backing storage; these are not exact GPU/browser memory measurements.
- `validation/native-stub/`: dedicated new SDK-free/headless CMake build; cache, binary signature, linked libraries and startup identity checked before API setup. Temporary resources/config/calibration and command tripwires. Actual export helper invoked, browser native JPEG/ROI/cloud/OBJ/PNG passed; raw/OBJ hashes unchanged after style/pose change. Focused CTest **10/10**, with `--no-tests=error`, including new dependency-free material registration. All 15 referenced/entry asset names served/copied byte-for-byte, and 16 installed copies (two legacy HTML locations) match. No deployed binary was run.

## Measured performance — explicit tradeoff, not a speedup claim

Repeatable old/new square/2.5 completed-render regression triggered the required parent investigation gate. A bounded test-only representation differential isolated a post-upload SHORT-capacity-dependent cost on **Chromium 155 / ANGLE SwiftShader / Ryzen 5600X Linux**, not proven driver internals. Parent explicitly retained packed SHORT after investigation; **no Float32 fallback or production capacity redesign was approved**.

Final repeat, milliseconds **median / p95**; 3 rounds × 8 warm-up + 20 measured repetitions, fixed deterministic input/range/pose/800×600 DPR1:

| Records | Old preparation | New preparation | Old completed render | New completed render | Old/new no-update orbit median |
|---|---:|---:|---:|---:|---:|
| 40,000 | 1.1 / 1.2 | 0.5 / 0.7 | 3.9 / 5.5 | 12.9 / 15.0 | 4.0 / 4.3 |
| 307,200 | 5.7 / 6.4 | 2.2 / 2.9 | 29.4 / 32.1 | 38.0 / 44.6 | 29.1 / 29.1 |

Completed timing includes **test-only** `gl.finish` plus synchronous one-pixel readback; CPU submission is reported separately. `finish` alone was misleadingly asynchronous, so its preliminary timings are not completion evidence. Production has no forced synchronization. Each final benchmark proves actual nonbackground point coverage and exact submitted counts.

At 40k, count-sized SHORT diagnostic completed draw was 5.4ms versus full-cap SHORT 12.6ms; full-cap Float32 XYZD was 4.2ms. At maximum, SHORT remained ~38ms versus Float32 ~30ms. These diagnostic buffers are **not shipped**. Circle/square 2.5 costs were similar within noise. Large diameter increases fill/overdraw: maximum cloud circle10 ~85.7ms completed draw; near perspective10 (bounded up to20 CSS px) ~183.3ms. Bounded size does not guarantee 30 FPS.

Full medians/p95, per-round data, actual GL calls/bytes, CPU capacities, input/source hashes, environment, diagnostic disposition and limitations: `validation/performance.md` and `validation/benchmark-{confirmed,diagnostic-confirmed,extremes-confirmed}/{report,raw}.json`. HTTP bytes are unchanged; attribute capacity is not total memory. No universal FPS improvement or physical-hardware performance claim.

## Images / next gate

`screenshots/qualification/manifest.json` under the evidence root contains **22 actual screenshots +22 actual viewport PNG exports**, matched square/circle2.5, sizes1/10, both sizing modes, 1920/1280/390/320 widths, 200%-equivalent reflow and fullscreen. `screenshots/square-baseline-match/manifest.json` repeats the original 16-view matrix with optional complete square/2.5 style; original assets/captures remain in `baseline/`. Refreshed manifests preserve explicit rendered `cloud.frameId`, `cloud.recordCount`, normalized `cloud.range` and `cloud.pointStyle`, with final pose/effective-DPR/hardware counters under `cloud.renderer`. Unavailable IR/WebGL captures explicitly report null rendered ID/count/range/renderer rather than borrowing top-level source frame metadata; empty clouds retain their valid ID and zero count. `node tests/console_capture_provenance_browser.js` regenerated both matrices and checks these fields, final renderer consistency, actual PNG dimensions and demo-only safety. Representative actual desktop/mobile/fullscreen/reflow/size/native-export images were inspected; this is not operator visual approval.

Independent read-only critic completed (`review/critic.md`): no blocking runtime correctness or safety defect, one low-severity capture-manifest provenance loss. Parent accepted that finding; the tool-only correction, focused regression and refreshed captures are recorded in `handoffs/capture-provenance-fix.md`. Earlier qualification/benchmark artifacts retain their historical review status; this checkpoint and the follow-up handoff supersede that status without changing performance evidence.

Parent inspected the tool-only fix and focused test, verified the refreshed integrated source hashes, and independently reran `node tests/console_capture_provenance_browser.js` successfully (all 38 captures). The accepted finding is resolved; no further critic pass was needed for this narrow metadata-preservation change. Renderer/UI/native sources are unchanged from the reviewed and validated implementation. Retain the documented SwiftShader regression/tradeoff and pending physical GPU/browser/thermal/network/optical qualification. Do not deploy or promote `/console.html` by inference from these local checks.
