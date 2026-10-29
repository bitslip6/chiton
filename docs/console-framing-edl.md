# Cloud framing and optional eye-dome lighting

Implemented and locally validated; independent review completed. Its main-fit pan-anchor finding was fixed and the narrow correction was re-reviewed without further actionable defects. This extends the point-style/GPU-color work without changing sensor processing, the palette, point validity, network formats or device exports. Operator clarification: sub-600 mm sensor flooding and beyond-5.6 m no-return data are outside the useful sensor range. This change does not reinterpret those limits or introduce browser validity thresholds; every returned record is still submitted.

Later renderer extension: [reversed colors, sensor-depth sizing and adjustable
stronger EDL](console-renderer-depth-style.md). The original implementation and
performance evidence below remain historical; EDL strength now defaults to 4
(previously fixed at 2), with a local 0–8 slider.

## Controls

In **Display controls → View / navigation** in 3D (lighting is under **Point appearance**):

- **Fit all** (also F): frame bounds of all returned XYZ records; preserve orientation.
- **Fit main cloud**: camera-only fit using the 1st/99th percentile of each signed XYZ axis, then report how many records lie within all three intervals. Below 100 records, use all records. All records remain uploaded/rendered; outliers can lie outside the viewport. A visible last-fit status distinguishes framing from filtering. Pan limits use the selected fit center, so an outlier-heavy main fit does not jump back toward the all-record center on the first pan. This is not semantic object detection.
- **API view**: Front looks from +Z onto XY, Top from +Y onto XZ, Side from +X onto YZ. These axes are API coordinates, not a calibrated physical sensor pose. Existing cloud-centered colored axes remain the orientation guide.
- **Projection**: Perspective (default) / Orthographic. Orthographic zoom uses the same navigation distance and preserves scale at the target plane. Switching projection/orientation does not refit. Reset view restores home orientation and Fit all, retaining the selected projection and lighting.
- **Eye-dome lighting**: optional, **off by default**, viewer-only. Applies adjustable depth-edge/silhouette shading, not geometry smoothing, interpolation or meshing. Also applies to viewport PNGs. The PNG footer identifies EDL when on and orthographic/main framing when selected.

Projection and lighting are in-memory page settings restored on renderer recreation, mode changes and context-loss retry. Reload returns to defaults. Fits/presets are explicit camera actions; ordinary frames/style/range/lighting changes never refit. Paused/hidden local controls do not restart acquisition or painting. The subsequent [display-first change](console-display-first.md) keeps the single navigation and appearance groups permanently in a toggleable panel inside the existing fullscreen viewport; no reparent-on-fullscreen path, cloned IDs or second settings state.

## Rendering and compatibility

Plain points keep the existing single scene pass, palette, packed signed XYZD and opaque depth-tested point material. EDL lazily adds one bounded full-resolution color/depth target and a fullscreen pass, with eight neighboring depth samples at approximately 1.5 CSS px. It uses view-space depth reconstructed for the selected projection. Clear background pixels are preserved; surface RGB is only darkened. Axes share the depth/color pass. Near/far planes for EDL derive from **all** authoritative XYZ bounds plus visible axes, not just the main-fit subset, to improve depth precision without cropping outliers by that subset.

EDL needs WebGL2 or WebGL1 depth-texture support, plus high-precision fragment arithmetic. Unsupported EDL is disabled with an explanation while plain points remain available. A failed EDL framebuffer returns to plain points and releases EDL resources; ordinary renderer/context failures retain the existing unavailable/retry flow. Targets/material/quad are reused on toggles, resized with the bounded drawing buffer, and disposed with the view. Once first used, cached target resources remain until renderer disposal even if EDL is switched off; disabled EDL adds no postprocessing draw pass. No derivatives, floating-point color attachments, new library, network dependency, GPU synchronization in production or additional animation loop.

## Validation and performance

Evidence: `_build/pointcloud-framing-edl/` (ignored).

- `node tests/console_cloud_framing.js`: empty/small/maximum/signed-outlier percentile bounds, unchanged records and orientation, finite presets, reset-to-all.
- `node tests/console_cloud_framing_edl_browser.js`: actual WebGL1 and WebGL2 framebuffer depth-step shading, unchanged background and flat interiors, exact plain-mode restoration, no point upload from lighting/fit, reusable targets/programs, orthographic rendering, actual PNG provenance, inactive-view suppression, fullscreen/mobile controls, context retry, live no-write behavior and unsupported/failing optional EDL fallback.
- Existing material GPU, appearance/browser/layout/viewport/resource/scheduler/source/measurement/Kalman/legacy web suites passed. Layout retains the previous 320 px, Large/Presentation and reflow matrices. The appearance fullscreen test now checks the shared fullscreen control host rather than requiring direct-child appearance placement.
- A fresh isolated SDK-free native build passed `tests/console_native.py`, actual export helper and Chromium native checks with EDL/main-fit/orthographic enabled; raw/OBJ hashes unchanged. Focused CTest **11/11** with `--no-tests=error`, including the new pure framing test. Temporary simulator resources and command tripwires only; no physical camera or deployed binary.

Reproduce the test-only comparison with `node tools/benchmark_console_edl.js`. Chromium 155 / ANGLE SwiftShader, Ryzen 5600X, 800×600 DPR1, circle2.5, identical synthetic input/pose/range, two reversed-order rounds, six warmups and 15 measured samples per round. Completed draw includes test-only `finish` + one-pixel synchronous readback (never production):

| Records | EDL off updated draw median/p95 | EDL on updated draw median/p95 | Off/on orbit median |
|---|---:|---:|---:|
| 40,000 | 13.1 / 14.1 ms | 20.0 / 22.3 ms | 4.3 / 11.4 ms |
| 307,200 | 38.1 / 42.6 ms | 44.5 / 54.3 ms | 30.2 / 36.1 ms |

Exact samples/environment/counters: `validation/benchmark.json`; command log `validation/benchmark.log`. The previous packed-SHORT software upload cost remains, not claimed fixed. EDL costs additional fill/texture work and attachment memory, especially at high DPR; default off is deliberate. These software timings are not physical-GPU or Wi-Fi measurements and do not promise 10 FPS on every client. No change to cloud transfer/acquisition cadence.

Representative matched plain/EDL depth-step PNGs, console views, top orthographic, narrow fullscreen controls and native export are in `screenshots/` and `native/runtime/`. Visual approval, physical GPU/browser/device and full release/sanitizer qualification remain separate. No commit, push, deployment or default-page promotion.
