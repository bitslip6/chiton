# Potential code-level improvements

## Scope and priorities

The original review covered the working tree, including its existing uncommitted desktop/stub-camera changes. Focus: the application built by Make/CMake, frame processing, HTTP handlers, resource loading, the shipped web UI, and supporting utilities. Vendored libraries, generated assets, and obsolete LVGL implementations were not exhaustively reviewed. The review itself changed no application code; subsequent implementation status is tracked below.

- **P0:** fix first — memory safety, remotely triggerable crashes, unsafe concurrent access.
- **P1:** correctness and important reliability fixes.
- **P2:** small robustness features, performance improvements, and regression prevention.
- **Confirmed by probe** means a small executable called the actual project functions. Other bugs below are evident from source inspection; hardware-specific behavior and performance gains still need validation.

## Current implementation status

**[x] Implemented** means the listed repair is present in source, not hardware-verified. **[ ] Partial** identifies completed pieces and remaining work; other unchecked items remain open. Bug descriptions, line numbers, probes, and proposed validations below are the **original review snapshot**, not current failure claims.

- **Implemented:** 1–7, 10, 15–17, 19–20, 27.
- **Partial:** 8–9, 12–14, 18, 21–23, 26, 32.
- **Open:** 11, 24–25, 28–31.
- Web-backlog cross-reference: **F04/F05/F06/F07/F10/F11/F12/F13 complete**. **F20 desktop gates/hosted CI/required-status enforcement complete; full hardware qualification remains outstanding.** Broader overlapping items may remain partial here. In particular, item 18 also includes the separate JSON calibration updater, which F04 did not repair.
- All proposed product features **N01–N09** in `docs/web-server-review.md` remain unimplemented.
- For this single-user development system, attacker-focused security work is deferred, not checked off. Item 17/F07 framing is complete and verified. F13 now disables unsupported UI handlers and fixes PNG type/name/labels, unique download IDs and duplicate listeners in both shipped copies. The missing firmware fallback is also fixed: strictly validate the existing 32–99 selection, report invalid input without a request, and surface upgrade failures. Node UI/download/firmware-selection and Chromium viewer recovery tests passed. Server raw/cloud/OBJ attachment filenames are now implemented; export content/header/error/HEAD tests and F11 regressions passed against isolated normal and combined ASan/UBSan builds. The shipped UI inventory is complete in `tests/ui_contracts.json` and `docs/http-contracts.md`, with structural control/binding gates, mocked remaining-handler failure tests and simulator schema checks. Inert calibration fields are explicitly disabled; Wi-Fi/diagnostic failures are UI-visible. Chromium native download capture passed for both copies (actual canvas PNG and local mock raw/OBJ bytes, filenames, single-click counts and error-without-file checks). F13 is complete for its selected shipped-UI scope. F20 now validates the repaired contracts and supported mode/failure cases with normal/ASan/UBSan/TSan CMake/CTest and both Chromium copies, in local and hosted CI. `master` enforces the GitHub Actions `Web release gate` check, including administrators and up-to-date checks, without requiring review approvals. The operator reports a successful hardware smoke test; full target qualification and real administrative operations remain unvalidated. See `docs/web-server-review.md`, `docs/release-gates.md` and `tests/SAFETY.md` for evidence and boundaries.
- **Recommended next coding item: F09**, observational retrieval/capture lifecycle/freshness, followed by F08's bounded connection/job architecture. This is a recommendation requiring separate scoped approval, not completion of items 26/30 or a new-feature prerequisite being counted as a feature.

## P0 — immediate fixes

### [x] 1. Fix the HTTP body writer's final chunk and partial-write handling

**Implemented:** bounded `write_all` recomputes remaining bytes, handles partial sends/interruption/disconnects, and enforces a monotonic deadline for each write operation. Central headers/bodies use the helper.

**Locations:** `src/webserver/webserver.cpp:97–174` (`send_headers`, `buffered_write`).

`buffered_write` computes the next chunk before subtracting the bytes just written. Sending 1,501 bytes therefore performs two 1,500-byte writes: the second reads past the supplied buffer and sends unrelated memory. It can also falsely report success. Both writers treat legitimate short writes/interruption as failure rather than continuing.

**Improvement:** implement one `write_all` helper using `size_t`/`ssize_t`, recomputing the remaining length before every write; handle `EINTR`, zero progress, disconnects, and an overall deadline. Use it for headers and bodies. Larger chunks can reduce syscall overhead once correctness is fixed.

**Validation:** confirmed by probe: a 1,501-byte response produced 3,000 bytes; an exact-size allocation triggered an AddressSanitizer heap over-read. Test sizes 0, 1, 1,499, 1,500, 1,501, and large payloads, plus short writes and disconnects.

### [x] 2. Repair static-file loading and buffer ownership

**Implemented:** resolved paths are used unchanged; sizes are initialized, bounded, alignment/read errors checked, and byte/16-bit buffers use consistent array ownership. HTTP tests assert asset bodies.

**Locations:** `src/graphics/images.cpp:601–664`; `src/webserver/webserver.cpp:337–352`; `src/system/resources.cpp:25–40`.

The resource resolver returns a resolved path, but `get_data_u8` prepends the executable directory again. Absolute resource paths consequently fail to load. Its failure path assigns `out_sz = 0`, not `*out_sz = 0`, leaving the server's local size uninitialized. Successful allocations use `calloc`, while the server releases them with `delete[]`. `get_data_u16` also uses unchecked `tellg()` and allocates a byte count as a count of 16-bit elements.

**Improvement:** accept the resolved path unchanged; initialize the output length immediately; check open, file size, allocation limits, and complete reads. Return an owned byte vector or another consistently managed buffer; validate 16-bit file alignment where appropriate.

**Validation:** confirmed by probe: an existing absolute asset returned null and left the output size unchanged; freeing a successful relative-path load as the server does triggered an allocation/deallocation mismatch. Assert actual homepage/JS/CSS contents, not merely an HTTP-looking status line.

### [x] 3. Validate complete HTTP argument lists before indexing them

**Implemented:** per-route counts, strict bounded numeric parsing, checked slices, and a request-boundary exception handler reject invalid input before indexing/mutation.

