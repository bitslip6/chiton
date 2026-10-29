# Chiton console redesign: implementation handoff

**Status:** implementation plan, not implemented. **Repository:** `/home/cory/Work/chiton`.

## 1. Requirement and delivery contract

Build a professional, futuristic sensor console with larger measurements, faster selected-point updates, an optional Kalman estimate, useful statistics, and a history graph. Develop and visually review it entirely locally: no physical camera or live-system access is available. The same page must later consume the existing camera APIs through an adapter, without redesigning the UI or inventing backend endpoints.

Visual reference reviewed: `/home/cory/Pictures/portfolio/depth-grey.jpg`. The current image occupies most of the page while measurements and settings are crowded into a small scrolling sidebar. Preserve the large sensor view; improve hierarchy, typography, precision, and state visibility. Aim for a **precision instrument console**, not a decorative sci-fi dashboard.

This is the governing implementation plan. It refines [the earlier measurement recommendations](web-measurement-improvements-plan.md), especially by separating API history statistics from locally collected samples. Follow this plan where they differ.

### Required release scope

- New isolated page, responsive dark visual design, local demo and automated screenshots.
- Functioning point/ROI selection, larger readings, local measurement-rate control, robust request lifecycle, optional selected-point Kalman estimate, bounded history graph, baseline delta, and CSV/PNG export.
- Live adapter for existing APIs, validated using a localhost API mock and, when dependencies permit, the repository's SDK-free simulator.
- Existing image WebSocket transport retained, with existing-protocol HTTP fallback.
- Old application left intact and accessible. No default-page cutover until explicit design approval and integration acceptance.

### Not part of the initial release

- No live-camera tests, deployment, Wi-Fi provisioning, firmware upgrades, register writes, illumination-policy experiments, or operating-system changes.
- No new API, persistent measurement WebSocket service, backend acquisition recorder, or API-history contract changes.
- No full-frame Kalman processing. Initial filtering improves the selected-point estimate only; it does not filter images, point clouds, or camera exports. Section 14 defines the follow-up if full-depth filtering is required.
- No framework migration, CDN dependency, external font fetch, fake hardware accuracy claims, or imitation controls with no implementation.

## 2. Repository facts the implementing agent must retain

Read first: `docs/measurements.md`, `docs/http-contracts.md`, `docs/image-streaming.md`, `tests/SAFETY.md`, and the relevant actual source. The contracts override assumptions in this plan if the repository changes before implementation.

- Modern application: `release/files/app.html`, mirrored in `release/app.html`. Root `app.html` is an older interface. `bin/` contains tracked/generated deployment copies; do not edit them opportunistically.
- `src/system/resources.cpp` resolves assets with explicit overrides and installed-resource precedence. Use `CHITON_ASSET_DIR` for simulator tests so an older installation cannot mask checkout edits.
- **Static assets must initially be flat files.** The current server in `src/webserver/webserver.cpp` serves recognized root-level filenames, not arbitrary asset subdirectories. Use `/console.css` and `/console-model.js`, not `/console/assets/...`. JS/CSS/SVG/JPG/JSON are recognized; do not assume font formats are served.
- CMake installs `release/files/`; `tools/package_aarch64.py` copies it and links bundle `app.html` to `files/app.html`. Inspect packaging tests before adding promotion logic. Do not modify `app.html` or bundle aliases in the preview phases.
- `release/files/image-stream.js` exposes `ChitonImageStream` and diagnostics helpers. Reuse its bounded JPEG decode/credit/fallback behavior. Do not load the old application inline controller into the new page; that would start duplicate polls and bind nonexistent controls.
- Current `load_depth()` waits 1000 ms after successful responses. Image target FPS is independent. Preserve this independence, not the 1-Hz limit, in the new controller.
- `/get/value/client/<id>/...` returns coherent processed-pixel measurements. Point histories retain up to 60 distinct frames observed by the server for that client. ROIs are spatial statistics; their `value` is the center pixel. Rectangle end coordinates are exclusive; planar rectangles are unsupported.
- `stddev` is in sample units. Legacy `stdd` is multiplied by 1000. `diff` is max-minus-min. The current invalid-sample policy includes every uint16 value; do not silently discard zero or 65535.
- Units are `mm`, `depth_sensor_units`, or `ir_intensity_units`; calibrated units have an AOI scope, not a guarantee that background pixels are physical distances. Time is host frame receipt, not hardware exposure.
- Frame IDs are process-local and restart from one. Measurement and JPEG endpoints can represent different frames; do not claim a numeric value necessarily belongs to the currently displayed JPEG.
- Device-global settings are not viewer-local settings. Several settings have mutation routes but no standalone read route; never synchronize the UI by writing presumed defaults.

## 3. Product and visual specification

### Layout

Desktop, approximately 1280 px and wider:

```text
┌────────────────────────────────────────────────────────────────────┐
│ compact logo / CHITON     DEPTH CONSOLE       source / state / gear │
├─────────────────────────────────────────────┬──────────────────────┤
│ Depth | IR      Grayscale / Color / 3D      │ MEASUREMENT          │
│ Point | ROI                 Fit / Fullscreen│ Selected X/Y         │
│                                             │ 1,174 mm             │
│             SENSOR VIEW                     │ Measured depth       │
│                                             │ Kalman estimate      │
│                                             │ Mean / Std dev       │
│                                             │ Min / Max / Range    │
│                                             │ Count / window       │
├─────────────────────────────────────────────┤                      │
│ HISTORY       Measured —   Estimated —       │ Filter / reference   │
│ [time plot, pause, window, CSV]              │                      │
├─────────────────────────────────────────────┴──────────────────────┤
│ Image FPS / Measurement Hz / Age / CPU / Vixel / connection status  │
└────────────────────────────────────────────────────────────────────┘
```

Use a 56–64 px header, 300–360 px inspector, 16–24 px spacing, and a 150–200 px history area. Treat these as starting dimensions, not hard-coded constraints. The image must retain its aspect ratio without cropping; unused viewport space is preferable to distorted data. At 1280×720, prioritize the viewport and primary measurements; collapse secondary details or history instead of shrinking all text. Avoid page-plus-sidebar scrollbars. Allow ordinary page scrolling at small sizes and high zoom rather than clipping content to a fixed viewport.

At tablet widths, use a narrower inspector or stack it below the image. Below approximately 900 px, stack viewport → primary readings → statistics/history. Essential measurements must not disappear inside a mobile navigation menu. Settings open in a labeled modal/drawer with appropriate focus behavior.

### Visual tokens

Define CSS variables once: background `#0B1118`, panels `#131D29`, dividers `#293849`, primary text `#E8EFF6`, secondary text `#9AAABD`, accent `#48D4E8`, warning amber, fault red. Check actual text contrast before acceptance; the palette is a starting point. Use restrained radii (8–12 px), subtle separation, and minimal shadows. No animated background, glow-heavy borders, arbitrary gauges, or decorative motion.

Use a system sans-serif stack and tabular numbers; optional system monospace for metadata. Primary value 44–56 px (32 px minimum in compact mode), supporting numbers 22–28 px, labels/status approximately 16 px. Provide persisted Normal/Large/Presentation sizing. Do not persist source selection or device settings as if they were device truth. A light theme can follow later; initial acceptance is a polished dark theme, not two unfinished themes.

Use real buttons, labels, input controls, and a definition-list/table structure for readings, not navigation links. Consistent inline SVG icons should support labels rather than replace them. Keep existing branding compact; do not introduce a new company mark. Values can be selected/copied. Use em dashes before data arrives and explicit units; no temperature placeholders for statistics.

### Viewport and inspector

- High-contrast reticle with a small sensor-coordinate badge; restrained ROI outline. Optional overlays can be hidden.
- Show current representation, depth/IR state, calibration/smoothing/plugin metadata, and source label.
- Rename STDD → **Std dev**, Diff → **Range**. Remove ambiguous Dist or relabel **Radius (px)** if retaining its radial-image-position meaning. Angle is optional details, rounded and labeled radians/degrees explicitly.
- Use a depth/color legend only when the representation's mapping is known. The current grayscale path is not automatically a linear calibrated-depth map: show “grayscale intensity” or no quantitative legend rather than inventing a millimeter scale. Color-depth range can use acknowledged `/range/setting` data with correct units/scope.
- Keep measured depth and Kalman estimate distinct. When off, say “Filter off”; do not show a second identical number as if it were improved data.
- Point mode identifies temporal observed-frame statistics. ROI mode says **Center pixel** for value and **Spatial ROI statistics** for mean/deviation/range; hide the point-only estimator.
- Keep detailed pipeline telemetry and device configuration collapsed. Status should stay legible rather than becoming a wall of diagnostics.

## 4. Local-first architecture and proposed files

Use plain HTML/CSS/JavaScript without a build step. Put deployment files flat in `release/files/`:

| Proposed file | Responsibility |
| --- | --- |
| `console.html` | Semantic shell, local scripts/styles, default demo entry |
| `console.css` | Tokens, responsive layout, components, size modes |
| `console-model.js` | Pure validation, context identities, bounded rings, statistics, formatting |
| `console-scheduler.js` | Single-flight requests, deadlines, cadence, abort/backoff |
| `console-kalman.js` | Pure estimator with documented parameters and reset logic |
| `console-demo.js` | Seeded numeric scene, demo frame/measurement source, scenarios |
| `console-live.js` | Existing API adapter, image-stream bridge, metadata normalization |
| `console-view.js` | DOM updates, viewport mapping, persistent graph rendering |
| `console-app.js` | Bootstrap/state/controller and user actions |

Names can be consolidated if modules stay small and testable. Use classic deferred scripts with one namespace (for example `window.ChitonConsole`), and CommonJS exports or Node VM loading for pure helper tests. No dependency on the legacy DOM, jQuery, or Bootstrap. Reuse existing `three.min.js` only for the optional 3D view and existing `image-stream.js` for the live transport. Do not modify these shared libraries merely for visual styling.

