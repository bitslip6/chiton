# Offline point-cloud artifact investigation

Status: local tooling implemented and supplied OBJ examined; **no production fix or confirmed device root cause**. Governing scope: [investigation plan](pointcloud-artifact-analysis-plan.md). No physical camera access, deployment, new endpoint, or projection/calibration/filtering changes. Existing modified/untracked console and release work was retained.

## Run

Python 3 standard library only; no network or application process:

```sh
python3 tools/analyze_pointcloud.py ./chiton-pointcloud.obj \
  --out _build/pointcloud-analysis/my-case --recover-export-integers \
  --note 'Record known source/release/frame/settings and remaining gaps here'
python3 tests/test_analyze_pointcloud.py
```

`--out` must be new. The tool saves original bytes, SHA-256/provenance manifest,
JSON statistics and independent orthographic **point** plots (XZ, XY, YZ), without
meshing/connections or validity filtering. `uniform.svg` is the unchanged baseline;
`zero-y.svg`, `radius.svg` and `order.svg` are labeled diagnostic colorings.
`distributions.svg` shows radius, elevation and azimuth histograms. Plot-only
stride caps displayed samples at 12,000 per panel; all records participate in
statistics. SVGs open directly in a browser; optional `rsvg-convert file.svg -o
file.png` makes PNGs. API axes are not established gravity/world axes.

OBJ input is ASCII vertex-only Chiton-format data, bounded to 24 MiB and 307,200
vertices and the signed export coordinate domain. Blank/comment lines are allowed;
faces, extra vertex fields, nonfinite/out-of-domain coordinates are rejected.
XYZ is multiplied by **6000**, not interpreted as meters. Without the recovery
flag, the serialized coordinate precision is retained. Opt-in integer recovery
uses nearest-integer rounding, checking the signed-int16 lattice and the export's
maximum 0.3 source-unit serialization error per component. This is appropriate
for the supplied known-format download, not a general inverse projection.

For a saved binary capture, supply **that body's own headers**, as a JSON object:

```sh
python3 tools/analyze_pointcloud.py saved-cloud.bin --headers own-headers.json \
  --out _build/pointcloud-analysis/binary-case
```

Headers use actual names, e.g. `Content-Type`, `X-Chiton-Layout`, etc. The tool
checks MIME, mode/type/layout, frame/revision, count/length, units/scope/policy and
byte order, retains original header bytes, and bounds the body to 2,457,600 bytes.
It preserves signed XYZD, including the first record, origins and negative depth;
it never reinterprets negative D as unsigned. Depth coloring and positive-signed-D
`norm(XYZ)-D` residuals are available only for binary input. OBJ has **no** depth,
pixel identity, frame identity, calibration state or source-unit acknowledgment;
none is invented. Origins remain counted, excluded only from undefined angles.

## Production-path geometry fixture

Only diagnostic targets need to be built. This does not build/run the application
or link a camera SDK. CMake still requires the repository's normal host development
headers/libraries. Rediscover library override paths on another host.

```sh
cmake -S . -B _build/pointcloud-analysis/build-normal \
  -DUSE_ADITOF_STUB=ON -DUSE_SDL2=OFF -DCHITON_DEFAULT_DISPLAY=headless \
  -DZLIB_LIBRARY_RELEASE=/usr/lib/libz.so -DI2C_LIBRARY=/usr/lib/libi2c.so
cmake --build _build/pointcloud-analysis/build-normal \
  --target pointcloud_geometry_test -j4
ctest --test-dir _build/pointcloud-analysis/build-normal \
  -R '^pointcloud_' --no-tests=error --output-on-failure
_build/pointcloud-analysis/build-normal/pointcloud_geometry_test \
  --sidecar _build/pointcloud-analysis/example.csv
```

`tests/pointcloud_geometry.cpp` includes the **production** calibration translation
unit, with private output/SLAM storage and logging stubs, plus production
`core.cpp`/`shapes.cpp`. There is no second synthetic optical projection presented
as ground truth. Optional `--config sanitized.json` accepts exactly the four
integer geometry fields `center_x`, `center_y`, `inner_dist`, `outer_dist`.
Without it, sidecar geometry is the **repository example**, not assumed device
settings: `(314,174,130,295)`. Custom configurations with no emitted records are
rejected with a diagnostic and exit status 2, without creating sidecar files.

