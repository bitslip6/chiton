# Live console boundary cleanup (experimental)

Implements the approved [integration design](../plan-cleanup-console-integration.md).
The classifier and parameters are described in [sampled-boundary-cleanup.md](sampled-boundary-cleanup.md).
**Off by default. No target deployment or sensor-CPU/effectiveness qualification is implied.**

## Operator workflow

Open `/console.html?source=live`, deliberately select Depth and 3D, open Display
controls → 3D → **Boundary cleanup · experimental**.

1. The device must provide calibrated-mm depth within its processed calibration
   domain. IR, sensor units, plugins, calibration sampling and invalidated topology
   remain unfiltered. Cleanup never changes acquisition/calibration automatically.
2. Choose **Enable server classification (CPU cost)** and an algorithm: Exact
   (revision 1, including the exact interior optimization) or Conservative early
   exit (revision 2, identical MixedCandidate rejection set, fewer other labels), or
   **Aggressive boundary bands** (revision 3, fixed radius 6, deliberately sacrifices
   ramps and thin intermediate surfaces). The aggressive choice appears only when
   the server advertises ID 3. Its anchor tolerance, ramp residual and both neighbor
   controls are not applicable; their values are preserved for switching back.
   See [the exact aggressive rule and limitations](aggressive-boundary-bands.md).
3. Edit thresholds/support parameters and select **Apply device settings**.
   This is a complete, atomic, device-global update, not a per-tab setting.
4. Select the independent **Local display** policy: Off / Highlight mixed suspects
   (magenta) / Hide mixed suspects. Local Off alone does NOT stop server CPU work;
   uncheck server classification and Apply to do that.

Read-back is acknowledged, not optimistic. Drafts retain their original generation
while other tabs update device state; stale writes fail with 409 rather than
clobbering the newer settings. Invalid/failed writes roll the form back and show
an error; Reload obtains the current state. Lost acknowledgments are never retried
automatically. Settings and generation counters are process-local; Reload after
a server restart. No persistence or cross-restart transaction epoch is added.

The display only acts on a compatible same-frame flag prefix with the acknowledged
configuration generation **and complete parameter values**. Observed restart or
counter/configuration reuse invalidates cached flags and drafts. Older pending frames, unsupported contexts, legacy/demo
clouds without flags and older servers remain unfiltered and explicitly marked
unavailable/pending. A known missing API falls back to the legacy cloud reader;
network failures do not silently turn a supported stream into a different format.

Only MixedCandidate is highlighted/hidden. Unclassified and BoundaryRisk remain
visible. Original signed XYZD, measurements, device raw/binary/OBJ exports, point
count, camera pose, framing reference, and clipping settings are unchanged.
Clipping and cleanup intersect in the early vertex rejection path, including EDL
color/depth rendering. Status distinguishes source classes, clipping matches and
the count after both predicates. PNG provenance includes requested/applied cleanup
generation, availability, local policy, applied algorithm/parameters and counts.

## HTTP API

Same origin as the existing server. Ordinary legacy endpoints keep their exact
formats. New endpoints bypass allocating legacy request/JSON/response helpers.
This is not authentication: use the existing trusted-network deployment model.
Browser POSTs carrying Origin must have an HTTP(S) authority matching Host;
cross-host or duplicate Origin is rejected with 403. Native tools may omit Origin.

### Read current configuration

`GET /api/cleanup/v1/config`

Returns bounded JSON with `generation`, `config`, publication `available`,
`frame_generation`, `frame_id`, numeric `status`, and parameter constraints. The
configuration is desired device state; an older publication may still be pending.
The `constraints` object describes storage/protocol bounds only, not tuning advice.
Statuses: 0 NotInitialized, 1 Off, 2 Ready, 3 UnsupportedContext,
4 InvalidConfiguration, 5 TopologyChanged, 6 UnsafeTopology.

### Atomically update parameters

`POST /api/cleanup/v1/config?<complete fields>` with **zero request-body bytes**.
All eleven fields are required exactly once, unsigned decimal, unescaped:

```text
enabled=1&algorithm=2&evidence_min=250&evidence_max=6000&boundary_gap=200&anchor_tolerance=40&center_margin=60&ramp_residual=30&surface_support=4&center_support=3&expected_generation=1
```

Use the generation from GET, not a guessed constant. For example (replace host
and expected generation):

```sh
curl http://HOST/api/cleanup/v1/config
curl -X POST 'http://HOST/api/cleanup/v1/config?enabled=1&algorithm=2&evidence_min=250&evidence_max=6000&boundary_gap=200&anchor_tolerance=40&center_margin=60&ramp_residual=30&surface_support=4&center_support=3&expected_generation=1'
```

`enabled`: 0/1; `algorithm`: 1/2/3, advertised in `constraints.algorithm`.
ID 3 uses only evidence min/max, boundary gap and center margin; the other fields
still round-trip unchanged but do not influence its decisions. All six depth/threshold parameters accept unsigned
integers 0–65535; both support counts accept 0–255 (their wire/storage ranges).
There are **no tuning limits or cross-field validation rules**: zero values,
reversed/equal evidence endpoints, overlapping tolerance bands, arbitrary margins
and support counts beyond the neighborhood size are accepted unchanged. No
clamping or automatic adjustment is performed. These choices can produce no
suspects or less conservative results. A reversed evidence interval supplies no
evidence; an empty anchor group cannot supply a mean even with zero requested
support. See the classifier document for units and meaning; the evidence interval
is not an SDK validity policy.

200 returns the committed complete state and incremented generation. Invalid,
missing, duplicate, unknown, overflowing or inconsistent fields return 400 with
**no mutation**. Stale expected generation returns 409 with no mutation. Camera
ownership busy for 100 ms returns 503; no camera ownership is held during response
serialization/socket writes. Tuning does not change measurement revision/history.

### Coherent cloud and flags

`GET /api/cleanup/v1/cloud[?after=FRAME&timeout_ms=0..1000]`

Requires enabled capture and cloud mode 4. It never changes mode. Returns
`application/octet-stream` with exact Content-Length, no-store and connection
close. One immutable lease covers the complete response. Response body:

```text
128-byte CHCLD001 envelope | original native-endian int16 XYZD (8*N bytes) | N flag bytes
```

Envelope integer fields are little-endian. Reserved bytes are zero. Layout:

| Offset | Type | Meaning |
| ---: | --- | --- |
| 0 | 8 ASCII | CHCLD001 |
| 8 | u32 | Header size = 128 |
| 12 / 16 / 20 | u32 each | Records N / geometry bytes / flag bytes |
| 24 / 32 / 40 / 48 | u64 each | Frame ID / source revision / host capture time ms / applied cleanup generation |
| 56 / 60 | u32 each | Applied algorithm version / status |
| 64 / 65 | u8 each | Units (1 mm, 2 sensor, 3 IR) / scope (0 full sensor, 1 calibration domain) |
| 66..70 | u8 each | Depth / smoothing / plugin / sampling / classification-enabled booleans |
| 71 | u8 | XYZD byte order: 1 little, 2 big |
| 72..82 | u16 each | Evidence min, max, gap, tolerance, margin, ramp residual |
| 84 / 85 | u8 each | Surface support / center support |
| 88..112 | u32 each | Gated, mixed, boundary, unclassified, retained, detailed, early-retained counts |

Flags are exclusive: 0 Off, 1 Retained, 2 BoundaryRisk, 4 Unclassified,
8 MixedCandidate. Off writes only the active prefix. Zero clouds have a 128-byte
body and zero flags. The client validates magic, lengths, capacity, units/scope,
algorithm, context, class bytes/counts and counter identities before accepting data.

Default reads return the latest complete frame, or 503 if unavailable. A supplied
`after` filters to a strictly newer frame. Positive `timeout_ms` bounds polling;
expiry returns 504. Capture disabled/stopping or exhausted read resources return
503; wrong mode or IR geometry returns 409; invalid queries return 400. Unknown
routes/methods return 404/405. Socket sends have a shared two-second deadline.
No application resource grows on errors, disconnects, retries or slow clients.