**Locations:** `src/webserver/settings.cpp:56–64, 78–305`; `src/main.cpp:366–385, 628–665`; `src/system/utils.cpp:791–797`.

Settings requiring `params[2]` accept a vector of size two; several requiring `params[3]` accept size three. Requests such as `/quality/setting` therefore access past the vector. `sample2` assumes five parts and uses throwing numeric conversions. The rectangle-value handler accepts five parts but slices through index five, which requires six. Exceptions escaping the HTTP thread can terminate the whole process.

**Improvement:** define per-route argument counts, validate before access, use strict bounded numeric parsing, and reject malformed requests with HTTP 400. Add a request-boundary exception handler as a final safeguard. Make `slice` bounds-checked or avoid allocating slices entirely.

**Validation:** exercise every route with missing, extra, nonnumeric, very large, and malformed numeric arguments under ASan/UBSan; capture must continue afterward.

### [x] 4. Bound coordinates using the correct image dimensions

**Implemented:** pixel versus exclusive rectangle bounds, source height/capacity checks, and mapping capacity/count validation prevent processing unwritten entries.

**Locations:** `src/webserver/webserver.cpp:195–209`; `src/main.cpp:641–704`; `src/graphics/images.cpp:262–289, 393–424`.

`url_point_to_2d` allows inclusive coordinates up to 1,500 × 480 irrespective of the active image. Planar sampling allows `x == 1500` and `y == 165`, then indexes a `1500 * 165` mapping. Rectangle helpers have no height/buffer bounds and can read outside the intended frame. `calc_src_dst_points` checks capacity only after writing and uses `>` rather than `>=`; its caller ignores the number of mappings actually generated.

**Improvement:** validate pixel coordinates against `[0, width)`/`[0, height)`, separately validate rectangle endpoints, and pass source height/capacity to window helpers. Preserve explicit validity/count information for mappings rather than processing unwritten entries. Check capacity before every mapping write.

**Validation:** test all corners, coordinates exactly at width/height, reversed/empty rectangles, planar mode, and unusual configured centers/radii.

### [x] 5. Replace check-then-set flags with real synchronization

**Implemented:** `cam::state_mutex()` owns SDK/settings/calibration/frame processing; data helpers use the same recursive mutex. HTTP copies owned JPEG/raw/cloud snapshots under the lock and sends after releasing it. Preview/status ownership remains separate. **Validation limit:** F20 now passes headless simulator concurrency/ownership regressions in local and hosted TSan gates; real SDK concurrency behavior remains unqualified. See `tests/SAFETY.md` for the ownership contract.

**Locations:** `src/system/camera.cpp:35–47`; `src/main.cpp:590–599, 941–1185`; `src/webserver/settings.cpp:61–64`; `src/data.cpp`; `include/system.h:56–117`.

`camera_waiting` and `GOPTS->waiting` are atomic integers, but waiting for zero and subsequently assigning one is not mutual exclusion: both threads can enter simultaneously. A timed-out wait also proceeds anyway. JPEG memory can be freed while HTTP sends it. Settings, calibration arrays, frame metadata, lookup tables, and calibrated samples are also read/written concurrently without a common synchronization policy. Locking only `requestFrame` would not protect the later processing steps.

**Improvement:** use an RAII mutex for camera SDK ownership and coherent settings/frame publication. Publish owned JPEG/raw/point-cloud snapshots with their dimensions and lengths; perform network writes outside the producer lock. Apply buffer-changing settings at a safe frame boundary. Keep the existing preview snapshot approach.

**Validation:** run concurrent image/value/settings requests and repeated mode switches under ThreadSanitizer; verify coherent snapshots and shutdown with a slow client.

### [x] 6. Make plugin reload safe, or disable it until it is safe

**Implemented:** live `sdk_reload` returns HTTP 409 requiring restart; no live unload/replacement is attempted.

**Locations:** `src/webserver/settings.cpp:318–325`; `src/main.cpp:828–867, 1041–1043`.

`sdk_reload` closes the plugin while the capture thread retains local callback pointers into it. It neither synchronizes with callback execution nor refreshes those pointers or the runline registration. Continuing capture can call unloaded code. Reload also bypasses the normal resource resolver and ignores `dlopen` failure.

**Improvement:** the simplest safe fix is to reject live reload and require restart. If retained, reload on the capture thread between frames, resolve/validate the replacement first, atomically replace the complete plugin state, and retire the old handle only after it is unused.

**Validation:** missing libraries, missing symbols, repeated reloads, and reload during capture must not leave dangling callbacks.

### [x] 7. Fix calibration smoothing's kernel allocation and indexing

**Implemented (F04):** normalized Gaussian kernels use `(2 * radius + 1)^2` owned elements, matching stride, radius/border checks, and independent valid-neighbor weighting for near/far samples. Sanitized kernel tests cover radii and borders.

**Locations:** `src/graphics/shapes.cpp:1912–2006`; `src/main.cpp:463–469`.

`make_gausian_kernel(1)` allocates eight floats but writes a nine-element 3 × 3 kernel; this is the size used by `/smooth/calibrate`. `apply_kernel` uses `size` as the row stride rather than `2 * size + 1`. The supposedly Gaussian weights are currently all one, and callers discard or leak the allocated kernel.

**Improvement:** allocate `(2 * radius + 1)^2` elements, use that same stride everywhere, validate radius, and use owned storage. Either implement normalized Gaussian weights or accurately name/document the existing box filter; account for actual valid-neighbor weight sums.

**Validation:** confirmed by ASan probe: radius one writes immediately beyond a 32-byte allocation. Test radii 0, 1, and 2, constant images, impulses, and borders.

### [ ] 8. Handle empty calibration neighborhoods and angle buckets

**Partial:**
- [x] Wall sampling clamps frame/bucket bounds, uses wide totals/counts, skips empty buckets, and returns a no-wall result for empty data; boundary tests exist.
- [ ] `sample_index`/infill neighbor bounds and zero-count division remain unsafe. Wall-angle conversion was deliberately preserved, not corrected or hardware-validated.

**Locations:** `src/graphics/calibration.cpp:567–633, 1259–1359`.

`sample_index` divides by `num_points` before checking whether any usable neighbors exist, and accesses neighbors without complete row/column bounds. `/infill/calibrate` can encounter an entirely empty calibration. `find_closest_wall_radian` divides by `angle_idx[i]` even for empty buckets; excluded angles naturally leave empty buckets, making `/wall_radian/camera` unsafe.

