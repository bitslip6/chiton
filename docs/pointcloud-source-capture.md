# Same-frame cloud → sensor-pixel diagnostics

An **opt-in, one-shot diagnostic**. Schema 2 excludes missing/unrepresentable
samples from the diagnostic cloud instead of aborting a capture. The ordinary
live application/API and cloud bytes are unchanged. A separate offline viewer
maps picked cloud record IDs to the exact sensor pixels recorded by the producer.
Implementation scope: [`plan-pointcloud-source-pixel-capture.md`](../plan-pointcloud-source-pixel-capture.md).

## Safety and acquisition

**Target execution/deployment is an operator action requiring separate approval.**
Stop the normal camera-owning Chiton instance using your established procedure
before starting the diagnostic. Never run two camera owners. This process has no
HTTP, monitor, Ethernet/networking or display worker, does not load plugins, and
exits after one capture. It does not restore/start the ordinary service itself.

It loads the same persistent configuration/calibration through the existing
`CHITON_CONFIG_FILE`, `CHITON_CALIBRATION_FILE`, `CHITON_RESOURCE_DIR` resolution.
Runtime-only settings from a previous live process are **not persisted or adopted**.
The ordinary startup defaults are depth, EMA off, calibration on, legacy dynamic
illumination off. No file is rewritten to force those settings. Invalid existing
calibration files are refused rather than silently replaced in this diagnostic.
A missing calibration file uses the ordinary legacy coefficient construction;
its effective coefficients are saved, not assumed to match a different process.

The diagnostic rejects smoothing, plugins (even an unused `api.so`), calibration
sampling, IR acquisition, dynamic or experimental illumination. It deliberately
uses the ordinary SDK mode/noise-reduction/geometry-control setup and records it.
It does not supply an IR frame or claim depth/IR synchronization. Normal startup
resource loading and camera setup may allocate; the recorder/export phase uses
startup-reserved memory and no extra source/XYZD snapshot arrays.

After installing a separately approved build, an operator can run, for example:

```sh
# Target: only after stopping the existing camera owner. Parent directory must exist.
# The bundle launcher establishes the existing private-library/resource/state paths.
/usr/local/chiton/current/run.sh \
  --diagnostic-capture-dir /home/root/captures/arm_case_001 \
  --diagnostic-settle-frames 3 --diagnostic-timeout-seconds 30
```

Choose an appropriate existing parent and a **new** child directory; nothing is
overwritten. A bundle's `chiton`/`chiton.sh` launcher symlinks also forward these
arguments. Do not bypass its SDK library/resource environment accidentally.
`--display headless` is accepted but no presentation backend is created in this
mode. `--sampled-boundary-cleanup experimental-mm-v1` optionally records default
cleanup classes; `--sampled-boundary-cleanup aggressive-mm-v1` explicitly selects
algorithm 3 with the existing default evidence/gap/margin values (not the offline
120/30 reference tuning). Classification is off by default and never changes original XYZD.

Settle count 0–300 discards acquisitions before taking the single retained frame.
It does not mean EMA warm-up (EMA is rejected). The timeout 1–300 seconds bounds
capture setup/export with a process watchdog. SDK/filesystem hangs cannot always
be gracefully stopped: watchdog exit 124 leaves an incomplete capture, not a
promise that camera hardware recovered. Verify shutdown/recovery before restarting
the ordinary application. Ordinary errors attempt camera stop; successful capture
stops acquisition **before** writing the completion manifest. SIGINT/SIGTERM are
checked between acquisitions and writes; the watchdog bounds a stuck syscall.

The retained SDK `Frame` owns the depth plane. No new request occurs during export.
Read-only disassembly of the recovered SDK confirms FrameImpl allocates its own
storage, returns its plane pointers and writes into the Frame on request. The
hardware SDK's depth+IR container is 640×960, with a 640×480 depth plane; the
SDK-free stub reports 640×480. The diagnostic distinguishes those layouts and
rejects other dimensions/types. This is source/binary inspection, not hardware
qualification for every SDK generation.

## Files and identity

Successful capture has `capture.json`, `SHA256SUMS`, and **no** `INCOMPLETE` marker
or `capture.json.tmp`. Any incomplete directory is rejected by the host tool.
Failure files are retained for investigation, not silently completed or reused.

