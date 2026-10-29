# In-console 3D point-cloud rendering: implementation handoff

**Status:** plan only; 3D is not implemented by this document. **Repository:** `/home/cory/Work/chiton`.

## 1. Requirement and authority

Replace the console's mode-4 legacy-viewer placeholder with an actual interactive point-cloud view inside `/console.html`. First release: depth-colored points, orbit/rotate, pan, zoom, Fit/Reset view, independent cloud updates, PNG screenshots and existing device OBJ export. Preserve the measurement panel, its fixed-width readouts, all 2D modes, and the legacy application.

The operator permits local resources for testing. Build and validate using the synthetic demo, a localhost API fixture, and the verified SDK-free C++ simulator. Do not contact a physical camera, start a real SDK/camera-owning service, deploy, or promote the console to the default page. Continue through local implementation/review without treating an unanswered visual review as approval; stop for unresolved scope/contract problems or the operator's context-window limit (70%). Record a durable checkpoint if stopping.

This plan governs the **incremental 3D feature**, not a rewrite of the console. Retain the source/lifecycle/measurement requirements in [console-redesign-implementation-plan.md](console-redesign-implementation-plan.md), supporting [measurement analysis](web-measurement-improvements-plan.md), and current implementation/evidence in [console-preview.md](console-preview.md). Actual API/source contracts override stale planning assumptions.

### Included

- Native WebGL point rendering using the already bundled Three.js.
- Pointer, touch and keyboard-accessible camera navigation and explicit Fit/Reset controls.
- Independent, bounded cloud fetching; truthful frame/rate/age/point-count status.
- Coherent deterministic synthetic clouds and fault injection.
- Live existing-API cloud adapter, localhost fixture and native-stub integration.
- PNG of the active 3D viewport; unchanged device OBJ and raw export contracts.
- Responsive/fullscreen integration, resource cleanup, automated browser tests and screenshots.

### Excluded

- Mesh reconstruction, surfaces/normals, registration, SLAM changes, measurement rulers, volume calculations, or acquisition recording.
- 3D point picking, inverse projection to sensor coordinates, ROI selection in 3D, or new pixel-index metadata.
- Full-frame/browser cloud Kalman filtering. The existing optional estimate remains one selected depth pixel in the inspector only.
- New endpoints, a cloud WebSocket protocol, compression, backend concurrency/pipeline fixes, or an upgrade of shared Three.js.
- IR intensity used as a physical depth geometry, new sensor validity rules, or hardware performance/accuracy promises.

## 2. Preserve the actual working baseline

**Important handoff hazard:** at planning time, most console runtime files, tests and documentation are untracked working-tree additions; only the CMake test registration is a tracked modification. An agent starting from HEAD alone will not have this console. Supply the entire current working tree or first make an operator-approved baseline commit. Do not `git clean`, reset, or create a HEAD-only worktree and assume the console is present. Preserve unrelated/pre-existing work; no commit/push is requested by this plan.

Read first:

- `docs/measurements.md`, `docs/http-contracts.md`, `docs/image-streaming.md`, `tests/SAFETY.md`.
- `release/files/console-{app,live,demo,view,model,scheduler,kalman}.js`, `console.html`, `console.css`.
- Existing cloud reference in `release/files/app.html`: `read_cloud`, `draw_cloud`, `init_point_cloud`, and its pointer navigation. It is a reference, **not** a controller to load or copy blindly.
- `src/main.cpp::camera_fn`, `frame_response`, `frame_json`; cloud generation in `src/graphics/shapes.cpp` and storage in `src/data.cpp`.
- `tests/console_{fixture,browser,layout,source_contracts,native_browser}.js`, `tests/console_native.py`, `tests/console_assets.py`, `tools/{preview_console.py,capture_console.js,console-browser-driver.js}`.
- Actual cloud assertions in `tests/http_exports.py` and `tests/http_modes.py`.
- Static/resource/package handling in `src/webserver/webserver.cpp`, `src/system/resources.cpp`, `CMakeLists.txt`, `tools/package_aarch64.py`.

Verified current integration gaps:

1. `console-app.js::startFrames()` returns early in mode 4. Status application shows `cloud-message`; point/ROI mouse selection is bypassed there. Measurement reads still run separately.
2. `LiveSource.startFrames()` explicitly treats mode 4 as unsupported and otherwise owns JPEG WebSocket/HTTP fallback. `exportFrame()` already supports raw/OBJ, not a cloud subscription.
3. `DemoSource` supplies a seeded uint16 scene and acquisition clock but no cloud reader. Its current frame source is 2D synthetic rendering.
4. `View.png()` reads the 2D sensor canvas/last image. It must explicitly dispatch to the active representation; otherwise a cloud PNG can save an old image.
5. The localhost fixture currently implements OBJ but **not** `/pointcloud/camera`; it cannot validate live clouds unchanged.
6. Bundled `three.min.js` is **r128**, MIT-licensed; `DynamicDrawUsage` and `setDrawRange` exist. OrbitControls is **not** bundled. Do not assume a modern Three.js API or import nonexistent controls. Existing 2D code must not change libraries for this feature.
7. Runtime assets must remain flat, e.g. `/console-cloud.js`, not `/console/assets/cloud.js`. Asset-copy/install checks must include every added file.

## 3. Wire/data contract: use existing bytes honestly

### Live cloud

`GET /pointcloud/camera` is an observational read requiring acknowledged global mode **4**. It does not select that mode. JPEG reads in cloud mode and cloud/OBJ reads outside cloud mode return 409. Disabled/not-ready capture can return 503. Ordinary HTTP handling is sequential; do not occupy it with long fresh-frame waits.

The body is consecutive **signed int16 X, Y, Z, depth** records, 8 bytes/record, with **no metadata record/prefix**. Coordinates and depth are already produced by the backend; do not reproject live points from a browser pinhole approximation or apply OBJ's `/6000` normalization to these bytes.

Validate before publication:

- Successful HTTP status and `application/octet-stream` content type; reject error JSON as data.
- `X-Chiton-Layout: int16_xyzd`, `X-Chiton-Color-Mode: 4`.
- Frame ID: positive JS-safe integer; revision: nonnegative safe integer. Missing required header must not become `Number(null) === 0`.
- Actual frame type, source/sample units and scope, known invalid-sample policy, and byte order (`little` or `big`). Units/type/representation must be consistent. Header details are in `frame_response` and the measurement contracts.
- `X-Chiton-Width` is **record count**, not 640 sensor pixels; height is **1**. Require record count × 8 to equal the actual body length.
- Bound record count to 640 × 480 and body to **2,457,600 bytes**, matching the legacy viewer's existing bound. Check both declared length and streamed byte count before an oversized allocation. Reject truncated/nonmultiple-of-8 bodies and unsafe numeric headers.
- Zero-length body with count 0/height 1 is a valid **Empty cloud** when the server supplies it, not malformed geometry or a fake disconnection. Inspect current producer if this assumption changes; do not change backend behavior to satisfy a test.
- Validate host timestamp/age when present; preserve unavailable optional metadata as unknown. Use `performance.now()` for browser receipt/elapsed time. Never subtract wall-clock `Date.now()` transport fields from the monotonic clock.

Use `DataView.getInt16(offset, littleEndian)` or an equally explicit endianness-correct path. The raw uint16 export and signed int16 cloud are different contracts: do not reinterpret signed depth as uint16 or claim the cloud supports arbitrary full-uint16 distances losslessly.

### Meaning, display coordinates and color

- Start by rendering **the API XYZ axes unchanged**. Camera navigation changes the view transform, not the data. Do not silently inherit the legacy browser's X mirror; if any view-basis transform is necessary, document/test it with an asymmetric fixture and preserve original coordinates for provenance.
- Label axes X/Y/Z as **API coordinates** until physical handedness/orientation is established. Do not claim a world frame, gravity direction or calibrated accuracy.
- Depth colors use the record's fourth component and an explicitly shown color range. Reuse acknowledged range settings when meaningful; viewer-local changes must be labeled local and must not write device defaults. Show unavailable legend/neutral coloring rather than inventing an unknown range. Clamp color interpolation, not the decoded data.
- Keep all decoded records by default, including zero/negative components. Do not copy `draw_cloud()`'s browser-only `depth < 600` or Y-band exclusions and label those points invalid.
- Backend cloud production itself can omit mapped/input samples and uses legacy coordinate/signed-range behavior. Point count is **returned/rendered records**, not valid sensor percentage. The all-uint16 measurement policy does not prove the cloud contains every sensor pixel or that every coordinate is a valid distance.
- `mm` is scoped to calibration AOI; uncalibrated coordinates are sensor units, not meters. No physical accuracy/scale guarantee is inferred. OBJ continues using `normalized_6000_<source-units>`.