**Improvement:** validate neighbors in two dimensions, accumulate in wider types, return an explicit no-sample result before dividing, and skip empty wall buckets. Return a clear no-wall result if none qualify. Correct the wall-angle conversion to use the bucket count that actually generated the indices.

**Validation:** empty/partial calibration, all-zero frames, no qualifying wall, and first/last rows and columns must not crash.

### [ ] 9. Fix the SLAM metadata overflow, angular units, and neighbor bounds

**Partial (F20):**
- [x] Both active main/data allocations now use `make_slam_buffer()` (101 blocks), preserving the extra AOI metadata entry. Spike filtering starts at a valid lower neighbor; source run endpoints and angle buckets are bounded; invalid history indices are rejected. Focused sanitized boundary tests and supported mode transitions are release gates.
- [ ] Reciprocal angular-unit correction, synthetic obstacle/illumination behavior and hardware power bounds remain. Passing buffer-safety tests does not validate these physical semantics.

**Locations:** `src/main.cpp:917`; `src/data.cpp:137`; `src/graphics/calibration.cpp:270–340`; `src/graphics/images.cpp:345–356`; `include/calibration.h:11–12`.

SLAM buffers contain `100 * SLAM_POINTS` elements, but metadata starts at block 99 and accesses `slam_info[1024]`. Since `SLAM_POINTS` is 1,024, this is exactly one element past the entire allocation: `vector_slam` writes there and `create_slam_jpeg` reads there. Separately, `SLAM_ANGLE` is bins per radian but is used as radians per bin in the AOI cosine calculation. Spike rejection can access `i - 3`, and the bucket check accepts an index equal to the buffer length and only logs larger indices before continuing.

**Improvement:** separate metadata from sample history, or explicitly reserve and validate its extra element; update every allocator consistently. Use explicitly named reciprocal angular constants, reject invalid buckets, and use valid-neighbor or intentional circular indexing. Bound illumination output to the documented hardware power range.

**Validation:** confirmed by ASan probe: rendering a buffer with the application's allocation size reads immediately beyond its 204,800 bytes. Test SLAM mode and dynamic illumination under ASan, known angular offsets, edge buckets, and synthetic obstacles; illumination behavior also needs on-target validation.

## P1 — correctness and reliability

### [x] 10. Correct statistics overflow and the rolling sample history

**Implemented (F11):** wide sums/double squared deviations, exact population statistics, explicit empty handling, empty-start chronological histories, reset-before-sample semantics, and non-destructive range. Explicit client histories isolate users and deduplicate observed frame IDs; legacy unprefixed points and ROIs are stateless. Full-frame references and client/selection/eviction/expiry regressions passed. See `docs/measurements.md` for units, invalid-sample policy, and compatibility.

**Locations:** `src/system/utils.cpp:871–874, 1297–1312`; `src/graphics/images.cpp:232–246`; `src/data.cpp:42–100`; `src/main.cpp:677–720`.

Mean accumulation uses a signed `int`; variance/RMS use 32-bit sums, and sufficiently large 16-bit differences overflow signed multiplication before accumulation. The history starts with 60 synthetic zeros; `point_range` sorts it in place, destroying ring-buffer chronology. Clearing history does not reset its cursor, and point selection is cleared only after statistics are computed.

**Improvement:** use wide/double accumulators with explicit empty handling; compute mean and variance without narrowing intermediates. Use a real bounded ring/deque containing only received samples, reset before adding a newly selected point, and calculate range non-destructively with `minmax_element`.

**Validation:** confirmed by probe: 10,000 alternating 0/6,000 samples should have standard deviation/RMS 3,000, but returned about 640. Test full 16-bit range, startup, selection changes, and eviction of the oldest sample.

### [ ] 11. Remove per-pixel calibration leaks and unintended mutation

**Locations:** `src/graphics/shapes.cpp:188–249, 1789–1790`; `src/graphics/calibration.cpp:1034–1045`; `src/data.cpp:289–326`.

`find_calibration_for_radian` sometimes returns a borrowed configuration pointer and sometimes a newly allocated result; callers cannot safely decide whether to free it. Its temporary `GainPair` allocations also leak. Startup calls this function for many pixels, including repeated mapping generation. `make_all_polar_points_int2` writes `gain = 1.46484f` through the returned pointer, potentially modifying the original configured calibration.

**Improvement:** return a small calibration value, make lookup read-only, and use stack/value containers or a two-nearest scan instead of allocating and sorting pointers. Remove the hard-coded gain mutation unless explicitly configured.

**Validation:** compare startup RSS/LeakSanitizer results and calibration values before/after mapping construction; repeatedly switch mapping format.

### [ ] 12. Correct RGB pixel placement and planar color ordering

**Partial (F20):**
- [x] SLAM rendering writes the final pixel's valid blue channel (`+2`), not the next-pixel byte (`+3`); the focused last-pixel regression is gated.
- [ ] Other RGB run placement, planar color ordering and conversion/saturation findings below remain open; full-mode success tests are not pixel-orientation reference tests.

**Locations:** `src/graphics/images.cpp:345–369, 440–446, 507–519`.

`create_color_jpeg` increments its RGB offset before storing the first pixel, shifting each run and potentially writing beyond the last pixel for edge-reaching runs. `create_planar_color_jpeg` ignores `dst_ptr` and writes column-generated samples sequentially, unlike the correctly scattered grayscale path. The SLAM renderer writes channel `+3`, which is the next pixel rather than a valid channel of the current RGB pixel.

**Improvement:** address RGB pixels as `3 * destination_index + channel`, with channels 0–2; use the mapping's destination for both planar rendering paths. Saturate grayscale conversion instead of allowing out-of-range samples to wrap in 8-bit output.

**Validation:** compare grayscale/color orientation using asymmetric gradients, verify first/last pixels, and test extreme samples and full-edge runs under ASan.

### [ ] 13. Fix depth/IR selection and apply camera settings to hardware