Additional tooling/tests:

- `tools/preview_console.py`: loopback-only static preview server, prints full URL, no camera startup, no API forwarding, no non-loopback access. Optional photo-reference fixture support must remain local.
- `tools/capture_console.js`: disposable headless Chromium/CDP capture runner; reuse the repository's existing browser-launch patterns rather than adding Playwright solely for screenshots.
- `tests/console_model.js`, `tests/console_scheduler.js`, `tests/console_kalman.js`: deterministic helper tests.
- `tests/console_source_contracts.js`: same adapter contract exercised against demo and localhost API fixture.
- `tests/console_browser.js`: actual-page interaction, rendering, screenshots, lifecycle, and local API integration tests.
- `docs/console-preview.md`: implemented commands, architecture, scenario list, review decisions, and remaining limitations.

Keep generated screenshots/reports under an ignored `_build/console-review/` or a user-specified output directory, not mixed into release assets. Commit only a small approved visual baseline set if needed. Never reference `/home/cory/...` in deployed HTML. The supplied screenshot may be used as a local visual reference, but do not crop its old UI into the new page or pretend its pixels are measured demo data.

### Source boundary

Both sources implement the same explicit contract, conceptually:

```text
startFrames({onFrame, onStatus, onError, signal}) -> disposable handle
readMeasurement({clientId, selection, signal}) -> normalized snapshot
readStatus({signal}) -> acknowledged metadata/settings with unknowns explicit
applySetting({name, value, signal}) -> acknowledged result
exportFrame({format, signal}) -> blob + filename + source metadata
stop() -> cancel/dispose all owned work
```

The scheduler/controller calls `readMeasurement`; the adapter must not start another measurement polling loop. Frame transport may own its existing bounded render queue, but do not add a second unbounded decode/paint queue around it. Only API adapters and explicit source operations perform network I/O; rendering/statistics/filter code receives plain data.

Normalize measurement fields without changing meaning: `frameId`, `capturedAtMs`, `frameAgeMs`, `revision`, `frameType`, `units`, `unitsScope`, `statisticsKind`, `value`, API aggregate fields, sample count, history bounds/reset, and processed-state metadata. Keep source provenance. Optional absent fields become unavailable, not zeros; required malformed fields reject the whole update. Validate finite numeric values, supported modes, safe integer IDs/counts, dimensions, and known metadata categories. Treat frame IDs beyond JS safe integers as unsupported unless an end-to-end integer-string contract is introduced; do not silently round them.

## 5. Demo operation and review scenarios

### Entry/source safety

Initially `/console.html` defaults to `?source=demo`. Show **DEMO · SYNTHETIC DATA** persistently, including presentation/fullscreen layouts. `?source=live` selects the same-origin API adapter explicitly. Unknown source values fail closed with an error. Switching source cancels all work, generates a fresh client identity, clears contextual state, and requires a deliberate user action/reload. Never fall back from failed live data to demo values.

The demo adapter does not call device APIs, including on startup or setting changes. Device-wide settings in demo update simulated state and say “Simulated”; production-only functions remain unavailable. Do not add arbitrary device-host URLs or CORS workarounds. The localhost mock test of the live adapter is separate from this default demo mode.

### Deterministic numeric source

Generate a real synthetic 640×480 uint16 depth field with annular coverage and distinct surfaces/edges, then render the corresponding grayscale/color image. A seed plus frame index determines values; sensor time advances independently of HTTP reads. Point values, ROI stats, history, and image metadata must derive from the same numeric scene implementation, not unrelated random text. If rendering and measurement use different frame times, record their actual IDs rather than forcing them to appear synchronized.

Use an injected monotonic clock and seeded PRNG for tests; interactive mode advances at a nominal 30-Hz source cadence. No request-count-based frame IDs. Freeze/seek to a documented time/frame for screenshots, including deterministic history pre-roll. Do not create synthetic HTTP histories of 60 samples at startup. Use a fresh per-source/per-tab client identity and implement existing point history bounds/reset/deduplication in the fixture so adapter contract tests are meaningful. Demo all-uint16 statistics stay compatible; the synthetic geometry mask is not a new physical sensor-validity rule.

Provide selectable scenarios:

1. Stable surface with seeded stationary noise.
2. Single-frame spikes.
3. Sudden sustained depth step/occluding object.
4. Gradual movement.
5. Slow acquisition (for example 5 Hz) versus faster polling.
6. Delayed/failed measurement requests independent of healthy images.
7. Failed image transport with healthy measurements.
8. Frozen source frame with successful repeated responses.
9. Disconnect/reconnect and process-style frame-ID restart.
10. Depth → IR, calibrated → sensor units, smoothing metadata changes.
11. ROI selection, empty/startup/warming states, and large/full-range numbers.
12. Panorama aspect ratio and unavailable/unmapped selection behavior.

