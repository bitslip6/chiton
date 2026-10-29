# Plan: faster, clearer point measurements and optional Kalman filtering

Status: recommendations only; no runtime changes. Based on the repository inspected for this request, not measurements on the physical camera.

**Implementation handoff:** use [Console redesign implementation plan](console-redesign-implementation-plan.md) as the governing plan for the combined redesign, local preview/review workflow, and measurement system. This document remains supporting analysis; the handoff takes precedence where scope or details differ.

## 1. Current application and constraints

The relevant modern page is `release/files/app.html`, mirrored in `release/app.html`. They are currently identical. The root `app.html` is an older, different interface; changing only that file would miss the bundled application. Asset lookup in `src/system/resources.cpp` prefers `files/`, permits explicit resource/asset overrides, and can find installed assets before checkout assets. Confirm the actual deployed asset location before testing. `tools/package_aarch64.py` packages `release/files/`.

Verified implementation:

- `release/files/app.html::load_depth()` calls `/get/value/client/<tab-id>/<x>/<y>`, or the same prefix with rectangle bounds. Its successful polling interval is hard-coded to **1000 ms**. `run_stream()` adds that delay after completion for non-image reads, so actual frequency is slightly below 1 Hz.
- Image rendering has an independent 30-FPS target and already uses bounded JPEG WebSocket streaming with HTTP fallback. Changing Target FPS does **not** accelerate measurements. Do not rewrite the working image transport to fix point polling.
- The depth stream already has one in-flight request, cancellation, timeout, exponential failure backoff, stale-response protection, and hidden-page pause. Preserve these protections. `tests/web_streams.js` explicitly asserts the present 1-Hz behavior; those assertions must change deliberately.
- `src/main.cpp::value_fn()` reads coherent processed pixels under the camera lock. Point requests with a client ID accumulate at most **60 distinct observed frames**, independently for each tab. Repeated frame IDs do not add samples. Rectangles report spatial statistics of one frame, not temporal noise. Rectangle `value` is its center pixel, not its mean.
- Current displayed Value is the acknowledged processed pixel, not a browser-smoothed estimate. Mean, population standard deviation, and Diff come from the API. Diff means **maximum minus minimum**, not change since the last frame. The page uses `stddev`; legacy API `stdd` is 1000 times that quantity and must not be substituted.
- The API already returns min/max, sample count, history timestamps, RMS about zero, and a one-standard-deviation trimmed mean that the main numeric display does not expose. Units can be calibrated millimeters, uncalibrated sensor units, or IR intensity units. All uint16 values currently participate; there is no per-pixel physical-validity mask.
- Existing on-device Smoothing is an exponential moving average, not a Kalman filter: approximately 0.2 new-sample weight without Chiton calibration and 0.3 with calibration, over configured runlines (`src/graphics/calibration.cpp`). Avoid silently stacking filters.
- Measurements live inside compact navigation rows with fixed-width labels. Mean/STDD/Diff initially show misleading `60°` placeholders. The optional `draw_spark()` is disabled and creates a new Chart object each time; do not enable it unchanged.

Contracts to preserve: `docs/measurements.md`, `docs/http-contracts.md`, and `docs/image-streaming.md`. Raw exports already mean processed samples, not untouched SDK data. Settings can be device-global, while browser display filtering should remain local.

## 2. Recommended delivery order

| Phase | Change | Recommendation |
| --- | --- | --- |
| 1 | Independent, faster measurement polling; larger readout; reveal existing fields | First release, minimal backend risk |
| 2 | Local selected-point Kalman estimate and comparative graph | Opt-in, clearly labeled; does not change images/exports |
| 3 | Robust statistics and optional persistent measurement subscription | Add only after profiling/usefulness checks |
| 4 | Full-depth-image Kalman processing | Separate opt-in backend project with hardware qualification |

Do not conflate filtering the selected readout with improving the entire depth frame. Phase 2 improves the estimate shown for one selected pixel. Phase 4 is required if filtered depth must feed image rendering, point clouds, or downstream consumers.

## 3. Increase useful measurement updates

### Initial implementation