**Partial:**
- [x] API true means DEPTH; the IR checkbox agrees, reads back state, and rolls back on failure. Distinct fixtures and mocked UI tests cover selection (F12).
- [x] Explicit HTTP camera enable checks `camera->start()` and reports failure.
- [ ] Startup acquisition lifecycle and checked runtime noise-reduction application remain incomplete; the stub does not enforce acquisition start/stop. Real-SDK behavior was not tested.

**Locations:** `src/webserver/settings.cpp:101–105, 194–201`; `src/main.cpp:895–901, 953–956`; `release/files/app.html:744–752`; `src/stubs/aditof.cpp:135–165`.

The UI sends `depth` when depth is selected, but the handler sets `depth_frame` true for `ir`; capture interprets true as DEPTH. The noise-reduction endpoint changes only the global integer, while SDK `setControl` is called only at startup. `run_camera` does not explicitly call `camera->start()` after configuration, although the SDK exposes a separate acquisition-start operation; the stub accepts requests regardless of start/stop state and can mask lifecycle errors.

**Improvement:** parse an explicit frame-type enum, fix the reversed comparison, and apply SDK controls through the synchronized camera owner with checked results. Validate/start the hardware acquisition lifecycle explicitly; optionally make the stub enforce it for tests.

**Validation:** use distinct IR/depth patterns and assert actual returned pixel values. Verify changed controls via a recording stub; acquisition lifecycle needs real-SDK/on-target validation.

### [ ] 14. Guard calibration fits and clamp corrected samples

**Partial:**
- [x] `process_calibration` skips nonpositive input/output ranges; file loading rejects non-finite angles/invalid indices; configuration validates measurement values before publication.
- [ ] Other fit/conversion helpers and corrected-sample finite/range clamping still need repair; do not treat skipping degenerate ranges as complete numeric safety.

**Locations:** `src/graphics/calibration.cpp:148–176, 1069–1117`; `src/system/utils.cpp:568–569`.

Two nonzero calibration samples may still be identical, producing a zero input range and an infinite/NaN gain. Invalid ordering or corrupt input can produce negative/extreme fits. Corrected floats are converted directly to `u16` without checking finiteness or representable range; the older helper narrows before applying its offset and does not clamp the lower bound.

**Improvement:** reject degenerate/inconsistent fits, retain a documented fallback gain/offset, perform calculations in a sufficiently wide signed/float type, and clamp finite final values before conversion. Keep validation code out of global fast-math assumptions.

**Validation:** identical near/far samples, reversed targets, negative offsets, corrupt calibration, and saturated sensor values.

### [x] 15. Keep physical camera geometry separate from display geometry

**Implemented:** raw snapshots/byte counts use fixed `CAM_WIDTH * CAM_HEIGHT`, independent of mutable display dimensions. F11 now moves frame metadata out of public pixels into JSON/snapshot headers. Only a private legacy-plugin copy retains its modulo-65536 final-element sentinel; ordinary plugin edits still publish, while the exported final measurement is preserved.

**Locations:** `src/main.cpp:1036–1038`; `src/webserver/settings.cpp:139–142`; `src/data.cpp:269–326, 355–366`.

The SDK frame is always 640 × 480, but the frame-number write uses mutable output dimensions, which switch to 1,500 × 165 or configured planar dimensions. This overwrites a different physical pixel; sufficiently large configured output dimensions can index beyond the SDK frame. Raw responses likewise derive their size from output geometry instead of an immutable description of the underlying data.

**Improvement:** maintain separate sensor dimensions, output dimensions, and buffer capacities. Store frame identifiers as metadata rather than modifying sample pixels, or preserve the existing plugin convention at the fixed physical last pixel until the API can change.

**Validation:** switch all color/mapping modes and verify raw byte counts, sample positions, metadata, and buffer bounds.

### [x] 16. Produce exactly one valid HTTP response per request

**Implemented (F05/F06):** all registered handlers return owned `HttpResponse`; dispatcher sends exactly once with CRLF, correct lengths/status phrases, connection-close semantics, serialized JSON, and explicit errors. HTTP/serialization regression tests cover success and failure paths.

**Locations:** `src/webserver/webserver.cpp:220–222, 319–326`; `src/main.cpp:390–441, 476–522`; `src/webserver/settings.cpp:109–121, 255–263`; `src/webserver/software.cpp:58–79`.

Fallback bodies advertise length 12 but contain only `OK!` or `FAIL`. Callback return codes are discarded, so camera enable/disable and unknown camera actions can close without a response. The `sample2` calibration path sends two responses. Several JSON bodies use unquoted keys/string values, and `generalwrite` appends an extra closing brace. Header strings mix LF and CRLF.

**Improvement:** establish one response contract: handlers either return a typed status/body for the server to serialize, or explicitly report that a streaming response was sent. Use the existing JSON library's serializer, correct content lengths/status phrases, CRLF headers, and explicit connection-close semantics.

**Validation:** parse each response with an HTTP client and JSON parser; assert status, body length, and exactly one response for successful, unknown, and failed actions.

### [x] 17. Parse requests incrementally and decode only URL components

**Implemented (F07):**
- [x] Separate method/path/query, split encoded segments before checked decoding, limit extension handling to static routes, explicit homepage routing, static HEAD, and unsupported-method rejection without mutation.
- [x] Incremental CRLF framing with 16,384 total header bytes, 4,096 bytes per line, 100 fields, and a non-renewable 1,000-ms steady-clock receive deadline. Shutdown is checked at most every 50 ms while receiving.
- [x] Strict request/header/control checks and HTTP/1.1 Host presence/uniqueness; reject duplicate/list/invalid Content-Length, every Transfer-Encoding, Expect, folded headers and coalesced trailing data before dispatch. URL-decoded controls are rejected too.
- [x] Legacy routes deliberately have a zero-byte body limit: absent/zero length only; nonzero Content-Length returns 413 without waiting for a body. Incomplete EOF/shutdown closes without dispatch; timeout/aggregate header overflow return 408/431. JSON mutation/credential bodies remain separate API/security work (F01/F02).

**Verification:** `sh tests/run_safety.sh`, `node tests/web_ui.js`, and both simulator HTTP suites passed against isolated normal, ASan-only, UBSan-only, and combined ASan/UBSan builds. Raw-socket tests cover byte-fragmented lines/headers/terminators, separately fragmented rejected bodies, exact/over-limit sizes, malformed/ambiguous framing, trickles, disconnects, and incomplete-request shutdown. Mutation URLs rejected or disconnected before framing completion preserve settings. F11 statistics/history/export/plugin contracts passed unchanged.