Fault injection must honor AbortSignal and expose active request counts. Keep test controls behind a clearly labeled Demo panel and optionally query parameters; production mode must not show fake hardware health controls.

## 6. Measurement collection and lifecycle

### Scheduler

- Default point target **20 updates/s**, options 5/10/20/30. Image target defaults to 30 and is separate. For ROI use an independent default target of 5 Hz; do not repeatedly compute a full-frame ROI at point cadence.
- Use `performance.now()` for intervals. Schedule start-to-start: `max(0, targetInterval - elapsedRequestMs)`. At most one measurement request in flight; slow requests reduce actual rate, with no catch-up burst or queued obsolete work.
- Cover fetch headers, body, and parsing with a bounded request deadline. Start with 2 seconds for measurements/status, retaining the existing image transport's deadlines. Inject timers/clock so tests do not sleep.
- Retry only observational reads: 500/1000/2000/4000/8000-ms capped backoff after failures, returning to the configured cadence on success. No setting mutation auto-retry. No tight loop when images fail.
- Use selection/source/context generations so canceled old responses cannot mutate the UI, estimator, history, or settings. Pointer release outside the canvas and pointer cancellation must not leave collection paused.
- Hide/navigation/pagehide stops HTTP reads, sockets, graph work, and timers. Resume starts fresh reads; no backlog replay. Dispose image bitmaps, object URLs, listeners, and WebGL resources when replaced/unmounted.
- Successful duplicate frame responses can refresh transport-health information but must not append data, run the estimator, or increment new-sample Hz. Ignore out-of-order results within a session. A confirmed lower sequence after reconnect/source restart resets local session state. See restart limits below.

### State and reset boundaries

Maintain separate state slices for source, device acknowledgment, selected sensor coordinates/ROI, image transport, measurement transport, API aggregates, estimator, local history, reference, and preferences. Update only after validation; coalesce visual DOM writes to the next RAF.

Reset local observed history, estimator, and baseline on selection/source/frame-type/units/mapping/configuration-revision changes, confirmed restart, or server `history_reset`. Include mode and representation context; do not mix panorama and sensor coordinates. Filter parameter changes reset the estimator and filtered comparison window, **not** the API's measured statistics. Measurement-rate changes reschedule safely; retain measured history but show its actual elapsed span. A long outage starts a new estimator/comparison segment and leaves a visible graph gap.

Process-restart detection cannot be perfect with existing process-local frame IDs: a fast restart could advance beyond the last observed ID. Use explicit reconnect boundaries, regressions, metadata consistency, and history reset; document the residual limitation rather than claiming an existing session UUID. Adding a server process ID is a follow-up API improvement, not a required initial endpoint change.

### Truthful status

Show independent image and measurement states: Connecting, Live, Warming up, Paused, Delayed/Stale, Disconnected, Capture disabled. Derive frame freshness from API `frame_age_ms` plus monotonic elapsed browser time, not only last successful fetch. This is estimated host-receipt age and excludes network transit; label it accordingly. Unknown age is unknown. Separate transport request rate from distinct-frame Hz; show the latter prominently over a rolling 2-second window. Suggested stale threshold is `max(500 ms, 3 × configured measurement interval)`; make state transitions deterministic/tested. Do not infer a physical sensor fault from slow Wi-Fi alone.

History and API means can differ sharply from the current value after a scene change; show their scope/count/time span instead of implying they are simultaneous independent measurements. No invented precision/quality percentage.

## 7. Statistics, filter, graph, and export contracts

### Keep two statistical scopes explicit

**API statistics:** show returned mean/stddev/min/max/range/count, kind, and the actual first/last timestamp span when provided. Retain the existing 60-distinct-observed-frame contract. Display its warming state and never relabel it a fixed two-second window.

**Browser-observed statistics:** median, MAD, P05/P95, and measured-versus-estimated variation are computed from validated distinct samples received by this page. Use a trailing **2-second window capped at 60 samples**, separately labeled. A response can fail after the server sampled it, so local and API histories are not guaranteed identical. Do not combine their counts or compute a variance-reduction percentage using windows from different scopes.

Use sorted copies for median/quantiles, document linear-interpolated quantiles, and raw MAD (not silently scaled). No samples → unavailable; one sample → zero observed deviation but “warming up.” Start “ready” at at least 10 samples and one second of observed span, explicitly a UI heuristic rather than a metrology guarantee. Avoid adding low-value RMS fields to the main panel; optional details can expose existing `rms_about_zero` and `1std_mean` with their documented meaning.

### Selected-point Kalman estimator

Off by default until evaluated. Only depth point selections, never IR or ROI-spatial means/center pixels without a separately specified feature. Display **Measured (processed)** and **Kalman estimate**; upstream EMA/plugin processing remains visible. Do not change device smoothing automatically.

Scalar random walk:

```text
first valid sample: x = z; P = R
subsequent distinct sample:
    Ppred = P + q × dt
    K = Ppred / (Ppred + R)
    x = x + K × (z - x)
    P = (1 - K) × Ppred
```

