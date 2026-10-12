# Shipped UI and export contract inventory (F13)

F07 and the selected F13 repairs are complete. Both shipped UI copies are identical.
`tests/ui_contracts.json` inventories every named visible application control in
those copies; `web_contracts.js` rejects missing/extra controls, duplicate IDs,
missing/duplicate event bindings, wrong download links and enabled inert controls.
The unnamed Bootstrap navigation toggle and unlabelled sidebar anchors are local
navigation, not API actions. Commented-out controls are not shipped interactions.
This is not an inventory of every dormant helper or backend legacy route.

## Camera exports and downloads

All camera routes below retain GET-only behavior: HEAD is rejected with 405.
Responses are centrally serialized with accurate lengths and connection close.
Successful snapshots retain F11 `X-Chiton-*` metadata and `Cache-Control: no-store`.
Disabled/not-ready capture returns the ordinary JSON error shape with 503, **no
attachment header**. See [measurements.md](measurements.md) for measurement units,
byte order, invalid-sample policy, histories and compatibility.

| Consumer / route | Type and layout | Filename / disposition | Verification |
| --- | --- | --- | --- |
| Raw export `/raw/camera` | `application/octet-stream`; 640 × 480 uint16 processed samples, row-major; 614,400 bytes | `attachment; filename="chiton-raw.bin"` | Actual simulator bytes, dimensions, byte order, constant fixtures, F11 references/final-pixel preservation |
| Live cloud / binary export `/pointcloud/camera` | `application/octet-stream`; signed int16 X/Y/Z/distance records, 8 bytes each | `attachment; filename="chiton-pointcloud.bin"` | Nonempty aligned records, metadata record count, F10 mocked fetch/render regression |
| PointCloud download link `/pointcloud_obj/camera` | `text/plain; charset=utf-8`; ASCII OBJ vertices only, XYZ divided by 6000 with four decimal places | `attachment; filename="chiton-pointcloud.obj"` | Actual OBJ parsing, finite coordinates, count and numerical comparison with constant-fixture binary cloud |
| Live preview `/image/camera` | `image/jpeg`; JPEG bytes, dimensions recorded in snapshot metadata | No attachment header; unchanged live polling | JPEG SOI/EOI bytes and HTTP type; documented transient 503 on mode transition; viewer tests |
| UI PNG download (canvas only, no export route) | `image/png` data URL of the rendered canvas | `chiton.png` via anchor `download` attribute | Both copies: native Chromium saved bytes equal the real canvas PNG, signature/IHDR/native image decoding, one download and no navigation; mocked legacy fallback also checked |

Attachment headers do not change fetch response bytes or introduce new endpoints.
Filenames are fixed ASCII literals, not request parameters. The browser may apply
its own download-name collision rules. Existing OBJ units/precision are unchanged.
F09 removes cloud/export mode side effects: select device-global cloud mode (4)
explicitly first. Incompatible cloud/OBJ reads or JPEG reads in cloud mode return
JSON 409 with `current_mode`, without attachment disposition. No new mesh
faces, metrology guarantee, or binary-format migration is implied.

`tests/http_exports.py` is invoked by `http_safety.py` only after simulator
identification and benign fixture setup. It checks all three attachment headers,
MIME/layout, lengths and metadata, actual file content, GET-only/HEAD error policy,
JSON disabled-capture errors without disposition, and live JPEG recovery. The
constant fixture makes cloud/OBJ comparisons valid across distinct observed frame
IDs; it does not assume those independent requests capture the same frame.
`http_measurements.py` separately tests F11 reference fixtures and legacy plugins.

## F09 global modes, lifecycle and freshness

- Mode changes remain explicit device-global settings, not per-viewer rendering.
  GET/HEAD homepage/static assets and all camera reads leave the mode unchanged.
  Both UIs poll `/colormode/setting` read-back within their single-flight frame
  task and follow other viewers' acknowledged mode without sending mutations.
- `/colormode/setting` returns integer `value`, boolean `capture_enabled`,
  `frame_id`, `frame_mode`, and `frame_age_ms` (null before the first frame).
  `/colormode/setting/<0..6>` remains the explicit mutation. Read-back does not
  increment configuration revision. Mode/read-back responses use `no-store`.
- `/enable/camera`, `/disable/camera` and `/camera/setting/<enable|disable>` use
  one synchronized SDK lifecycle, with idempotent acknowledged state. Startup
  explicitly starts acquisition when enabled. SDK failures return 500 and retain
  the previously acknowledged state. `/camera/setting` reads boolean `value`.
