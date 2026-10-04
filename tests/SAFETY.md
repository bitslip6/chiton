# Safety and web correctness regressions

## F20 required release gates (current)

```sh
python3 tests/release_gate.py --all --output /tmp/chiton-release-checks --jobs 2
```

The output directory must be new. Required gates are isolated normal, ASan,
UBSan and TSan CMake/CTest simulator builds plus Chromium recovery/native
PNG/raw/OBJ downloads for both shipped copies. `mkrelease.sh` runs this gate
before copying the separately built target binary; CI runs the variants in
separate jobs and publishes a final `Web release gate` status. See
[../docs/release-gates.md](../docs/release-gates.md) for dependencies, commands,
branch-protection setup, sanitizer policy and the manual hardware boundary.

**Passed locally:** `--all --output /tmp/chiton-f20-complete-final --jobs 4`, including
12/12 CTest tests in each normal/ASan/UBSan/TSan variant and both browser copies.
Logs are retained in that directory. Full-app leak checking remains disabled
for existing module allocations; focused helper leak checking is enabled.
No hardware, host sysctl/network change, provisioning, actual upgrade or real
register operation was performed. The GitHub-hosted workflow was not launched
from this session; the release support matrix/hardware signoff still needs the
owner's approval.

`http_modes.py` now covers supported modes 0–6 for IR and depth, both transition
directions, real JPEG dimensions, raw/cloud contracts, invalid-mode rejection,
and disabled/re-enabled capture. UI tests add the same mode sequence with
HTTP/network/JSON/unexpected-acknowledgement rollback and recovery. Simulator
SDK faults at `c0ff`, directory-target calibration save/load failures and
forbidden-host-command tripwires exercise failure propagation without hardware.
Runtime log copies survive fixture cleanup, including restart runs, and local
HTTP suites coordinate ports through an advisory lock. Gate-wiring unit tests
verify fail-closed behavior, missing-browser errors and complete required suites.

The new full-mode ASan test first failed in `vector_slam`: its AOI metadata
entry was past the two 100-bank allocations. Both now use the existing 101-bank
allocator. `wall_bounds.cpp` additionally tests history frame zero/last, invalid
run endpoints and buckets, lower-neighbour spike bounds, and the final RGB
pixel's blue channel. All modes remain enabled in the gate.

The verification sections below retain earlier per-fix history; statements that
TSan/CMake/CTest or browser downloads were not run refer to those earlier slices.

## Individual suites

From the repository root:

```sh
sh tests/run_safety.sh
SANITIZERS=thread sh tests/run_safety.sh
make USE_ADITOF_STUB=1 USE_SDL2=0 DEBUG_MODE=ON -j4
python3 tests/http_safety.py bin/chiton
python3 tests/http_measurements.py bin/chiton
node tests/web_ui.js
# Optional real-browser recovery check (requires Chromium; local mock sensor only):
node tests/web_stream_browser.js
```

The focused executables test socket writers, asset loaders, coordinate/window validation, slice bounds, strict numeric parsing, and exception-safe mutual exclusion, including short sends, disconnects, and stalled-reader deadlines. `web_correctness.cpp` additionally checks atomic/versioned and legacy calibration I/O, failure preservation/cleanup, checksums/shape, Gaussian kernel sizes/stride/validity, CRLF/length/JSON/binary/HEAD serialization, URL components, and complete configuration validation. `run_safety.sh` compiles these against current source with ASan/UBSan (or the selected sanitizer); compiling the additional production sources can take several minutes. `measurements.cpp` checks F11 against production statistics/data helpers: full-frame constant/gradient/checkerboard/random/full-uint16 references, empty/startup/reset handling, chronology and oldest eviction, client isolation/frame deduplication, context reset, LRU/expiry bounds, and private plugin sentinel preservation.

The HTTP tests require a simulator build and unused HTTP ports 80 or 8080. It checks exact assets, actual depth/IR data from distinct fixtures, all JSON responses through a normal HTTP parser, malformed arguments, boundaries, simulated register I/O, disabled-camera errors, calibration save/load/smooth and restart persistence, atomic color limits, concurrent snapshots/settings, and software restart response/shutdown. Calibration files and fixtures live in a temporary directory. Embedded-only actions must return 501; the suite refuses hardware builds before invoking valid register/system actions. `web_ui.js` uses Node's VM with mocked DOM/fetch to test both shipped HTML copies: correct IR semantics, read-back, failure rollback, paired color-limit validation and atomic updates. It also runs `web_streams.js`, using deterministic timers/abort/image mocks to verify single-flight live reads, whole-read/decode deadlines, HTTP/network/invalid payload failures, capped backoff, readiness recovery, acknowledged-value preservation, obsolete selection/mode suppression, hidden-canvas sampling, visibility/page lifecycle resumption, and object-URL cleanup. It parses every complete inline script. F11 checks also cover explicit per-tab client routes, acknowledged value/mean/deviation precision, duplicate chart-frame suppression, context/history resets, and rendering the first real cloud record. These deterministic tests do not require a browser.

