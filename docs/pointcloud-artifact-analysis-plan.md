# Plan: investigate point-cloud cone/ray artifacts

**Status:** investigation plan, not an implemented fix or confirmed diagnosis.

**Reported symptom:** cone-like lines near the apparent bottom of the point cloud, apparently extending from the sensor. Establish whether these are annotations, projected points already present in the exported data, processing artifacts, or incorrect geometry. Do not assume the screen's bottom is physical down or that the displayed axes mark the sensor origin.

This request authorizes planning and local source/offline analysis, **not physical-camera access, device-setting changes, deployment, new endpoints, or changes to calibration/projection/filtering**. Preserve the existing working tree and release for comparison. Relevant contracts: [measurements](measurements.md), [HTTP exports](http-contracts.md), [3D implementation requirements](console-3d-implementation-plan.md), [current console](console-preview.md), and [test safety](../tests/SAFETY.md).

## 1. Findings that inform the plan—not a diagnosis

| Current source fact | Diagnostic significance |
| --- | --- |
| `console-cloud-view.js::CloudView` draws `THREE.Points`, plus an `AxesHelper`. The axes helper is placed at the **cloud bounds center**, not API `(0,0,0)`. | First rule out visible RGB axes; they are annotations, not evidence of physical rays. Points aligned in space can look like lines without any line primitives. |
| Live XYZ values are copied unchanged from validated signed records into position attributes. The browser does not reproject sensor pixels. | Inspect exported coordinates before blaming camera orbit or a browser pinhole calculation that is not used. Still verify the actual deployed assets and frozen-data rendering. |
| `src/main.cpp` invokes `calibration::vector_point_cloud(calibrated)` after averaging/calibration and the optional legacy plugin. | `/raw/camera` and cloud input are processed data, not necessarily untouched SDK depth. The point inspector's local Kalman estimate does not affect cloud geometry. |
| `calibration.cpp::make_calibration_data` selects an annular/angular AOI, but its piecewise vertical-angle lookup only covers integer image radii **137 through 285**. `vertical_angle` starts at zero and has no alternative assignment outside those intervals. | **High-priority lead:** admitted pixels outside the lookup receive zero elevation, hence `Y = 0`, despite potentially different nonzero depths. Distinguish image-pixel radius from physical depth. |
| The repository's `release/chiton.json` example uses center `(314,174)` and inner/outer radii **130/295**; utility defaults also allow an outer radius beyond 285. | A supported-AOI/model-domain mismatch is reachable in the checkout. This is **not proof of the deployed device's effective configuration**; releases preserve external per-device state. |
| Projection factors are `(-sin(a) cos(e), sin(e), cos(a) cos(e))`; XYZ is processed depth times these factors, narrowed to signed int16. | Positive depth varying along a fixed direction naturally creates radial samples. Unit-vector algebra, depth semantics, integer conversion, and source values must be checked separately. |
| The current producer iterates `i = srcstart; i < (runlen & ~3); i += 4`, emitting one XYZD record per iteration. `runlen` is an **exclusive end offset**, not a count. | Audit the actual scalar sampling/order and alignment/tail behavior. Do not mistake the commented-out SIMD implementation for active four-lane output. |
| Legacy `app.html::draw_cloud` excludes depth `<600` and `-1 < Y < 0.5`, and mirrors X. For signed integer Y that band removes exactly `Y=0`. The new console retains all records and does not mirror X. | The old viewer could have concealed the suspect band. Reproduce its exclusions only as a labeled diagnostic comparison—not as a fix or sensor-validity policy. Valid horizon points can also have `Y=0`. |
| Binary cloud and OBJ share the same backend signed XYZ storage. OBJ divides XYZ by 6000, writes four decimal places, and omits depth/pixel indices. | An artifact in both exports points upstream of the browser, but agreement between them does not prove projection accuracy. Independent HTTP reads can represent different frames. |

Source anchors: `src/graphics/calibration.cpp::make_calibration_data`, `vector_point_cloud`, `average_frame`, `calibrate_frame`; `src/system/core.cpp::calc_distance`, `value_map`; `src/graphics/shapes.cpp::pixel_to_radians`; `src/main.cpp::camera_fn` and camera processing/publication; `src/data.cpp` cloud allocation; `release/files/{console-cloud.js,console-cloud-view.js,app.html}`.

### Existing local evidence

An offline summary of the **previously saved SDK-free constant-800 fixture**, not the reported sensor capture, found **4,411 of 40,979 records with API `Y=0`**, and no origin records. Recovered XYZ radii were approximately 798.34–800.00 in source units, consistent with a quantized constant-range surface. This establishes a useful baseline with a substantial zero-Y population; it does **not** identify all those points as unsupported or reproduce the reported physical artifact.

