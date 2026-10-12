# Illumination: source evidence and blocked production contracts

**Historical prerequisite checkpoint.** The original findings below are preserved
as inspection evidence. Subsequent operator-supplied contracts/assumptions and the
new default-off integration are documented in
[illumination-controller-integration.md](illumination-controller-integration.md).
In particular, the operator approved 1..12, reverse independent-sector mapping,
OK/all-four acknowledgment, depth <250 as a temporary proxy, bounded one-frame
lag, and assuming non-OK leaves the old tuple unchanged. These are operator
assertions, not newly discovered SDK documentation or hardware qualification.

## Inspection / authority

Baseline inspected: `63ee1cc16197d698a53f015d16c116e1089aac65`.
Initial `git status --short` contained only the operator's untracked
`docs/dynamic-illumination-controller-plan.md`; it remains untouched.
No build, test binary execution, ARM execution, SDK/device access, deployment,
SD write, calibration/configuration change or artifact replacement was performed.
`git diff --check` is a source whitespace check, not compilation/test validation.

Read the handoff and all six required references completely: camera pipeline
performance plan (including phase 5), AArch64 cross build, deployment bundle,
image streaming, performance metrics and SIMD. Inspected the specified current
camera/control sources, headers, lifecycle/stub tests, packager and wrapper.
Also inspected the SDK recipe/patch and extracted the text of the supplied
`Orbital Sensor Start Up Guide/Jabil_Orbital_Sensor_User_Manual_Nov_11.docx`.
The manual describes safety/setup/UI and processed RAW downloads, not the missing
control contracts. Its historical setup commands were NOT executed.

The recovered SDK package at
`/home/cory/Work/chiton-cross/yocto-sd/stage/lib/cmake/aditof/aditofConfig.cmake`
imports `libaditof.so.1.5.0` but selects this checkout's `include` directory.
No independent aditof headers were found in the recovered sysroot. Repository
headers and the simulator must not be mistaken for verified driver semantics.

## Reproduced legacy findings (unchanged)

- `calibration.cpp::find_min` takes minima; `dynamic_illumination` uses sectors
  [128,320), [320,512), [512,704), [704,896). `vector_slam` uses processed samples
  greater than 250, fills empty buckets with 9999, removes spikes and smooths
  against history. These are not saturation observations.
- `min_dist_to_power_level` casts the unclamped `core::value_map` of 400..2000
  into signed 8-bit -1..13, uses +/-2 deadband, moves roughly halfway and returns
  unsigned 8-bit. 9999 maps to about 82; negative results can wrap on return.
  Out-of-domain floating-to-signed casts are not a portable fixture contract.
- `main.cpp` begins `beta_ctl=0`, software values=12. Neither establishes actual
  hardware state. Active writes are at `frame_number % 3 == 1`, reverse tuple
  bytes into addresses 0x4011..0x4014, and discard SDK status. Other matches in
  that file are commented-out code, not active alternate writers.
- Dynamic mode uses calibrated/smoothed/plugin-processed depth. Mode 0 computes
  control before refreshing the selected ten-slot SLAM history; initial zeros
  and approximately ten-frame-old feedback are possible. Other modes refresh
  SLAM first. No legacy ordering/cadence/value semantics have been corrected.
- Fixed mode requests `0x0C0C0C0C`, including IR. The dynamic UI toggle selects
  the existing heuristic, not this experiment.
- The plausible high-power/distorted-short-depth/reduce/recovered-depth/increase
  feedback mechanism is demonstrated by a fictional fixture, NOT established
  as the hardware fault by code inspection.

## Original contract matrix (before subsequent operator answers)

