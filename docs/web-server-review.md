# Web server review and product discussion backlog

## Implementation status

Status key: **[x] Implemented** = the selected fix is complete in source; **[ ] Partial** = completed pieces are listed, but remaining work is still open; **[ ] Open** = not implemented. Completion does not imply hardware validation. New-feature prerequisites do not count as completed features.

Completed fixes: **F04, F05, F06, F07, F10, F11, F12, F13**. Partial fixes: **F02, F09, F14, F16, F19, F20**. **N01–N09 remain proposals.**

## Implementation update — selected correctness fixes

F04, F05/F06, F10, F11, and F12 have now been implemented for the single-user development workflow. F11 provides reference-tested wide statistics, stateless ROIs/legacy points, bounded per-client distinct-frame histories, explicit units/frame metadata, and a private legacy-plugin counter copy that preserves exported measurement pixels. See [measurements.md](measurements.md) for compatibility and sampling contracts. F10 adds bounded, single-flight frame/depth/temperature reads, completion-driven polling, capped retry backoff, stale/disconnected status, and visibility/selection/mode cancellation in both shipped UI copies. Calibration I/O is checked/atomic/versioned with legacy loading; kernels use correct sizes/strides and owned storage; all registered handlers return one centrally serialized response; JSON/status/error paths are consistent; depth/IR semantics and the IR checkbox agree; color limits support atomic paired updates; complete measurement configuration is validated before publication. See `README.md` and `tests/SAFETY.md` for the current contracts and tests.

Verified with the extended ASan/UBSan helper suite, headless simulator HTTP tests (also against a full ASan/UBSan build), mocked UI tests for both deployed HTML copies, complete-page Chromium recovery tests against a local mock sensor, and non-simulator handler syntax checks. Full-application leak checking was disabled for existing module allocations; helper leak checking remained enabled. F20 subsequently passed isolated normal/ASan/UBSan/TSan CMake/CTest gates and both Chromium UI copies; see [release-gates.md](release-gates.md). Hardware behavior remains unvalidated. Security/deployment proposals remain deprioritized for this single-user system; other backlog items remain proposals.

The review findings and runtime observations below are the original review snapshot, **not a claim that those selected bugs remain present**.

## Scope and review baseline

Reviewed the **current working tree**, not just HEAD: `src/webserver/`, registered handlers in `src/main.cpp`, request validation, frame snapshots, resource resolution, relevant system utilities, and the shipped UI in `release/files/app.html`. `bin/files/app.html` currently matches that release UI. The root `app.html` and other app variants are not the primary homepage.

The initial review changed no application code; subsequent approved fixes are tracked above and under each item. Unchecked proposals are not approved for execution. Existing user changes were preserved.

There is already a broader `plan-improvements.md`; parts describe older code. This review accounts for the fixes now present: bounded partial socket writes, checked URL escapes and route arguments, coordinate validation, owned image/raw/point-cloud snapshots, a shared frame/settings mutex, and rejection of live plugin reload. Those should be retained, not reimplemented. In particular, the current `sample2` handler no longer sends the earlier duplicate response.

### Recommended first discussion

1. **Deployment boundary:** Is this strictly a single-user development tool on an isolated network, or a supported appliance accessed by multiple users?
2. **Security blockers:** Protect administrative endpoints, remove credential leakage, and make firmware installation trustworthy before broader deployment.
3. **Correctness/reliability:** Repair response contracts, request parsing, remotely reachable calibration hazards, and viewer recovery.
4. **Product foundation:** Add capabilities/current-state discovery and health reporting, then decide between recording, calibration workflows, and external integrations.

Priorities below: **P0** = deployment/safety blocker; **P1** = existing-product correctness or reliability; **P2** = maintainability/performance. Effort is relative: **S** localized, **M** cross-layer, **L** architectural or hardware-dependent; not a schedule estimate. Findings are source-inspected unless explicitly identified as runtime-confirmed.

## Fixes to the existing product

### [ ] F01 — Protect device administration and limit OS privileges
**P0 · L**

- **Evidence:** The listener binds `INADDR_ANY` (`src/webserver/webserver.cpp:228–230`). No authentication or method authorization precedes route dispatch. Settings include register writes and network changes; software routes include upgrade/restart/debug. Responses advertise wildcard CORS. `release/sudoers:80` grants the Chiton account unrestricted passwordless sudo.
- **Impact:** Anyone able to reach the server can operate sensitive device functions. Wildcard CORS also permits cross-origin reading where browser network policies allow it; removing CORS alone does not protect state-changing requests.
- **Proposal:** Configurable address/port; authenticated viewer/operator/admin roles; POST/PATCH for mutations; CSRF/origin protection for cookie sessions; same-origin by default, explicit origin allowlist for integrations. Use TLS directly or through a documented supported proxy. Replace broad sudo with narrowly scoped privileged operations and an unprivileged server account.
- **Acceptance:** Unauthenticated clients cannot modify settings, write registers, retrieve diagnostics, or install firmware; state-changing GETs are rejected; a compromised web process cannot execute arbitrary privileged commands.
- **Discuss:** Local pairing versus managed accounts/API tokens? Are remote viewers allowed to read sensor data without login?

### [ ] F02 — Keep Wi-Fi credentials out of URLs, logs, and public UI
**P0 · M · Partial; remaining security work deprioritized for single-user development**

- [x] Removed explicit SSID/password logging from the settings handler; server splits encoded path segments before decoding.
- [ ] Credentials still travel in legacy URLs; client encoding, password masking, script/request-log redaction, restrictive storage, and HTTPS/authentication remain.