Summary/hash: `_build/pointcloud-analysis-plan/stub-summary.json`. Input: `_build/console-cloud-download-review/chiton-pointcloud.obj`. XYZ was recovered by nearest-integer rounding after multiplying this known-format OBJ by 6000; no depth field or pixel identity was inferred.

## 2. Evidence to preserve first

Obtain operator-supplied files before changing the scene, configuration, or rendering:

- An affected **Download cloud (.obj)** file and screenshot from the new console. Record release/archive name, timestamp, cloud frame label, units, source mode, and camera pose if available. A second angle helps distinguish a sheet/cone from a projected line.
- A screenshot with **Overlay off**. In 3D this hides the axes/legend, not point records. Note whether the suspect lines disappear, remain, or simply change appearance with orbit.
- Scene description: floor/walls, reflective or dark objects, nearby obstacles, motion, and whether artifacts occur only after startup/mode changes or persist after settling.
- Effective geometry settings: `center_x`, `center_y`, `inner_dist`, `outer_dist`; acknowledged Depth/mode 4; calibration, smoothing, plugin status and units/scope. Do not substitute repository defaults for these.
- Build provenance: deployed `manifest.json`, relevant `SHA256SUMS`/binary and console asset hashes. A commit ID plus dirty-file names is not a reproducible source snapshot; preserve the actual bundle and relevant working-tree patch when needed.

The OBJ and screenshot can already answer useful questions. If needed, request the binary XYZD body **with its own response headers**; it retains the fourth depth component that OBJ omits. A saved processed raw frame, calibration copy/hash and sanitized geometry configuration are later-stage evidence, not prerequisites for opening the first OBJ.

Keep captures private/local under ignored `_build/pointcloud-analysis/<case>/`. Save original bytes and SHA-256 hashes, annotate derived reports separately, and avoid collecting unrelated credentials, whole browser HARs, or network configuration.

## 3. Investigation sequence and gates

### A — Cheap visual and provenance checks

1. Establish whether the report concerns **live sensor data** rather than the explicitly synthetic demo projection.
2. Toggle Overlay off and orbit a fixed view. If only three colored axes disappear, classify those lines as annotations; their anchor is the cloud center, not the physical sensor.
3. Import the affected OBJ into an independent **point** viewer or offline scatter plot, with no meshing, point connections, smoothing or automatic outlier removal. Compare several views and a uniform color/point-size rendering.
4. Replay a single captured binary cloud through the console decoder/renderer in a loopback-only diagnostic harness labeled **recorded data**. Stop fetching and retain exactly one snapshot. Do not masquerade as a current live camera or manufacture matching measurements.
5. Compare uploaded XYZ/count with that snapshot, check draw range after shrinking clouds, and disable annotation geometry for comparison.

**Gate:** determine whether the suspect feature is an annotation, exists in the coordinates, or appears only during console rendering. If only a renderer issue is demonstrated, investigate that seam rather than changing optics/calibration.

### B — Describe the abnormal coordinates quantitatively

Build a small offline analyzer (proposed `tools/analyze_pointcloud.py`, **not implemented yet**) accepting local files only:

- Validate binary MIME/layout metadata, byte order, record count/body length, frame/revision/type/units/scope; preserve signed int16 XYZD, first record, zeros and negative components. Cap cloud input at 307,200 records / 2,457,600 bytes. Validate OBJ finite vertex-only input and bound file/record allocation as well.
- Report XYZ/depth histograms, origin/near-origin counts, exact and near-zero Y bands, signed extrema, record count and bounds. Negative XYZ alone is normal; negative decoded depth needs separate signed-range analysis, not unsigned reinterpretation.
- Plot XZ, XY and YZ slices; API elevation `atan2(Y, hypot(X,Z))`, azimuth `atan2(X,Z)`, and radius distributions. These are mathematical API coordinates, not established gravity/world axes.
- Exclude zero-radius points only from undefined angular calculations, while retaining/counting them in the dataset. Analyze directional bins for broad radial ranges that could form spokes. Record bin widths/tolerances and compare multiple resolutions so quantization or chosen bins do not manufacture a finding.
- Color diagnostic plots separately by returned depth, radius, zero-Y classification, and record order. Clearly distinguish an investigative highlight/subset from the unchanged baseline cloud.
- For safely representable positive depths, examine `norm(XYZ)-depth`. Unit factors imply an approximately constant-radius surface for constant input, with coordinate-truncation error below roughly `sqrt(3)` source units plus floating-point error. A good residual proves algebraic consistency only—not the optical model or physical distance semantics.
- From OBJ alone, do not invent the missing depth, frame ID, pixel index, or calibration state. Record normalization/rounding explicitly. Never compare `/6000` OBJ magnitudes directly to unscaled binary XYZ.