### Depth versus IR

Initial in-console geometry is **depth-only**. If IR is acknowledged, disable entry to 3D with an explanation and require a separate deliberate Depth action; do not silently change acquisition type. If another viewer switches to IR while mode 4 remains selected, cancel cloud work, clear depth geometry, and show “3D geometry unavailable for IR.” Numeric IR measurements can continue. Recheck frame-type headers to reject races even when settings still say depth. Missing acquisition acknowledgment must not be assumed depth. No automatic device-mode repair/retry.

## 4. Architecture and ownership

Prefer one small pure cloud module and one renderer/navigation module, e.g.:

| File | Responsibility |
| --- | --- |
| `release/files/console-cloud.js` | Pure header/body validation, endian decoding, bounds/color helpers, normalized cloud snapshots |
| `release/files/console-cloud-view.js` | Three.js renderer, GPU buffers, camera/navigation, resize/context lifecycle, PNG capture |
| Existing `console-live.js` | Same-origin `readCloud({signal})`; bounded bytes/metadata; no new polling loop |
| Existing `console-demo.js` | `readCloud({signal})` from its seeded scene/clock; fault scenarios |
| Existing `console-app.js` | One cloud Scheduler, active representation transitions, status/controls/cleanup |
| Existing `console-view.js`, HTML/CSS | Active viewport dispatch, stable telemetry, focus/overlays, exports |
| New cloud tests + existing fixture/browser/native/capture tests | Pure, mock, actual-page and SDK-free integration evidence |

Names may vary if seams stay small/testable. Use the current `ChitonConsole` namespace/CommonJS testing pattern. Load the existing local `three.min.js` without importing the old page controller. Keep shared `image-stream.js`/Three.js unchanged.

Conceptual source API:

```text
readCloud({ signal }) -> {
  source, frameId, revision, frameType, units, unitsScope,
  capturedAtMs?, frameAgeMs?, receivedMonoMs,
  recordCount, decoded signed records or owned position/depth arrays
}
```

The source does I/O and normalization; renderer receives validated owned data. Choose/document a single ownership representation rather than retaining redundant full cloud copies. Reuse the controller's existing deadline/cancellation helpers. The controller owns the only cloud poller; do not create another timer inside renderer or adapter.

### Active representation invariant

```text
Acknowledged JPEG-compatible mode -> one existing JPEG transport, zero cloud reads
Mode 4 + depth + renderer ready   -> one cloud reader, no JPEG subscription/polls
Unknown type / IR / WebGL failure -> explanatory state; no cloud geometry reads
Hidden / viewer paused / pagehide -> no HTTP reads, socket, timers or paint work
```

Entering/leaving either path increments a generation, cancels obsolete reads/decodes/RAF and clears incompatible visual state. Resolve acknowledged mode transitions through current setting policy; mode selection still requires explicit device-wide confirmation in live mode. Failed/lost setting acknowledgment is outcome-unknown, one attempt only, followed by observational read-back. Follow another viewer's acknowledged changes without mutations. Do not let the adapter's known mode, controller generation and renderer drift independently.

## 5. Scheduling, freshness and performance limits

### Separate cloud acquisition, display and measurements

- New viewer-local **Cloud updates/s**, default **2**, options **1/2/5/10**. These are requests/targets, not sensor FPS. Keep image target FPS and point/ROI polling independent.
- Use the existing Scheduler: one in flight, start-to-start interval budgeting, bounded complete-body/parser deadline (initially 2 seconds), read-only 500/1000/2000/4000/8000 ms retry, no catch-up bursts.
- Initially read the latest cloud without `after=` waits; duplicate IDs are legal. A long wait on the serial listener can delay measurements/settings. A future zero-wait freshness optimization requires separate tested semantics, not a new endpoint.
- Duplicate successful responses update transport information, not distinct-cloud Hz, GPU uploads or source freshness. Ignore out-of-order results within an established context. Regressions with restart/reconnect evidence reset contextual state; retain the documented fast-restart/session-ID limitation.
- Rendering can run up to 30 local FPS **only while dirty or interacting**. It need not fetch a cloud each paint. No auto-rotation, continuous damping loop, or cloud-driven measurement cadence.
- Hold history remains a measurement-graph function; Pause viewer stops all local work, including 3D. No accidental global capture mutation.

