# In-console 3D: implementation checkpoint

Status: core implemented and locally integration-tested; **not final acceptance**, design approval or hardware qualification. Governing requirement: [3D handoff](console-3d-implementation-plan.md). This is a durable context-budget checkpoint. No physical camera was contacted; no SDK/camera-owning service, deployment, commit/push or default-page cutover occurred. `/app.html`, shared Three.js and image-stream.js are unchanged.

## Run / navigate

```sh
python3 tools/preview_console.py --host 127.0.0.1 --port 8765
# http://127.0.0.1:8765/console.html?source=demo&scenario=cloud&seed=7
node tools/capture_console.js --url http://127.0.0.1:8765/console.html \
  --matrix 3d --out _build/console-review/3d/screenshots
```

Primary drag rotates; Shift/Alt or secondary drag pans; wheel zooms. One touch rotates, two pan/pinch. Rotate/Pan selector and Zoom ± work without gesture modifiers. Focus viewport: arrows rotate, Shift arrows pan, +/− zoom, F fits, R resets home orientation and fits. Fit/Reset do not run on ordinary cloud updates. Legacy link remains available for unavailable WebGL/IR.

The preview command is static/loopback-only, never an API forwarder. An existing static preview on port 8765 was reused and left untouched; its `/console.html` bytes match the checkout. A redundant launch failed with address-in-use (`logs/preview.log`), not a camera/API error. Run the command only if that preview is no longer running. Live is now the default for missing/empty source parameters as well as explicit `?source=live`, with no synthetic fallback. Offline review requires `?source=demo`. Current UI/rate changes: [display-first checkpoint](console-display-first.md).

## Implemented ownership / contracts

- `console-cloud.js`: strict required metadata, units/scope/type/layout/MIME, safe IDs, explicit endian decoding, bounded streamed bytes (2,457,600), empty bodies, signed extrema, pure bounds/color/navigation and distinct-cloud tracking.
- Owned snapshot representation: one Int16Array of signed XYZD records, no prefix/sentinel skip, no browser validity filtering. Wire storage is temporary, not retained alongside a second decoded cloud. All API axes pass unchanged to Float32 positions. No OBJ /6000 normalization is applied to binary records.
- `console-cloud-view.js`: existing Three.js r128; one scene/camera/Points/geometry/material, reusable full-cap position/color attributes (7,372,800 bytes), draw-range shrinking/empty, finite camera bounds, on-demand <=30-Hz paints, pointer capture/cancel, touch and focused keyboard controls. DPR <=2; each drawing-buffer dimension <=4096, area <=8,388,608 pixels. Axes use one additional helper geometry/material. Counters describe CPU buffers and renderer objects, not exact GPU memory.
- Controller alone owns the cloud Scheduler (default 2; options 1/2/5/10/15/20/30 updates/s). Latest reads, no `after` waits; independent JPEG and measurement cadences. Single flight, full-operation deadline, bounded backoff and generation suppression reuse existing Scheduler. Unknown acquisition type, IR, WebGL failure/loss, pause/hide/pagehide stop cloud work; explicit retry recreates unavailable WebGL. Transitions dispose obsolete GPU resources and clear incompatible cloud state. Read errors retain last geometry, mark stale and expose the error; valid empty clouds clear draw range.
- Duplicate IDs refresh transport age, not distinct rate or geometry uploads. Range-only status changes deliberately recolor the retained snapshot without new-cloud counting/refitting. Out-of-order IDs are ignored unless contextual/reconnect evidence permits a reset. Process IDs still cannot reliably identify fast restarts already beyond the previous ID.
- Numeric sensor measurement/history/Kalman remain separate. Entering 3D converts ROI to its clamped sensor center, resets measurement context and disables ROI. No record-to-pixel picking exists. PNG dispatches to active WebGL view, render-copies immediately without permanent preserveDrawingBuffer, includes source/frame/units/stale provenance, and rejects unavailable/empty views. Device raw/OBJ and sensor-history CSV retain their contracts.
- Synthetic projection is explicitly **not camera optics**: `XYZ=((x−320)d/320, (240−y)d/320, d)` using the same seeded uint16 scene/acquisition time as point reads. Its asymmetric left/upper surfaces show orientation. Zero values are retained. `cloud`, `cloud-empty`, `cloud-single`, `cloud-negative`, `cloud-max`, `cloud-failure` complement existing motion/step/slow/frozen/context/failure scenarios (select 3D explicitly for existing 2D scenarios). Full-uint16 `long-values` is explicitly unsupported in signed cloud geometry rather than wrapped.
- Flat assets are added; no dependency upgrade, OrbitControls import, backend endpoint or backend implementation change.

## Actual evidence

All artifacts are ignored local outputs under `_build/console-review/3d/`; logs record exact command output. Previous console evidence remains in [console-preview.md](console-preview.md).