`web_stream_browser.js` is an optional real Chromium check of the complete shipped page against a temporary local mock sensor: native image decoding/errors, cloud HTTP/network/truncation/deadline recovery, actual HTTP server stop/restart without reloading the page, delayed single-flight polling, and synthetic visibility transitions with native fetch cancellation. External page requests are blocked via CDP; a rejecting local proxy also blocks background external connections. It uses a disposable `/tmp` profile and headless software WebGL with `--no-sandbox` for portability, never a personal browser profile. Set `CHITON_CHROMIUM` to the Chromium executable or `CHITON_ASSET_DIR` to another shipped asset directory. No provisioning, updates, or real hardware operations are served by the mock.

A complete TSan stub build can be isolated from ordinary objects:

```sh
mkdir -p /tmp/chiton-safety-tsan/bin
make USE_ADITOF_STUB=1 USE_SDL2=0 DEBUG_MODE=ON \
  BUILD_DIR=/tmp/chiton-safety-tsan/build BIN_DIR=/tmp/chiton-safety-tsan/bin \
  PERF='-O1 -g -fsanitize=thread -fno-omit-frame-pointer' -j4
cp bin/chiton.json /tmp/chiton-safety-tsan/bin/
TSAN_OPTIONS=halt_on_error=1 CHITON_ASSET_DIR="$PWD/bin" \
  python3 tests/http_safety.py /tmp/chiton-safety-tsan/bin/chiton
```

`wall_bounds.cpp` exercises the production wall detector with private geometry fixtures: empty buckets/frames, first/last pixels, oversized/reversed run ranges, oversized bucket indices, and negative/extreme projected distances. Bucket indices and frame endpoints are clamped before access; empty buckets are skipped and no usable wall returns `-1`. The existing angle conversion remains unchanged.

`http_measurements.py` checks actual simulator API results against full-frame/subregion/reversed constant, gradient, checkerboard, and full-uint16 references, plus stateless legacy requests, explicit client isolation/reset, distinct-frame deduplication, depth/IR context changes, coherent export metadata, final-pixel preservation, and disabled-capture errors. It also compiles a tiny temporary legacy plugin to verify counter compatibility and ordinary edit publication without sentinel leakage. It refuses hardware builds before valid camera operations. Run normal and sanitized builds from isolated output directories; the tests use temporary fixtures/calibration state and do not provision or upgrade anything. See `docs/measurements.md` for the complete contract.

## F07 framing contract and verification

`include/http_framing.h` gates dispatch on a complete, validated header block. Bounds: **16,384 header bytes including CRLFCRLF**, **4,096 bytes per line excluding CRLF**, **100 fields**, **zero body bytes**, and **1,000 ms total receive time** measured with `steady_clock` from receive entry immediately after accept. Fragments never extend that deadline; polling checks shutdown at most every 50 ms. Only HTTP/1.0 and HTTP/1.1 origin-form requests with exact space-separated request lines and CRLF headers are accepted. HTTP/1.1 needs one nonempty Host. Header names must be tokens; obs-fold, whitespace before colon, prohibited controls/DEL, duplicate/list/signed/nonnumeric Content-Length, every Transfer-Encoding, and Expect are rejected. Decoded path controls are rejected. Unknown methods still reach the existing 405 policy only after framing validation; queries, legacy routes, static HEAD and central serialization remain unchanged.

Absent or all-zero decimal Content-Length is accepted. Any nonzero decimal length returns **413** without waiting for a body, including overflow-sized lengths; no legacy route consumes bodies. Coalesced extra bytes return **400**, aggregate oversized headers **431**, other malformed framing **400**, and total deadline expiry **408**. Premature EOF or shutdown closes without dispatch/response. This is deliberately not chunked/keep-alive/pipelining/100-Continue support. Future bytes arriving after valid zero-body framing are discarded on connection close; they cannot be interpreted as another request, but detecting them before the first dispatch is not guaranteed. Arbitrary header-value semantics are not fully validated, and the server remains single-client (F08).

`http_framing.cpp` adds focused sanitized syntax, fragmented socketpair, deadline, disconnect and cancellation checks. `http_framing.py`, invoked by `http_safety.py` only after simulator identification, adds byte-fragmented mutation request lines/headers/terminators, separately fragmented rejected bodies, exact 16-KiB and over-limit headers/lines/field counts, malformed framing/control characters, duplicate/conflicting lengths, transfer encodings, Expect, coalesced pipelines, trickles, idle expiry and incomplete disconnects. Read-back assertions prove rejected/incomplete mutation URLs leave the paired range setting unchanged. The simulator suite also sends SIGTERM while a mutation request is incomplete and verifies timely clean shutdown with no response; the receive gate does not dispatch incomplete requests.

