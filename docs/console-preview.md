# Local depth console preview

Status: implemented preview; design provisional, user approval pending. No default-page cutover, deployment, physical camera contact, or hardware qualification. Governing requirements: [console redesign plan](console-redesign-implementation-plan.md), supported by [measurement recommendations](web-measurement-improvements-plan.md).

## Run locally

```sh
python3 tools/preview_console.py --host 127.0.0.1 --port 8765
# http://127.0.0.1:8765/console.html?source=demo&scenario=stable&seed=7
node tools/capture_console.js --url http://127.0.0.1:8765/console.html \
  --out _build/console-review/latest
```

The Python server is static, loopback-only and does not forward APIs or start a camera. On a native server, open `/console.html?source=live` deliberately to select existing same-origin APIs. Never use the static preview's live mode as evidence of camera integration. Unknown source/scenario values fail closed. Source switching reloads with a fresh client identity; live failures never substitute demo data.

Chromium must be installed at `/usr/bin/chromium` or configured with `CHITON_CHROMIUM`. Screenshot/browser tools use disposable profiles, a rejecting background proxy, and origin-allowlisted page requests. The API fixture additionally needs `cjpeg` (or `CHITON_CJPEG`), generating JPEGs from the same synthetic uint16 field used for measurements, not an unrelated photo.

## Architecture and contract inventory

All runtime assets are flat files in `release/files/`, with no build step, CDN, font download, legacy controller, or new API. `console.html` loads the pure model/scheduler/Kalman helpers, independent demo/live adapters, view, and controller. Existing `image-stream.js` is reused unchanged. `console_assets.py` checks asset references, AArch64 asset-copy inclusion, IDs, and legacy page parity. CMake installs the existing `release/files/` directory automatically; no alias/promotion changes are needed.

| Existing feature | Console treatment |
| --- | --- |
| Selected pixel / ROI | Local overlay, numeric coordinates, pointer selection, keyboard point movement; actual content-rectangle mapping |
| Point update rate | Viewer-local 5/10/20/30 Hz, default 20; ROI independently defaults to 5 |
| Image target FPS | Independent viewer-local control, default 30; bounded existing WebSocket transport with JPEG HTTP fallback |
| Value / Mean / STDD / Diff | Measured processed value, Mean, Std dev (`stddev`, never `stdd`), Range; min/max/count/actual API span |
| Depth/IR, modes 0–6 | Read acknowledged settings; mutations only on explicit action; live changes require confirmation |
| Mode 4 / 3D | Explicitly labeled legacy-viewer fallback; no fake 3D rendering and no JPEG requests while acknowledged cloud mode |
| Range, quality, EMA, calibration | Explicit Apply; validated acknowledgment; no mutation retry/default writes; unknown outcomes and unsupported read-back explicit. Demo retains/applies the color range; JPEG quality is explicitly unavailable (canvas, not JPEG encoder). |
| Raw / OBJ | Validated bounded device downloads, unchanged processed formats, never local-Kalman-filtered |
| PNG / CSV | Native browser downloads of actual viewport / bounded browser-received history; demo provenance retained |
| CPU / Vixel / pipeline | Collapsed telemetry; temperature independent 4-second reads, optional 1-second diagnostics |
| Firmware, network, registers, diagnostics archive, illumination | Remain in legacy Advanced device tools; no new console actions |

The legacy entry and both shipped `app.html` copies remain unchanged and accessible. No backend runtime or API-history changes were made. Console assets are not mirrored into `release/` or `bin/`.

## Measurement semantics

- API point statistics retain up to 60 distinct server-observed frames per tab; ROI statistics describe spatial pixels and the main value is **Center pixel**, not mean. All uint16 values, including 0 and 65535, participate.
- Browser median, raw MAD and linear-interpolated P05/P95 use received distinct samples over trailing 2 seconds, cap 60. Ready means at least 10 samples and 1 second of observed span: only a UI heuristic. Browser and API histories may differ.
- One persistent graph; 300-record cap; 5/10-second display windows; at most 20 redraws/s. The x-axis is browser reception time, not host acquisition/receipt timing; CSV retains the separate API host timestamps. Hold history freezes the view, not collection. Pause viewer stops only this page's streams. Context changes clear local history/reference/filter state; parameter changes preserve API statistics and begin a new comparison segment.
- Host receipt age adds browser monotonic elapsed time to API age and excludes network transit/exposure timing. Absent age stays unknown. Lack of new distinct IDs also marks frozen successful reads stale. Image and measurement frames retain separate identities.
- Requests are single-flight, start-to-start budgeted, deadline-bounded (including bodies), obsolete-generation suppressed, hidden/pagehide canceled, with 500/1000/2000/4000/8000 ms read-only retry. Large ROI work remains on the existing server contract and has not been hardware-profiled.
- Process IDs remain process-local. Regressions with reconnect/history-reset evidence reset state; a fast server restart already beyond the last seen ID is not reliably detectable. No session UUID was invented.