| Evidence | Result / path |
| --- | --- |
| Original model/scheduler/Kalman/source/browser/layout/assets baseline | Passed, `logs/baseline.log` |
| Pure cloud protocol, endian, streamed cap/cancellation, signed first/max records, bounds/nav/color, fixture fault/deadline, demo/point coherence, OBJ numerical comparisons | Passed `node tests/console_cloud.js`, `logs/post-review.log`, final CTest log |
| Independent cloud cadence and duplicate/frozen/order/restart/context tests; shared deadline/backoff/generation regressions | Passed `node tests/console_cloud_scheduler.js`, same logs |
| Actual Chromium WebGL, mouse/touch/keyboard/fullscreen/resize/fit, stable camera across data, native PNG nonbackground pixels/pose changes and exactly-one download, 50 mode transitions + 50 hidden/resume cycles, WebGL loss/unavailable/retry, live cloud-only transport, stale/error/empty/single/IR, cancellation/pause, frozen range recoloring | Passed `node tests/console_cloud_browser.js`, `logs/final-validation.log`; `browser/report.json`, `browser/cloud-{pose,reset}.png` |
| Existing model/scheduler/Kalman/source/browser, assets and web_ui/image_stream/performance regressions | Passed, `logs/post-review.log` |
| Fixed-width layout | Passed `node tests/console_layout.js`: 21 measurement variants ×11 states and 21 cloud variants ×8 states; 320px/high-DPR/Large/Presentation/200%-equivalent reflow. No bounding-box shifts/page overflow; supported measurement values unclipped. `_build/console-review/fixed-width/report.json`, `logs/final-validation.log` |
| Screenshot matrix | 16 real Chromium PNGs, `screenshots/manifest.json`; six required sizes, rotated/panned, inspector/filter, empty/stale/IR/WebGL/settings, Large/Presentation/200% reflow. Browser version, seed/frame/count/range/camera/DPR/render-ready/errors included. No exceptions or external requests. Parent inspected desktop, mobile, IR and reflow images; provisional design only. |
| SDK-free native build + browser | Passed, `logs/native.log` and `logs/final-validation.log`; runner verifies CMake stub flag and startup signature before mode/depth writes; isolated temporary resources/config/calibration/tripwires and process cleanup. Actual C++ serves all 15 static reference occurrences (including a repeated legacy link) with exact checkout bytes. Native cloud XYZ equals rendered positions; native header/byte counts verified. `_build/console-review/native/{runtime.log,browser.json,stub-cloud.png}` |
| Existing production export suite | `http_exports.exercise(get)` now genuinely invoked by verified native runner, passed cloud/OBJ count, /6000 numerical comparison, types/headers/mode/errors/capture recovery (not an empty standalone helper invocation). Same native logs. |
| CTest registration | Opt-in stub release block; final focused CTest **9/9** passed with `--no-tests=error`, `logs/final-validation.log`. Chromium suites stay explicit. |
| Packaging/install | `console_assets.py` passes exact flat AArch64-copy asset bytes/parity. CMake install to `_build/console-install`; all referenced/entry/legacy installed bytes compared against checkout, `logs/final-validation.log`. |

Native commands used existing host overrides after checking cache and `/usr/lib/libz.so`, `/usr/lib/libi2c.so` symlinks:

```sh
cmake -S . -B _build/console-stub -DUSE_ADITOF_STUB=ON -DUSE_SDL2=OFF \
  -DCHITON_DEFAULT_DISPLAY=headless -DCHITON_RELEASE_TESTS=ON \
  -DZLIB_LIBRARY_RELEASE=/usr/lib/libz.so -DI2C_LIBRARY=/usr/lib/libi2c.so
cmake --build _build/console-stub -j4
python3 tests/console_native.py _build/console-stub/chiton
ctest --test-dir _build/console-stub -R 'console_|web_ui|web_performance' --no-tests=error --output-on-failure
cmake --install _build/console-stub --prefix "$PWD/_build/console-install"
```

Initial browser-development retries corrected test-only WebGL fallback interception (`experimental-webgl` too), selector quoting, and a legend assertion that sampled before coalesced RAF presentation. The last such failing log is `logs/final-focused.log`; its following install comparison also failed because the aborted command chain had not reinstalled changed assets. The corrected UI-ready wait and complete rerun/install/byte comparison passed in `logs/final-validation.log`; these failures are not counted as passes.

## Independent critic

One fresh-context read-only critic reviewed the exact core diff (including untracked files), requirement/contracts, screenshot and focused test evidence. It did not edit, spawn agents or independently run tests. Exact reviewed snapshot: `review/core.diff`; full report copied to `review/critic.md`.

Two evidence-backed findings accepted and fixed:
1. Rejected status/headers must cancel the response body before reader acquisition. Added stream-cancel callback regressions.
2. Frozen-ID range changes could mismatch colors and legend/PNG. Added explicit retained-snapshot recoloring, renderer-owned range for PNG and browser checks for range change/unavailability without distinct-ID or camera changes.

Affected pure/browser/layout/native/CTest/install validation passed after fixes. Parent retains acceptance; critic review is not design approval. No additional review was represented as completed.

## Resume / remaining qualification

The core feature is usable, but do not treat this checkpoint as every acceptance assertion completed. Continue from this working tree, including untracked files; do not reset/clean or create a HEAD-only worktree. No unresolved API contract/scope question was found.

Remaining focused acceptance work:
- Strengthen browser resource accounting beyond current single-canvas/single-transport/RAF checks: assert listener and geometry/material baselines across transitions, and document measured heap/buffer trends where the browser exposes them. Current counters/bounded arrays and explicit disposal are implemented; do not infer real GPU memory from them.
- Complete an explicit browser matrix for delayed-header/body timeout/backoff, disconnect/restart and revision/type/unit race injection. Many primitives are tested in pure/source/fixture suites, but not every combination has full-page evidence.
- Tighten native PNG provenance/overlay pixel assertions and visual review beyond current native cloud rendering + fixture native-download evidence; native OBJ numerical bytes are already checked independently.
- Review the small post-critic fix blast radius if warranted; do not relaunch a broad review without new evidence. Final exact working diff: `review/checkpoint.diff`.
- Full release/sanitizer matrix, hardware acquisition/network/thermal/optical qualification and operator design approval remain **not run/not granted**. Software WebGL, accelerated cycles and stub results establish no device performance or calibrated accuracy.

At stop: no active native process, API fixture, cloud poller or review agent created by this task; the pre-existing static preview (PID 1903532 at handoff, cwd this repository) remains untouched. No dirty mutation result is pending. Changed-file inventory is `review/changed-files.txt`. The next task is bounded qualification/handoff, not a renderer rewrite or backend change.