### Truthful 3D status

In cloud mode replace image telemetry with fixed-width **new cloud Hz**, render FPS, returned/rendered point counts, request latency/bytes and estimated host receipt age. Keep measurement Hz/age separate. Render FPS is not distinct cloud Hz or capture FPS. Unknown age stays unknown; frozen successful HTTP responses become stale. Suggested cloud stale threshold: `max(1000 ms, 3 × cloud interval)`; use source age plus browser elapsed time and distinct-ID elapsed time. Preserve last good geometry visibly marked stale on failures, clear it on incompatible contexts, and clear it on a valid empty cloud.

### Bounds/budgets

Worst-case body is 2.46 MB: approximately **4.92 MB/s / 39.3 Mbit/s at 2 Hz**, **12.29 MB/s / 98.3 Mbit/s at 5 Hz**, before HTTP overhead. A 30-Hz full-size cloud would be ~73.7 MB/s / 590 Mbit/s; it is not a reasonable unqualified default. Lower refresh does not fix an oversized/slow single response; make failures explicit rather than queueing work.

Two full-size Float32 position/color arrays occupy about **7.03 MiB**, in addition to ~2.34 MiB wire bytes, GPU buffers, canvas and temporary storage. Allocate bounded reusable buffers, not new BufferGeometry/Float32BufferAttribute/material per response. Set draw range to current count and mark changed attributes for upload once per distinct frame. Handle shrinking/zero counts so old points are not rendered.

First release can render all returned records up to the protocol cap; do not add a hidden quality decimator. If browser profiling requires a point limit, expose a viewer-only deterministic stride with **received vs rendered** counts and explicit spatial-detail tradeoff. It must not reduce transfer-byte claims or alter device exports/statistics. Cap canvas DPR (initially 2) and drawing-buffer dimensions. Record CPU/GPU-facing renderer counters and heap/buffer capacities where available; do not promise an exact GPU-memory measurement from JS.

## 6. Renderer and navigation requirements

- One mounted WebGLRenderer/scene/Points/material/geometry for the active cloud view; no accumulation across updates or transitions. Use r128-compatible dynamic attributes/draw ranges and correct bounding volumes/frustum policy.
- A perspective camera is a reasonable initial choice. Fit to first nonempty contextual bounds; choose finite positive near/far/radius with padding and bounded ratios. Empty/coincident/degenerate bounds must never produce NaN matrices or division by zero.
- **Do not auto-fit every frame**: it would make the scene jump and undo user navigation. Keep the camera stable across ordinary cloud updates; explicit Fit or context reset can reframe. Define Reset view as home orientation plus fit to the current cloud.
- Orbit target/polar angle/radius/pan must be bounded and finite; wheel/pinch cannot invert or lose the cloud indefinitely. Retain camera pose during harmless viewport resize, and update aspect/projection/drawing buffer with CSS size/DPR changes.
- Default controls: primary drag orbit; Shift/Alt-drag or secondary/middle drag pan; wheel zoom; touch one-pointer orbit and two-pointer pan/pinch. Handle pointer capture, pointercancel, release outside viewport, lost capture, and browser context menu intentionally. Do not leave a stuck drag/RAF loop.
- Provide labeled Rotate/Pan navigation mode or equivalent touch controls, Zoom ±, Fit, Reset, and keyboard shortcuts/help. Limit keyboard interception to the focused 3D viewport; arrow keys there navigate the camera, not the hidden 2D sensor canvas.
- Either implement a small testable local navigation controller or vendor a pinned, license-preserved r128-compatible controls file. Do not fetch controls/fonts/CDNs at runtime or silently upgrade Three.js. Document any added dependency's origin/license.
- On WebGL creation failure/context loss, stop cloud fetching/painting, show a helpful unavailable/retry state and retain a legacy link. Do not substitute unrelated JPEG/demo data or automatically mutate mode. Test a bounded explicit restore/retry path. Dispose renderer/geometry/material/controls/listeners/observers on unmount; do not require a fresh WebGL context every frame or retain old contexts across repeated mode switches.

## 7. Inspector, selection and export behavior

### Sensor measurements are separate from 3D navigation

Hide the 2D overlay and disable pointer point/ROI selection in cloud mode. Keep numeric sensor X/Y point selection, explicitly labeled **“Sensor pixel — numeric selection, not a picked 3D point.”** If entering from an ROI, convert to its clamped sensor center as a new point context and reset reference/history/estimator according to existing rules. Disable ROI controls until returning to 2D. No implicit mapping from returned record order to sensor pixel: records can be sparse/reordered and contain no pixel index.

