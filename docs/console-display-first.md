# Display-first console checkpoint

Implemented for `/console.html` only. Live is the default; the legacy `/app.html` and homepage routing are unchanged. No camera, deployment, cross-build, commit or push. Governing requirement: `plan-display-first-ui.md`. Independent review disposition is recorded below.

## Operator behavior

- The actual viewport fills the first screen beside the retained right-hand measurement inspector. Desktop inspector scrolls independently; the existing narrow-screen order remains viewer → inspector → history. History and detailed telemetry remain below the viewer.
- **Display controls**, **Pause/Resume viewer**, and **Fullscreen/Exit fullscreen** stay available. The non-modal, internally scrolling controls panel starts closed. Toggle, Close, or Escape within the panel closes it without applying/discarding drafts, changing pose, resizing the canvas, or writing storage.
- Sections: View/navigation (device-wide acquisition/mode confirmation retained), Point appearance/EDL, Clipping/slices, Refresh/display, Export, Help/advanced. Existing inputs are moved once, not cloned. One permanent panel serves ordinary and viewport-only fullscreen layouts.
- Device/source settings remain a separate native modal. Opening it suspends the panel; closing restores panel/focus state. No backdrop or focus trap is added to the non-modal panel; uncovered canvas remains interactive.
- Applied clipping intervals, matching/submitted counts, Reset, context-reset notices and last-main-fit indicator are outside the panel. PNGs still contain only the viewport and required provenance, never the DOM panel/sidebar.
- Cloud target choices: **1, 2, 5, 10, 15, 20, 30 updates/s; default 2**. Measurement, ROI, JPEG and local paint rates remain separate. Invalid targets are rejected atomically even when inactive. Pause/hide retains the target without starting work.
- Missing/empty source selects same-origin Live without device mutations or startup storage writes. Explicit `?source=demo` is required for offline preview and deterministic capture. Invalid sources fail closed; unavailable live APIs never substitute synthetic data. The static preview launcher already supplies the explicit demo URL.

The former elapsed-from-last-paint RAF check achieved only **20.43 FPS** in an exact 60 Hz deterministic test. Fractional-deadline pacing now gives 30 FPS at 60/120 Hz and approximately 29.98–30 with jitter, without catch-up queues, idle painting or more than one pending renderer RAF. Explicit capture/export render remains a separate path. Cloud/render telemetry retains 64 timestamps for its two-second window (including boundaries).

## Validation / reproducibility

Ignored evidence: `_build/display-first-ui/{baseline,validation,screenshots,review,handoffs}/`. Baseline includes the full original tracked patch, relevant tracked/untracked file copies and SHA-256 hashes, focused test logs and matched desktop/mobile/reflow captures. Existing clipping, appearance, framing/EDL and board-script packaging changes were preserved.

Passed Node suites (each `node tests/<name>.js`; individual logs in `validation/`):

- `console_model`, `console_scheduler`, `console_cloud_scheduler`, `console_cloud_cadence`, `console_kalman`, `console_source_contracts`
- `console_cloud`, `console_cloud_clipping`, `console_cloud_material`, `console_cloud_framing`
- `console_browser`, `console_layout`, `console_viewport`, `console_display_controls_browser`
- `console_cloud_browser`, `console_cloud_material_gpu`, `console_cloud_clipping_browser`, `console_cloud_clipping_ui_browser`, `console_point_appearance_browser`, `console_cloud_framing_edl_browser`, `console_cloud_lifecycle_browser`, `console_capture_provenance_browser`
- `web_ui`, `web_image_stream`, `web_performance`; also `python3 tests/console_assets.py`

`viewport.json` covers **144 combinations**: 2D/3D × eight resolutions (1920×1080 through 320×844 and 640×360 reflow) × Normal/Large/Presentation × DPR1/2/3 (effective cap retained). Actual viewport height is approximately window height minus two border pixels, versus the 180 px baseline at 1280×720. Open/closed viewport bounds, drawing-buffer dimensions, pose and reference distance are unchanged; no horizontal page overflow or scroll jump. Desktop sidebar is outside the panel bounds; mobile reading order is preserved.

The new interaction suite uses native mouse/touch/keyboard events, hidden-panel focus checks, draft/error/scroll preservation, modal/fullscreen restoration, 100 no-listener/no-paint/no-upload toggle cycles, source-default safety, paused invalid-rate rejection and fast/slow single-flight 30 Hz wiring. Existing lifecycle tests retain 50 renderer transitions and real GL accounting. Fixture cloud headers now round timestamps/ages to integer milliseconds, matching the native wire contract rather than accidentally sending fractional synthetic times.

Matched **19 display-first screenshots**, plus cloud PNG exports and manifests, are in `screenshots/matched/`: desktop/mobile/reflow open/closed, invalid draft, zero-match, fullscreen, 30 target, image/panorama, stale/unavailable. Parent inspected actual screenshots. Existing capture provenance tests also regenerated their 38 scenes and validated demo-only requests and PNG bytes.