| File | Layout |
| --- | --- |
| `sensor-depth.u16le` | Original SDK DEPTH plane, full 640×480 unsigned row-major; SDK units explicitly unknown |
| `processed-depth.u16le` | Exact full cloud-input buffer, unsigned row-major; mm when calibration applied |
| `cloud.i16le` | Original signed XYZD, four int16 values per record |
| `source-index.u32le` | Original row-major pixel offset per retained record, recorded inside the actual emission loop |
| `pixel-status.u8` | Full 640×480 exclusion mask; codes and counts described below |
| `record-model.f32le` | Per record: gain, offset, X/Y/Z factors; IEEE754 float32 |
| `cleanup-flags.u8` | Exact same-frame class prefix; zero when off |
| `runlines.json` | Actual `[start, exclusive_end]` pairs |
| `capture.json` | Schema/layout, frame/revision/time/process identity, binary SHA256, processing/SDK setup, geometry and applied cleanup configuration, sizes |
| `SHA256SUMS` | SHA256 of every data file and final manifest (not itself) |

All numerical data is explicitly little-endian; the capture writer refuses other
host byte orders/float formats instead of dumping a native struct. Maximum record
capacity is 230400. The extra startup-owned **derived index table** is 921600 bytes
(~0.88 MiB), plus a 307200-byte status mask (~0.29 MiB) and bounded scalar/hash/
model/formatting scratch well below 64 KiB. Total extra tables are ~1.17 MiB.
Raw depth and XYZD are written directly from existing owned buffers. Small SHA256
compression blocks are hashing state, not classifier depth-window staging.

Source X=`index % 640`, Y=`index / 640`; origin top-left, no mirroring. Emission
preserves the actual `start; i < (exclusive_end & ~3u); i += 4` loop, including its
existing alignment/tail behavior. Not every source pixel has a cloud record.
Never infer indices by inverting XYZ or assuming one stride across the whole AOI.

### Missing and out-of-range samples (schema 2)

Only captured runline pixels are processed. Invalid values do **not** abort any
diagnostic output; even an all-missing frame produces a complete bundle and PNGs
with an empty cloud. Original SDK depth is retained across the whole frame.
The diagnostic checks the actual calibration expression once, not both hypothetical
staged/fused outcomes. It never executes an unsafe conversion or clamps a bad
value into a plausible depth.

`pixel-status.u8` codes:

| Code | Meaning |
| --- | --- |
| 0 | Valid diagnostic value |
| 1 | Outside runlines (not inspected) |
| 2 | Zero/no-return original sensor value |
| 3 | Nonfinite calibration coefficient/result |
| 4 | Corrected value cannot safely convert to uint16 |
| 5 | Corrected value truncates to zero |
| 6 | Nonfinite projection factor/result at a sample candidate |
| 7 | Depth or XYZ cannot safely fit the existing signed-int16 cloud layout |

Calibration-invalid/no-return pixels have zero placeholders in the processed
frame, distinguishable by the mask; safely calibrated values that cannot be
projected remain in processed depth but are omitted from the cloud. Projection
checks apply only at the existing sample candidates. This is a diagnostic exclusion
policy, **not SDK confidence or a live sensor-validity policy**. No arbitrary
millimeter tuning thresholds are applied.

The manifest records the policy, per-code pixel counts, total sample candidates
and excluded sample count. Retained record IDs are compacted; the source-index
sidecar is authoritative and the host verifies its exact order against saved
runlines **and mask**, never the full unfiltered record count. Retained XYZD uses
the production projection loop and original factors. The ordinary live pipeline
is unchanged; a filtered diagnostic cloud is not claimed byte-identical to its
unfiltered live cloud. Optional cleanup flags use the diagnostic processed frame,
including zeroed excluded neighbors, rather than claiming normal-stream parity.

Only real SDK/context/topology/file/timeout failures abort capture. Schema 1
complete bundles remain readable as explicitly unfiltered legacy captures.

`acquisition_backend` explicitly distinguishes `hardware-sdk` and
`file-backed-stub`; synthetic test bundles are labeled `synthetic-fixture`. The
viewer and PNG footers preserve that label rather than implying live hardware.

The metadata's frame ID is local to this diagnostic acquisition sequence, not an
ID from a previous live process. Process/time/binary hash plus payload checksums
identify the bundle. Preserve the deployed package manifest alongside it for
additional source/toolchain provenance. This is integrity/provenance, not signed
or authenticated evidence.

## Host: validate, inspect, write images

Copy the completed directory to the host using your normal approved procedure.
The tool reads local files only. It has no camera address, device API or automatic
acquisition action. Python 3 standard library and the repository's bundled Three.js
are sufficient; use a WebGL-capable browser.

```sh
python3 tools/pointcloud_capture.py /path/to/arm_case_001 --validate-only
python3 tools/pointcloud_capture.py /path/to/arm_case_001 --out /existing/parent/new_analysis
```