## Ownership, memory and allocation boundary

Three startup-owned slots replace the original cloud buffer and flag buffer.
Each reserves actual producer capacity: 921600 int16 components and 230400 flag
bytes. Total payload = 6220800 bytes; net additional payload ≈3.96 MiB. Pool
metadata/headers bring `sizeof(Pool)` to 6221600 bytes on this host. The producer's
server specialization has no full internal flag buffer; standalone fixtures retain
an internal-buffer specialization. The domain table reuses its interior bit.
Payload pages are touched before acquisition. The pool is constructed in BSS,
not stored as megabytes of zero bytes in the executable's initialized-data section.

The unchanged sample loop writes XYZD and flags directly into the writable slot.
There is no source-frame copy, staged neighborhood, duplicate XYZD or concatenated
geometry response. Only fixed derived metadata is serialized. Leases are bounded
to two, so one of three slots is always writable. Reader-owned slots are never
reused. If only the unleased latest slot is writable, withdraw it before writing;
new reads may return unavailable, never block capture or allocate a fourth slot.

Publication ownership has its own short mutex, never held for SDK or socket work.
Legacy exports still snapshot the completed cloud under their original camera
ownership; those pre-existing unrelated allocations are not part of cleanup.
The new framing front end uses a fixed 16384-byte request buffer, 100 header views,
4096-byte line bound, original zero-body/smuggling protections and one-second
receive deadline. It adapts to legacy owning strings/maps ONLY after cleanup routes
have returned. Fixed-size automatic header/JSON buffers use bounded, allocation-free
unsigned decimal formatting (including the recovered GCC 7.3 library baseline),
not dynamic JSON/string/stream builders. No extra production thread is introduced.

## Validation and performance evidence

- `tests/cleanup_delivery.cpp`: actual production emitter + pool + bounded API,
  first-call/lifecycle allocator and source-copy guards; read-only source; leased
  reuse/exhaustion; concurrent alternating immutable frames; config CAS/errors;
  partial-write timeout/disconnect release; zero/shrinking output.
- `tests/http_cleanup.py`: verified file-backed CMake server only; complete/stale/
  invalid tuning, desired/applied generations, wire/legacy XYZD identity, unchanged
  processed raw, capture progress through a slow reader, mode/disabled gates.
- `tests/console_cleanup.js`: signed/endian envelope and invalid-byte/count/config
  validation. `tests/console_cleanup_browser.js`: real WebGL1/2 highlight/hide,
  clipping, unsupported/pending prefixes, atomic/stale drafts, pose preservation
  and nontruncated PNG parameters. Existing native-browser tests exercise the UI
  against the actual C++ configuration API and verify unchanged device exports.

For a host-only full-publication/HTTP sender microbenchmark:

```sh
_build/sampled-boundary-cleanup/host/cleanup_delivery_test --benchmark
```

One unsanitized Release run on this host, 34532-point synthetic boundary clutter,
250 iterations per mode, measured mean **thread CPU** milliseconds:

| Mode | Producer including pool publication | HTTP framing + direct send |
| --- | ---: | ---: |
| Off | 0.125 | 0.039 |
| Exact | 1.007 | 0.037 |
| Early exit | 0.525 | 0.041 |

This uses an AF_UNIX socket-pair reader harness and excludes reader-thread CPU,
fixture setup, thread/socket creation and reporting. It is not Wi-Fi/TCP bandwidth,
whole-process CPU, acquisition latency or target cadence qualification. The
strong dense-mixed producer benchmark remains necessary; this clutter scene is
not the worst case. Browser allocations are not covered by sensor guarantees.

Evidence: ignored `_build/sampled-boundary-cleanup/integration/{validation,benchmarks,review}/`.
Rollback: disable server classification with Apply (or restart with default Off),
set local display Off; revert this integration to remove its reserved slot memory.
