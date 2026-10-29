# Implementation plan: display-first console, toggleable controls, 30 Hz cloud refresh, live default

**Status: plan only.** The current request authorizes this plan, not implementation, a cross-build, device access or deployment. No implementation/review agents were launched. Repository: `/home/cory/Work/chiton`.

## 1. Confirmed scope and requested outcome

The operator confirmed **only the new `/console` system** (`/console.html`), not the legacy `/app.html` application, and added **Live as the default source**.

1. A prominent main display occupying nearly the full available window/display area.
2. Display settings in a pleasant popup/overlay that can be toggled, rather than permanently consuming display space.
3. Preserve the console's measurement sidebar and all its functionality. Although originally described as the left sidebar, the console's existing `#measurement` inspector is on the **right**; retain that placement rather than relocating or removing it.
4. Increase the maximum point-cloud display rate to **30 FPS**.
5. Opening the console without a source parameter selects **Live**, using existing same-origin APIs. Synthetic Demo remains available only through explicit selection.

The page-selection question is resolved. Leave `release/files/app.html`, its left sidebar, legacy copies and default-page routing unchanged. Do not merge the two interfaces or interpret a live-source default as authorization to promote the console to the site's homepage.

**Meaning of full display:** the display fills the usable main region **beside the retained sidebar**. It does not cover or replace the sidebar. Ordinary browsing should not require the browser Fullscreen API to reclaim the display area.

## 2. Current findings and baseline to preserve

Current console evidence:

- `.sensor-panel` has a full-window minimum height, but `.viewport` only has a 180 px minimum. Toolbar, navigation, appearance, clipping/status/help and selection controls remain normal-flow siblings and compete for that height.
- Opening clipping/help adds more page height or shrinks the actual canvas. A full-height *panel* is not a full-height *display*.
- `_build/pointcloud-clipping/screenshots/matched/intersection-perspective-plain.png` shows this at 1280×720: the canvas is only about 180 px high while control sections dominate.
- `#cloud-fullscreen-controls` currently receives navigation, appearance and clipping only after entering `#viewport` fullscreen. It is a partial overlay solution, not the normal-window layout.
- A separate `#settings` dialog contains viewer rates, text preferences, device-wide writes, exports and source/demo settings.
- `console-app.js` currently defaults a missing/empty source query to `demo`. Change that default to `live` while preserving explicit `source=demo`, invalid-source rejection and the same-origin source boundary.
- Rendering is already on-demand and capped at 30 Hz in `console-cloud-view.js`. The cloud refresh selector offers only 1/2/5/10 updates/s, default 2. `Scheduler.restart()` already permits up to 30.
- Measurements, JPEGs, cloud reads and local rendering have separate cadences. Cloud updates are full signed XYZD snapshots, not cheap animation frames.

The usable baseline includes the existing uncommitted/untracked clipping, GPU color, point appearance, framing/EDL and required board-script packaging work. Preserve the entire working tree, not HEAD alone. Do not reset, clean, stash away user changes or make an unapproved commit. Before implementation record status, patches, relevant untracked copies/hashes and matched captures under ignored `_build/display-first-ui/baseline/`.

Read before implementing:

- `release/files/console.html`, `console.css`, `console-app.js`, `console-view.js`.
- `release/files/console-cloud.js`, `console-cloud-view.js`, `console-cloud-material.js`, `console-scheduler.js`, `console-live.js`, `console-demo.js`.
- `docs/console-pointcloud-clipping.md`, `docs/console-framing-edl.md`, `docs/pointcloud-point-style-gpu-color-checkpoint.md`.
- `docs/measurements.md`, `docs/http-contracts.md`, `tests/SAFETY.md`.
- Existing layout/browser/lifecycle/scheduler/capture tests and `tools/capture_console.js`.

## 3. Proposed layout

### Normal desktop/tablet layout

```text
┌────────────────────────────────────────────────────┬──────────────────┐
│ Main display: full available width and height       │ Measurement      │
│                                        [Controls]  │ sidebar retained │
│                               [Pause] [Fullscreen] │ on the right     │
│                                                    │                  │
│              image or point cloud                  │ Selection /      │
│                                                    │ statistics /     │
│                                                    │ existing tools   │
│ Compact source / stale / clipping / count status    │ Independent      │
│                                                    │ scroll if needed │
└────────────────────────────────────────────────────┴──────────────────┘
```