## Fixed-width readouts

Measurements now use viewport-sized, content-independent slots with tabular numerals, reserved line heights and no numeric wrapping. Measured/estimated units occupy separate stable rows; API/local statistics reserve room for signs and two decimal places. Container-sized statistic fonts accommodate the uint16 range even at 320 px and Large/Presentation sizing. Telemetry uses fixed grid columns rather than content-sized flex items. Frame IDs, counts, spans and model uncertainty have explicit numeric widths. Exceptional unbounded counters/spans remain horizontally scrollable within their slots rather than clipping values or resizing the page.

`node tests/console_layout.js` exercises 21 viewport/text-size combinations (including 320 px, high DPR, and 200%-equivalent reflow), each with 11 numeric states from placeholders through 65,535, fractional statistics, negative deltas, and maximum safe frame IDs. It asserts unchanged bounding boxes for readouts/units/adjacent controls and no clipping of supported depth/statistic numbers or page overflow. Evidence/screenshots: `_build/console-review/fixed-width/`. Existing actual-browser/source/export tests, verified native-stub integration and focused CTest 7/7 also passed after this change. A fresh read-only critic inspected the bounded layout diff and test evidence and found no actionable issues (no independent test execution claimed).

## Local Kalman estimate

Off by default, depth points only, displayed separately from measured processed values. It does not change images, point clouds, exports, upstream EMA, plugin behavior, or API statistics. First `x=z, P=R`; subsequent random-walk recurrence uses positive host receipt timestamp differences. Missing timestamps suspend; discontinuities or gaps over 2 seconds reinitialize. No spike rejection, no inferred physical validity.

Demonstration mm tuning: R=25 mm², q=1250/125/12.5 mm²/s for Responsive/Balanced/Stable. Live enabling requires explicit tuning acknowledgment; sensor-unit inputs require manually configured unit-specific R/q. Advanced variances require finite `0 < R ≤ 1e9`, `0 ≤ q ≤ 1e9`. Model √P is not certified accuracy. Only display sizing and precision persist, never presumed device state or source truth.

Synthetic balanced checks: stationary filtered/measured deviation ratio **0.4793** (target <0.7); 90%-step settling **0.25 s** at 20 Hz (target ≤1 s). These validate demonstration math/tuning, not camera performance or optical accuracy.

## Scenarios and visual review

`scenario=` accepts `stable`, `spikes`, `step`, `movement`, `slow`, `measurement-failure`, `image-failure`, `frozen`, `restart`, `contexts`, `roi`, `warming`, `ir`, `panorama`, `long-values`. The Demo laboratory selects scenarios explicitly. Seed plus acquisition time/frame index determines the numeric scene; HTTP request count does not. Interactive capture runs nominally 30 Hz (5 Hz in slow scenario). Geometry background zeros are included in statistics, not labeled invalid.

Screenshots use `ChitonConsoleApp.prepareCapture()` to stop live work, seek the seeded source, and explicitly replay actual source reads at declared observation times before freezing. This is review-only pre-roll, not synthetic startup API history. Warming capture starts with one sample. Restart outage preserves the last good value and marks it delayed rather than drawing zeros.

Review artifacts: `_build/console-review/iteration-04/` (18 PNGs + manifest), plus browser/native test reports. Matrix: 1920×1080, 1440×900, 1280×720, 1024×768, 768×1024, 390×844; ROI, filter, drawer, stale/outage, warming, IR, panorama, full uint16 numbers, Large/Presentation, and 200% equivalent reflow. The zoom case uses a 640×360 CSS viewport at DPR 2 for a 1280×720 physical image; it tests changed layout width, not a scaled wide screenshot or a personal browser zoom preference.

Parent inspected desktop/720p/mobile/filter/panorama/zoom rendered PNGs. Iteration 2 separates the workspace stack from inspector height so the history graph is visible at 1280×720. Small widths/high zoom use ordinary page scrolling, not clipped fixed-height sidebars; primary readings are directly below the viewport. Demo provenance stays in fullscreen/image exports. Frozen image freshness uses source age and distinct image arrival with the browser monotonic clock; the shared transport's wall-clock `received` field is not mixed into that clock. Settings use a native modal, labeled controls, Escape/Tab containment and focus restoration. No 20-Hz numeric live announcements. Checked palette contrast ratios on actual token pairs: primary/panel 14.66:1, secondary/panel 7.17:1, accent/estimate 8.37:1, warning/state 9.40:1, live/state 10.77:1 (normal-text threshold 4.5:1).