`dt` is the positive difference in host frame-receipt timestamps in seconds, not browser response spacing. If timestamps are absent, discontinuous, or invalid, do not silently pretend precision: suspend/reinitialize and label why. Reinitialize after a documented gap (initially two seconds), context changes, and parameter changes. Validate finite bounded parameters, `R > 0`, `q >= 0`; retain float precision internally and do not round the state to display precision.

For the synthetic calibrated-mm scenario, use an explicit testable starting R of 25 mm² (noise σ=5 mm), with Responsive/Balanced/Stable q/R ratios of 50/5/0.5 per second. These are demonstration tuning values, not camera calibration. Live mode defaults Off; enabling requires explicit noise/preset acknowledgment. For uncalibrated units require a unit-specific configuration rather than reusing millimeter variances silently. Save local preferences with their unit/context identity only.

Initial implementation has no rejection gate: isolated spikes are attenuated but sustained steps must remain trackable. Compare step settling lag and stationary variation. Add optional robust innovation handling only as a separate tested change; never reject all genuine object changes as outliers. `sqrt(P)` is model uncertainty, not certified sensor accuracy. Kalman filtering cannot repair calibration bias or multipath.

### History and reference

- One persistent canvas/SVG graph, not a new chart instance each update. Default 10-second display, maximum 30 Hz × 10 seconds = 300 distinct records. Use a bounded ring and drop oldest; a long-lived tab must not grow unbounded.
- Measured/estimated series have both labels and distinguishable line styles. Use real timestamps on the x-axis, gaps across outages/resets, no interpolation across incompatible units, no animated transitions. Retain samples at collection cadence but redraw at at most 20 Hz and only while visible.
- Hold/Resume **history view** does not change device capture or measurement collection; show an unmistakable “History held” label and keep storage bounded. A local **Pause viewer** stops this page's streams and changes no global capture setting. Do not use a generic Pause label for both.
- Capture reference stores the current measured processed value with selection/context/units; Reference delta is current measured minus reference. Invalid reference becomes unavailable after context changes. Keep this distinct from Range.

### Exports

CSV exports a snapshot of this page's bounded received history: frame ID, host receipt timestamp, selection/ROI and revision, units/scope, measured value, optional estimate, local filter parameters/segment, source/demo marker, and optional API aggregate snapshot fields with API count/kind. Do not export only formatted screen strings or fabricate skipped frames. Snapshot before serializing so the download is internally stable. Escape CSV safely and quote string fields; do not permit device-controlled text to become spreadsheet formulas.

PNG exports the current rendered viewport with an explicit option for overlays; demo export visibly identifies the demo source. Raw/OBJ/binary exports remain device outputs, never browser-Kalman-filtered. Preserve MIME/filename/error handling and one-download-per-click behavior. No downloads from error JSON. Cap in-memory export buffers and clean up object URLs.

## 8. Live API adapter and setting policy

Use same-origin routes exactly as they exist. No automatic camera or global-mode mutation on page load. Read acknowledged mode/type/range before painting controls as authoritative; a read failure displays unknown, not an assumed default.

| Operation | Existing route and required semantics |
| --- | --- |
| Point | `/get/value/client/<id>/<x>/<y>`; encoded, 1–64 allowed-character client ID |
| ROI | Same prefix with `<x1>/<y1>/<x2>/<y2>`; normalized exclusive bounds, no empty rectangles, no planar ROI |
| Mode read/write | `/colormode/setting`, `/colormode/setting/<0..6>`; global mode, no mutations for reads |
| Depth/IR | `/depth/setting`, `/depth/setting/<depth|ir>`; true means depth, explicit `frame_type` |
| Range | `/range/setting`, `/range/setting/<min>/<max>`; validate and apply pair atomically |
| JPEG | `ChitonImageStream` at `/stream/camera`; fallback `/image/camera` with its existing frame-header/freshness contract |
| Cloud | `/pointcloud/camera` only in mode 4; existing int16 XYZD layout, no fake metadata prefix |
| Metadata | `/frame/camera`, `/dimension/camera`; use their actual schemas |
| Temperature | `/temp/setting`; independent low-rate read, approximately every 4 seconds |
| Diagnostics | `/stream/metrics`; optional, about once/second, failure never blocks measurements |
| Device processing | `/smoothing/setting/<on|off>`, `/chitoncal/setting/<on|off>`; explicit device-wide actions only |
| JPEG quality | `/quality/setting/<integer>`; bounded input, explicit Apply, no polling this mutation URL |
| Downloads | `/raw/camera`, `/pointcloud_obj/camera`; matching representation/type/status headers required |

Use an exact mode map: 0 SLAM, 1 grayscale, 2 gain, 3 color, 4 point cloud, 5 grayscale panorama, 6 color panorama. Depth versus IR is a separate acquisition setting, not a colormode value. Do not send the old UI's unsupported `-1` no-image option to the server; viewer pause is local. Expose common modes prominently, others under View. Panorama dimensions are 1500×165; sensor-space images are 640×480. Mode 4 preserves cloud behavior without JPEG retries.