**Limits:** single-client server, no bodies/chunked/100-Continue/keep-alive/pipelining; later bytes after a complete zero-body request are discarded on close, not guaranteed detected before dispatch. This bounded custom helper is not a general-purpose HTTP parser; arbitrary header-value semantics are not fully validated. F08/F13, authentication/API migration, hardware, TSan and CMake/CTest remain outside this work. See `tests/SAFETY.md`.

**Locations (original review):** `src/webserver/webserver.cpp:45–85, 290–325`; `release/files/app.html:283–286`.

A single socket read is assumed to contain a complete request. The entire request is percent-decoded before request-line tokenization: an encoded space in an SSID can become a separator, and an encoded slash can change routing. Invalid `%xx` sequences use an unchecked `sscanf` result. Splitting the last path component at a dot can also reinterpret values/passwords as filename extensions.

**Improvement:** read bounded headers through their terminator with a total timeout; parse method, target, path, and query independently. Split encoded path segments before decoding, reject bad escapes, and restrict extension handling to static routes. Prefer a JSON POST body for credentials/settings; encode components while legacy URLs remain supported.

**Validation:** fragmented requests, overlong headers, malformed escapes, encoded spaces/slashes, dotted passwords, queries, and unsupported methods.

### [ ] 18. Make calibration persistence checked and atomic

**Partial:**
- [x] Binary calibration persistence is complete (F04): one configurable path, checked full-frame I/O, temporary validated load, version/shape/ABI/byte-order/CRC checks, legacy loading, and flushed/closed atomic sibling-file save preserving prior data on failure.
- [ ] `utils::update_json_calibration` still renames its open output before serializing/flushing; this separate JSON updater was not repaired.
- **Compatibility:** new saves retain native point layout and cannot be read by older Chiton binaries; no separate rollback archive is maintained.

**Locations:** `src/system/utils.cpp:1148–1175, 1187–1224`; `src/main.cpp:450–458, 1410–1429`.

Calibration reads/writes call `fread`/`fwrite` even if `fopen` failed, do not require complete input, and overwrite the live file directly. Raw struct serialization has no version/shape integrity check. The manual save path uses `/tmp/calibration.bin`, load uses `calibration2.bin`, and startup uses `calibration.bin`. JSON calibration update renames its still-open output before serializing/flushing it.

**Improvement:** centralize one configurable calibration path; validate file size/version/shape and read into temporary storage before applying. Check all I/O results. Save to a temporary sibling, flush/close (and sync when durability matters), then atomically rename; retain the previous valid file on failure.

**Validation:** missing/unreadable/truncated files, disk-full errors, interrupted saves, and a save/load/restart round trip.

### [x] 19. Validate configuration as a complete unit

**Implemented (F12):** `validate_configuration` checks required unique measurement IDs 1–8, field types/ranges, geometry, and log settings before publishing configuration; validation errors are caught/reported. Radians lookup also guards missing measurements.

**Locations:** `src/system/utils.cpp:636–750`; `src/graphics/shapes.cpp:206–246`; `src/webserver/settings.cpp:226–235`.

Only JSON syntax and a few field types are checked. Later code unconditionally dereferences calibration IDs 1–8. Missing/duplicate IDs, wrong measurement field types, invalid centers/radii, or negative values narrowed to unsigned types can cause crashes or invalid mappings. Measurement conversion exceptions occur outside the parsing catch block.

**Improvement:** parse into a temporary typed configuration; require the necessary unique IDs and numeric fields, validate geometry/ranges/log levels, and publish only after all checks succeed. Report the specific offending field instead of failing later in per-pixel code.

**Validation:** missing/duplicate measurements, malformed field types, out-of-image centers, inverted radii, and valid legacy configurations.

### [x] 20. Recover the browser stream after errors

**Implemented (F10):**
- [x] Bounded shared live reads with HTTP/payload validation and a 10-second whole-read/decode deadline; frame readiness restored in `finally`.
- [x] Single-flight frame/depth/temperature tasks, completion-driven polling, and failure backoff capped at eight seconds (not faster than the normal interval).
- [x] Per-stream connecting/live/stale/disconnected/paused status and last-successful-update age; acknowledged measurements remain intact on failure.
- [x] Hidden/page-away cancellation/resumption, obsolete mode/selection result suppression, valid hidden-canvas sampling, and image object-URL cleanup. Color-mode mutations are bounded but never automatically retried.
- [x] Deterministic regressions for both shipped copies plus optional real Chromium tests of decoding, network/payload failures, timeout, server stop/restart, and visibility transitions. Capture freshness metadata remains separate F09 work.

**Locations:** `release/files/app.html:402–471, 603–640, 1680–1712`; corresponding deployed HTML copies.

Point-cloud fetch sets `CHITON_READY` false, but its catch handler never resets it, so one failed request stops updates permanently. The depth-value polling timer schedules the next request without waiting for the previous one to finish, allowing requests to accumulate behind the single-client server when it slows down.

**Improvement:** restore readiness in `finally`, check `response.ok`, use request timeouts/abort, and schedule polling only after completion. Apply capped retry backoff and show disconnected/stale state. Ensure image-load failure also releases the stream gate.

**Validation:** temporarily stop/restart HTTP, return errors/truncated data, and delay responses; updates must recover with at most one request in flight per stream.

### [ ] 21. Protect administrative HTTP actions and credentials

**Partial; remaining security work deprioritized for single-user development:**
- [x] Simulator rejects embedded Wi-Fi/gateway/upgrade/diagnostic operations; register access uses the simulated SDK. Explicit settings-handler credential logging was removed.
- [ ] Bind/authentication/authorization, POST mutations, origin/CSRF policy, secret-free URLs/logs, restrictive credential storage, and shell-free program invocation remain.

**Locations:** `src/webserver/webserver.cpp:233–234`; `src/webserver/settings.cpp:249–291`; `src/webserver/software.cpp:39–81`; `src/system/wifi.cpp:41–55`.

HTTP listens on all interfaces and permits unauthenticated state-changing GET requests for I2C, Wi-Fi, upgrades, and restart, with wildcard CORS. Wi-Fi passwords are included in URLs and explicitly logged. The separate `nmcli_connect` helper concatenates SSID/password directly into a shell command, creating quoting/injection risks if used with untrusted input.