Sidecar output is synthetic constant-800 CSV linking emitted order to source
pixel/offset, integer/float radius, lookup segment (`-1` unsupported), factors and
norm, model elevation, input depth, pre-narrowing floats and published XYZD.
Sibling `.svg` and `.json` files show source support/model elevation and classify
zero-Y contributions. All three paths must be new. No live protocol is modified,
and this CSV must **not** be matched to the supplied OBJ by record order alone.

Fixtures cover constant depths 0/1/800/6000/32767, model-domain-only and crossing
annuli, both sides/exact knots, slight float-radius extrapolation, shifted center,
ramp/edge, independent declared ray/plane intersection, EMA warm-up/on/off,
irregular/short/unaligned runs, row transitions, offset zero and an explicit run
reaching the final pixel. Four independently declared asymmetric source directions
also verify API XYZ signs and axes through factor generation and actual emission;
these expectations do not reuse production angle/factor helpers. Checks include factor norms/finiteness, intended AOI,
run/index uniqueness, exclusive-end sampling, first/last records, count/capacity,
output canaries, exact scalar narrowing, and quantized radius residuals.
The allocation is the actual `640*480*3` signed components, not a fictional
full-density XYZD buffer. Run-builder omissions are reported, not repaired.
Calibration-enabled processing, optional SIMD/plugin paths and actual SDK depth
semantics are not qualified by these fixtures.

32768/65535 are explicitly outside positive signed-D representation: the producer
converts a float to int16 out of range. The safe suite does **not execute undefined
conversions** or claim lossless full-uint16 geometry. It tests safely representable
maxima and preflights every executed conversion. UBSan additionally enables
`float-cast-overflow` for this target; no wrapping output is treated as valid.

## Supplied capture: evidence and bounded findings

Original: `./chiton-pointcloud.obj`, 978,704 bytes.
SHA-256: `bd7fc132c644d0f932aba0e2977805b7ec5e1c84ebaba46c448fc90c54942a96`.
Preserved evidence and plots:
`_build/pointcloud-analysis/supplied-obj-final/`.

| Observation | Result |
| --- | --- |
| Returned vertices | 39,957 |
| Exact Y=0 | 3,811 (9.54%) |
| Origin records | 549 (1.37%) |
| Non-origin Y=0 | 3,262 |
| Absolute Y <= 1 source unit | 3,887 |
| Radius, source-unit scale | 0–6675.28; median 2006.17 |
| Non-origin Y=0 radii | 1–6080.39 |

The independent point plots visibly contain a concentrated zero-elevation band;
this subset **exists in exported coordinates**, not just annotation geometry.
The operator's exact cone-like screen feature cannot yet be identified against a
screenshot. A constant Y plane also appears line-like in XY/YZ projection, so
that appearance alone does not prove a bad optical ray. Directional bins at
0.5/1/2 degrees quantify broad radial spans, but may mix legitimate surfaces,
quantization and low-radius points: they are not an automatic defect classifier.

The production example fixture emits **40,979** records: **4,411 Y=0**, consisting
of **1,095 below radius 137**, **3,212 above radius 285**, and **104 supported-domain
points quantized to zero Y** at depth 800. No exact supported horizon sample is
emitted in this particular fixture. The source/model plot visibly reproduces the
elevation collapse on unsupported image radii. Evidence:
`_build/pointcloud-analysis/production-example.csv{,.json,.svg}` and
`production-example.png`. This establishes a reachable producer/model-domain
mismatch, **not correspondence to the affected capture**: its record count differs,
and the deployed configuration/build/plugin/runlines are unknown. The corner-center
fixture also reports one AOI-admitted pixel omitted by the existing run builder;
that is diagnostic sampling evidence, not tied to the reported artifact.

