# Measurement and frame contracts (F11)

## Endpoints and histories

- `/get/value/<x>/<y>` is a **stateless single-pixel** measurement of the current processed frame: mean equals value, count is one, deviation/range are zero. It no longer uses a shared implicit history.
- `/get/value/<x1>/<y1>/<x2>/<y2>` returns **spatial population statistics** of one locked frame. Endpoints are exclusive, reversed corners are normalized, empty regions are rejected, and planar rectangles remain unsupported.
- `/get/value/client/<id>/<x>/<y>` opts into **temporal statistics of distinct observed frames**, isolated by caller-supplied ID. The UI generates a fresh ID per page/tab. IDs are 1–64 ASCII letters, digits, `_`, or `-`; they are identities, not authentication tokens.
- The same client prefix accepts rectangles. They remain spatial/stateless; selecting a rectangle resets that client's previous point context.

Each point history starts empty and retains at most **60 distinct observed frames** in chronological order. Repeated polls of the same frame do not add samples. Selection, processing/configuration revision, units, or mapping context changes reset history **before** adding the new sample. At most **32 clients** are retained, with least-recently-used eviction and lazy expiry after **five minutes** of inactivity. Eviction/expiry returns a fresh history with `history_reset: true`.

This is not a fixed-rate camera recorder: only frames observed by that client are sampled, and gaps are not synthesized. `first_frame_id`, `last_frame_id`, `first_captured_at_ms`, `last_captured_at_ms`, `sample_count`, and `history_capacity` make the sampled window explicit. Sharing an ID intentionally shares a history. Legacy unprefixed requests cannot mix clients because they are stateless.

## Numeric semantics

Responses include `statistics_kind` (`spatial` or `temporal_observed_frames`), `sample_count`, `mean`, `min`, `max`, `diff`, and population `stddev`. Mean/deviation are not narrowed to integer intermediates. Summation uses 64-bit integers, squared deviations use double precision, and min/max never sorts history storage.

Compatibility fields:

- `stdd` = `stddev * 1000` (legacy milli-unit encoding).
- `rms` = mean-centered RMS, mathematically population deviation; this preserves the old intended formula.
- `rms_about_zero` = actual RMS about zero, in the sample's units.
- `1std_mean` = mean of values within one population deviation of the full mean; `filtered_count` reports how many qualify. If floating-point boundary rounding leaves none, the full mean is the explicit fallback.
- `temporal` appears only for explicit client point histories and equals their population deviation. Spatial ROI spread is no longer mislabeled as a shared median of “temporal noise.”

**Invalid-sample policy:** all uint16 values, including zero and 65535, participate. The SDK path does not supply a per-pixel validity mask, so no validity/saturation threshold is invented. These statistics describe the supplied numeric pixels, not proof that every pixel is a physically valid distance. Empty helper inputs return zero/count zero; empty HTTP regions are errors.

## Units, frame identity, and exports

Each measurement response and `/frame/camera` includes:

- `frame_id`: monotonic 64-bit sequence **within this process**, starting at one; restarts reset it.
- `captured_at_ms`: Unix milliseconds of **host frame receipt**, not a hardware exposure timestamp.
- `configuration_revision`, actual `frame_type`, `calibration_applied`, `smoothed`, and `plugin_applied`.
- `units`: `depth_sensor_units`, `ir_intensity_units`, or `mm` when Chiton depth calibration is applied. `units_scope` is `calibration_aoi` for calibrated data and `full_sensor` otherwise. Calibration does not make excluded/background pixels valid distances; plugin authors must preserve the declared units.
- `invalid_sample_policy: "all_uint16_included"`.

`/raw/camera` retains its legacy body: **processed** uint16 samples, fixed 640×480, row-major, native byte order. It is not necessarily unmodified SDK data: calibration, acquisition averaging, and ordinary plugin edits may apply. The last pixel is now an actual processed sample, **never a frame counter**. Metadata is outside the body.

Binary/OBJ/JPEG snapshots carry `X-Chiton-Frame-Id`, `X-Chiton-Captured-At-Ms`, `X-Chiton-Configuration-Revision`, `X-Chiton-Frame-Type`, `X-Chiton-Width`, `X-Chiton-Height`, `X-Chiton-Layout`, `X-Chiton-Units`, `X-Chiton-Source-Units`, `X-Chiton-Units-Scope`, `X-Chiton-Invalid-Sample-Policy`, and `X-Chiton-Byte-Order`. These headers are copied with the owned snapshot under the frame lock; a separate `/frame/camera` request may describe a newer frame. Live responses use `Cache-Control: no-store`.

- Raw layout: `processed_uint16_row_major`; byte order is `little` or `big`.
- Point cloud: legacy signed int16 `x,y,z,depth` records (`int16_xyzd`) in processed sample units. This legacy representation targets conventional sensor/calibrated ranges, not arbitrary full-uint16 cloud depths. Raw/statistics tests exercise the full uint16 range independently.
- OBJ: the existing x/y/z coordinates **divided by 6000** (`obj_xyz_divided_by_6000`), labeled `normalized_6000_<source-units>`; not newly asserted to be meters.
- JPEG: `image_intensity`, with actual encoded dimensions; not a numeric depth export. JPEG/OBJ byte order is `not_applicable`.

Disabled/not-ready capture yields HTTP 503 for measurement/frame requests.
F09 now makes retrieval observational: mode changes are explicit and global;
cloud/OBJ requires cloud mode (4), and incompatible outputs return 409. Camera
reads support bounded `after`/`timeout_ms` freshness queries. Frame JSON adds
`color_mode`, `current_mode` and monotonic `frame_age_ms`; snapshots add
`X-Chiton-Color-Mode` and `X-Chiton-Frame-Age-Ms`. Both UIs show host receipt age,
not exposure time. See [http-contracts.md](http-contracts.md) for exact limits.

## Legacy plugin compatibility

`process_standard_frame` keeps its ABI. It receives a **private scratch frame** with the old modulo-65536 counter (zero-based) in the final element. Ordinary pixel edits are copied back; edits to the final sentinel are not. The live processed final pixel and SDK final pixel are never stamped with metadata. The callback must not retain its pointer beyond the call. No hardware/plugin metrology or redesigned plugin ABI is claimed.

## Verification

```sh
sh tests/run_safety.sh
node tests/web_ui.js
CHITON_ASSET_DIR="$PWD/release/files" python3 tests/http_measurements.py /path/to/isolated/stub/chiton
CHITON_ASSET_DIR="$PWD/release/files" python3 tests/http_safety.py /path/to/isolated/stub/chiton
# Optional complete-page Chromium check, using only a local mock sensor:
node tests/web_stream_browser.js
```

`measurements.cpp` checks actual helpers/history/legacy plugin adaptation under ASan/UBSan: constant, gradient, checkerboard, random and full-range references; empty/startup/reset handling; oldest eviction without destructive sorting; separate clients; duplicate frames; context changes; LRU/expiry; and plugin sentinel isolation.

`http_measurements.py` uses isolated simulator processes and temporary fixtures/configuration state, including a tiny temporary legacy plugin. It checks full-frame/subregion/reversed ROI results, explicit client isolation/reset/deduplication, depth/IR context changes, metadata headers, last-pixel preservation, and disabled-capture errors. It refuses hardware builds. Normal and fully ASan/UBSan-instrumented application runs passed; full-app leak detection remains disabled for pre-existing module allocations. Focused helper leak detection stays enabled. CMake/CTest, TSan, and real hardware/plugin behavior were not validated.