**Improvement:** add configurable bind address and authentication for administrative actions; require POST and origin/CSRF protection for mutations. Remove credential logging/URLs, restrict credential-file permissions, and invoke external programs using argument arrays rather than shell concatenation. In desktop/stub mode, explicitly reject hardware-only actions instead of attempting privileged operations.

**Validation:** unauthorized and cross-origin mutation attempts are rejected; passwords with spaces/metacharacters work without appearing in logs; no desktop request changes host system configuration.

### [ ] 22. Correct I2C decoding/error propagation and UART buffer handling

**Partial:**
- [x] UART transmit checksum is now at index six; HTTP register operations use checked SDK calls instead of the faulty direct helpers.
- [ ] Direct I2C decoding/descriptor/error handling and UART configuration/read completeness/write-result checks remain. Switching HTTP to SDK calls did not repair auxiliary I2C utilities.

**Locations:** `src/system/utils.cpp:284–309, 498–547`; `src/system/rover.cpp:12–44, 59–123`.

`two_bytes_to_int` stores the shifted high byte in `u8`, discarding it. Temperature assembly uses signed `char` arithmetic rather than explicit wire-format decoding. SMBus negative error results are narrowed to `u16` before testing `< 0`, making that check ineffective. I2C open treats valid descriptor zero as failure and does not fail/close cleanly when slave selection fails. UART writes `tx_buffer[7]` in a seven-byte array, leaving the transmitted last byte uninitialized; receive ignores short/failed reads. UART configuration also proceeds after unsuccessful open/`tcgetattr`.

**Improvement:** decode unsigned bytes into a wide integer, then sign-extend temperature according to the sensor format. Keep syscall results signed until checked; use `-1` as the invalid descriptor and RAII closure. Put the UART checksum at index six, validate complete packets, and check configuration/read/write results.

**Validation:** confirmed by probe: bytes `0x12 0x34` decode as `0x0034`, not `0x1234`. Add byte-order, negative-temperature, mocked syscall failure, partial UART packet, and ASan transmit tests. UART is auxiliary code, but its overflow should be fixed before reuse.

### [ ] 23. Make startup/worker failures follow coordinated shutdown

**Partial:**
- [x] HTTP camera start and register operations check SDK results and return errors; startup ignores invalid calibration with a logged fallback.
- [ ] Worker exception boundaries, SDK-wide error propagation, partially started thread guards, and exclusive creation of demonstrably absent stub files remain.

**Locations:** `src/system/utils.cpp:822–829`; `include/utils.hpp:74–75`; `src/main.cpp:869–890, 1463–1499`; `src/stubs/aditof.cpp:25–39`.

SDK failures call `exit` inside a worker through `PANIC_IF`, bypassing the new join/cleanup path. Worker exceptions are not caught, and thread creation after earlier threads have started is not exception-safe. Some SDK return values are ignored. The stub treats any failed default-file open as permission to create/truncate the path, rather than distinguishing a missing file from other errors.

**Improvement:** propagate an error result to the camera owner, record a failure reason, request stop, and let main join workers. Catch exceptions at worker boundaries and use a guard for partially started threads. Check SDK calls consistently. Create stub defaults only after confirming absence, using exclusive creation so existing images cannot be truncated.

**Validation:** unavailable camera, SDK request/control failure, allocation/thread-launch failure where injectable, and inaccessible existing default image files. Verify nonzero exit plus joined workers and retained input files.

### [ ] 24. Make early logging safe

**Locations:** `src/system/utils.cpp:129–130`; `include/utils.hpp:78–83`.

`log_fn` uses `ReturnVoidIf` when configuration is unavailable; that macro invokes `WARN`, which calls `log_fn` again. Logging before initialization therefore recurses until stack overflow rather than quietly returning or producing a fallback message.

**Improvement:** use a plain conditional return or direct minimal `stderr` fallback that cannot re-enter the logger. Give logger configuration a lifetime that extends through worker shutdown.

**Validation:** confirmed by ASan probe: a log call with `GOPTS == nullptr` caused recursive stack overflow. Test before initialization, missing log map, and after cleanup.

## P2 — straightforward performance and robustness enhancements

### [ ] 25. Avoid rebuilding expensive geometry that the current loop does not consume

**Locations:** `src/data.cpp:125–139, 289–326`; `src/main.cpp:924–933`; `src/graphics/shapes.cpp:1706–1718, 1758–1855`.

Startup generates the polar mapping in `init_defaults` and again in `run_camera`. The latter retains a mapping pointer that is not used by the current frame loop. Mapping construction searches a roughly 1,500-entry angle list linearly for every valid pixel and repeatedly calculates distance/trigonometry/calibration values.

**Improvement:** first remove the unused duplicate work; lazily construct mappings used only by legacy features. Cache geometry by center/radii and reuse it. Replace nearest-angle linear scans with `lower_bound` on the sorted angle list, or a carefully validated direct bin calculation; reuse already-computed distance.

**Validation:** measure time-to-first-frame and startup RSS before/after; compare generated indices and calibration data exactly or with documented floating-point tolerance. No speedup is claimed without measurement.

### [ ] 26. Reduce JPEG work and decouple it from slow consumers

**Partial:**
- [x] HTTP sends an owned snapshot outside the frame lock; point-cloud mode resets `jpeg_len` to zero when freeing the JPEG buffer.
- [ ] Compressor/output reuse, demand-driven encoding, configurable simulator pacing, and measured CPU/allocation/latency improvements remain.

**Locations:** `src/main.cpp:903–909, 1157–1185`; `src/graphics/images.cpp:526–590`; `src/stubs/aditof.cpp:157–168`.

Every capture frame creates/destroys a JPEG compressor and frees/reallocates its output, even when no image client is consuming frames. The synchronous stub has no frame pacing, so capture/encoding can run flat out. Waiting for HTTP transmission can stall capture. In point-cloud mode the code frees `jpeg_ptr` without resetting `jpeg_len` or generating a replacement.

**Improvement:** preserve/reuse compressor and destination capacity where supported; throttle or request JPEG production independently of capture. Pace the simulator with a configurable frame rate. Publish immutable encoded snapshots instead of holding capture behind a socket send, and represent unavailable images with a zero length/explicit status.