| Hypothesis | Supporting evidence | Limit / counter-evidence |
| --- | --- | --- |
| Annotation-only | Console has cloud-centered axes | Export itself has a zero-Y concentration; no screenshot/Overlay-off comparison to identify the reported feature |
| Unsupported image radii collapse elevation | Production fixture directly links unsupported source pixels to zero factors and Y=0 | Effective device AOI/build unknown; example and capture counts differ; valid/quantized horizon points also have Y=0 |
| Browser-only geometry corruption | Could still affect the reported view | Independent plots contain the band without console rendering; no same-body frozen binary replay available |
| Low/zero/variable processed depth creates radial samples | Capture includes origins and broad radius distributions | OBJ omits D and source pixels; cannot distinguish input, processing, multipath or projection causes |
| Sampling/index defects | Active emitter is stride four; fixture audits exact order/tails and reports run-builder omissions | Sparse sampling can expose bands without incorrect coordinates; no correspondence to capture proven |

**Conclusion:** the zero-elevation concentration is real exported geometry; the
unsupported-radius fallback is reproduced in production code and remains a strong
candidate. The available capture does **not** establish which source pixels made
its points, why its origins occurred, or that this candidate explains the exact
reported screen feature. No optical/root-cause certainty is claimed.

## Fix proposal / next evidence gate (not implemented)

The smallest next evidence request is the affected screenshot (ideally Overlay
off), effective four-field geometry and deployed build provenance. A binary body
with its own headers would enable signed-depth residuals and a truthful frozen
renderer replay. Do not request whole HARs, credentials or remote camera access.
If the actual AOI excludes unsupported radii, downgrade this lead.

If exact-build/config reproduction confirms unsupported radii are responsible,
first obtain the intended optical-domain/AOI contract. Then propose either an
approved completed model or an explicit unsupported-geometry/AOI policy, with a
**failing production-path regression expressing that approved behavior**, boundary
and legacy-export checks. The present tests characterize existing behavior, not
an approved correction. Do not silently crop, remove all zero-Y/low-depth points,
increase sample density, change signs or apply calibration twice.

Any subsequent proposal must state record-count/order and binary/OBJ impact,
measurement independence, rollout/rollback and hardware acceptance. A model/AOI
change needs a separately approved release and reversible configuration/binary
baseline; acceptance needs controlled physical scenes and acknowledged depth
semantics. No fix/deployment is authorized by these local investigation results.

## Validation and retained work

- **20 Python tests passed** with `CHITON_POINTCLOUD_GEOMETRY` set to the compiled
  `pointcloud_geometry_test` path (also set automatically by CTest): parser bounds,
  malformed input, both endian orders, signed first record, empty/max input,
  metadata, stats, residuals, CLI evidence preservation/no-overwrite and graceful
  rejection of empty custom geometry. Without that variable, the 19 analyzer-only
  tests run and the compiled-CLI regression is explicitly skipped.
- Focused CTest: **2/2 passed in each normal, ASan and UBSan build**. ASan leak
  checking enabled; UBSan includes explicit float-cast-overflow checks.
- Actual supplied OBJ analyzed; parent inspected uniform, highlighted,
  distribution and production support PNGs. Input SHA agrees with preserved copy.
- Logs: `_build/pointcloud-analysis/validation/`.
- Pre-existing patch/status/untracked hashes:
  `_build/pointcloud-analysis/implementation-baseline/`.
- Full release/browser/hardware suites were not run: production runtime/assets
  were unchanged. No same-body recorded binary, effective device configuration,
  screenshot, deployed bundle or physical-scene evidence was supplied.
- Fresh-context read-only critic completed review; report:
  `_build/pointcloud-analysis/review/critic.md`. Both findings were accepted and
  corrected: independent asymmetric axis/sign expectations, and graceful empty
  geometry rejection plus a CLI regression. Normal/ASan/UBSan focused CTest and
  all 20 Python tests passed again. A scratch-only production X-sign inversion
  now fails specifically in `asymmetric_orientation`, proving the new regression
  catches the previously untested mirror; `validation/sign-mutation.log` records
  it. Actual production source was not modified. Parent accepted the bounded
  corrections; critic did not independently run tests or approve hardware.