- `/frame/camera`, `/raw/camera`, `/image/camera`, `/pointcloud/camera` and
  `/pointcloud_obj/camera` accept optional decimal `after` (uint64 frame ID) and
  `timeout_ms` (0–1000, default 1000, requires `after`). Unknown/duplicate fields,
  malformed values and overflow return 400. Without `after`, return the latest
  available compatible snapshot; reads do not trigger capture. With `after`,
  return a snapshot with a strictly greater ID or JSON 504 by the wait deadline.
  Disabled/shutting-down capture returns 503 immediately; incompatible formats
  return 409 before waiting. A pending new mode without a published compatible
  output returns 503 on ordinary reads, or can be waited for with `after`.
- One monotonic wait budget includes frame ownership acquisition/reacquisition.
  Waits release frame ownership and check shutdown at most every 25 ms (1 ms
  during ownership contention). The budget does not include network transfer,
  request framing or response construction. Ordinary legacy reads and SDK
  lifecycle operations are not made deadline-bounded by F09. F08 remains open:
  the sequential listener can still be occupied by a bounded fresh-frame wait.
- Cloud mode now acquires/publishes continuously, paced at approximately 30 Hz
  plus processing time; neither cloud retrieval nor OBJ export changes mode.
  Export formats, units, metadata and filenames are preserved.
- Frame JSON includes `color_mode` (snapshot mode), `current_mode` (acknowledged
  device mode) and `frame_age_ms`. Exports add `X-Chiton-Color-Mode` and
  `X-Chiton-Frame-Age-Ms`, bound to the exported frame. Age uses a monotonic host
  receipt clock; `captured_at_ms` retains the host wall-clock receipt time.
  Neither is a sensor exposure timestamp. The UI adds elapsed client time to
  the reported age, including during failures; it is an estimate excluding
  network transit. UI-update age remains separate. On process restart, UI
  read-back detects a lower frame sequence and clears the obsolete `after` ID.

`http_freshness.py` runs inside the simulator `http_safety.py` gate: all global
modes, two connections/viewers, homepage reload/HEAD, observational exports,
fresh/latest/frozen IDs, age, input limits, deadline expiry, both aliases and
SDK start/stop failure acknowledgement. Restart tests cover shutdown during a
newer-frame wait. The simulator enforces start-before-request and stop-after-use;
`ADITOF_STUB_PAUSE_FILE` and `ADITOF_STUB_LIFECYCLE_FAILURE_FILE` are isolated test
fixtures only. Node and Chromium tests check global-mode following and frame age
in both shipped copies. Real SDK lifecycle still needs on-target validation.

## Visible control and route inventory

All HTTP interactions below use the preserved legacy GET routes. F07 rejects
unsupported methods and malformed framing before handlers. Runtime UI tests cover
settings, read-back, mode/stream recovery and mocked administrative failures;
simulator HTTP tests check actual schemas and refuse hardware before operations.

| Control ID(s) | Interaction / route | Consumed response / behavior |
| --- | --- | --- |
| `calibration_nav` | Local sidebar expansion | No API action |
| `chitoncal` | `/chitoncal/setting/<on\|off>` | Boolean `value` |
| `depth` | `/depth/setting`, `/depth/setting/<depth\|ir>` | Boolean `value`, string `frame_type`; read-back and error rollback |
| `smoothing` | `/smoothing/setting/<on\|off>` | String `setting`, boolean `value` |
| `dynamic` | `/dynamic/setting/<on\|off>` | Boolean `value` |
| `calibration_angle`, `near_dist` | Explicitly disabled; unavailable title | No active handler; no pretend calibration workflow |
| `interval` | Local target FPS for adaptive polling | No HTTP mutation; frame/depth polls use `CHITON_INTERVAL` |
| `quality` | `/quality/setting/<integer>` | Integer `value` |
| `min_val`, `max_val` | `/range/setting`, `/range/setting/<min>/<max>` | Integer `min`, `max`; atomic validation and failure rollback |
| `colormode`, `colormode2` | `/colormode/setting`, `/colormode/setting/<0..6>` | Global integer `value`; read-back synchronizes both controls; bounded mutation attempt and rollback |
| `software_ver`, `upgrade_btn` | `/upgrade/software/<version>` | Integer `upgrade`; selection 32–99; invalid choice sends nothing; simulator 501 |
| `debuginfo_btn` | `/debug/software`, then returned archive path | String `value`; anchor download only on success; simulator 501 and UI error |
| `gateway` | `/gateway/setting/<eth0\|wlan0>` | Integer `retcode`; simulator 501 and UI error |
| `wlan0_ssid`, `wlan0_pass`, `savenet_btn` | `/wlan0/setting/<ssid>/<password>` | String `value` (`updated`); simulator 501 and UI error; F02 URL/security limits remain |
| `raw_link`, `download_png`, `download_pointcloud` | Downloads listed above | Matching types, filenames, single-click behavior and error contracts |
| `canvas` | Local mouse selection then `/get/value/client/<id>/<x>/<y>` or ROI bounds | F11 numeric value/statistics, units/frame/context metadata; stateless ROI and isolated point history |