Keep independent point measurements/history and the optional point-only depth estimator. Label that the numeric measurement and cloud snapshot can be different frames. The Kalman estimate must not color, move or replace point-cloud records. Preserve fixed-width measurement slots, tabular numerals, units and no-layout-shift tests; cloud counters need the same treatment.

### PNG and other exports

- PNG must capture the **current active 3D view**, including the current camera pose, not `View.sensor`/the last 2D JPEG. Implement bounded render-then-copy/capture so WebGL's cleared default buffer does not produce a blank download. Prefer an explicit render/copy over permanently enabling costly `preserveDrawingBuffer` merely for screenshots.
- Add provenance to the exported image: source/demo marker, cloud frame ID, units and appropriate stale state; do not imply it matches the current numeric measurement frame. Demo source labeling remains visible regardless of overlay checkbox.
- The overlays option applies to meaningful 3D axes/legend/labels, not a fake 2D reticle. Canvas export dimensions are bounded. Keep MIME/filename, single-click/single-download, error and object-URL cleanup behavior.
- OBJ remains `/pointcloud_obj/camera`, independently sampled device output in its existing normalized-6000 coordinate convention. Preserve current errors and mode requirement; no camera transform, local point limit, recoloring or Kalman is applied to device OBJ/raw bytes.
- Measurement CSV remains browser-received **sensor measurement history**, not a cloud export. Label it accordingly; no fabricated cloud rows or 3D picks.

## 8. Coherent demo and localhost fixture

Extend the existing acquisition clock/seeded scene, not request-count random clouds. Document an explicit **synthetic** projection (e.g. a declared radial/angular mapping derived from synthetic sensor coordinates). Use the same scene values/frame metadata for point and cloud snapshots when sampled at the same acquisition time. This projection is not the camera's optics/calibration. Preserve different actual IDs if views are sampled at different times.

The demo may constrain/project its cloud into signed-int16-compatible geometry with a documented synthetic convention. Do not silently wrap the `long-values` scene's 65535 into signed cloud distance or imply lossless equivalence; show that scenario as unsupported for cloud geometry or provide a separately labeled bounded cloud scene. Depth/IR and calibration/unit context changes reset incompatible buffers/pose/history as specified. Demo mode makes zero device API requests/WebSockets, including when selecting 3D or navigating.

Add cloud cases covering:

- Static seeded nonempty cloud with an asymmetric marker proving orientation; known depth colors.
- Motion/step in scene with stable user camera pose; coherent point/cloud observations.
- Valid empty cloud, single/coincident points, bounded negative/zero coordinates, max-size records.
- Slow acquisition versus faster polling; frozen IDs; independent cloud/measurement failures.
- Delayed header/body, timeout, cancellation, disconnect/restart, incompatible mode/revision/type/unit races.
- WebGL unavailable/lost/restored and repeated 2D ↔ 3D transitions.

Extend `tests/console_fixture.js` with an allowlisted binary `/pointcloud/camera` route and coherent headers/body from the same scene. Include little- and big-endian fixtures, malformed/truncated/oversized/count-mismatch responses and 409/503/504. Keep response/body allocation bounds in the fixture as well. Align fixture OBJ coordinates and `/6000` scaling with its cloud when testing same-frame outputs; do not leave the current standalone placeholder OBJ as the only evidence. Unknown routes/mutations still fail closed. Never send fixture test requests to physical addresses.

## 9. Implementation packages and gates

| Package | Work | Required evidence |
| --- | --- | --- |
| A — inventory | Confirm baseline files, r128 APIs, actual cloud headers/producer bounds and transition behavior | Contract checklist + existing console/browser/layout tests; no backend changes |
| B — pure data | Bounded metadata/byte parsing, endian decode, bounds/color helpers, empty/degenerate policy | Deterministic pure tests with first record preserved, both byte orders, signed extrema, malformed headers/bodies |
| C — renderer/demo | Seeded cloud source, reusable renderer and navigation, local cloud control/telemetry | Real Chromium screenshots, navigation/fit stability, zero device-network assertion |
| D — live integration | Existing route + single cloud Scheduler, mutually exclusive JPEG/cloud lifecycle, acknowledged global-mode following | Localhost fixture tests; cancellation, faults, freshness, no startup writes/retries |
| E — selection/exports | Numeric-only sensor point behavior, fixed-width counters, correct cloud PNG and unchanged OBJ/raw/CSV | Native downloads, selection/context tests and no-shift matrix |
| F — qualification/handoff | Actual C++ static/API stub, packaging/regressions, visual review, independent critic | Exact logs, test counts, PNGs/manifests, issue disposition and documented hardware limits |