Isolated native validation (host library paths were checked locally):

```sh
cmake -S . -B _build/display-first-ui/native \
  -DUSE_ADITOF_STUB=ON -DUSE_SDL2=OFF \
  -DCHITON_DEFAULT_DISPLAY=headless -DCHITON_RELEASE_TESTS=ON \
  -DZLIB_LIBRARY_RELEASE=/usr/lib/libz.so -DI2C_LIBRARY=/usr/lib/libi2c.so
cmake --build _build/display-first-ui/native -j4
CHITON_CONSOLE_NATIVE_OUT="$PWD/_build/display-first-ui/validation/native-runtime" \
  python3 tests/console_native.py _build/display-first-ui/native/chiton
ctest --test-dir _build/display-first-ui/native \
  -R 'console_|web_ui|web_performance' --no-tests=error --output-on-failure
cmake --install _build/display-first-ui/native \
  --prefix "$PWD/_build/display-first-ui/install"
```

CMake cache, binary simulator signature and startup identity were checked before temporary fixture setup; linked libraries recorded. Actual HTTP exports and native browser JPEG/ROI/PNG/raw/OBJ/binary checks passed. Focused CTest **13/13** passed. All 15 unique console entry/reference/legacy asset names match static/package/install bytes. Native 30-target trial over four seconds: 121 cloud requests, 79 measurement requests, four status requests, approximately **28.06 distinct Hz / 28.19 render FPS**, no cloud error or measurement starvation. This is the SDK-free serial server, not a physical sensor qualification.

## Measured performance and limits

Reproduce: `node tools/benchmark_console_display.js`. `validation/throughput.json` records browser version, all **64 fixed-800×600-DPR1 cases** (40k/307200 records × clipping off/depth/box/intersection × EDL off/on × targets2/10/20/30), completed response payload bytes, latency/preparation/count/body+decode/CPU-submission medians/p95, actual GL uploads/bytes, counters and heap samples. Frame content, pose and style stay fixed during rate comparisons; source IDs advance. Body+decode timing includes transfer, not isolated parser timing. Two-second rate estimates depend on the observation phase, especially at target2. CPU submission and renderer FPS do not measure GPU completion.

Representative target30 results, Chrome155/ANGLE SwiftShader on this host:

| Records | EDL | Distinct Hz | Local render FPS | Completed payload MB/s | Read latency median/p95 ms |
| --- | --- | ---: | ---: | ---: | ---: |
| 40,000 | off | 29.8 | 29.8 | 9.60 | 3.2 / 6.0 |
| 40,000 | on | 29.7 | 29.8 | 9.59 | 5.1 / 6.4 |
| 307,200 | off | 28.9 | 20.8 | 72.59 | 20.7 / 26.4 |
| 307,200 | on | 24.0 | 17.4 | 61.19 | 23.9 / 38.8 |

Separate retained-frame completed draws (test-only `finish` + one-pixel readback, eight measured draws after four warmups): 800×600 plain **28.7/31.6 ms** median/p95; 889×718 plain **32.2/34.3 ms**. EDL: **33.8/35.2 ms** versus **41.7/42.7 ms**. These isolate enlarged-buffer cost without changing frame content/pose; they are not updated-frame or hardware GPU benchmarks.

Delayed headers/body, stalled transfers and a ten-second sustained recovery run retained one concurrent cloud request and independent measurement/status progress. No backend concurrency redesign or quality/record reduction was used. At full size, 30 responses/s could consume **73.73 MB/s (~590 Mbit/s)** before HTTP overhead; duplicate snapshots also consume bandwidth. Clipping does not reduce network, decoded/submitted records or normal uploads. EDL, DPR and larger canvases can exceed the frame budget. No guaranteed 30 distinct frames or completed GPU FPS is claimed.

Residual qualification: physical GPU/browser/device/Wi-Fi/thermal behavior, real mobile virtual keyboards and operator visual approval are not covered by headless Chromium. The viewport declares `interactive-widget=resizes-content`, uses dynamic viewport/safe-area bounds and retains reachable sticky Close at tested reflow sizes. Full sanitizer release qualification was not rerun for this frontend-only change.

## Independent review

Fresh-context read-only critic completed. One medium finding was accepted: hiding the clipping status whenever applied clipping was off also hid the only Reset button for unapplied drafts. The visibility predicate now retains Reset when clipping is applied, the clipping section is open, or drafts differ from applied settings. No new input/listener or apply/reset policy was introduced.

A failing-before/passing-after browser regression covers valid and invalid unapplied drafts with clipping off, closing the section/panel, native pointer access to Reset, restored drafts/errors and unchanged pose/uploads/uniforms. Clipping UI, display-controls, all144 viewport variants, native browser/export checks, CTest13/13 and asset parity passed again; installed assets were refreshed. Parent inspected the narrow correction; no second critic pass was necessary. Review and disposition: `_build/display-first-ui/review/critic.md`; feature-only patch and file list are alongside it. No unresolved critic findings.