The initial visual prototype may stage 3D rendering later, but the live-ready handoff must either implement/test existing cloud rendering or make its absence explicit and retain a link to the legacy 3D view. No functioning-looking 3D button that returns a 404 or silently changes the wrong mode.

Device setting changes use one bounded attempt, pending UI, validated acknowledgment, and rollback/error state. Lost mutation acknowledgments can mean the operation succeeded: mark outcome unknown, re-read observational state where available, and never retry automatically. Several settings (quality, dynamic, smoothing, Chiton calibration) lack a direct generic read-back URL. Use processed-frame metadata only for the state it actually describes, show unknown where appropriate, and never call `/quality/setting` or write a default simply to initialize a form. Explain device-wide impact. Do not add dynamic illumination controls in this scope.

Firmware/network/register/diagnostic archive management remains in the legacy page, labeled Advanced device tools. Do not load its external firmware-list request into the console. Preserve old functionality through that route instead of expanding this redesign into device management.

### Coordinate mapping

Store selection in sensor/representation coordinates, never CSS coordinates. Map PointerEvent positions through the actual image content rectangle, excluding letterbox bars and accounting for fit/zoom/pan and DPR. Clamp points to `[0,width-1] × [0,height-1]`; ROI far edges can equal width/height. Use pointer capture and handle pointercancel. Resizing must not change the selected sensor point. Use numeric X/Y controls and keyboard adjustments as accessible alternatives. Mode changes reset incompatible selections. In cloud mode preserve the last 2D selection only if clearly labeled; do not pretend a 3D click is mapped to a sensor pixel without implementing that mapping.

The server may draw a device-global preview marker influenced by another viewer. Local overlay/selected-point identity is authoritative for this tab; document the existing shared-marker limitation rather than claiming full visual isolation.

## 9. Preview and review workflow

Implement these commands (they are planned interfaces, not tools that already exist):

```sh
# Terminal 1: synthetic source only; no native binary or device required
python3 tools/preview_console.py --host 127.0.0.1 --port 8765
# Open http://127.0.0.1:8765/console.html?source=demo&scenario=stable&seed=7

# Terminal 2: reproducible screenshots and interaction reports
node tools/capture_console.js --url http://127.0.0.1:8765/console.html \
  --out _build/console-review/iteration-01

# Deterministic helpers and local-only integration
node tests/console_model.js
node tests/console_scheduler.js
node tests/console_kalman.js
node tests/console_source_contracts.js
node tests/console_browser.js

# Existing application must remain intact
node tests/web_ui.js
node tests/web_image_stream.js
node tests/web_performance.js
```

Use Chromium installed on this machine (`/usr/bin/chromium` was found during planning), configurable through `CHITON_CHROMIUM`. Use a disposable profile, blocked external network, loopback targets only, software WebGL when needed, and bounded shutdown cleanup. Browser tests fail with an actionable message if Chromium is missing; do not claim screenshots from a DOM mock. Do not use the operator's personal browser profile. The screenshot tool must wait for fonts/layout, seeded demo pre-roll, and a stable render-ready signal, then freeze the clock and disable cosmetic motion before capture.

Capture a matrix at 1920×1080, 1440×900, **1280×720** (the supplied reference scale), 1024×768, 768×1024, and 390×844; also test 200% zoom/reflow, large text, and presentation mode. Cover stable point, ROI, filter on, settings drawer, stale/disconnected, warming, IR, panorama, and long values. Save PNGs plus a manifest containing source, seed, frame/time, viewport, DPR, browser version, scenario, console errors, failed requests, and test results. For 200% zoom, actually test browser-equivalent reflow, not a visually scaled screenshot.

### Approval gates and iteration

1. **Layout gate:** static shell with deterministic numbers and real synthetic sensor rendering. Present desktop and narrow screenshots; confirm hierarchy, branding, density, and palette. Static placeholder values are permitted only here and visibly labeled mockup.
2. **Interaction gate:** real demo adapter, selection/ROI, drawers, font-size modes, graph, exports, filter, and fault scenarios. Capture the same viewpoints after each iteration so comparisons are meaningful.
3. **Data-contract gate:** identical page in `source=live` against a localhost fixture implementing actual response shapes and the JPEG wire protocol. Verify no demo requests/values leak into live mode.
4. **Integration gate:** SDK-free simulator where available, package/static-asset checks, legacy regressions, independent critic, and final screenshot review. Actual camera qualification remains explicitly pending.
5. **Promotion gate:** user explicitly approves the design and rollout. Until then, keep `/app.html` unchanged and share `/console.html` only.

At each visual gate inspect the rendered images, not just test output. Provide paths and a short change list. Ask the user for review when interactive; if continuing without a response, label the design provisional and do not infer approval. Track decisions and outstanding issues in `docs/console-preview.md`. Prefer bounded targeted revisions over changing the entire style each pass.