- **Evidence:** `release/files/app.html:283–291` concatenates SSID/password into a URL without component encoding. `src/webserver/settings.cpp:320–329` explicitly logs both. `release/radio.sh` also logs passwords. Credential files are written with a generic `ofstream` helper, not an explicit restrictive-permission policy. The password input is `type="text"`.
- **Impact:** Plain HTTP traffic, request logs, device logs, and on-screen display expose credentials. Spaces, `/`, `?`, `+`, and filename-like suffixes can break this route even if the client starts encoding components, because the server decodes before splitting.
- **Proposal:** Authenticated HTTPS JSON request; password field with optional reveal; redaction at every logging layer; owner-only secret storage; explicit SSID/passphrase validation including supported open-network behavior. Never return saved secrets.
- **Acceptance:** Special-character SSIDs/passwords work; tests verify secrets are absent from logs, URLs, errors, and diagnostics; credential files have appropriate permissions.
- **Discuss:** Which Wi-Fi security types must provisioning support?

### [ ] F03 — Make firmware updates verified and recoverable
**P0 · L**

- **Evidence:** `src/webserver/software.cpp:48–68` runs a privileged upgrade script synchronously. `release/upgrade.sh:8–9` downloads firmware and its MD5 file over HTTP, then extracts/runs the package and replaces the current release symlink. There is no cryptographic signature or post-boot rollback mechanism. The script itself reboots, potentially before the HTTP caller receives a result.
- **Impact:** An attacker able to alter the download path can replace both package and checksum. Interrupted or incompatible updates can leave devices unusable.
- **Proposal:** TLS with certificate verification, signed release manifest/packages rooted in a device-trusted key, hardware/format compatibility checks, bounded download and extraction, staged activation, and last-known-good rollback. Expose progress through an authenticated job API rather than implying success before reboot.
- **Acceptance:** Altered packages and incompatible releases are refused; failures preserve the running release; simulated interrupted activation recovers; update/reboot state is visible to the UI.
- **Discuss:** Is A/B storage feasible, or should the first release use staged directories and a boot-health-confirmed symlink switch? Should offline signed-package upload be supported?

### [x] F04 — Remove remotely reachable calibration crashes/corruption
**P0 · M · Implemented**

- [x] Shared `CHITON_CALIBRATION_FILE`/`calibration.bin` path; checked full-frame load into temporary storage; atomic sibling-file save with checked flush/close.
- [x] Version/shape/ABI/byte-order/CRC validation, complete legacy-file loading, and failure preservation of active data and the previous file.
- [x] Correct normalized Gaussian kernel allocation/stride, owned storage, border handling, and independent valid near/far weighting.
- **Scope:** no separate rollback archive or guided preview/commit workflow; those remain N05. New saves are not readable by older binaries and retain a native-layout payload.

- **Evidence:** `/load/calibrate` calls `read_calibration("calibration2.bin")` (`src/main.cpp:463–466`). `src/system/utils.cpp:1190–1249` uses `fread`/`fwrite` without checking successful `fopen` or complete transfer. `/smooth/calibrate` requests a radius-one kernel (`src/main.cpp:475–481`); `src/graphics/shapes.cpp:1898–1929` allocates eight floats but writes nine. `apply_kernel` also uses the wrong kernel row stride.
- **Impact:** Missing/unreadable calibration files can cause undefined behavior outside the C++ exception boundary; smoothing can corrupt memory. Authentication does not make these operations safe for legitimate users.
- **Proposal:** Disable unsafe actions until fixed. Check every file operation; load into temporary validated storage; correct kernel shape/stride and ownership. Centralize the calibration path: manual save currently uses `/tmp/calibration.bin`, manual load uses `calibration2.bin`, and startup uses `calibration.bin`. Add version/shape/integrity validation and atomic save with a retained previous version.
- **Acceptance:** Missing, truncated, incompatible, and unwritable files return meaningful errors without damaging active calibration; radius-one/other kernels pass ASan/UBSan; save/load/restart round-trips use the same documented file.
- **Discuss:** Should calibration changes require preview/explicit commit, rather than modifying active coefficients immediately?

### [x] F05 — Guarantee exactly one standards-compliant response
**P1 · M · Implemented**

- [x] Owned `HttpResponse` from every registered handler; one central send with CRLF, accurate lengths/status phrases, connection-close semantics, and explicit unknown/not-ready errors.

- **Evidence:** `src/webserver/webserver.cpp:214–216` advertises length 12 for four-byte `FAIL` bodies; the dispatcher discards handler return codes at line 320. Camera enable/disable, not-ready, unknown, and wall-angle paths can return without sending anything (`src/main.cpp:499–632`). Several handlers construct headers independently using LF and an `OK` reason phrase for all statuses.
- **Runtime-confirmed:** `/missing.css` returns `404 OK`, declared length 12, actual length 4. `/missing/camera` closes with zero response bytes.
- **Proposal:** Turn the existing skeletal `HttpResponse` into a real status/headers/body contract; serialize centrally with CRLF, accurate lengths, and explicit connection semantics. Distinguish completed/streamed responses from errors. Return 404 for unknown routes and a documented 503/not-ready response when capture is unavailable.
- **Acceptance:** Every success/error path produces exactly one response accepted by a normal HTTP client, with the right status and exact body length.
- **Discuss:** Preserve legacy route aliases while moving serialization behind a common interface?

### [x] F06 — Return valid, consistent JSON and propagate failures
**P1 · M · Implemented**

- [x] JSON-library serialization, typed setting results, checked register SDK outcomes, and reported calibration/software/system errors.
- [x] Shared error shape `{"error":"message","status":<HTTP status>}`; active UI JSON calls check HTTP success before accepting results.
- **Scope:** this is the legacy API response repair, not a versioned schema or a comprehensive device-I/O rewrite.