| Contract | Evidence | Still required |
| --- | --- | --- |
| Saturation classifier | `FrameDataType` offers RAW/DEPTH/IR; `FrameDetails` only dimensions/type; no documented flags/confidence/amplitude classification | Matching SDK/sensor documentation defining sample validity, saturation units and detector for each supported frame type. Low/zero/missing depth, IR brightness, display clipping and `shapes.cpp` depth buckets are not proof. |
| Sector assignment/coupling | Software angle-bucket sectors and reverse byte writes | Physical emitter/register orientation, pixel assignment, overlap/crosstalk and qualification of independent or coupled control. A permutation test proves encoding only. |
| Register encoding | `DeviceInterface::writeAfeRegisters` accepts uint16 arrays; software requests 12 | Permitted encodings, monotonic power direction, limits/zero legality, tuple latch/atomicity and partial-write behavior. Neither uint16 nor 12 establishes safety. |
| Acknowledgment | Header calls OK “Success”; BUSY/error enums exist | Whether OK acknowledges the entire tuple, how uncertain/partial calls behave, trustworthy readback/initialization recovery. Non-OK is never treated as unchanged hardware. |
| Repeat writes | Current three-frame opportunities unconditionally write | Heartbeat/refresh/latch/recovery requirements; whether identical refresh changes effect generation/settling. No write suppression introduced. |
| Exposure association | Host frame IDs/receipt timestamps published after acquisition | Device exposure/command generation metadata, or documented worst-case frame delay/settling including buffered frames. Receipt times do not qualify frames. |
| Initial/lifecycle state | Capture tracker acknowledges start/stop; startup BUSY adoption requires an actual successful first frame | Known tuple/readback, effects of initialization/restart, exposure/mode/frame-type changes, dynamic transitions and recovery procedure. No startup stop/start normalization or BUSY retry added. |
| Other writers | `settings.cpp` generic register writes share `cam::state_mutex()` with camera worker | All writes to these addresses must invalidate/block experimental operation. Current generic path does not track controller state. SDK firmware/initialization internals are unavailable. |
| Modes / current frame | Raw DEPTH/IR acquisition precedes display/calibration; cloud/planar/SLAM are derived representations | Qualified detector independent of display/smoothing/calibration, extracted from immutable current measurements before modification. Production tests for 0/1/3/4/5/6 remain blocked. |

`src/stubs/aditof.cpp` is a file-backed simulator with immediate map-backed
register writes; it does not establish real latency/atomicity/range. It was not
modified. `tests/camera_lifecycle.cpp` verifies startup adoption without an
artificial stop/restart; that behavior is untouched. Existing packaging/launcher
and accepted binaries/bundles are untouched.

## Review checkpoints (no commits made)

1. **Evidence / diagnostic fixture:** this report and the test-only `legacy`
   function in `tests/illumination_fixture.h`, exercised in
   `tests/illumination_controller.cpp`. Limit the reproduction to representable
   400/2000 inputs; do not pretend to define legacy out-of-range casts.
2. **Pure bounded policy / synthetic responses:**
   `include/illumination_controller.h`,
   `src/graphics/illumination_controller.cpp`, the fixture/controller tests and
   `docs/illumination-controller-tests.md`. All parameters are injected, with no
   production defaults. No camera/SDK dependency in policy.
3. **Injected register boundary / fake tests:**
   `include/illumination_register_adapter.h`,
   `tests/illumination_register_adapter.cpp` and shared fixture. Only SDK status
   enum declarations are used. This is NOT a production DeviceInterface wrapper.
   Its implementation must wait for the contracts above.
4. **Optional test registration:** `tests/illumination/CMakeLists.txt` can be
   configured independently; root CMake has a default-OFF fixture-test option.
   Neither build path links the policy into Chiton. Makefile source lists are
   explicit and unchanged. There is no production experimental selector to
   accidentally activate or silently fall back; selection remains unavailable.

At this original checkpoint, stages D production integration, E
extraction/metrics/UI/manifest/launcher policy selection and hardware qualification
were deliberately NOT implemented. The later source integration is separate;
hardware qualification remains pending. Fake
owner-invalidation tests are not proof of production lifecycle-hook coverage,
mutex collision tracking, current-frame extraction or disabled-path regressions.
Those tests need the contracts and a separately reviewed integration change.

## Questions before proceeding beyond pure/fake work

Please supply/approve: (1) matching SDK/driver and sensor saturation metadata
specification; (2) legal encodings, power direction, physical mapping/coupling,
atomicity and repeat-write contract; (3) acknowledgment-to-exposure/settling bounds;
(4) initial state/readback, all-writer ownership/invalidation and documented
recovery; (5) supported modes and independently justified scene reset policy.
Do not guess these from this fixture. Keep production control disconnected until
those answers and external correctness/hardware gates exist.