**Gate:** identify a reproducible subset—e.g. zero-elevation rays, a radius discontinuity, sign/range failures, boundary stripes, or transient trails—rather than just the visual phrase “cone lines.”

### C — Trace suspect records to actual production geometry

Use a local C++ diagnostic test/harness calling the **production** mapping/emitter functions, not the demo's synthetic projection or a second approximation presented as ground truth. Proposed test: `tests/pointcloud_geometry.cpp`.

1. Load a sanitized copy of the effective center/AOI configuration. Generate the actual factors/runlines and enumerate their published sample order.
2. Produce a **test-only local sidecar** linking record order to source offset/X/Y, integer/float image radius, angular segment, factor XYZ and norm, processed input depth, pre-narrowing float XYZ and published int16 XYZD. Do not add pixel-index records/prefixes to the live protocol.
3. Check every emitted index: within the sensor buffer and intended AOI, initialized finite factors, no overlaps/duplicates, correct exclusive-end interpretation, and record count/storage capacity. Test row transitions, unaligned/short runs, offset zero, and a run reaching the final frame pixel.
4. Specifically classify emitted samples at image radii **below 137, within 137–285, and above 285**. Test lookup knots and both sides of boundaries, including integer-radius branch selection with float-radius interpolation and possible slight extrapolation.
5. Plot pixel-space radius/angle/elevation and compare suspect records with their source region. Determine how much of `Y=0` comes from unsupported lookup radii, legitimate zero-elevation directions, integer quantization, or zero/low processed depths.
6. Audit the `i+=4` sampling and rounded end independently of any proposed fix. Sparse sampling may make banding visible without producing incorrect coordinates. Compare a separate full-sampling reference diagnostically, not by silently changing the shipped producer.
7. Audit cloud allocation before changing sample density: current storage is `640*480*3` signed components, while each record consumes **four** components. Current stride does not imply capacity for an arbitrary full-density frame. Do not replace `i+=4` with `i++` as a quick experiment without safe bounded buffers and a capacity proof.

Wire records contain no sensor-pixel indices. A forensic correspondence requires the exact producer/build/configuration (including any plugin changes to runlines), verified against controlled inputs. Do not elevate record order or inverse projection into a general UI picking contract.

**Gate:** for suspect points, demonstrate the responsible input pixels and intermediate calculations. If the actual configuration does not admit unsupported lookup radii, downgrade that hypothesis rather than forcing it.

### D — Controlled synthetic tests of the real producer

Use only SDK-free simulator/unit fixtures with isolated resources/config/calibration and verified stub identity before any runtime setup:

| Fixture | Expected diagnostic value |
| --- | --- |
| Constant positive depth, several safe values | Spherical patch under the current radial model; expose zero-elevation concentration and sampling seams without noisy sensor input. A constant-depth fixture is **not** a flat-wall fixture. |
| Annuli/sectors crossing lookup boundaries | Establish correspondence between unsupported radii and collapsed elevation, plus continuity/angle-sign behavior. Include current effective AOI and a supported-domain-only comparison. |
| Known plane generated by an independent ray/plane intersection | For unit ray `u`, use `d = L/(n·u)` on a declared safe positive-distance domain. Tests the projection against a plane rather than a copied formula alone. |
| Single-pixel/isolated patch, asymmetric directions | Verify exact source selection, signs/axes, first/last records and source-index correspondence. |
| Depth ramp and sharp foreground/background edge | Separate expected ray-aligned geometry from interpolation, processing, selection and quantization errors. |
| Zeros, low readings, safely representable maxima, and separately 32768/65535 | Characterize legacy signed-range limitations. Do not claim lossless full-uint16 cloud support or treat narrowing/wrapping as valid geometry. |
| Startup/mode switch and repeated constant frames, EMA on/off | Test zero-initialized processed-buffer warm-up and transient intermediate distances; confirm a frozen renderer does not accumulate previous clouds. |

Check finite factor norms, index/count/capacity bounds and float-to-integer conversions. Run focused ASan/UBSan and explicit float-cast-overflow checks where supported. Compare scalar/optional processing paths if relevant to the deployed flags. Do not execute a hardware-camera build as a simulator or run AArch64 hardware binaries on the host.

Calibration/gain/offset processing belongs upstream of `vector_point_cloud(calibrated)`. Its unused local gain/offset pointers are not evidence that calibration is missing: avoid “fixing” this by applying calibration twice. Establish whether supplied depth is radial range, axial distance, or another processed convention from the actual SDK/optical/calibration documentation before changing the physical model.