- **Evidence:** `src/webserver/settings.cpp:146–159` produces unquoted `on`/`off`; `generalwrite` adds an extra brace. `src/webserver/software.cpp` uses unquoted keys/single-quoted values. Calibration returns `NA` with JSON content type, including unknown actions. Gateway/register helpers do not consistently propagate operation errors to HTTP.
- **Runtime-confirmed:** `/dynamic/setting/off` returns `{"value":off}`; `/missing/calibrate` returns HTTP 200 with `NA`. Both fail JSON parsing.
- **Proposal:** Use the already vendored JSON serializer; define typed booleans, explicit units, and a common error shape (`code`, safe `message`, optional field). Reject unknown actions; report actual persistence/SDK/OS outcomes rather than unconditional success. Serialize validated values rather than echoing raw numeric text.
- **Acceptance:** All advertised JSON endpoints parse successfully; unknown actions and failed hardware/system calls return the documented error status/body; UI checks `response.ok` before interpreting success.
- **Discuss:** Keep existing response fields for compatibility or introduce a versioned schema?

### [x] F07 — Replace one-read URL guessing with bounded HTTP parsing
**P1 · M/L · Implemented**

- [x] Separate method/path/query handling, per-segment decoding with checked escapes, explicit homepage routing, static HEAD, and rejection of unsupported methods without mutation.
- [x] Bounded incremental framing before dispatch: 16,384 header bytes including terminator, 4,096 bytes per line excluding CRLF, 100 fields, and a 1,000-ms monotonic total deadline starting at receive entry after accept. Poll slices are at most 50 ms for shutdown cancellation; fragments do not renew the deadline.
- [x] Strict CRLF/request-line/header syntax and control checks; HTTP/1.1 requires one nonempty Host. Duplicate Content-Length (even equal), lists/signed/nonnumeric lengths, all Transfer-Encoding, obs-fold, whitespace before colons, and Expect are rejected before handlers. Overflow-sized decimal lengths cannot wrap.
- [x] Deliberate **zero-byte body limit** for all legacy routes: absent or decimal-zero Content-Length is allowed; nonzero lengths return 413 without waiting for/ignoring the body. Coalesced trailing bytes/body/pipelining return 400. No chunked/body-consuming API was introduced. Connection-close behavior and central responses are retained; incomplete EOF/cancel closes without dispatch, timeout returns 408, aggregate oversized headers return 431, other invalid framing returns 400.
- **Verified:** focused ASan/UBSan helpers, Node UI regressions, and both HTTP suites (`http_safety.py`, including new raw framing cases, and F11 `http_measurements.py`) passed against isolated normal, ASan-only, UBSan-only, and combined ASan/UBSan simulator builds. Coverage includes byte-fragmented lines/headers/terminators, separately fragmented rejected bodies, exact/over-limit sizes, malformed framing, trickle deadline, incomplete disconnects, and shutdown while receiving. Rejected/incomplete mutation URLs preserve read-back settings; no handler is reachable without successful framing. See `tests/SAFETY.md` for commands/results.
- **Remaining limits:** one connection at a time, no request bodies/chunked/100-Continue/keep-alive/pipelining, no full semantic validation of arbitrary header values, and no hardware/TSan/CMake validation. Bytes arriving after a complete zero-body request are not another request and are discarded on close; no promise to detect future trailing bytes before dispatch. A small custom framing helper was retained instead of adding a dependency; this is not a general-purpose HTTP server. JSON POST migration/authentication, F08 concurrency/jobs, and F13 UI/download changes remain separate.

- **Evidence (historical):** `src/webserver/webserver.cpp:50–99, 289–321` reads once, decodes the entire request, splits the target on `/` and `?`, and routes by the second part. It does not enforce HTTP methods or read a framed request body. Paths with fewer than two parts become the homepage.
- **Runtime-confirmed (historical):** `/app.html?cache=123` becomes a malformed 404 response; `HEAD /app.html` sends the entire body; `POST /quality/setting/74` is treated like a mutation GET; `/missing` serves the homepage.
- **Proposal:** Evaluate a small maintained HTTP library against embedded constraints before growing the custom parser. Either way, impose header/body size and total-time limits, assemble fragmented requests through the header terminator, parse method/path/query separately, split encoded segments before decoding, and handle bodies using explicit framing. Serve the homepage only at explicit routes; support HEAD/OPTIONS deliberately and 405 otherwise.
- **Acceptance:** Fragmented requests, large headers/bodies, embedded controls, malformed escapes, queries, unsupported methods, and premature disconnects cannot cause misrouting or partial mutation.
- **Discuss:** Footprint/platform constraints that would rule out an HTTP library? Which legacy URL behaviors must remain?

### [ ] F08 — Keep slow clients and administrative work from blocking everyone
**P1 · L**

- **Evidence:** `web_server` accepts and fully processes one connection at a time. Socket reads/writes are now bounded, but upgrades/debug generation call blocking subprocesses (`src/webserver/software.cpp`), and Wi-Fi/gateway actions execute shell commands while holding the camera/settings mutex (`src/webserver/settings.cpp`). `get_command_output_lines` and `execute_system_cmd` have no execution deadline.
- **Impact:** One slow request or hung command stalls other users/status requests; network changes can also stall capture. Socket deadlines do not bound these operations.
- **Proposal:** Bounded connection handling plus a serialized device-command owner/queue; long operations return 202 and job IDs, with deadlines/cancellation and shutdown cleanup. Keep subprocess execution outside frame locks. Retain owned snapshots and memory/queue limits—do not simply run existing handlers in unlimited threads.
- **Acceptance:** A delayed/mock-hung job and slow reader do not prevent health/viewer requests; concurrent mutations remain ordered; shutdown terminates outstanding subprocesses within a documented bound.
- **Discuss:** Required concurrent viewers and latency budget? Which jobs may be cancelled safely?

### [ ] F09 — Make frame retrieval observational and freshness explicit
**P1 · M/L · Partial**

- [x] F11 adds frame IDs, host receipt timestamps, actual processing/unit metadata, and snapshot-bound export headers; frame/measurement requests return 503 for disabled/not-ready capture. Empty JPEGs already return 503.
- [ ] Static/cloud/export mode side effects, unified capture lifecycle, and bounded newer-frame waits remain. Host receipt time is not a hardware exposure timestamp.