**Validation:** measure CPU, allocations per second, capture latency, and image age with no client, one client, and a slow client; verify mode switches and shutdown.

### [x] 27. Remove unnecessary statistics copies and sorts

**Implemented with F11:** statistics helpers take const references; min/max no longer sorts pixel/history storage; ROI filtering computes sums/counts without a second filtered vector. Bounded window helpers already return truly empty selections. Stable deviation uses a deliberate second pass, and small owned history snapshots remain bounded at 60 values. No measured speedup is claimed; further single-pass/view tuning is conditional on profiling.

**Locations:** `src/main.cpp:665–687`; `src/graphics/images.cpp:232–289`; `src/system/utils.cpp:871–874, 1305–1312`; `src/data.cpp:97–100`.

Statistics helpers take vectors by value, repeatedly copying the same selected window. Full sorting is used only to obtain min/max, and `get_window` allocates a synthetic nonempty zero-filled result even for an empty-width/height rectangle.

**Improvement:** accept `const` references/views; compute simple moments/min/max in a single pass over a validated region. Use `minmax_element` rather than sorting and return truly empty regions. If profiling warrants it, perform the one-standard-deviation filtered mean in a second pass without constructing another vector.

**Validation:** match corrected reference statistics for random/empty regions; benchmark a full-frame selection and repeated point polling.

### [ ] 28. Right-size and own long-lived buffers

**Locations:** `src/data.cpp:134–146`; `src/main.cpp:905–917`; `src/graphics/calibration.cpp:695–745`; `include/utils.hpp:64`.

Several allocations mix element counts and byte counts: `jpeg_rgb` reserves twice the maximum RGB image requirement, `planar_idx` multiplies its element count by `sizeof(uint16_t)` again, and other buffers are deliberately oversized or unused by current modes. Data/calibration module allocations are not released by main. The macro allocator does not check failure and expands into multiple statements.

**Improvement:** document buffer element type/capacity, size each from actual supported geometry, allocate optional outputs lazily, and migrate ownership to vectors/unique owners or add explicit module cleanup. Replace `NEWMEM` with a checked function where migration is incremental; retain explicit bounds rather than relying on spare memory.

**Validation:** compare steady-state/peak RSS, exercise every mode under ASan/LeakSanitizer, and inject allocation failures where practical.

### [ ] 29. Cache immutable static assets and reuse WebGL buffers

**Locations:** `src/webserver/webserver.cpp:327–352`; `src/system/resources.cpp:25–40`; `release/files/app.html:417–464`.

Each static request resolves paths and reloads the whole asset. Point-cloud display creates new `Float32BufferAttribute` instances for positions/colors on every update, copying arrays and replacing GPU attributes repeatedly.

**Improvement:** cache bounded immutable asset data and resource roots; add appropriate `Cache-Control`/ETag handling. Allocate point-cloud typed arrays/attributes once, update them in place with `needsUpdate`, and set the active draw range; dispose resources on teardown.

**Validation:** measure filesystem reads/syscalls and browser allocation/GPU-memory behavior over an extended session; ensure changing point counts and asset invalidation work correctly.

### [ ] 30. Bound administrative work and add basic health/staleness reporting

**Locations:** `src/webserver/software.cpp:16–31, 49–57`; `src/webserver/settings.cpp:283–291`; `src/system/utils.cpp:95, 166–194`; `src/main.cpp:1058–1064`.

The server processes one connection at a time, and upgrade/debug/Wi-Fi handlers invoke external commands synchronously without an execution deadline. Socket timeouts do not bound these commands or a whole multi-write response. `Wait_Until` returns no indication of timeout, so callers may consume stale data after its approximately 7.5-second limit.

**Improvement:** return a wait result with an explicit deadline; fail requests rather than proceeding after timeout. Bound subprocess execution and serialize long-running administrative jobs outside frame locks, returning job status if necessary. Add a small health response reporting camera state, last-frame age, last error, and queue/request state; show stale-frame status in the UI.

**Validation:** hung/mock commands, disabled/stalled capture, slow clients, and shutdown during an administrative action; other status requests must remain responsive.

### [ ] 31. Preserve mapping order and correct small auxiliary utility bugs

**Locations:** `src/graphics/shapes.cpp:1726–1754`; `src/system/system.cpp:70–80`; `include/demos.h:85–96`; `src/system/system.cpp:101–104`; `tools/update_json/update_json.cpp:28–41`.

`order_samples` sorts a copy of each vector and never stores it back. Registered shutdown callbacks execute only when the callback list is empty because the condition is inverted. `DisplayBuffers::sz` is 16-bit, so 320 × 240 truncates to 11,264 and under-sizes its legacy buffers. The JSON merge utility sorts identity vectors and uses their new positions to index the original unsorted objects, potentially appending the wrong object.

**Improvement:** sort mapped vectors by reference, fix the shutdown condition, use a suitably wide buffer-size type, and merge JSON objects by a stable explicit identity rather than sorted positional indices. These paths are partly legacy/auxiliary; avoid expanding their use until corrected.

**Validation:** unsorted sample slices, a registered cleanup callback, a 320 × 240 size assertion, and JSON inputs whose identity order differs from sorted order.

### [ ] 32. Strengthen regression coverage, CI, and safe build variants

**Partial; F20 desktop gate scope complete, broader build work still open:**
- [x] Twelve CTest entries per normal/ASan/UBSan/TSan headless simulator variant; framing/JSON/settings/calibration/measurement/client/export regressions, modes 0–6 with IR/depth and recovery, SDK/file failure propagation and forbidden-host-command tripwires.
- [x] Required deterministic UI and Chromium recovery/native-download coverage for both shipped copies; gate wiring rejects missing tools, nonzero results, sanitizer diagnostics and reused build trees. Failed-log excerpts and full artifacts are retained.
- [x] `mkrelease.sh` requires `tests/release_gate.py --all` before copying the separately built target binary. Isolated CMake builds prevent sanitizer object mixing and avoid fast-math. SIMDe 0.7.2-6/0.8.2 compatibility regressions pass.
- [x] Hosted CI verified: run 37233153411, commit `84b658b91df18bca463bb57a946c75623d4d2a7e`, all jobs successful. `master` requires the GitHub Actions `Web release gate` with strict checks and administrator enforcement; no review approvals required.
- [ ] SDL-enabled CI variants, Make compiler/flag rebuild tracking, and non-destructive `make clean` remain outside the completed F20 desktop scope. Full hardware/support qualification remains manual/outstanding; the operator-reported successful hardware run is a smoke test only.