F07 results: **passed** `sh tests/run_safety.sh` (ASan/UBSan helper leak checking enabled), **passed** `node tests/web_ui.js`, and **passed both** `tests/http_safety.py` and `tests/http_measurements.py` in each isolated build below. F11 constant/gradient/checkerboard/full-uint16, client history, export metadata and temporary legacy plugin regressions passed unchanged. Build/test logs are in the temporary variant directories (focused/UI logs: `/tmp/chiton-f07-focused.log`, `/tmp/chiton-f07-ui.log`).

| Output directory | PERF |
| --- | --- |
| `/tmp/chiton-f07-normal` | `-O1 -g` |
| `/tmp/chiton-f07-asan` | `-O1 -g -fsanitize=address -fno-omit-frame-pointer` |
| `/tmp/chiton-f07-ubsan` | `-O1 -g -fsanitize=undefined -fno-omit-frame-pointer` |
| `/tmp/chiton-f07-sanitized` | `-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer` |

Reproduce each with its directory and PERF from the table:

```sh
out=/tmp/chiton-f07-normal
perf='-O1 -g'
mkdir -p "$out/bin"
make USE_ADITOF_STUB=1 USE_SDL2=0 DEBUG_MODE=ON \
  BUILD_DIR="$out/build" BIN_DIR="$out/bin" PERF="$perf" -j4
cp bin/chiton.json "$out/bin/"
export CHITON_ASSET_DIR="$PWD/bin" PYTHONDONTWRITEBYTECODE=1
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1
python3 tests/http_safety.py "$out/bin/chiton"
python3 tests/http_measurements.py "$out/bin/chiton"
```

Full-app leak checking remains disabled for pre-existing module allocations. No ordinary `bin/chiton` rebuild, real hardware, provisioning, upgrades, authentication/API migration, F08/F13 work, TSan or CMake/CTest was performed. Browser recovery was not rerun for F07 (no UI changes); Node regressions were rerun. Existing working-tree changes were retained.

## F13 selected UI/download fixes

F07 remains marked complete in both backlogs. The selected F13 repairs are now complete (incremental verification history follows): both shipped HTML copies now disable the dormant `noise`, `gain`, `offset`, `version`, and `targets` helpers with an unavailable message and no HTTP request, polling, absent-control access, or upgrade-link mutation. Supported firmware actions are unchanged. Download anchors have distinct `download_png` and `download_pointcloud` IDs; PNG uses one click listener, suppresses placeholder navigation, and consistently exports `image/png` as `chiton.png` with a PNG label. The legacy canvas anchor helper has the same type/name contract.

**Passed:** `node tests/web_ui.js`, including new `web_downloads.js` contracts for both copies and existing settings/F10/F11 regressions/complete-script parsing; **passed:** `node tests/web_stream_browser.js` against the local mock sensor with external traffic blocked. New tests assert unavailable handlers never fetch/poll/access absent controls, ID uniqueness, point-cloud href preservation, one export per click, navigation suppression, PNG MIME/name, and both synthetic-event and old-browser fallback paths. Canvas/DOM download checks are mocked; native browser save dialogs/file contents were not tested. Both deployed copies match. No C++ changes or application rebuild was needed; full simulator/sanitizer HTTP suites were not rerun for this UI-only slice.

The firmware-selector fallback is now fixed in both copies: no reference to missing `software_ver2`; only exactly two decimal digits in the existing 32–99 UI range are accepted. Invalid/empty/placeholder/missing/out-of-range selections produce one error message and no request or upgrade announcement. Valid selections retain `/upgrade/software/<version>`; HTTP/network failures are reported without retry. `web_firmware.js` tests these contracts with mocked fetch, including boundary versions 32/99 and malformed numeric forms. **Passed again:** `node tests/web_ui.js` and `node tests/web_stream_browser.js`. No actual firmware operation was executed and no backend range/API was changed.

### F13 server exports and initial inventory

Raw/cloud/OBJ responses now suggest fixed ASCII attachment filenames (`chiton-raw.bin`, `chiton-pointcloud.bin`, `chiton-pointcloud.obj`). These headers are added after snapshot metadata assembly, preserving F11 headers and bytes. Live JPEG responses remain without attachment disposition. `http_exports.py`, invoked by `http_safety.py` after simulator verification, tests actual raw bytes/shape, binary-cloud record alignment/count, OBJ ASCII vertices/finite coordinates/numerical agreement with the constant-fixture cloud, MIME/disposition/length/metadata, HEAD 405 behavior, disabled-capture JSON errors without attachment headers, and live JPEG recovery after leaving cloud mode. Initial camera export and selected UI contracts are documented in `docs/http-contracts.md`; this is not yet an exhaustive visible-control inventory.