- **Evidence:** Serving `app.html` resets global color mode (`src/webserver/webserver.cpp:332–334`). `/pointcloud/camera` switches the global mode to 4; OBJ export temporarily changes/restores it (`src/main.cpp:545–592`). Requests use `frame_request` and a polling wait with a roughly 7.5-second limit. Empty JPEG snapshots can still be sent as HTTP 200. Camera/settings routes expose two different enable/disable mechanisms.
- **Impact:** A new browser or export can change another user's view or the physical preview. Disabled/stalled capture yields ambiguous empty/stale data and delayed failures.
- **Proposal:** Separate acquisition configuration from requested presentation format; never mutate device state on static asset reads. Publish frame IDs/timestamps/readiness and support bounded waits for a frame newer than an ID. Unify capture lifecycle controls and return an explicit unavailable status instead of a successful empty JPEG. Review remaining unlocked reads of non-atomic settings in the frame wait loop when expanding concurrency.
- **Acceptance:** Loading the UI/exporting a cloud does not alter another viewer's mode; stopped capture returns a documented timely response; frame age is discoverable.
- **Discuss:** Is output mode intentionally device-global, or should it be per viewer? If global, make ownership and conflicts visible.

### [x] F10 — Recover the viewer after errors and prevent request pileups
**P1 · M · Implemented**