Retain the console inspector's existing right-side placement and responsive reading order.

- Allocate the main display region approximately one dynamic viewport height (`100dvh` with a `100vh` fallback), less only unavoidable outer spacing. Remove permanent multirow display-control strips from normal flow.
- Make the actual canvas host fill that region, not merely its surrounding panel. Use explicit grid/flex sizing and `min-width:0` / `min-height:0` where needed.
- Preserve sidebar content and width policy, readable measurements, selection/history/Kalman functionality and independent scrolling. Do not crowd display controls into the retained sidebar as a workaround.
- Keep history and detailed telemetry accessible without subtracting from the first-screen canvas height. For the console, they may remain below the first-screen viewer shell; preserve responsive reading order and all actions. Do not remove them or silently replace them with placeholders.
- Images retain aspect-correct containment, including panorama letterboxing; “fill the display region” is not permission to crop/stretch image data. Cloud camera aspect follows the host.
- Do not automatically refit/zoom the cloud to make a screenshot look larger. The geometry, pose, pan anchor and size reference distance remain independent of UI panel visibility.

### Always available, compact actions

Use small floating, labeled actions aligned to an edge of the main region:

- **Display controls** toggle (primary entry point; always discoverable).
- **Pause / Resume viewer**.
- **Fullscreen / Exit fullscreen**, with existing user-gesture behavior.

Reset/Fit, projection, acquisition/view selection and download actions remain one toggle away inside the panel. Avoid a new permanently expanded toolbar. No hover-only controls, automatic hiding of keyboard focus, or timer-based dismissal while editing.

### Toggleable display-control overlay

Use one non-modal, anchored panel inside the display/fullscreen root. Prefer an ordinary hidden/visible HTML region controlled by a button; no new framework or external dependency. It overlays pixels rather than allocating another layout row/column.

- Closed by default. Opening/closing does **not** change viewport CSS bounds or drawing-buffer dimensions.
- Open with **Display controls**; close with the same toggle, an explicit Close button, or Escape while focus is in the panel. Restore focus to the opener when appropriate.
- Desktop target width approximately 360–420 CSS px, bounded by available main-region width. Bound height by the visible region with internal vertical scrolling and a sticky title/Close row.
- Do not cover the retained desktop sidebar. No page-wide dark backdrop on desktop: operators should see the effect of local changes and can navigate the uncovered canvas.
- At narrow widths, use the same DOM panel as a bounded sheet/near-full-width overlay. Keep the toggle and Close reachable, including with virtual keyboard, safe areas and 200% reflow. A narrower panel may obscure much of the image while open, but closing immediately restores the large view.
- Do not add outside-click dismissal in the first version: clicking the uncovered canvas should intentionally navigate/select, not both dismiss a panel and accidentally move the camera. Explicit toggle/Close/Escape is sufficient.
- Closing never means Apply or Reset. Preserve draft values, invalid input, selected section and scroll position. Reload may return panel visibility/section to defaults; do not add storage writes.

Use clear expandable sections rather than a single long undifferentiated form:

| Section | Existing controls to preserve |
|---|---|
| View / navigation | View mode, acquisition type, Rotate/Pan, zoom, Fit all/main, API orientation, projection, Reset view; mark acquisition/view mode as device-wide where applicable |
| Point appearance | Circle/square, diameter, sizing, reset appearance, EDL and capability/fallback explanation |
| Clipping / slices | Depth/XYZ enable drafts, integer limits, Apply, bounds seeding, Reset, units/scope, inline error, unapplied-change explanation |
| Refresh / display | Cloud target updates/s up to 30, current JPEG target, text size/precision, overlay/legend choice; distinguish measurement polling from cloud FPS |
| Export | PNG and its overlay option, OBJ/raw with existing eligibility and independent-frame/unclipped warnings |
| Help / advanced | Input/axis semantics, provenance/resource details and an entry to device/source/demo settings |

Keep destructive/device-global operations visibly distinct from viewer-only appearance/clipping/rate controls. Moving an input does not change its apply/confirmation policy. Do not put multiple forms inside one enclosing form.

For the console, move relevant existing viewer/export nodes out of `#settings` rather than cloning them. Retain a separate explicitly labeled device/source settings dialog for the remaining controls. Opening that modal suspends/hides the non-modal panel; on close restore the previous panel/focus state. Do not create stacked focus traps or two concurrently editable copies of a setting.

## 4. Sidebar, responsive and fullscreen policy