1. Add a separate **Measurement updates/s** control, default **20 Hz**, with 5/10/20/30 options. Keep Image target FPS separate. These are targets, not guarantees; capture, HTTP service time, and network latency limit actual updates.
2. Retain the existing `run_stream` lifecycle. For measurements, budget the interval from request start: `delay = max(0, 1000 / targetHz - elapsedMs)`. Do not change temperature polling or all non-frame streams incidentally. Slow responses must not generate catch-up bursts.
3. Limit each measurement stream to one request in flight. Keep bounded failure backoff independent of the success interval. Image failure must never accelerate measurements or produce a 5-ms retry loop.
4. On selection, rate, unit, mode, or processing-context changes, invalidate obsolete requests and restart safely. On hide/navigation, cancel timers and requests. On resume, request the current sample rather than replaying a backlog.
5. Use `frame_id` to distinguish successful HTTP responses from genuinely new samples. Do not update filters, history, or graphs twice for the same frame. Track decreasing IDs/restarts; preferably add a process/session identifier to metadata so restarts are unambiguous even when IDs have already advanced past the last observed value.
6. Coalesce display changes through `requestAnimationFrame`; paint only the newest acknowledged state. A 20-Hz data stream does not require rebuilding the whole sidebar 20 times a second.
7. Show **distinct measurement Hz**, request latency, host-receipt age, and skipped frame IDs separately from image/display FPS. Skipped IDs mean unobserved frames, not necessarily network loss. Successful repeated reads of a frozen frame must not look like fresh sensor data.

The point history currently covers roughly 60 seconds at 1 Hz, but only three seconds at 20 Hz. Initially preserve its API semantics and display the actual count and timestamp span. Subsequently offer a documented time-based window, such as **2 seconds capped at 60 samples**, with a context reset when window settings change. Do not silently present old and new window statistics as equivalent. Keep the browser graph's window separately explicit.

### Backend work only if profiling justifies it

Point responses are small, but normal HTTP service is sequential and measurements acquire camera ownership. Profile point latency and capture throughput with one and several viewers; benchmark large ROIs separately. Full-frame spatial statistics under the camera lock should not automatically run at point cadence. Start large ROI updates at 5 Hz and expose their own measured rate; copy an owned ROI under lock and compute outside it if profiling identifies lock contention.

Do **not** add blocking `after=` waits to the current measurement route on the serial listener. That route does not currently support the frame-endpoint freshness query. Waiting there can delay settings and exports.

If polling cannot meet requirements, add a versioned measurement subscription to a bounded reactor, borrowing the existing image stream's lifecycle rather than placing a long-lived request on the HTTP worker. Each subscription needs bounded rate/selection, a selection revision, per-client state, admission limits, latest-only delivery, and slow-viewer handling. Capture-time sampling can then observe frames independently of browser rendering, but this is a new contract, not the current observed-frame history. Keep subscription sampling separate from slow-client delivery and preserve HTTP fallback.

## 4. Kalman filtering

### Phase 2: selected-point display estimate

Add a local filter selector **Off / Kalman**; keep Off as the compatibility default during qualification. Show **Measured depth** and **Kalman estimate** together, not one silently replacing the other. “Measured” here is the API's processed pixel and may already include device smoothing/plugin edits. Clearly mark any existing upstream smoothing.

Start with a scalar random-walk model for a mostly stationary selected pixel:

```text
z = new measured depth
Δt = elapsed host frame-receipt time, in seconds
P_pred = P + q × Δt
K = P_pred / (P_pred + R)
x = x + K × (z - x)
P = (1 - K) × P_pred
```

Here `x` is estimated depth, `P` is model error variance, `q` is process variance per second, and `R` is measurement variance. Use floating-point internal state. Initialize `x` from the first sample rather than zero and initialize `P` to a documented finite positive variance. Reject non-finite/invalid parameters; require positive bounded R and nonnegative bounded q.

Operational requirements:

- Update only on distinct frames using their timestamps, not response arrival spacing. Handle nonpositive intervals and long gaps explicitly; reset after a documented outage instead of integrating an arbitrarily large interval. Monotonic receipt metadata would improve resilience to wall-clock changes.
- Reset on point/context change, units, depth/IR transition, calibration/plugin/filter configuration changes, server history reset, or process restart. Disable the depth estimator in IR mode. Start with point-only filtering; ROI center pixels and spatial means must not be silently treated as equivalent inputs.
- Offer **Responsive / Balanced / Stable** presets plus an advanced variance panel. Tune their values from recorded sensor data in the declared units. Do not use identical numeric variance defaults for millimeters and raw sensor units without calibration.
- Estimate measurement noise from stationary captures or a bounded robust stationary estimator. Do not blindly set R to the rolling temporal variance: movement increases that variance and could cause the filter to ignore real movement. ROI spatial deviation is not a substitute for temporal sensor noise.
- Test isolated spikes separately from genuine depth steps/occlusions. Innovation-based spike handling is optional, but repeated consistent changes must trigger increased responsiveness/reinitialization rather than permanent rejection. A fixed pixel can abruptly observe a different object.
- Preserve measured-history statistics. If displaying filtered standard deviation, label it separately and use the same source-frame window for comparison. `sqrt(P)` is **model-estimated uncertainty**, not calibrated sensor accuracy or a guaranteed confidence interval.
- Do not automatically reject zero or 65535. The present physical-validity contract supplies no such rule. Any future validity policy must be explicit, versioned, and separately reported.

A depth/velocity model can be evaluated later for moving targets; it adds assumptions and tuning and should not be the initial default. A smoother readout does not correct calibration bias, multipath, or systematic ToF error.

### Phase 4: full-frame depth filtering

If the requirement includes the actual depth stream, add an explicit depth-processing policy **None / Existing EMA / Kalman**. Do not silently run EMA and Kalman in series. Preserve existing defaults until the new policy is qualified; bypass the new policy for IR and calibration sampling.

Maintain per-pixel float estimate and variance, plus initialization state. At 640×480, two float arrays require about **2.34 MiB**, excluding masks and output buffers. Initialize/reset by frame context, not by viewer requests. Run once per acquired frame, independent of client count. Retain floating precision between frames; saturate/round only when publishing the existing uint16 format.

Define ordering explicitly. A candidate is calibrated, unsmoothed processing → existing plugin → Kalman output, so the filter sees final published-unit samples while the plugin retains its previous input semantics. Review plugin requirements before choosing this order. Calibration sampling must remain unaffected. Preserve an owned pre-Kalman comparison output or introduce a distinct filtered representation before changing any established export semantics.

Audit every consumer: color rendering, grayscale/planar paths, point clouds, measurements, preview, and exports do not all necessarily read the same buffer. Publish metadata such as filter kind, parameter/configuration revision, and reset state; never label an unfiltered image as filtered. Document what existing `smoothed` means after the change. Treat calibrated AOI/background explicitly rather than assigning distance validity to every pixel.

Budget and measure the additional processing time on i.MX8 with the real SDK, plugin, image mode, and JPEG load. Do not promise 30 FPS based on simulator performance. Record stationary noise reduction, moving-edge trailing, depth-step settling time, memory, temperature, and capture/display throughput. Keep immediate rollback to the existing policy.

## 5. Statistics worth displaying

### Useful now: no new statistical API required

- **Measured value and units**, selected sensor X/Y, plus the separately labeled Kalman estimate when enabled.
- **Mean, standard deviation (σ), min/max, and range (max − min)**. Rename STDD to “Std dev” and Diff to “Range.” Point statistics describe observed frames; ROI statistics describe spatial pixels.
- **Sample count and actual window duration**, including “warming up” until a useful window has accumulated. One sample's zero deviation is not evidence of a noise-free sensor.
- **Measurement Hz and frame age**, including explicit stale/disconnected status and frame ID. Retain the last good value but visibly mark it stale; never replace failures with zero.
- Put existing `rms_about_zero` and `1std_mean` in an optional details area. RMS about zero is usually dominated by absolute distance, and the one-σ mean is not a robust outlier estimator. Neither deserves primary-screen space by default.

### New calculations worth adding next

- **Median and median absolute deviation (MAD)**: resistant to occasional spikes; show raw MAD or explicitly label any normal-distribution-scaled equivalent. Add to bounded point history first. For ROIs, assess the cost and choose an exact uint16 histogram/selection algorithm outside camera ownership if needed.
- **P05/P95 interval**: easier to interpret than extrema dominated by one spike. Specify quantile interpolation and minimum sample behavior; use the same point/ROI scope as other statistics.
- **Noise before/after filtering** over aligned frame windows, accompanied by step-response lag. Suppressed variation alone does not prove improved accuracy.
- **Reference delta**: let the user capture a baseline and display current-minus-reference, clearly distinct from range. Baselines must reset or become invalid across unit/context changes.
- **Spike/rejection count** only if explicit detection is enabled; distinguish that from physical validity. Valid-pixel percentage and SNR should wait for documented sensor validity/amplitude evidence rather than being invented from depth values.