- [x] Shared live-read wrapper checks HTTP success; validates cloud record alignment/capacity, finite measurement fields, and image decoding. The 10-second deadline covers headers, body, and decoding.
- [x] One in-flight task per frame/depth/temperature stream; readiness cleanup in `finally`; polling/retry scheduling follows completion. Failure backoff grows from 500 ms to an 8-second cap (never faster than a stream's normal interval).
- [x] Per-stream connecting/live/stale/disconnected/paused status and time since the last successful UI update, retaining acknowledged values on failure. This is client receipt age, not sensor capture-age metadata (F09).
- [x] Hidden/page-away views abort and stop reads; resumption restarts them. Mode/selection revisions cancel and discard obsolete results; hidden-canvas selection remains valid in cloud mode. Image object URLs are revoked on success, error, and cancellation.
- [x] Color-mode mutations receive one bounded attempt, without automatic retries; controls recover on failure. Deterministic tests cover both copies; Chromium verifies native JPEG decoding, cloud failures/deadlines, server stop/restart, and visibility transitions using a local mock sensor.

- **Evidence:** Point-cloud requests set `CHITON_READY=false`, but their catch path never restores it (`release/files/app.html:402–472`). Depth and temperature timers schedule the next request independently of completion (`:587–668`). Most calls do not check HTTP status or apply deadlines.
- **Impact:** One failed cloud request can freeze the viewer until reload. Slow responses accumulate polling requests behind the sequential server, and stale measurements look current.
- **Proposal:** Shared API wrapper with `response.ok`, AbortController timeout, validated payloads, readiness reset in `finally`, and completion-driven polling with capped backoff. Pause unnecessary polling in hidden tabs; cancel obsolete mode/selection requests. Show connecting/disconnected/stale state and last update time; never automatically retry a non-idempotent mutation without a defined policy.
- **Acceptance:** Network loss, server restart, invalid/truncated cloud buffers, and delayed replies recover without reloading; at most one request per polling stream is in flight.
- **Discuss:** Keep polling first, then introduce streaming only after the server lifecycle is reliable?

### [x] F11 — Correct measurement semantics and isolate sampling history
**P1 · M · Implemented**

- [x] 64-bit sums, double-precision population deviation/RMS, exact means, filtered counts, and non-destructive min/max; full-frame constant/gradient/checkerboard/random/full-uint16 references pass.
- [x] Legacy points and ROIs are stateless. `/get/value/client/<id>/...` gives each client a chronological history of at most 60 distinct observed frames; selection/context resets happen before sampling. Histories are capped at 32 clients with LRU eviction/five-minute idle expiry; ROI spread is not mislabeled temporal noise.
- [x] Frame IDs/host timestamps/configuration/units in JSON and owned export headers, with an explicit all-uint16 inclusion policy. Image pixels are never stamped with frame numbers; legacy plugins receive their sentinel only on a private copy, preserving ordinary edits and the exported last measurement.
- [x] Both UI copies use per-tab identities, skip duplicate plotted frames, reset context before display/history updates, show units/sample scope, and display actual values without an implicit browser blend. Cloud rendering no longer discards the first real record as metadata.
- [x] Sanitized helper tests, normal/full-ASan-UBSan simulator API tests, UI tests, and complete-page Chromium recovery checks passed. Real hardware/metrology, CMake/CTest, and TSan were not run. Contracts and compatibility are in [measurements.md](measurements.md).

- **Evidence:** `/get/value` uses one static saved point and one shared history (`src/main.cpp:639–755`, `src/data.cpp:24–108`). It calculates results before clearing history for a changed point. History begins with 60 zeros, and `point_range` sorts the ring storage in place. Variance/RMS use 32-bit accumulators (`src/system/utils.cpp:1307–1315`, `src/graphics/images.cpp:262–271`). The final raw/calibrated pixel is overwritten with a frame number (`src/main.cpp:1059–1060`).
- **Impact:** Different clients/points influence each other's reported noise; early samples are biased; ring replacement no longer follows time order. Large ROI statistics can overflow. A nominal raw frame contains a non-measurement pixel.
- **Proposal:** Define raw versus calibrated units and invalid-sample policy; use wide accumulators and reference-tested statistics. Clear/reinitialize before sampling a new point; avoid sorting ring storage. Prefer stateless ROI requests tied to an immutable frame ID, with explicit per-client/time-window sampling for temporal noise. Move frame metadata out of image pixels, preserving any plugin compatibility deliberately.
- **Acceptance:** Known constant/gradient/checkerboard inputs match reference statistics; two clients do not mix histories; full-frame ROI calculations do not overflow; exports document all units and metadata.
- **Discuss:** Are measurements spatial statistics of one frame or temporal statistics across distinct frames? Browser polling frequency must not silently define the sample set.

### [x] F12 — Correct depth/IR mapping and validate settings as a unit
**P1 · M · Implemented**

- [x] DEPTH/IR API semantics and checkbox agree; state read-back, disabled-in-flight control, failure rollback, and distinct simulator-fixture/UI tests.
- [x] Atomic `/range/setting/<min>/<max>` with `0 <= min < max <= 9000`; legacy individual routes retain the same invariant.
- [x] Complete configuration/measurement validation before publication and guarded radians lookup; current/accepted state returned.

- **Evidence:** `src/webserver/settings.cpp:138–143` sets `depth_frame=true` for the string `ir`, while `src/main.cpp:981` interprets true as `FrameDataType::DEPTH`. The current checkbox labeled IR happens to send `depth` when checked, masking the inversion for that UI. Min/max color settings are validated separately, not as a pair. `/radians/setting` assumes calibration IDs 1–8 exist before dereferencing them.
- **Proposal:** Explicit frame-type enum and documented API vocabulary; tests with different IR/depth fixtures. Validate cross-field invariants and complete calibration/configuration data before publishing. Expose atomic settings updates so clients can change dependent values without invalid intermediate state. Return the accepted/current state after changes.
- **Acceptance:** Selecting depth/IR returns the correct fixture; invalid ranges and incomplete measurement IDs are rejected without partial state changes or dereferencing missing entries.
- **Discuss:** Which settings are persistent versus session-only, and which require capture restart?

### [x] F13 — Repair UI/backend contract mismatches and downloads
**P1 · S/M · Implemented for the shipped UI**

- [x] Both shipped UI copies read `inner_dist`/`outer_dist` consistently with the API.
- [x] Unsupported `noise`, `gain`, `offset`, `version`, and `targets` helpers now report unavailable without fetching, polling, accessing absent controls, or rewriting upgrade links. The separate supported firmware action is unchanged.
- [x] Unique `download_png`/`download_pointcloud` IDs, one PNG click listener with placeholder navigation suppressed, and matching PNG label/MIME/`.png` filename in both canvas helpers and both shipped copies.
- [x] `tests/web_downloads.js` verifies disabled handlers, ID/label/link contracts, one download per click, PNG type/name, legacy anchor helper, and old-browser dispatch fallback for both copies. `node tests/web_ui.js` and `node tests/web_stream_browser.js` passed; the latter uses a local mock sensor with external traffic blocked. The new download checks use mocked canvas/DOM, not native browser download capture.
- [x] Removed the nonexistent `software_ver2` fallback. Firmware selection now requires exactly two decimal digits in the existing 32–99 UI range; placeholder/empty/malformed/out-of-range selections display an error and send no request. Valid selections preserve the upgrade route; HTTP/network failures are shown in the UI without retry. `tests/web_firmware.js` covers invalid and missing selections, valid boundaries, and mocked failures in both copies; Node UI and Chromium viewer recovery suites passed again. No actual upgrade was run.
- [x] Server raw/cloud/OBJ exports now have fixed attachment filenames (`chiton-raw.bin`, `chiton-pointcloud.bin`, `chiton-pointcloud.obj`) added after F11 metadata assembly. Live JPEG polling remains without attachment disposition; bytes, layouts, units, GET-only policy and existing mode side effects are unchanged. New `http_exports.py` checks actual file type/content/counts, cloud-to-OBJ coordinates, headers, transient JPEG recovery, HEAD rejection and disabled-capture JSON errors without attachment headers.
- [x] Began the tested contract inventory in [http-contracts.md](http-contracts.md), covering camera exports and selected existing UI contracts. Both HTTP suites passed on isolated normal and combined ASan/UBSan simulator builds; Node UI regressions passed again. Full-app leak checking stays disabled for pre-existing allocations.
- [x] Completed the named visible-control inventory in `tests/ui_contracts.json` and [http-contracts.md](http-contracts.md). Structural tests detect unlisted controls, duplicate IDs/bindings, missing handlers and incorrect links. The two inert calibration fields are explicitly disabled with unavailable titles. Added mocked success/HTTP/network failure tests for remaining active handlers, UI-visible Wi-Fi/diagnostic errors, and actual simulator schema checks.
- [x] Chromium captures actual saved PNG/raw/OBJ files for both shipped copies. PNG bytes match the frozen canvas and decode natively; raw/OBJ bytes match known local mock responses, filenames/single-click counts are checked, and JSON export errors create no downloaded files. Production exports are independently checked over simulator HTTP. Node UI/inventory tests, Chromium recovery/download tests for both copies, and both normal/combined-ASan-UBSan simulator HTTP suites (including F11) passed.
- **Limits:** browser downloads use a local mock, not the simulator; actual diagnostic archives, provisioning and upgrades are not run. Existing F02 credential/security, F09 mode side effects, F15 diagnostic privacy and F20 broader failure/CI coverage remain separate. No authentication/API migration, hardware, F08 or F09 implementation was performed.

- **Evidence:** UI expects dimension fields `inner`/`outer`, but server returns `inner_dist`/`outer_dist` (`release/files/app.html:99–108`, `src/main.cpp:608–618`). Dormant UI helpers reference unsupported setting routes such as `noise`, `gain`, `offset`, `version`, and `targets`; remove them or mark them explicitly unavailable before exposing them. The firmware selector can fall back to absent `software_ver2`. Two download anchors share `dwn_link` and the same JPEG listener is registered twice. The JPEG download path actually serializes PNG data and gives it a `.jpeg` filename.
- **Proposal:** Publish a tested route/schema inventory; remove dead controls/helpers or implement them intentionally; validate version selection. Use unique element IDs, one handler per action, correct MIME/extension, and server-side `Content-Disposition` for binary/OBJ exports.
- **Acceptance:** Every visible control works against the current server; firmware selection fails gracefully; each click produces one correctly named download; dimensional metadata matches the UI.
- **Discuss:** Which development-only controls should disappear from the supported product UI?

### [ ] F14 — Make provisioning non-destructive and error-aware
**P1 · M/L · Partial**

- [x] Checked credential-file writes, surfaced command failures, validated gateway choices, and simulator rejection of host-network changes.
- [ ] Preserve unrelated profiles, verify actual connectivity, use bounded jobs, and implement timed rollback. No live provisioning was tested.

- **Evidence:** `release/radio.sh` deletes all saved NetworkManager connection files during fallback and uses Bash-style tests under a `/bin/sh` shebang. Gateway routing is hard-coded to wlan0, and subprocess failures are discarded (`src/system/utils.cpp:1252–1281`). Wi-Fi file-write/command completion is not enough to establish connection success.
- **Proposal:** Use a dedicated connection profile and NetworkManager API/explicit argv invocation rather than shell pipelines; preserve unrelated profiles; discover supported interfaces; validate gateway choices; track association/DHCP/connectivity status. Stage changes with timed rollback if the device becomes unreachable.
- **Acceptance:** Invalid credentials retain the prior working connection; unrelated profiles remain intact; missing commands/permissions/DHCP failures produce accurate job errors; desktop mode never changes host networking.
- **Discuss:** Required fallback AP/recovery method and how users regain access after an address change?

### [ ] F15 — Make diagnostic bundles useful, private, and bounded
**P1 · M**

- **Evidence:** `make_debug_info` uses fixed shared directories and ignores command failures (`src/webserver/software.cpp:18–33`). Several collectors call `execute_system_cmd`, whose output redirection into the bundle is commented out (`src/system/utils.cpp:1024–1081`). Archives are moved into publicly served `current/files`; logs may already contain Wi-Fi passwords.
- **Proposal:** Authenticated job-generated bundle with actual captured stdout/stderr, a collection manifest, secret redaction, and platform-aware collectors. Store outside the public asset root; use authorized expiring download routes, byte/time limits, retention cleanup, and unique per-job directories.
- **Acceptance:** Downloaded archive contains promised diagnostics plus explicit unavailable sections, contains no test secrets, cannot be fetched anonymously, and expires without unbounded disk growth.
- **Discuss:** Which information is support-essential and what user consent/redaction is required?

### [ ] F16 — Declare capabilities; hide unavailable or placeholder operations
**P1 · M · Partial**

- [x] Simulator Wi-Fi/gateway/upgrade/diagnostic actions return 501 without privileged embedded operations; register requests use the simulated SDK; placeholder rover actions return 501.
- [ ] Capabilities endpoint, capability-driven UI controls/reasons, and explicit unavailable-temperature presentation.

- **Evidence:** Stub builds skip hardware monitoring but registered HTTP administrative actions still attempt embedded system operations. Rover routes are registered (`src/main.cpp:1478`) although `src/webserver/rover.cpp` motor actions only construct `OK`, perform no motor operation, and retain 404 status. Some calibration UI controls have no active event binding.
- **Proposal:** Capabilities endpoint covering simulator/hardware, available outputs, network provisioning, temperature, calibration, and optional plugins. Enforce capability checks server-side and hide/disable unsupported UI controls with reasons. Unimplemented actions should report unsupported, not imply successful behavior.
- **Acceptance:** Simulator requests cannot invoke sudo, reboot, network changes, or real I2C; UI presents simulated/unavailable temperature distinctly; unsupported routes have a consistent response.
- **Discuss:** Remove rover support from the sensor product, or fund a separate safety-reviewed robotics feature? Do not activate motors without a watchdog/dead-man stop and an explicit safety design.

### [ ] F17 — Use safe DOM rendering and make selection accessible
**P1/P2 · M**

- **Evidence:** `add_info` writes message strings into `innerHTML`, including user-controlled values (`release/files/app.html:559–568` and gateway/setting handlers). Canvas selection relies on mouse events and offset calculations; it rounds coordinates without a consistent final clamp. Controls often use spans rather than associated labels.
- **Proposal:** Render messages with `textContent`/DOM nodes, reserving sanitized markup for explicit cases. Use pointer events, pointer capture, `getBoundingClientRect`, separate pixel versus exclusive ROI bounds, keyboard-accessible selection, proper labels/focus, and responsive layout.
- **Acceptance:** HTML entered into fields is displayed literally; selections are accurate at different zoom/layout sizes and edges; touch and keyboard workflows work without triggering invalid requests.
- **Discuss:** Desktop-only engineering tool versus tablet-friendly installation/support UI?

### [ ] F18 — Reduce repeated work without weakening correctness
**P2 · M**

- **Evidence:** Every static request resolves/loads its whole asset (`src/webserver/webserver.cpp:330–345`, `src/system/resources.cpp`). No cache validators/policy are emitted. The cloud viewer allocates new arrays/GPU attributes repeatedly (`release/files/app.html:413–465`). Capture repeatedly creates JPEG compressors even without an image viewer.
- **Proposal:** Bounded immutable asset cache, correct MIME mappings, ETag/conditional responses, versioned asset caching, and explicit no-store/freshness policy for live data. Reuse WebGL typed buffers/draw ranges, dispose on teardown, and consider demand-driven encoding/quality adaptation. Constrain static serving to a deliberate public asset set; do not expose operational `.bin`/diagnostic files via generic fallback.
- **Acceptance:** Measure startup, bytes/sec, CPU, allocation rate, memory, and image age with zero/one/multiple viewers; retain only measured improvements and correct cache invalidation. `send_precompressed_data` must negotiate encoding if activated.
- **Discuss:** What bandwidth and RAM envelope is realistic on supported hardware?

### [ ] F19 — Consolidate web sources and make the API maintainable
**P2 · M/L · Partial**

- [x] Common owned response contract and central serialization replace handwritten handler responses; both deployed UI copies currently match.
- [ ] Canonical UI source/packaging, modular frontend, typed routes, relocated handlers, versioned API, and compatibility policy. Matching copies are not source consolidation.

- **Evidence:** Large inline UI script, multiple HTML variants/copies, similar handwritten response code in every handler, and route logic spread between `main.cpp` and `src/webserver/`. Current route dispatch is driven by positional URL vectors; `HttpResponse` is only a skeleton.
- **Proposal:** One canonical web source tree with explicit packaging into release/bin; split UI into API/state/view modules. Introduce typed route definitions containing method, authorization, capability, request schema, and response schema; move handlers out of application startup code. Begin with a documented `/api/v1` and compatibility adapters, not a wholesale framework rewrite.
- **Acceptance:** One edit/build produces the shipped UI; route contract tests detect frontend/backend drift; clean/build does not silently overwrite local source edits; legacy aliases have a migration policy.
- **Discuss:** Is a build-time frontend toolchain acceptable, or should the shipped UI remain dependency-free/static?

### [ ] F20 — Turn current regressions into release gates
**P1 · M · Partial; desktop gates implemented, hardware qualification awaits approval**

- [x] Required push/PR/manual CI jobs for separate normal/ASan/UBSan/TSan headless simulator builds, 12 CTest entries per variant, and Chromium recovery/native-download checks for both shipped UI copies. Final `Web release gate` rejects failed/skipped/cancelled dependencies; failure logs/CTest evidence are retained. Restored the missing FunctionalPlus submodule mapping for clean checkout.
- [x] Fail-closed isolated `tests/release_gate.py --all`; `mkrelease.sh` requires it before copying the target binary. Missing tools/browsers, nonzero results and sanitizer diagnostics cannot silently pass; runtime tests cannot be registered for hardware builds. Simulator-only SDK faults and forbidden-host-command tripwires cover failure paths without privileged operations.
- [x] Existing framing/JSON/method/query/depth/register/calibration/client/export/download regressions are gates. Added supported modes 0–6 in both directions for IR/depth, actual JPEG dimensions, disable/re-enable failures, UI mutation error/rollback/recovery, SDK failure/recovery and calibration file-save/load failures.
- [x] The new mode-zero ASan gate exposed an out-of-bounds SLAM metadata write. Both allocations now use the existing correctly sized allocator; focused regressions also cover negative spike neighbours, invalid geometry/buckets and the last-pixel RGB channel boundary. No mode was excluded to make a gate pass.
- [x] **Local verification passed:** the complete gate, all 12/12 CTest entries in each normal/ASan/UBSan/TSan build, and both Chromium recovery/native-download runs. No sanitizer suppressions or allowed-to-fail TSan job. Full-app leak checking remains disabled for existing allocations; helper leak checking is enabled.
- [ ] Owner-approved hardware/support matrix and isolated target qualification. The manual signoff checklist is in [release-gates.md](release-gates.md). No real hardware, provisioning, updates or real register operations were authorized or performed.
- **Deployment limits:** workflow source is configured, but its first GitHub-hosted run and administrator-enforced required status remain external actions. Broader F01/F03/F08/F09/F14/F15/F16 work is not implemented by these tests. See [release-gates.md](release-gates.md) for exact scope, commands, evidence and limitations.

- **Evidence (historical):** Safety tests permitted LF headers and did not parse every JSON result or test all success/error routes. Native GitHub CI only checked out the repository (`.github/workflows/ubuntu1804_gcc_7.5.0_intel.yml`).
- **Proposal:** Stub/headless CI with a real HTTP parser and JSON schema checks, distinct IR/depth fixtures, method/query/fragmentation tests, authorization/capability tests, browser recovery/download checks, full-mode transitions, multiple clients, and mocked subprocess/file errors. Run separate ASan/UBSan and TSan variants. Hardware/update/provisioning checks belong on isolated target devices with explicit approval.
- **Acceptance:** The runtime-confirmed failures below become failing tests before repair and passing gates afterward; no automated desktop test can alter networking, register state, or reboot the host.
- **Discuss:** Which embedded targets and browser versions form the release support matrix?

## New features worth discussing

All nine features are **not implemented**. Existing calibration format/version checks (F04), setting read-back (F12), and simulator fixtures/tests are prerequisites, not N05/N06/N03 completion. Dependencies refer to fixes above.

| ID / status | Feature and product value | First useful slice / acceptance | Dependencies / decision |
|---|---|---|---|
| [ ] N01 | **Device status and capability dashboard** — replaces guesswork with visible readiness, frame age, capture FPS, viewer FPS, build/firmware/plugin versions, and actual supported operations. | `/api/v1/status` and `/capabilities`; stale/disconnected banner; simulator clearly identified; bounded lightweight health response. | F05–F09, F16. Define healthy versus ready versus degraded; unavailable temperatures are not numeric zero. |
| [ ] N02 | **Reliable live streaming** — smoother viewing with less polling overhead and reconnect support. | Start with SSE for status/job progress; evaluate MJPEG or bounded binary WebSocket frames for sensor output. Include frame ID/time/format and drop-oldest backpressure. | F08–F10. Choose transport based on device limits; do not begin with WebRTC unless latency/bandwidth requirements justify it. |
| [ ] N03 | **Recording and replay sessions** — capture intermittent faults, share reproducible data, and support offline engineering. | Record a bounded raw/depth session with timestamps and calibration/configuration manifest; stop/download; replay through the simulator and UI. | F02, F09, F11. Decide maximum duration/storage, privacy, and whether video or raw measurements are authoritative. |
| [ ] N04 | **Measurement workspaces and exports** — several saved points/ROIs, per-region charts, and CSV/JSON export for validation work. | Named ROIs with units, sample count, frame/time range, min/max/mean/noise; isolated histories; correctly scaled point-cloud export (consider PLY/PCD alongside existing OBJ). | F09, F11, F17. Define invalid/saturated sample handling and spatial versus temporal metrics. |
| [ ] N05 | **Guided calibration with versioning/rollback** — turns low-level development controls into a repeatable customer workflow. | Preconditions, target placement, progress, sample-quality checks, before/after validation, explicit save/apply, and rollback to a known-good calibration. | F04, F06, F12. Agree on hardware-verified quality thresholds; don't call a wizard complete merely because commands returned 200. |
| [ ] N06 | **Persistent profiles and synchronized multi-client state** — switch reliably among preview, installation, and measurement setups. | Named configuration presets, read current settings on page load, atomic apply, and revision/conflict responses; preview-only changes remain per viewer. | F09, F12, F19. Decide ownership of device-global changes and which settings survive reboot. |
| [ ] N07 | **Secure onboarding and recovery** — install a device without editing Linux files or losing connectivity. | First-run pairing, identity/hostname, network scan, tested connection job, reconnect guidance, and timed fallback to previous connection/AP. | F01, F02, F14, F16. Define physical reset/recovery and whether discovery via mDNS is appropriate. |
| [ ] N08 | **Supported integration API** — enables Python tools, robotics, and fleet dashboards without scraping a browser UI. | Versioned/documented REST and binary schemas: byte order, shape, scale/units, coordinate axes, calibration identity, frame ID; scoped tokens and a small Python example/client. | F01, F05–F09, F11, F19. Decide support commitment and compatibility period for old paths; MQTT/ROS adapters only if there is a concrete consumer. |
| [ ] N09 | **Threshold alerts and support telemetry** — notify operators about overheating, stale capture, connection failure, or ROI distance events. | Local alerts with configurable duration/hysteresis and a bounded history; optionally authenticated outbound webhook delivery with retry limits. | N01/N04, F01, F08. Define validated thresholds and privacy/offline behavior; these are advisory, not safety-rated control outputs. |

## Suggested sequence and dependency boundaries

**Current single-user development status:** F04/F05/F06/F07/F10/F11/F12/F13 are complete for their selected scope. Further backlog work needs its own scoped approval. Attacker-focused security/deployment work is deferred, not completed. The original broader-deployment sequence below remains conditional on product scope.

1. **Containment and release gates:** F01/F02/F04/F16, with F20 tests. Do not expose administrative functionality to broader networks while these remain unresolved.
2. **Protocol and viewer correctness:** F05/F06/F07, then F10/F12/F13. These deliver visible fixes without requiring new product architecture.
3. **Reliable device service:** F08/F09/F11/F14/F15; F03 is its own hardware/deployment workstream and remains a production-update blocker. Add N01 early to expose degraded state.
4. **Choose a product differentiator:** N03/N04 for engineering/measurement users; N05/N07 for appliance deployment; N08 for integrations. N02 should follow server backpressure/lifecycle work, not precede it.
5. **Measured optimization and maintainability:** F17–F19 incrementally, rather than coupling every feature to a full UI/server rewrite.

## Verification and limitations

**F20 update:** desktop release gates now pass locally in normal/ASan/UBSan/TSan CMake/CTest builds and both Chromium asset copies; hardware qualification remains unapproved. The earlier per-fix limitations below are historical. Current gate scope/evidence is in [release-gates.md](release-gates.md).

The implementation verification is summarized at the top and documented in `tests/SAFETY.md`. F10 passed `node tests/web_ui.js` (including full inline-script parsing and deterministic live-stream tests for both copies), `node tests/web_stream_browser.js` (real Chromium against a local mock sensor, blocking external traffic), and simulator HTTP tests against the existing isolated normal build with current release assets. F10 changed no C++ code and required no binary rebuild; real hardware was not exercised.

F11 additionally passed `sh tests/run_safety.sh`, `node tests/web_ui.js`, `node tests/web_stream_browser.js`, and both `tests/http_measurements.py` and `tests/http_safety.py` against isolated normal and full ASan/UBSan simulator builds. No regular `bin/chiton` rebuild, provisioning, upgrades, or real hardware operations were performed. Full-app leak checking remains disabled for existing module allocations; helper leak checks remain enabled.

### Original review verification (historical, before the fixes)

Executed from the repository root:

- **Passed:** `sh tests/run_safety.sh` (ASan/UBSan socket/asset/validation/lock tests plus wall-boundary tests). Compiler emitted existing narrowing warnings in calibration code.
- **Passed:** `python3 tests/http_safety.py bin/chiton` against the existing headless file-backed binary. Includes exact assets, malformed arguments, image/raw/cloud data, concurrent clients, and clean shutdown.
- **Additional temporary headless/stub probes:**

| Request | Observed result |
|---|---|
| `GET /missing.css` | `HTTP/1.1 404 OK`, Content-Length 12, four-byte body. |
| `GET /missing` | Homepage with HTTP 200 rather than unknown-route response. |
| `GET /missing/camera` | Connection closes with no response bytes. |
| `GET /missing/calibrate` | HTTP 200, JSON content type, invalid JSON body `NA`. |
| `GET /dynamic/setting/off` | HTTP 200, invalid JSON body `{"value":off}`. |
| `GET /app.html?cache=123` | Malformed 404 rather than the homepage asset. |
| `HEAD /app.html` | Sends the complete 78,327-byte HTML body. |
| `POST /quality/setting/74` | Executes the legacy path mutation without method/body enforcement. |

Probes used temporary image files/logs and only benign stub-mode requests; no valid Wi-Fi, gateway, register, upgrade, restart, diagnostic-generation, calibration-load, or smoothing actions were invoked. Test binaries were not rebuilt as a full application, so runtime results describe the existing `bin/chiton`; source findings were checked independently. The focused safety script does compile current source.

No browser automation, performance benchmark, external download/update, privilege-policy change, or real camera/I2C/network hardware validation was performed. Kernel/file hazards and deployment-script issues are inspection findings here, not newly reproduced crashes or verified exploit chains. Proposed effort/priorities should be revisited against the actual deployment and support requirements.