## 10. Test strategy without hardware

### Pure tests

- Scheduler: exact configured cadence, slow requests, one in flight, deadline during body parse, backoff cap/reset, abort, hidden/resume, obsolete generations, image failure independence, duplicate/frozen/out-of-order IDs, rate changes, shutdown cleanup.
- Data: unit validation/formatting, API-versus-browser scope, first/last/count spans, quantile/MAD references, chronological eviction, uint16 extremes, constant values, empty and warming states, ROI bounds, representation mapping, CSV escaping and segment resets.
- Kalman: analytic/reference recurrence, correct first sample, variable dt, invalid parameters/timestamps, long gap, duplicate suppression, static noisy series, isolated spike, sustained step and motion. Report stationary deviation and a defined settling metric, not only pass/fail. For the fixed balanced demo case, target filtered steady-state deviation below 70% of measured deviation and 90%-step settling within 1 second at 20 Hz; these are synthetic tuning checks, not hardware promises.

### Adapter/local fixture tests

Do not use the existing browser mock's avatar JPEG/random per-request frame IDs as the only design/measurement evidence. Create or factor a coherent seeded frame fixture for the new tests. Use a single fake acquisition clock for JPEG/raw/metadata/measurements, capture actual snapshot IDs, implement only allowlisted routes, and reject unknown paths/mutations. Include malformed JSON/numbers, missing optional fields, 400/409/503/504, hanging bodies, server restart, capture disabled, mode changes by another client, and failed setting acknowledgments. Never contact a physical address.

In `source=demo`, assert zero device API requests/WebSockets. In `source=live`, assert no demo fallback on failure and no startup mutation. Verify response metadata survives normalization and old payloads with unsupported optional details degrade visibly.

### Actual browser tests

- No uncaught errors, duplicate IDs, missing assets, overflowing/clipped core content, or unintended external requests.
- Correct selection after resizing, high DPR, touch/pointer cancellation, letterboxing, zoom/pan if offered, panorama, and 200% reflow.
- One measurement request in flight, at most one active frame subscription, bounded ring/chart count, hide/resume cleanup and repeated reconnect/source-switch stability. Sample after a multi-minute accelerated-clock run to detect unbounded storage; do not infer real-time CPU behavior from it.
- Keyboard access, visible focus, labeled controls, drawer focus trap/Escape/restore, normal document reading order, reduced-motion support, contrast ≥4.5:1 for normal text and ≥3:1 for large text. Announce state changes, not every 20-Hz value, through live regions.
- Native PNG/CSV downloads, one download/click, correct source labeling, stable filenames, no downloads on HTTP errors, and object-URL cleanup.
- Existing image streaming/polling/cloud regression behavior remains intact. Test both legacy shipped copies with their existing tests; the new console gets its own tests rather than weakening string/DOM assertions to accommodate a different page.

### Optional actual C++ simulator integration

Use the README's `USE_ADITOF_STUB=ON`, headless configuration in an isolated build directory and temporary generated input/configuration files. Use `CHITON_ASSET_DIR="$PWD/release/files"`; bind to unused local ports as supported by the existing server and verify the process is a stub before running routes. Run the new console against localhost with the live adapter and reuse `tests/http_measurements.py`/`tests/http_ui_contracts.py` where applicable. Do not start the real SDK or use an installed camera-owning service. Static repeated stub frames validate API wiring, not realistic optical noise/motion; the seeded fixture covers temporal cases. If unavailable, record that gap and do not claim end-to-end native integration passed.

## 11. Work packages and completion criteria

Implement sequentially; commit coherent milestones if requested by the operator. Keep an evidence log and do not mark incomplete integrations done.

| Package | Work | Completion evidence |
| --- | --- | --- |
| A — inventory/contracts | Read contracts, map old controls to keep/move/legacy, confirm flat assets and package behavior | Feature/API checklist; no runtime edits yet |
| B — visual shell | New HTML/CSS, seeded sensor visual, responsive panel/header/history shell | Layout-gate screenshots at desktop/720p/mobile; old page unchanged |
| C — model/demo | Source interface, deterministic fields/scenarios, context and state store | Pure/source tests; no-device-network assertion; coherent point/ROI numbers |
| D — collection | Independent rate controller, cancellation, health/freshness and bounded histories | Scheduler tests/fault scenarios; visible actual Hz and age |
| E — analytics | Kalman, explicit scopes, robust stats, graph, baseline and CSV/PNG | Reference tests, step/noise report, native download evidence |
| F — live adapter | Existing routes/stream bridge/settings/exports; cloud implemented or explicit legacy fallback | Local API fixture/browser evidence; no default writes/new endpoints |
| G — polish/review | Accessibility, responsive/zoom, screenshot iterations, loading/error states | User-review-ready artifacts and issue log; approval recorded or pending |
| H — integration/handoff | Packaging/static route tests, legacy regressions, optional stub, docs, critic | Exact commands/results, changed-file list, limitations and rollback steps |