- Preserve the right-side console measurement inspector and all its controls. It must remain visible and usable in ordinary desktop layout, regardless of display-panel open/closed state.
- Keep the existing mobile sidebar/disclosure/reflow behavior initially. Do not force a 300+ px permanent sidebar beside a 320 px viewport, and do not remove its content to satisfy a screenshot. Any sidebar behavior change beyond the existing responsive policy requires separate approval.
- Retain one fullscreen root and one instance of every input/ID. Prefer placing the single display-control panel inside `#viewport` permanently so normal and fullscreen layouts share it, replacing the old conditional reparenting paths rather than creating a second control tree.
- Preserve the console's existing explicit viewport-only fullscreen behavior. Leaving fullscreen restores the unchanged sidebar; do not hide it automatically in normal mode or expand the fullscreen root as part of this redesign.
- Audit `readingOrder()`, fullscreen listeners, ResizeObserver behavior, stale/error placement, mode hiding, disabled controls and settings-dialog focus restoration. Opening a panel must never recreate the renderer or cause a fit.

## 5. Truthfulness, accessibility and interaction safeguards

A closed panel must not conceal important display state. Keep compact edge-anchored information outside it:

- Demo/live identity, selected representation, paused/stale/unavailable state and actual cloud frame identity.
- When clipping is applied, **Display clipped**, matching/submitted counts, applied D/XYZ intervals and a readily reachable Reset clipping action. Render intervals compactly with wrapping, not an entire control form. Do not hide active clipping when the panel or visual axes/legend are off.
- Zero matches remain “No records match clipping”; axes/overlays can still be present. Invalid drafts never replace the applied summary.
- The main-fit indicator remains distinguishable from clipping. Clipping context-reset notices remain nonfatal and available even if settings are closed.
- Long help/resource/provenance text belongs inside the panel or below the viewer, not across the canvas. Do not silently truncate critical errors or enabled-axis intervals; use bounded wrapping/scrollable status when necessary.

Use a labeled control region, `aria-controls` and `aria-expanded` on its toggle, native inputs/buttons, explicit focus return, visible focus and sensible DOM/tab order. Non-modal panel does not trap focus. Hidden contents must not remain tabbable. For mobile modal behavior, if chosen instead, use real modal semantics and test its focus boundary rather than adding `aria-modal` to a non-modal region.

Pointer/keyboard contracts:

- Panel scrolling, number/select/slider keys and touch gestures must not orbit/pan/zoom/select the underlying canvas.
- Do not move camera keyboard shortcuts to document/window scope. Preserve navigation only when the canvas owns focus.
- Prevent pointer propagation only on panel/control surfaces; uncovered canvas stays interactive. Avoid a full-viewport transparent click-capturing layer.
- Opening/closing, changing an accordion or following a local help link must not trigger acquisition, device-setting routes, geometry uploads, storage writes or new animation loops.
- While paused/hidden, local edits may update DOM/cached settings but must not resume network work or actual renderer paints.

PNG exports remain canvas-based, never DOM-panel screenshots: they contain the displayed cloud/image and required provenance, **not** the settings panel, popup backdrop or sidebar. Preserve clipped PNG intervals/counts and separate structured capture metadata. Actual browser screenshots may intentionally show the overlay for UI review.

## 6. 30 FPS / refresh change

### Fixed semantics

The console already allows up to 30 local paints/s, but only offers up to 10 cloud reads/s. Make the maximum **selectable cloud target** 30 updates/s, alongside the existing 30 FPS on-demand rendering ceiling. This is not a new backend capture setting or a guarantee of 30 distinct completed frames every second.

Proposed choices: **1, 2, 5, 10, 15, 20, 30**. Retain **2 updates/s as default**: the request changes the maximum, not the startup load. If the operator wants default 30 too, get explicit approval rather than increasing startup traffic silently.