At visual gates inspect PNGs, not only passing test output. Share paths and label the design provisional until the user approves it. No default cutover/deployment is authorized by this feature's completion.

## 10. Validation and acceptance

### Pure and scheduler tests

Add focused tests, for example `tests/console_cloud.js` and `tests/console_cloud_scheduler.js` (or extend existing helpers without making them monolithic). Assert:

- Exact XYZD decoding without sentinel skip/sign conversion; lengths 0/8/max, both endiannesses, safe IDs/revision/count, required-vs-optional metadata, MIME/layout/type/unit consistency and streamed-byte cap.
- Finite empty/degenerate bounds, orientation fixture, color clamp/range/unavailable rules, no arbitrary validity filtering; shrinking geometry draw range.
- Cloud cadence independent of image/point cadence, one active cloud read, complete-body deadline, capped backoff, generation suppression, frozen/out-of-order/restart behavior, no catch-up burst.
- Pure camera/nav math boundaries, stable fit/pose across data updates, resize/DPR changes, bounded zoom/pan and cancel state.

### Actual browser tests

Use the existing disposable Chromium driver and loopback-only network restrictions. Add cloud tests rather than replacing old JPEG/legacy tests with weaker assertions. Test:

1. Mode 4 renders real nonblank WebGL points from demo and live fixture; no JPEG subscription/poll in cloud mode and no cloud requests in 2D. Another viewer's acknowledged changes are followed without mode/depth writes.
2. Rotate/pan/zoom/fit/reset with actual input events, touch/pointer cancellation, keyboard focus, responsive resize, fullscreen and 200%-equivalent reflow. Camera pose remains stable across cloud updates; scene bounds do not crop legitimate data.
3. Cloud/measurement frame IDs, Hz, source age, empty/stale/capture-disabled/IR/WebGL states are truthful and independently visible. Presentation/fullscreen preserve DEMO labeling.
4. Repeat at least 50 2D ↔ 3D transitions and pause/hide/resume cycles: at most one active transport/renderer, no growing listeners/RAF chains/geometry/material counts, no stale callbacks. Test explicit WebGL context loss/retry and missing-WebGL fallback. Accelerated clock/resource tests do not establish real hardware performance.
5. All readouts including new point counts/byte/rate/age fields retain fixed dimensions across placeholders, small/large values, decimals, negative values where applicable and font-size modes. Preserve `console_layout.js` coverage.
6. Actual native PNG download contains nonbackground point pixels outside provenance/overlay text and changes with camera pose, uses the correct current representation/frame/provenance, supports overlay behavior and occurs exactly once per click. Error/empty-unavailable cases do not download an old 2D frame. Validate OBJ bytes/units/error handling separately from screenshot pixels and preserve CSV semantics.
7. No uncaught errors, missing assets, accidental external requests, duplicate IDs, clipped core controls, or uncontrolled page overflow. Interactive help/labels/focus work without a mouse; no high-rate screen-reader announcements.

### Screenshots

Extend `tools/capture_console.js`/`prepareCapture()` to freeze the demo cloud **and camera pose** with a render-ready signal. Include source seed/frame, point counts, color range, camera pose, viewport/DPR/browser version, exceptions and failed requests in the manifest.

Capture 1920×1080, 1440×900, 1280×720, 1024×768, 768×1024, 390×844 (also keep existing 320 px layout test), Large/Presentation, and equivalent 200% reflow. Cover default cloud, rotated/panned view, inspector/measurement relationship, empty/stale, IR unavailable, WebGL unavailable and settings. Store generated artifacts under ignored `_build/console-review/3d/`, not runtime assets.

### Commands

These new cloud-test filenames are proposed interfaces to implement, not existing tests:

```sh
# No camera or native binary required for demo review
python3 tools/preview_console.py --host 127.0.0.1 --port 8765
# Extend scenario/entry support, e.g. console.html?source=demo&scenario=cloud&seed=7
node tools/capture_console.js --url http://127.0.0.1:8765/console.html \
  --out _build/console-review/3d/screenshots

node tests/console_cloud.js
node tests/console_cloud_scheduler.js
node tests/console_cloud_browser.js

# Existing contracts and regressions, do not weaken
node tests/console_model.js
node tests/console_scheduler.js
node tests/console_kalman.js
node tests/console_source_contracts.js
node tests/console_browser.js
node tests/console_layout.js
python3 tests/console_assets.py
node tests/web_ui.js
node tests/web_image_stream.js
node tests/web_performance.js
```

Register applicable pure/source cloud tests in the existing **opt-in simulator** CMake release-test block, not hardware tests. Keep Chromium-dependent tests explicit unless deliberately integrating their dependency/failure policy into a release gate.

For native integration, follow `docs/console-preview.md` and extend `tests/console_native.py`/`console_native_browser.js` to exercise actual cloud mode and verify headers/bytes/rendering/static assets:

```sh
cmake -S . -B _build/console-stub -DUSE_ADITOF_STUB=ON -DUSE_SDL2=OFF \
  -DCHITON_DEFAULT_DISPLAY=headless -DCHITON_RELEASE_TESTS=ON \
  -DZLIB_LIBRARY_RELEASE=/usr/lib/libz.so -DI2C_LIBRARY=/usr/lib/libi2c.so
cmake --build _build/console-stub -j4
python3 tests/console_native.py _build/console-stub/chiton
ctest --test-dir _build/console-stub -R 'console_|web_ui|web_performance' \
  --no-tests=error --output-on-failure
cmake --install _build/console-stub --prefix "$PWD/_build/console-install"
```

Native library override paths above are host-specific; rediscover rather than copying them onto another machine. The runner must verify the CMake stub flag and startup stub signature **before** any mode/depth setup. Use temporary config/input/calibration, explicit `CHITON_RESOURCE_DIR`/`CHITON_ASSET_DIR`, command tripwires and process cleanup. Keep inherited plugin/resource isolation protections. Reuse actual export/mode suites via their supported harness if extending native coverage; do not claim a standalone helper with no main routine ran a suite. Preserve `/app.html` byte-for-byte/link target, confirm flat asset routes served by C++ and installed/bundled asset bytes, and use `--no-tests=error` so missing registrations cannot count as success.

### Initial done criteria

- The same console renders/navigates coherent demo and localhost/live-stub clouds using existing APIs.
- No backend/legacy/default-page regressions, fake 3D controls or silent device mutations.
- Bounded bytes, CPU-side/GPU object counts and lifecycle ownership demonstrated; distinct cloud/measurement timing and source semantics remain explicit.
- Active 3D PNG/OBJ/measurement workflows and fixed-width readouts validated at required sizes.
- Exact commands/results, rendered review artifacts, skipped evidence and remaining hardware limitations documented in `docs/console-preview.md` (or a linked 3D evidence document).
- One fresh-context read-only **critic** after nontrivial implementation and focused tests, per global instructions. Supply this requirement/plan, repo/cwd, exact current diff including untracked additions, contracts, screenshots and actual test evidence. Critic must not edit/spawn. Apply only evidence-backed in-scope fixes, rerun affected tests, and review only a fix blast radius when needed. Report reviewer infrastructure failure honestly.
- Clearly separate **implemented/local integration tested**, **design approved**, and **hardware qualified**. No on-device rate or optical promises from software-WebGL/stub results.

## 11. Final agent handoff/report

Report the local demo URL/command, scenario and camera controls; screenshot/manifest/log paths; changed files; actual unit/browser/fixture/native/package test outcomes; critic findings/disposition; explicit fallback behavior; known frame/unit/cloud-selection limitations; and pending design/hardware approvals. Confirm no physical device was contacted and no default-page cutover/deployment happened.

If blocked, leave a checkpoint with changed-file list, failing command/log, renderer/transport ownership state and the narrow question required to proceed. Do not hide a backend contract gap behind a synthetic response or broaden into backend changes without direction.

## Planning-time evidence

This document was prepared by inspecting the current console/fixtures, shared Three.js r128, legacy cloud reader/renderer, actual C++ cloud/OBJ response construction and relevant cloud producer paths. Existing console evidence is recorded in `console-preview.md`. No 3D code was implemented, no new 3D tests/screenshots were run, and no device was contacted for this planning task.