**Gate:** reproduce the identified pattern in a controlled case, or establish that geometry stays correct with controlled inputs and the sensor/processing data needs further examination.

### E — Additional physical evidence only if separately approved

If the supplied files and local reproduction cannot distinguish hypotheses, agree the target, release, read-only capture scope and operator/recovery procedure first. No camera contact is authorized by this plan.

Start with a static, safe scene and existing acknowledged Depth/cloud mode. An operator-approved collector can save a small bounded batch (e.g. 5–10 clouds at 1–2 reads/s), their headers, and allowlisted observational settings (`/dimension/camera`, `/frame/camera`, mode/depth/range read-back). Keep acquisition and calibration/illumination/plugin state unchanged initially. Avoid long `after=` waits on the sequential HTTP listener.

**Snapshot coherence is a hard requirement:**

- `/pointcloud/camera`, `/pointcloud_obj/camera`, `/raw/camera` and `/frame/camera` can all return different frames. Match each body's own headers: frame ID, revision, actual type/mode and units/scope; don't attach later frame JSON as if it described earlier bytes.
- `after=` means newer than an ID, not “retrieve that exact frame.” There is no existing exact-frame fetch contract.
- Pause viewer stops that page's work, **not sensor acquisition**. Disabling device capture is not a snapshot latch: exports return 503 while capture is disabled.
- Exact per-pixel numerical comparison needs a common saved input reproduced offline, an actually matched frame, or a separately approved coherent instrumentation capture. Near-simultaneous reads and a stationary scene are only approximate comparisons, not proof of identity.
- Record process/restart boundaries; frame IDs are process-local. Separate body/frame/revision mismatches from geometry defects.

Only after the baseline, and with explicit operator approval, vary **one** factor at a time: EMA, existing calibration enablement, a scene edge/reflective object, or another agreed scene condition. Calibration changes can change units; don't compare mm and sensor-unit magnitudes as if they were identical. Record before/after acknowledgments and restore known prior settings. No automatic calibration save, geometry edits, camera-mode repair, illumination/register experiments, firmware/network changes or deployment.

If artifacts follow raw/processed depth discontinuities despite verified projection, classify sensor validity, multipath/reflectivity or processing as a candidate—not a proved optical cause without appropriate evidence/SDK validity semantics.

## 4. Decision rules

- **Only annotation lines disappear with Overlay off:** classify the visible feature first; explain the cloud-centered axes or propose an annotation change. Do not alter XYZ to “fix” axes.
- **Frozen exported coordinates are correct but console rendering differs:** investigate decoder/endian/draw range/upload/representation state with the exact captured body; keep the backend model unchanged.
- **Unsupported radial bands correlate with zero-elevation spokes:** establish the actual optical domain and intended AOI policy before choosing correction. Options such as completing the model, aligning an AOI, or explicitly representing unsupported geometry require an approved contract—not a silent crop.
- **Both exports faithfully carry a reproducible incorrect projection:** fix the narrow producer/model/indexing defect only after a failing production-path regression exists. Binary/OBJ agreement alone is not independent optical validation.
- **Correct geometry exposes unstable/low/zero/ambiguous depth data:** document the data/processing behavior. A display-only filter, if desired, must be explicit and separately labeled; never silently declare all zero-Y, negative-coordinate, or depth-below-600 records invalid.
- **Insufficient coherent evidence:** leave the diagnosis unresolved, identify the missing capture/contract, and stop rather than guessing a threshold or calibration formula.

## 5. Deliverables and completion criteria

1. An evidence manifest with original capture hashes, effective configuration/build provenance, actual frame identities, source/units and known gaps.
2. Original-versus-diagnostic plots, including source-pixel support coverage when provenance is established; masks/subsets always labeled.
3. A deterministic local reproducer or a documented reason only physical evidence can currently reproduce it.
4. A hypothesis table with supporting/counter-evidence and a bounded root-cause conclusion—or explicit uncertainty.
5. A separate minimal fix proposal with a failing regression, protocol/measurement/export impact, rollout/rollback and hardware acceptance needs. Do not implement it merely because this investigation plan exists.

Prefer the shortest discriminating sequence: **axes toggle → affected OBJ/frozen binary → zero-elevation/support analysis → production geometry fixture → processing/approved physical comparisons if needed**. Stop expanding the investigation once evidence isolates a cause.

### Planning-time evidence and limits

Prepared by inspecting current producer, factor generation, processing/publication, binary/OBJ response construction, new/legacy renderer behavior and existing test contracts. Only an already saved SDK-free OBJ was summarized offline; the reported physical cloud has not been supplied or examined. No new geometry tests/tools, hardware captures, renderer changes, filters, calibration changes, or deployment were performed for this planning task. Existing modified/untracked implementation work was left intact.