- Validate the selected rate before storing/applying it, including paused/inactive paths. Reject malformed, nonfinite, zero, negative and >30 values without partial state change.
- Keep the existing single-flight start-to-start Scheduler, bounded headers/body/decode handling, cancellation/generation suppression and capped read-error backoff.
- Fast reads at target 30 should start approximately every 33.33 ms. Slower reads naturally lower achieved rate; never queue catch-up jobs, overlap cloud fetches, or add a parallel poller to reach the number.
- Changing a rate while paused/hidden stores it without restarting. Resume/retry/mode roundtrip restores the requested target consistently.
- Preserve duplicate-frame semantics: duplicates may update receipt information, never upload geometry or count as new clouds. Do not repeatedly repaint a frozen cloud just to manufacture 30 FPS.
- Keep measurement/ROI/JPEG/status rates independent. No automatic sensor mode/validity/format/palette/sampling change.
- Audit the dirty-render cap with realistic RAF timestamps (60/120 Hz and jitter): a naïve elapsed-time check can skip an extra refresh and run below the intended ceiling. Correct only a demonstrated cadence defect; retain one pending RAF, no idle loop and no >30 steady-state paint cadence. Capture/export's explicit render remains a separately documented path.
- Retain separate telemetry: **requested cloud target**, **achieved distinct cloud Hz**, **local render FPS**, latency/bytes/age. A renderer draw call counter is not a GPU-completion or acquisition FPS measurement.

### Bandwidth and performance disclosure

At the existing maximum 307,200 records × 8 B = **2,457,600 B/snapshot**:

| Target | Payload rate if every requested response is full-size |
|---|---|
| 2/s | 4.92 MB/s ≈39.3 Mbit/s |
| 10/s | 24.58 MB/s ≈196.6 Mbit/s |
| 20/s | 49.15 MB/s ≈393.2 Mbit/s |
| 30/s | 73.73 MB/s ≈589.8 Mbit/s |

These exclude HTTP overhead and competing traffic. Duplicate full snapshots can consume bytes too. Local clipping does not reduce response bytes, decoded records, submitted vertices or normal upload sizes.

Show concise high-rate guidance near the selector, especially at 20/30: network/device/GPU limits may lower achieved updates; reduce the target if other operations become sluggish. Do not claim the existing serial HTTP server or Wi-Fi can sustain full-cloud 30 Hz. If host-loopback qualification exposes unacceptable measurement/status starvation, record it and stop for disposition rather than introducing a backend concurrency/streaming redesign.

Existing software-WebGL maximum-cloud completed draws are around 35–42+ ms, already beyond a 33.3 ms frame budget in some configurations. A much larger canvas also increases fill/EDL cost. Preserve existing DPR/drawing-buffer caps and representation; do not silently lower quality, decimate, disable EDL or drop records to claim 30 FPS.

## 7. Live-by-default source behavior

- `/console.html` and `/console.html?source=live` select `LiveSource` and make only existing same-origin observational requests. Treat an empty `source=` as the default live source, consistent with the current empty/missing default convention.
- `/console.html?source=demo` explicitly selects `DemoSource`. Keep the source switch discoverable in the panel/settings and clearly identify synthetic data everywhere it is shown/exported.
- An unknown nonempty source still produces the existing invalid-source error and starts no source. Do not silently coerce unknown names to live or demo.
- API absence, disconnects, disabled capture, incompatible mode or unavailable WebGL must show honest unavailable/stale states. Never fall back to synthetic data automatically.
- Default live does **not** enable acquisition, select Depth/3D, change device settings, write browser storage, or alter any startup rate. Follow acknowledged device state; device-global changes retain explicit existing user actions/confirmation.
- Preserve demo-only `prepareCapture()`. A live/default-live session must reject deterministic capture before stopping/mutating the viewer or reading synthetic samples.
- Audit tests, preview links, documentation and capture tooling for bare console URLs. Synthetic examples/automation must request `?source=demo` explicitly; live tests must use loopback fixtures or the verified native stub only. The current capture tool already sets `source=demo` before navigation: retain that guarantee and test it.
- On a static-only preview server, a bare console URL is expected to show live API errors, not a demo. Document `console.html?source=demo` for offline visual review. Do not add a camera address, discovery mechanism or proxy.

## 8. Implementation sequence and file scope