Design remains provisional; continuing implementation was authorized, not design or rollout approval. At large desktop sizes the image intentionally takes most vertical space and history/telemetry can require page scrolling. No light theme, pan/zoom controls, quantitative grayscale legend, or in-console 3D renderer is claimed.

## Validation

```sh
node tests/console_model.js
node tests/console_scheduler.js
node tests/console_kalman.js
node tests/console_source_contracts.js
node tests/console_browser.js
python3 tests/console_assets.py
node tests/web_ui.js
node tests/web_image_stream.js
node tests/web_performance.js
```

Focused suites passed: helper math/statistics/mapping/CSV, exact cadence/backoff/deadline/cancellation, common demo/live contract, coherent ROI/point/history/unit cases, malformed/absent fields, lost mutation acknowledgment, native JPEG wire/fallback, actual downloads, a 5,000-frame accelerated bounded-history run, selection/resizing/high-DPR, modal focus and hidden/resume/source safety. Screenshot automation captures real Chromium, not DOM mocks. Accelerated tests do not establish real-time CPU use.

SDK-free local integration:

```sh
# This host has ARM bundle libraries in bin/; override erroneous native discovery explicitly.
cmake -S . -B _build/console-stub -DUSE_ADITOF_STUB=ON -DUSE_SDL2=OFF \
  -DCHITON_DEFAULT_DISPLAY=headless -DCHITON_RELEASE_TESTS=ON \
  -DZLIB_LIBRARY_RELEASE=/usr/lib/libz.so -DI2C_LIBRARY=/usr/lib/libi2c.so
cmake --build _build/console-stub -j4
python3 tests/console_native.py _build/console-stub/chiton
ctest --test-dir _build/console-stub -R 'console_|web_ui|web_performance' --no-tests=error --output-on-failure
cmake --install _build/console-stub --prefix "$PWD/_build/console-install"
```

Passed verified-stub browser/static integration (12 exact static routes, legacy bytes, native JPEG and ROI), focused CTest 7/7, and isolated installation. The critic correctly caught that the first CTest invocation registered no matching tests (release tests are opt-in). The corrected run enables `CHITON_RELEASE_TESTS=ON`, fails closed with `--no-tests=error`, and records all seven named passing tests in `ctest-console.log`; the initial exit-zero/no-tests output is not counted as validation. Native runner checks the CMake stub flag and startup stub signature before API setup, uses temporary input/config/calibration plus command tripwires and explicit resource/asset directories, and cleans up the process. No real SDK/service is started.

Existing `http_measurements.py` passed all constant/gradient/checkerboard/full-uint16 and plugin cases using a deeply isolated copy at `_build/console-stub/isolated/bin/chiton` with copied `release/chiton.json` and absolute `CHITON_ASSET_DIR`. Initial runs from a shallow build found the repository's existing release plugin before the test plugin; an attempted empty resource override lacked configuration. These were harness isolation failures, not suppressed assertions. The isolated retry passed. `http_ui_contracts.py` alone defines a helper and performs no standalone suite; do not count its empty output as a pass.

Full release/sanitizer matrix and hardware tests were not run for this additive web preview. Actual i.MX8 acquisition/network load, multiple physical viewers, camera noise/step tuning, thermal/CPU behavior, and visual approval remain open. Existing shipped regression suites passed. Test logs, install logs and native evidence live under `_build/console-review/` (ignored).

## Rollback / remaining work

Open `/app.html` to use the untouched legacy UI. Remove the added flat console assets to remove the preview; no data migration, device default change or API rollback is necessary. Do not promote `/console.html` to a default entry without explicit user approval and integration/hardware acceptance. Full-frame Kalman or acquisition-time recording remains a separate backend project. The planned in-console point-cloud increment is specified in [3D implementation handoff](console-3d-implementation-plan.md); it is not implemented yet.

Independent fresh-context critic completed read-only review of the exact implementation diff, source/contracts, screenshots and actual test logs. Findings accepted and corrected: (1) retain and render the simulated color range, disable unsupported demo JPEG quality and test read-back/render effects; (2) relabel the graph's reception clock; (3) correct the CTest evidence/registration gap above. Parent additionally fixed image freshness for duplicate HTTP frames and source age, including a regression for the shared WebSocket helper's wall-clock metadata versus browser monotonic time. Deterministic review now freezes the render clock and keeps only actually available images across simulated outages. Affected helper/source/browser/native/screenshot validation passed after fixes. The same critic reviewed only the fix blast radius and reported no remaining evidence-backed blockers; review was source/artifact inspection, not an independent test execution. Review does not constitute visual approval or hardware qualification.