Open the printed `http://127.0.0.1:PORT/` on the **same host**. The server binds
loopback, validates Host/Origin plus an export token, serves only allowlisted
assets, and writes only below the startup-selected new output directory. It has
no CORS or arbitrary-path API. Local captures contain private scene imagery;
keep them private. Stop with Ctrl-C. The web server is not for exposure on a LAN.

1. Use **Top · XZ**, **Front · XY**, or orbit/pan/zoom to see the unwanted points.
   API axes are not established gravity/world axes.
2. Enable **Select visible points**. Click a point or drag a rectangle. Picking
   uses a GPU record-ID pass with the same camera, point footprint and depth test,
   not an approximate inverse sensor projection. It selects visible fragments;
   rotate to expose occluded points. Toggle selection off to navigate again.
3. Magenta selected points have matching magenta source-pixel centers in the two
   depth canvases. The first 32 record IDs are labeled/listed; all selected IDs
   go into CSV. Selection does not imply the points are physically invalid.
4. Change **display ranges** if useful. These affect visualization only. Original
   SDK units and calibrated mm have independent, explicitly labeled scales;
   they are not asserted equal. No numerical frame is normalized or edited.
5. Select **Write paired images + CSV to disk**. Each click creates a new
   `export_NNNN` directory; failed exports retain `INCOMPLETE`.

Outputs:

- `cloud-view.png`: the frozen view used for selection, with selected points.
- `cloud-top.png`, `cloud-front.png`: same frozen cloud and selections, API-axis views.
- `sensor-depth.png`, `sensor-depth-marked.png`: original organized sensor values.
- `processed-depth.png`, `processed-depth-marked.png`: exact cloud-input values.
- `comparison.png`: labeled side-by-side selected view/source/processed images.
- `excluded-pixels.csv`: excluded runline pixel X/Y/index, raw/processed values and
  reason, including excluded pixels that were not sample candidates.
- `selected-pixels.csv`: retained record/source X/Y/index, raw/processed depths, coefficients,
  factors, original XYZD, cleanup flag, projected floats, factor norm and comparison.
- `selection.json`: record IDs, actual camera/view/projection matrices, point style,
  viewport/DPR, depth scales and capture hashes. Cleanup hiding, clipping and EDL
  are explicitly off; cloud data is unchanged.
- `numerical-report.json`, `render-manifest.json`: all-record comparisons, immutable
  capture provenance and output hashes.

Depth PNGs contain **640×480 native image pixels plus an 80px footer**, never an
upscaled screenshot in place of source data. Hatched processed pixels are outside
the processed domain. Red pixels indicate diagnostic exclusions (also shown on
marked sensor images); the unmarked sensor PNG preserves the original full frame.
In-domain valid unsampled pixels still show actual organized depth.
Cloud labels project the first 32 selected centers, which may be occluded in a
*different* saved view; that distinction is written in the footer. CSV/index map
is authoritative. PNGs are visualizations; retain the lossless u16 files.

The host loader verifies hashes/counts/order, matches signed D to processed source
depth and compares retained XYZ to the captured factors. Schema 2 additionally verifies
mask/domain consistency, exclusion counts and filtered source order. Projection mismatches are retained
and reported as evidence, not silently corrected. Calibration comparison accounts
for staged float32 and possible compiler FMA; it is not a universal SDK-unit or
physical-distance claim. Coefficients are saved from memory, not regenerated from
current settings. A single capture cannot establish temporal instability, optical
multipath or sensor confidence. No geometry/filter/calibration fix is performed.

## Host-only checks

```sh
cmake --build HOST_BUILD --target pointcloud_capture_test pointcloud_geometry_test cleanup_delivery_test chiton
ctest --test-dir HOST_BUILD -R '^(pointcloud_capture|pointcloud_geometry|cleanup_delivery|pointcloud_capture_tools)$' --output-on-failure
python3 tests/test_pointcloud_capture.py --binary HOST_BUILD/chiton
```

The Python test refuses a native binary unless its CMake cache and embedded stub
signature establish an SDK-free build. It isolates resources and command tripwires.
Node/Chromium browser checks cover WebGL1/2 ID picking, occlusion, top/front/orbit,
DPR and exports; unavailable browser prerequisites are explicitly reported as
skips, not hardware or picking qualification. Allocation interposition and ASan/
UBSan builds are separate. The real Yocto-header cross build/package audit is an
additional gate, never execution of ARM diagnostic capture on the host.