The endpoint adapter should make switching sources a configuration choice, but **do not call it simple until normalized schemas, selection mapping, global mode changes, frame provenance, exports, and error behavior are tested**. Those are where misleading measurements most often appear.

Initial done criteria: polished reviewable local demo; tested existing-API adapter; core workflows usable at all required sizes; correct numeric/source semantics; no old-page regression; bounded lifecycle/storage; reproducible screenshots and commands; user acceptance clearly recorded or still pending. “Design ready for review,” “local integration tested,” and “hardware qualified” are separate statuses.

## 12. Packaging, cutover, and rollback

During development only add the flat console assets and tooling/tests/docs. Verify existing CMake/package copy logic includes all runtime files and that their URLs are served by the actual C++ static handler; a Python static server alone will hide unsupported URL layouts. No need to duplicate the whole new HTML into `release/` merely to mirror legacy structure unless a specific packaging path requires it. Add a package manifest/static-reference test for missing JS/CSS.

Before optional promotion, inspect Make/CMake/AArch64 packaging paths and create one canonical console entry source. If `/app.html` becomes the console later, preserve a working legacy entry and all of its assets, update root/bundle entry references deliberately, and prevent duplicate-controller loading. Do not leave two independently edited console copies. Switching back must require only reopening the legacy URL or restoring the prior bundle, not undoing API/data-schema migrations (none are planned initially).

No deployment is authorized by completing this plan. Live activation and on-camera rate/optical/filter qualification are a subsequent operator-controlled task. A production entry should explicitly select live source; never ship a default demo dashboard that can be mistaken for the camera. Maintain unmistakable demo labeling even after promotion.

## 13. Independent review and evidence handoff

After nontrivial implementation and focused validation, follow the repository/global instruction to launch one fresh-context **critic**. Supply this requirement/plan, repository cwd, exact diff including untracked files, relevant data contracts, screenshot artifacts, and actual test evidence. Reviewer must not edit files or launch agents. Evaluate findings against current code, apply only evidence-backed in-scope fixes, and re-run affected tests; re-review only the fix blast radius if necessary. Report critic infrastructure failure honestly. This checkpoint is for implementation, not a claim that this planning document has been independently reviewed.

Final implementing-agent response should include:

- Local preview command and URL, source/scenario selection, screenshot/report paths.
- What changed, what remains legacy, and whether the user approved the design.
- Tests actually run with outcomes; separate mock, browser, C++ stub, and hardware evidence.
- Filter/statistics scope, demo tuning, and known restart/frame-provenance limitations.
- Confirmation that no live device was contacted and `/app.html` was not cut over without approval.

## 14. Explicit follow-up: whole-depth filtering and true acquisition collection

If improving actual incoming full-frame depth remains required beyond the selected-point display, treat it as a second project, not a checkbox that pretends browser filtering already does it:

1. Define an opt-in **None / Existing EMA / Kalman** depth-processing policy; do not stack EMA/Kalman silently. Preserve calibration sampling and IR behavior.
2. Decide calibration/plugin/filter ordering and which consumers use pre/post-filter samples. Current grayscale/planar/preview paths do not necessarily read the same buffer as measurements/color/cloud output. Specify each consumer and export before coding.
3. Implement a pure C++ per-pixel filter helper, float estimates/variances plus initialization state, initialized by acquisition context and updated once per acquired frame. Two 640×480 float arrays are about 2.34 MiB before masks/outputs. Bound all allocations, saturate/round only at uint16 publication, and preserve unfiltered comparison data where needed.
4. Extend frame/export metadata with explicit filter policy/configuration/reset state and source provenance. Preserve the existing API history/units/invalid-sample policy unless separately versioned. Provide a real process/session ID if revising frame identity.
5. Unit/sanitizer/stub tests can validate math, reset behavior, publication coherence, layout and ownership. They cannot qualify optical validity, moving-edge ghosts, actual i.MX8 processing budget, or sustained device throughput.
6. If every acquired frame must contribute to point history regardless of browser polling, add an explicitly bounded capture-time subscription/recorder contract. Existing API histories are observed-frame histories, not a recorder. Do not add blocking waits to the current serial HTTP measurement route or relabel browser samples as all acquired samples.
7. Leave hardware acceptance open until on-device stationary noise, step response, occlusion behavior, units/AOI, memory/temperature, acquisition/JPEG/display FPS, and rollback are measured. Do not enable full-frame filtering by default based only on local previews.

## Planning-time evidence

Inspected the current page, screenshot, server/static/resource code, API settings/measurement implementations, image transport, packaging paths, and browser-test infrastructure. Node v26.7.0, Python 3.14.7, and `/usr/bin/chromium` were present when checked. Earlier baseline `node tests/web_ui.js` and `node tests/web_streams.js` passed while preparing the measurement plan. No new UI, preview server, screenshots, filter, or integration tests have been implemented or executed by this planning task.