Startup/read-only interactions:

| Route / source | Consumed fields / behavior |
| --- | --- |
| `/radians/setting` | Finite `radian1` through `radian8` |
| `/dimension/camera` | Integer `inner_dist`, `outer_dist`, `center_x`, `center_y` |
| `/temp/setting` | Finite `cputemp`, `vixeltemp`; simulator values are not hardware readings |
| `/image/camera`, `/pointcloud/camera` | Live JPEG/cloud bytes; F09 newer-frame ID, snapshot age/mode headers; F10 deadline, validation, cancellation and recovery |
| `/get/value/client/...` | F11 measurement/statistics/history contracts; see measurements guide |
| External firmware list `http://robotserver.net/fw.php?&returnformat=json` | `id` options; failure leaves no valid release selection; tests block external traffic |

`web_contracts.js` executes the remaining toggle, quality, gateway, Wi-Fi and
diagnostic handlers with mocked success/HTTP failure/network failure responses.
Administrative calls are never sent to real services. `http_ui_contracts.py` checks
actual dimension/radian/setting/temperature schemas and simulator administrative
501 responses. Existing tests supply the depth/range/mode/firmware/F11 coverage.

## Additional UI contracts

- Unsupported noise/gain/offset/version/targets helpers report unavailable without
  HTTP requests, polling, or missing-control access (`web_downloads.js`).
- Firmware selection accepts only two decimal digits in the existing UI range
  32–99, rejects invalid selections without requesting an upgrade, and reports
  mocked HTTP/network failures without retries (`web_firmware.js`). Actual
  firmware upgrades are not tested; the backend's legacy 1–999 range is unchanged.
- Depth/IR read-back/selection, atomic color ranges, stream recovery/cancellation,
  F11 measurement/history metadata and duplicate-frame behavior are covered by
  `web_ui.js`/`web_streams.js` and simulator measurement tests.
- Dimensions use API fields `inner_dist`, `outer_dist`, `center_x`, `center_y`;
  simulator schema tests check exact keys and numeric types.

## Results and remaining work

Passed both `http_safety.py` (framing, export and shipped-UI schema cases) and
`http_measurements.py` against isolated normal and combined ASan/UBSan simulator
builds in `/tmp/chiton-f13-normal` and `/tmp/chiton-f13-sanitized`. Node UI/inventory
and mocked handler tests passed against both shipped copies. Chromium recovery
and native download tests passed separately for each copy (`release/files` and
`bin/files` assets), with external traffic blocked.

`web_native_downloads.js` captures completed Chromium downloads in the disposable
profile: PNG is compared byte-for-byte to the actual canvas encoding and decoded
natively; raw/OBJ files are compared to known local mock responses; suggested
filenames, exactly one export per click and JSON errors without downloads are
checked. Production raw/cloud/OBJ bytes and headers are independently tested over
real simulator HTTP. Browser tests are not end-to-end against the simulator, and
diagnostics/provisioning/upgrades are only mocked or simulator-rejected. This
composition is not hardware validation or a guarantee about OS save dialogs.
Commands/options are in [the safety test guide](../tests/SAFETY.md).

Selected F13 acceptance is complete for the current shipped UI. Broader coverage
of dormant routes, every failure/mode, native legacy-browser behavior and release
CI were subsequently covered by F20's desktop gates; F19 source consolidation
remains open. F09 global-mode/lifecycle/freshness repair is implemented above.
Existing F02 credential URL/security and F15 diagnostic privacy/collection limits
remain open. No real hardware, provisioning, actual upgrades,
authentication/API migration or F08 concurrency was performed.