**Locations:** `tests/desktop_runtime.py:87–93`; `tests/display.cpp`; `tests/aditof_stub.cpp`; `.github/workflows/ubuntu1804_gcc_7.5.0_intel.yml`; `Makefile:22, 99–107, 328–336`; `CMakeLists.txt:87–103`.

**Original evidence (historical):** the desktop runtime test accepted any HTTP-looking homepage response and checked depth changes by status alone; focused malformed-route/socket/kernel/statistics/concurrency tests and an active native build gate were missing. These regressions are now covered by the headless F20 gate. The separate SDL runtime suite is not yet a required CI variant. Make debug builds still enable fast-math, and object directories do not distinguish all compiler/flag changes. `make clean` copies release content into `bin` while deleting build outputs, which can unexpectedly overwrite local assets.

**Improvement:** add focused regression tests for the findings above, assert bodies and actual IR/depth data, and run them in native stub/headless CI plus SDL dummy-driver coverage. Add separate ASan/UBSan and TSan build/test jobs with appropriate compiler flags and no fast-math for validation. Track compiler/flag changes in Make rebuild dependencies or separate build variants; make cleanup delete build artifacts only, moving asset deployment into an explicit target.

**Validation:** both Make and CMake test paths should fail on the demonstrated regressions; confirm release/debug/sanitizer builds cannot reuse incompatible objects and cleanup does not modify shipped/user assets.

## Suggested implementation order

**Current status:** selected F13 shipped-UI contracts/downloads, 17/F07, 10/F11 and 20/F10 are complete; F20 desktop gates, hosted CI and required-status enforcement are complete. Full hardware qualification is still outstanding, despite the successful application smoke test.

**Next recommendation:** F09 first: stop homepage/cloud/OBJ reads from changing global presentation, unify capture aliases and specify bounded newer-frame retrieval. Decide request-scoped versus explicit global presentation before coding; preserve F11 measurement/export contracts and use the enforced F20 gates. Then address F08's bounded connections and command/job owner. The detailed proposed slices and acceptance checks are in `docs/web-server-review.md`.

Further open work requires separate scoped approval. Unresolved calibration/SLAM safety and physical-semantics items remain explicit above; prioritize them if those operations are needed immediately. Skip checked items rather than repeating their implementation. The original sequence below is historical planning context, with security work conditional on broader deployment.

1. Land socket-write and asset-loader fixes with regression tests (1–2).
2. Close route/coordinate and synchronization/plugin hazards (3–6).
3. Fix calibration/SLAM crashes, then arithmetic, image, and request correctness (7–19).
4. Harden error recovery, administrative access, device I/O, and logging (20–24).
5. Measure startup/capture/browser baselines, then apply low-risk performance work (25–29).
6. Add bounded operations/health reporting and make regression checks continuous (30–32).

## Implementation verification and limitations

**Current F20 evidence:** normal/ASan/UBSan/TSan CMake/CTest gates and both Chromium copies passed locally and in hosted CI, whose job results and required-status enforcement were verified through the GitHub API. Ubuntu's SIMDe compatibility failure was repaired without weakening tests. The operator reports successful hardware application operation, but full hardware/support qualification and administrative safety checks remain outstanding. See `docs/release-gates.md` for the exact hosted run, protection settings and manual checklist.

The per-fix verification paragraphs below retain earlier history; their statements that TSan/CMake/CTest or hardware were not validated describe those earlier slices, not the current desktop gate.

Previously passed after implementation: `sh tests/run_safety.sh`, headless simulator `tests/http_safety.py` against isolated normal and full ASan/UBSan builds, `node tests/web_ui.js`, complete inline-script parsing, and non-simulator handler syntax checks. See `tests/SAFETY.md` and `docs/web-server-review.md` for contracts and details.

Full-app leak checking was disabled for existing module allocations; focused helper leak checking remained enabled. Hardware behavior, TSan, and CMake/CTest were not validated. No provisioning, upgrades, or real hardware operations were performed. F10 additionally passed the expanded Node UI suite (both copies and all inline-script parsing), optional real Chromium tests against a local mock sensor with external traffic blocked, and simulator HTTP tests against the existing isolated normal binary using current assets. No C++ changes or binary rebuild were needed for F10.

F11 passed the expanded sanitizer helper suite (`measurements.cpp` plus response metadata serialization), Node UI tests, and optional Chromium complete-page recovery tests. `http_measurements.py` and `http_safety.py` passed against isolated normal and full ASan/UBSan simulator builds. Fixtures include full-frame constant, gradient, checkerboard and full-uint16 data, distinct clients/frames/contexts, and a temporary legacy plugin. The normal `bin/chiton` was not rebuilt. Sampling/format contracts are documented in `docs/measurements.md`; hardware, CMake/CTest, and TSan remain unvalidated.

## Original review verification and limitations (historical)

- **Passed:** `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests -p test_generate_stub_images.py` — four tests.
- **Passed:** the existing `tests/display.cpp` and `tests/aditof_stub.cpp`, compiled with the installed GCC into a temporary directory; display testing included SDL's dummy driver.
- **Confirmed using actual functions in temporary ASan/UBSan probes:** final socket-chunk over-read; kernel radius-one overflow; SLAM metadata over-read with the application's allocation size; asset `calloc`/`delete[]` mismatch; recursive pre-initialization logging; absolute asset path/unchanged error size; lost I2C high byte; variance/RMS overflow.
- **Not run:** a full CMake build/CTest — `cmake` is not installed on this machine. The full desktop runtime suite and real camera/framebuffer/UART/I2C tests were not run for this review.
- Probes/build outputs were kept outside the repository in `/tmp/chiton-review-build-a7EHMC`. Findings based only on inspection are not presented as hardware-verified behavior or measured speedups.
- Existing user changes were preserved; the original review only added this plan. Subsequent approved application fixes are tracked in the status notes above.