**Passed:** `http_safety.py` and `http_measurements.py` on isolated normal (`/tmp/chiton-f13-normal`) and combined ASan/UBSan (`/tmp/chiton-f13-sanitized`) headless simulator builds. PERF/options are the same as the corresponding F07 normal/combined rows above, with separate F13 BUILD_DIR/BIN_DIR; build, HTTP and measurement logs reside in those directories. ASan full-app leak checking was disabled for existing module allocations, with ASan/UBSan halt-on-error enabled. `node tests/web_ui.js` passed again. Native browser downloads were not captured; real exported files were validated over local HTTP. Chromium recovery was not rerun for this backend-only slice (it passed for the preceding UI fixes). No ordinary binary rebuild, hardware, actual provisioning/upgrades, authentication/API migration, F08/F09 repair, TSan or CMake/CTest was performed.

### F13 shipped-control inventory and native download completion

`tests/ui_contracts.json` now lists every named visible application control in both shipped copies, including local navigation, selection/FPS, API-bound actions, fields owned by submit buttons, download links and explicitly unavailable controls. `web_contracts.js`, invoked by `web_ui.js`, checks the real markup and active bindings: no missing/extra controls, duplicate IDs/listeners, enabled inert calibration fields or incorrect hrefs. The calibration-angle/distance inputs are disabled with explanations rather than connected to an unimplemented workflow. Wi-Fi and diagnostic failures now show UI errors. Remaining toggle/quality/gateway/Wi-Fi/diagnostic handlers are exercised with mocked success, HTTP failure and disconnect responses; no real administrative operations run. `http_ui_contracts.py` adds actual simulator field/schema checks for dimensions, radians, settings and temperature plus safe administrative 501 responses.

`web_native_downloads.js`, invoked by `web_stream_browser.js`, enables Chromium download events and saves files inside the disposable profile. It checks PNG bytes against the frozen real canvas encoding, signature/IHDR dimensions and native decoding, no placeholder navigation, exactly one completed download per click, expected suggested filenames and full raw/OBJ mock bytes. JSON error responses without disposition produce no downloaded files. The test cleans up all downloads/profile data. It uses only the local mock sensor; production exports are separately validated by `http_exports.py` over simulator HTTP, not by an end-to-end browser/simulator run.

**Passed:** `node tests/web_ui.js` for both copies, `node tests/web_stream_browser.js` with default `release/files` assets, and `CHITON_ASSET_DIR="$PWD/bin/files" node tests/web_stream_browser.js`. Both Chromium runs exercised recovery and native downloads with external traffic blocked. **Passed again:** `http_safety.py` (including new UI schemas) and `http_measurements.py` against the existing isolated F13 normal/combined-ASan-UBSan builds with current assets. No backend changes or rebuild were needed in this completion slice; sanitizer full-app leak checking remains disabled for pre-existing allocations.

F13 is complete for the current shipped UI; inventory/evidence is in `docs/http-contracts.md`. Broader dormant-route/full-mode failure coverage, legacy native browsers and release CI are still F20/F19 work. Real diagnostic archives, provisioning/upgrades, hardware, authentication/API migration and F08/F09 repair were not tested or implemented. Browser mock downloads do not claim to validate OS save dialogs or real hardware export timing. No actual firmware upgrades, provisioning, real hardware, authentication/API migration, or F08 work was performed.

## Ownership policy

`cam::state_mutex()` owns SDK access, non-atomic settings, calibration and geometry, and the entire frame-processing/encoding pipeline. Scalar/container data helpers acquire the same recursive mutex; borrowed buffer/mapping views require caller ownership throughout access. HTTP handlers copy JPEG/raw/point-cloud buffers while owning this mutex and release it before writing to sockets. Each snapshot's size comes from its owned storage; raw samples use fixed sensor dimensions and JPEG dimensions are carried in the encoded image. Frame IDs/host receipt timestamps/processing metadata publish under the same lock and are copied with export headers; they never replace measurement pixels. Per-client history access also owns this lock. The existing preview snapshot and status-string mutex remain separate.

Settings apply between complete frames. The frame loop yields ownership briefly between frames to prevent the synchronous stub from starving readers. Live plugin reload is rejected (409, restart required), because mutual exclusion alone cannot repair capture's retained callback pointers. Real camera SDK behavior and other unimplemented review items remain outside these tests. For a full ASan/UBSan application run, use an isolated build with `PERF='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer'` and run the HTTP suite with `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1`; leak checking is disabled there because existing application module allocations are not all released. Focused helper tests retain leak checking.