Use a single persistent graph with measured/filtered traces, a time axis, and a bounded 5–10-second ring buffer. Update at 10–20 Hz without animation and pause offscreen. Reuse a Chart instance or draw a simple canvas graph; do not reinstantiate a chart each update. Offer CSV export with timestamps, frame IDs, units, context, filter settings, and clear separation of measured/estimated values.

## 6. Make the data readable

Move measurements out of navigation links into a dedicated responsive **Measurement panel**, adjacent to the image on wide screens and directly below it on narrow screens. Leave infrequent settings and detailed pipeline telemetry collapsed by default.

Suggested hierarchy:

```text
Selected pixel: 320, 240       Depth · mm · Kalman: Balanced
MEASURED             KALMAN ESTIMATE
1234 mm              1232 mm
Mean       Std dev       Min–Max       Range
1233 mm    2.4 mm        1228–1238     10 mm
40 samples · 1.95 s window · 20.0 new samples/s · Live
[ measured / filtered time trace ]
```

Numbers above are illustrative, not observed data.

- Primary numbers: **40–56 px** on desktop, responsive minimum around **32 px**.
- Secondary statistics: **22–28 px**; labels, units, and status: **16–18 px**.
- Add a persistent **Normal / Large / Presentation** text-size preference. Presentation mode emphasizes image, primary numbers, and quality summary.
- Use tabular numerals, stable field widths, high contrast in both themes, generous spacing, and visible units. Avoid implying millimeter accuracy through unnecessary decimal digits; let users select display precision where useful.
- Use em dashes before data arrives; remove the temperature placeholders. Use text/icons as well as color for stale/warming/filter state. Permit selection/copying of numeric text despite the current global `user-select: none`.
- Support keyboard-accessible point selection/numeric coordinates and properly labeled controls. Do not announce every 20-Hz value through screen-reader live regions; announce state changes selectively.
- Verify phone/tablet widths and 200% browser zoom. Scaling the canvas/layout must preserve sensor-coordinate mapping and ROI bounds.

## 7. Implementation map and acceptance checks

Likely files:

- Both shipped `release/.../app.html` copies: polling control, panel, labels, local estimator integration, context handling. Prefer a small shared measurement JS/CSS module to avoid extending the large inline script; update package/build asset inclusion and add a parity check for duplicated pages.
- `include/measurement.h`, `src/data.cpp`, `src/main.cpp`: robust statistics/time-window API additions only when required. Preserve legacy fields and client bounds.
- Full-frame phase: `src/graphics/calibration.cpp`, `include/pipeline_kernels.h`, camera pipeline/publication metadata, and all affected consumers. Add a new independently testable filter helper rather than embedding math in the HTTP handler.
- Update measurement/HTTP contracts and user-facing controls together.

Acceptance criteria:

1. Deterministic scheduler tests prove configured cadence, start-to-start budgeting, no overlapping requests/catch-up burst, bounded retry, selection cancellation, pause/resume, and independence from image failures. Count distinct frames separately from requests.
2. Unit tests cover Kalman reference math, first sample, constant/noisy input, duplicate/out-of-order frames, timestamp gaps, restart/context resets, parameter bounds, uint16 extrema, and real steps versus isolated spikes. Comparison statistics use aligned samples and never overwrite measured-history statistics.
3. Extend native/HTTP measurement tests for new statistics and any window fields: points versus ROIs, all-unit contexts, existing compatibility fields, client isolation, and bounded storage.
4. Browser tests cover both shipped pages, mobile and desktop layouts, 200% zoom, dark/light themes, readable units, graph lifetime, correct scaled selection, downloads, and stale-state visibility.
5. Measure on target with one and multiple viewers: new measurement Hz, p50/p95 response/data age, CPU/memory, camera/JPEG/display FPS, ROI cost, filter noise reduction, and step lag. Initial healthy-LAN goal: approximately 20 distinct point updates/s when acquisition permits, with p95 host-receipt-to-display age below 150 ms. These are proposed qualification targets, not demonstrated performance. If slower, show the truth and reduce the rate rather than queueing data.

Baseline validation performed while planning: `node tests/web_ui.js` and `node tests/web_streams.js` passed; the two release UI copies compared identical. No camera performance, optical quality, or new filter implementation was tested. This is a documentation-only plan, so no implementation critic checkpoint was required.