1. **Preserve baseline.** Scope is confirmed as `/console.html`, retaining its measurement inspector. Run existing focused suites; capture actual canvas/sidebar bounds and controls open/closed at matched resolutions.
2. **Display-first shell.** Update HTML/CSS sizing and compact floating actions. Preserve sidebar and below-view content. Prove the canvas, not just its panel, uses the reclaimed area before moving all controls.
3. **Single overlay and state.** Relocate existing control nodes, integrate settings-dialog handoff, add accessible open/close/focus behavior and compact applied-state status. Remove obsolete fullscreen reparent assumptions. Keep business logic untouched except references necessitated by DOM relocation.
4. **30 Hz target and pacing.** Add validated rates through the existing controller/Scheduler seam. Exercise slow/fast/duplicate/error paths and render cadence before browser throughput benchmarking.
5. **Live default.** Update source selection and offline/demo links, add explicit source-default/error/safety tests, and keep all automated execution bound to demo or loopback. No device-global startup writes.
6. **Integration and evidence.** Extend capture tooling, update intentional layout assertions, verify PNG/device exports and native stub/static/package parity. Measure enlarged-viewport and high-rate costs separately.
7. **Review and handoff.** After implementation and focused validation, supply one fresh-context read-only critic with the requirement, exact feature-only diff including untracked files and actual evidence. Parent classifies findings, fixes evidence-backed in-scope issues and reruns affected checks. No concurrent writers are implied by this plan.

Likely console files: `console.html`, `console.css`, `console-app.js`, `console-view.js`; renderer/Scheduler only if a demonstrated rate/pacing seam requires it. Preserve cloud material/packed data/clipping helpers unless a specific necessary change is proven. Tests and capture tooling are in scope. No new runtime dependency is expected.

The latest user-authorized 30 Hz maximum supersedes older 10 Hz maximum requirements in prior console/clipping plans. Update current documentation to reflect that delta, without rewriting historical test/performance evidence or treating default 2 as obsolete.

## 9. Acceptance tests

### Actual display prominence

Test at 1920×1080, 1440×900, 1280×720, 1024×768, 768×1024, 390×844, 320×844 and 640×360/200%-equivalent reflow, with Normal/Large/Presentation and DPR1/2/capped DPR:

- Closed panel: canvas host fills at least **90% of available first-screen main-region height** and all main-region width apart from ≤16 px outer gutters. Target is nearly 100%; sidebar is excluded from the area denominator. Measure real `#viewport`/canvas rects rather than `.sensor-panel` height.
- Open panel and expanded clipping/help: viewport width/height and drawing buffer remain identical to closed state (≤1 CSS px layout tolerance), no refit/pose/reference-distance change.
- Sidebar remains present, readable and usable under its approved desktop/mobile policy. No horizontal page overflow, lost controls, focus clipping or inaccessible close/toggle buttons.
- No unexpected body-scroll jump when opening/closing, switching sections, entering/exiting fullscreen or displaying validation errors. Do not use scroll locking that breaks sidebar access in non-modal mode.
- Updated viewport tests must replace obsolete “toolbar contains these controls” assertions with explicit location/usability/area assertions. Preserve all unrelated source/stale/error/measurement checks; do not simply remove the old tests to make the redesign pass.

### Control and lifecycle behavior

Add a focused browser suite (e.g. `tests/console_display_controls_browser.js`) using the disposable loopback fixture:

- Keyboard/pointer/touch toggle, Close, Escape, focus return, tab order and hidden-panel non-focusability; controls remain usable at reflow/mobile/fullscreen sizes.
- Every existing action appears exactly once with unique IDs/listeners; no clone, stale DOM value, double submission or competing panel state.
- Clipping applied state and invalid draft survive close/open, mode/retry/context policy, pause/hide and fullscreen. Closing does not apply/discard; Reset remains explicit. Context reset still resets both.
- Panel interaction never changes camera/selection accidentally. Compare recorded pose and actual input events for sliders/numeric/select keys and scroll gestures.
- Active clipping/zero-match/source/stale/error truth remains outside the closed panel; detailed forms/help do not consume normal-flow height.
- Settings-modal handoff works in normal and fullscreen modes, including unavailable/retry transitions and lost focus targets.
- Repeated open/close (at least 100 cycles) does not grow listeners, RAF chains, contexts, materials/programs or buffers; geometry upload counts do not change merely from toggling controls.
- No device writes/storage writes/acquisition restarts from panel toggles or rate selection; paused/hidden state has no actual paints/network work.
- Downloaded PNGs remain valid, with exact applied provenance and no UI overlay capture; raw/OBJ/binary independence and measurements/history/Kalman remain unchanged.

### 30 Hz scheduling and throughput

Extend dependency-free `tests/console_cloud_scheduler.js` and existing scheduler/renderer tests:

- All offered rates through 30, fast reads (e.g. 5 ms), reads slower than one interval (e.g. 100/250 ms), jitter, failures/backoff, aborts, rapid rate changes, frozen/duplicate/new IDs, no catch-up or overlap.
- For slow reads assert start-to-start spacing is governed by `max(target interval, completion latency)`, not the old assumption that every target can be achieved. Use tolerances for fractional 33.33/66.67 ms intervals.
- Deterministic RAF timing establishes a capable client can approach 30 on sustained dirty work without exceeding the steady-state cap or leaving idle paints. Do not assert a constant30 on a software GPU.
- Distinct cloud Hz, render FPS and cached counts remain truthful at 30; timing windows/capacities do not silently undercount; counters never scan records on polling.
- Browser tests use a fast small cloud to prove the selected 30 target is really wired through, then max-sized/delayed clouds to prove bounded degradation and independent measurement/status progress.

Performance evidence at ~40k and 307,200 points:

- Hold pose, frame content, style, projection, EDL, DPR and buffer size fixed for rate comparisons; separately report the enlarged-layout cost instead of confounding it with rate changes.
- Compare requested 2/10/20/30, achieved distinct/frame/render rates, actual response bytes, latency median/p95, decode/count/preparation time, GL upload calls/bytes, interaction/paint time and memory/resource peaks. Test clipping off/depth/box/intersection and EDL off/on.
- Include fast, constrained and stalled loopback transfers and a sustained run; count concurrent requests and buffered work. Keep one-flight/no-backlog invariants even when 30 is unattainable.
- Synchronous GPU completion readback is test-only. Keep CPU submission distinct from completed GPU timing. Report browser/software-GPU limits honestly.

### Source-default acceptance

- Against the loopback live fixture, bare `/console.html`, `?source=` and `?source=live` start the live adapter, show live identity and follow acknowledged mode without any device mutations.
- `?source=demo` produces deterministic synthetic data with zero device API requests/WebSockets, including capture automation after the default change.
- Invalid nonempty source values start neither adapter. Missing/failing live APIs show errors/stale state and never instantiate DemoSource.
- Verify `prepareCapture()` rejects bare/default-live sessions before altering state; explicitly selected demo captures continue to pass with correct source/frame/clip provenance.
- Changing the source remains an explicit action with matching labels/links; reload without a source parameter returns to Live, regardless of prior demo use. No source preference is persisted.
- Keep the legacy page and routing byte-for-byte unchanged. Test startup/pause/hide cancellation and no setting-route/storage writes on the new default path.

### Regression/native gates

Retain the console clipping/material/framing/EDL/lifecycle/layout/viewport/capture suites, source/scheduler/model/Kalman suites, `console_assets.py` and legacy `web_ui`, `web_image_stream`, `web_performance` gates. The legacy left-sidebar page remains unchanged.

Follow `tests/SAFETY.md` and `tests/console_native.py` for an isolated SDK-free/headless native build. Verify identity before fixture setup; run the actual native browser/export helpers and named focused CTest registrations with `--no-tests=error`. Verify static/package/install bytes. Do not run a camera-owning/deployed binary. No deployment, boot/default-page promotion, script execution or device smoke test is implied.

## 10. Evidence and completion

Store ignored artifacts under `_build/display-first-ui/{baseline,validation,screenshots,review,handoffs}/`.

Capture matched before/after desktop/mobile/fullscreen scenes with controls closed and open, expanded clipping, invalid drafts, zero-match, stale/unavailable, 2D/panorama and 30 Hz selection. Manifests retain rendered frame/count/range, final pose/style/EDL/DPR, applied clipping/context, requested/achieved rates, panel state and viewport/sidebar rectangles. Inspect actual screenshots, not only numeric layout checks.

Complete only when:

- Only the new console is changed; its measurement sidebar is preserved and the legacy application is untouched.
- The console defaults to same-origin Live with no startup device mutations or synthetic fallback; Demo and deterministic capture require explicit demo selection.
- The actual display prominently fills its available region; toggling controls never shrinks it.
- All existing settings/functions remain accessible through one usable overlay, including full clipping truth and PNG provenance.
- 30 is a real selectable cloud target with bounded single-flight acquisition and the existing on-demand 30 FPS rendering ceiling; defaults and limitations are explicit.
- Focused/regression/native validation and independent implementation review have passed, with accepted findings resolved.
- A concise durable checkpoint records changed files, exact tests, viewport-area evidence, captures, measured rate/bandwidth/performance, review disposition and remaining hardware/browser limits.

No code change, cross-build, deployment, default-page change, commit or push is authorized by this planning document itself.
