# Dynamic illumination controller: implementation handoff

## Task and authority

Replace the oscillating distance-to-power heuristic with a separately qualified,
stateful saturation controller. This document authorizes planning/source work,
not device deployment, ARM execution, camera register experiments, SDK changes,
SD writes or changes to saved device calibration/configuration.

The operator has explicitly reserved **all builds for outside the agent**. The
implementing agent may add source/tests and document commands, but must not build
host or cross targets without renewed permission. Hardware comparisons and any
recovery/power cycle are operator-run. Do not run two camera owners concurrently.

Keep the replacement **default-off** until its observation and hardware contracts
are established and external correctness/hardware gates pass. Do not silently
change the existing dynamic mode when its current UI switch is enabled. Clearly
distinguish legacy dynamic mode, experimental controller, and ordinary fixed
illumination. A separate option selects the experimental policy; refuse that
selection if its required detector/contract is unavailable, rather than silently
falling back and pretending the new controller is active.

Do not combine this with SIMD, JPEG/transport changes, register-write suppression,
preview gating, camera startup/retry changes, calibration algorithm changes or
cleanup. Existing register cadence remains unchanged unless a separately reviewed
hardware contract requires otherwise.

## Required context and source inspection

Read completely:

- `docs/camera-pipeline-performance-plan.md` (especially phase 5 safeguards).
- `docs/aarch64-cross-build.md` (legacy startup/adoption failures).
- `docs/deployment-bundle.md` and `docs/image-streaming.md`.
- `docs/pipeline-performance-metrics.md` and `docs/pipeline-simd.md`.

Inspect current versions, not just the line numbers below:

- `src/graphics/calibration.cpp`: `find_min`, `min_dist_to_power_level`,
  `dynamic_illumination`, `vector_slam`.
- `src/main.cpp`: frame acquisition/processing, `beta_ctl`, four-register writes,
  SLAM history indexing, capture lifecycle and metadata publication.
- `src/system/core.cpp`: unclamped `value_map`.
- `src/webserver/settings.cpp`: dynamic enable and generic register writes.
- `src/system/camera.cpp`: acknowledged capture state and startup adoption.
- `include/aditof/device_interface.h`, `include/aditof/frame.h`, actual SDK headers
  and available sensor documentation; repository headers alone may be a stub.
- `tests/camera_lifecycle.cpp`, `tests/aditof_stub.cpp`, `src/stubs/aditof.cpp`,
  `tests/test_cross_build.py`, `tools/package_aarch64.py`, `cross-build.sh`.

Check `git status` before editing; preserve existing work and accepted artifacts.

## Findings to reproduce, not assume away

The existing implementation:

1. Takes sector minima from filtered/calibrated/smoothed SLAM depth, not an actual
   saturation/overexposure observation.
2. Maps distance 400..2000 to power -1..13 without clamping and moves approximately
   halfway toward that target, with a +/-2 power-level deadband.
3. Has no learned saturation ceiling, settling policy or observation hysteresis.
4. Treats illumination-dependent depth recovery as permission to raise power:
   high power -> saturation/distorted short depth -> reduce -> recovered depth ->
   raise -> saturation again. This is a plausible mechanism consistent with the
   reported behavior, not a hardware-proven diagnosis from code inspection alone.
5. Allows the SLAM empty sentinel 9999 to map to approximately 82; signed results
   returned as uint8 can wrap. The intended software full-power tuple is
   `0x0C0C0C0C`; **this is not proof of the sensor's permitted register range**.
6. In mode 0, calculates illumination before refreshing the selected SLAM history
   slot. Feedback can be old (about ten frames at steady capture) or initial zero.
7. Ignores the illumination write's SDK status, so requested and applied power
   diverge on failure. Actual cadence is `frame_number % 3 == 1`, despite comments
   saying five frames.

Do not modify these legacy semantics incidentally while adding the replacement.
Small legacy correctness fixes, if desired, are separate reviewed changes.

## Stage A — Resolve prerequisites; no new control behavior

Document the evidence/source and unresolved questions for:

- What proves saturation/overexposure? Prefer documented SDK flags, amplitude or
  confidence/validity metadata. Low depth, zero depth, clipping of a display image,
  and missing data are **not automatically saturation**.
- How are sectors assigned to physical vixels? Current sector ranges are
  [128,320), [320,512), [512,704), [704,896); current tuple bytes are written in
  reverse byte order to registers 0x4011..0x4014. Verify orientation/overlap/crosstalk,
  do not guess that these correspond one-to-one with independent emitters.
- Legal register encodings, increasing/decreasing power direction, limits, whether
  zero is permitted, tuple atomicity, partial-write behavior and acknowledgment.
- Whether repeated writes are required heartbeat/refresh/latch behavior.
- Command-to-frame latency/settling: receipt timestamps do not establish exposure
  time. Determine which frames can actually reflect an acknowledged new tuple.
- Known startup state/readback availability and effects of capture restart,
  initialization, generic register writers and dynamic-mode transitions.
- Which modes/frame types expose sufficient observation data? Depth, IR, cloud,
  planar and display mode must not silently change the control policy's meaning.

If these cannot be established, stop at pure controller/fake tests and diagnostics.
Do not invent production detector thresholds, register ranges, refresh intervals,
or a driver recovery sequence. Production experimental selection remains unavailable.

## Stage B — Pure controller and explicit interfaces

Suggested files (adapt naming to project conventions):

- `include/illumination_controller.h`
- `src/graphics/illumination_controller.cpp`
- `tests/illumination_controller.cpp`
- A small camera-owned SDK adapter/observation extractor, separate from the policy.

The pure controller must have no SDK, wall-clock, sleeps, filesystem, network or
hardware dependencies. Inputs contain:

- Monotonic frame ID/time and configuration/lifecycle generation.
- The generation of the power command that generated this observation, where the
  hardware contract supports it; otherwise an explicitly conservative eligibility
  policy based on documented frame delay/settling.
- Per-sector measured saturation count and eligible count, with an explicit
  known/unknown state and documented sample classification.
- An optional, independently justified scene-change signal. Saturation disappearing
  after lowering power is **not** evidence of a scene change.
- Write acknowledgments/failures from the adapter, not guessed applied state.

Keep state bounded and camera-worker-owned. Separate:

- Desired/proposed four-register tuple.
- Last acknowledged applied tuple; initially unknown unless established.
- Pending/uncertain command and command generation.
- Per-sector known-safe operating level and learned upper ceiling.
- Settling eligibility, consecutive clear observations and failed probe/backoff.
- State/reason enum, e.g. unknown, settling, reducing, holding, probing, capped,
  write-failed or unsupported-observation.

All power arithmetic uses a signed integer type, with final encoding only after
validated range checks. Every emitted tuple must lie in documented hardware bounds.
No hardware command is represented as applied until acknowledged. A pending desired
command must not be used as the baseline for further speculative increases.

### Control policy

Use named, injected parameters; initial synthetic values below are test-only.
Production values require explicit qualification.

1. **Unknown/stale/unsupported observation:** no increase. Do not interpret zero
   eligible pixels as zero saturation. Define how fresh saturation evidence may
   cause a conservative reduction without inventing a hardware-safe fallback.
2. **Settling:** discard ineligible/old-generation observations from adaptation.
   Use only frames demonstrably eligible under the documented latency contract.
3. **High saturation:** reduce promptly within validated limits; clear upward
   eligibility and remember the unsafe probe/boundary. Do not accumulate increases
   while a decrease or its outcome remains unacknowledged/uncertain.
4. **Hysteresis band:** hold. Use different high/low saturation thresholds; this
   is observation hysteresis, not the old +/-2 power-code deadband.
5. **Sustained clear observation:** permit at most one small upward probe, only
   when below the learned ceiling, outside settling and after enough eligible
   clear frames. Probe one sector at a time initially to aid attribution.
6. **Probe succeeds:** establish that level as known safe after qualified frames;
   allow a later probe, not an immediate multi-step climb.
7. **Probe saturates:** retreat to the last known-safe level (if known and legal),
   record a ceiling excluding the failed level, and hold there. Do not keep
   repeating the same unsafe probe in an unchanged scene. A timer alone must not
   continuously erase the learned ceiling and recreate oscillation.
8. **Ceiling reconsideration:** only under an approved independent scene-change
   signal or explicit operator reset policy. If no trustworthy scene-change signal
   exists, keep the ceiling until an explicit controlled reset. This trades power
   recovery for stability and must be documented, not hidden.
9. **Write failure/partial uncertainty:** do not advance acknowledged state or
   establish safe levels from ambiguous frames. Invalidate power/frame association
   and suspend increases until the documented readback/reinitialization/acknowledged
   recovery policy restores knowledge. No blind retry loop or invented full-power
   fallback. A failed multi-register call is not proof hardware stayed unchanged.

Account for possible sector crosstalk: a probe that affects multiple sectors is not
proof of four independent safe boundaries. Freeze affected upward adaptation or
use a separately qualified coupled policy. Do not claim independent ceilings solve
an unmodeled optical coupling problem.

## Stage C — Deterministic synthetic sensor responses

Implement a host-only virtual sensor driven by **acknowledged actual applied**
power, not the controller's desired value. Use an injected virtual frame counter
and fixed seed/replayable sequences; no sleeps or real SDK calls. Synthetic code
must label its saturation flags/units as test fixtures, not a real sensor detector.

Example test-only parameters: legal power 0..12, high saturation fraction 0.10,
low fraction 0.02, decrease step 2, probe step 1, five clear eligible frames before
an increase, command effect delayed two frames plus two settling frames. These
are deliberately fictional and must never become unreviewed production defaults.

Generate per-sector observations using a known safe boundary B:

- P <= B: saturation fraction 0.00 or deterministic low noise.
- P > B: saturation fraction 0.25, with apparent depth distorted to 400 instead of
  a recovered 2000. This reproduces the legacy feedback failure; new decisions
  use synthetic saturation evidence, not the distorted depth.
- Missing/invalid cases supply unknown state, not a saturation fraction of zero.

Required scenarios/assertions:

| Scenario | Required result |
| --- | --- |
| Static boundary B=6, start at 12 | Controller reduces, establishes a safe operating point, and stops persistent up/down cycling. |
| Start below B, probe succeeds then B+1 fails | Retains successful safe state, retreats after the failed probe and never re-probes that failed level without an approved reset/scene change. |
| Legacy diagnostic reproduction: depth switches 400/2000 with power | Test-only legacy model demonstrates oscillation; replacement converges under the same synthetic scene. This is a fixture demonstration, not hardware proof. |
| Noise near each threshold, including alternating high/low bursts | No rapid direction flipping; hysteresis, clear-count and probe rules work deterministically. |
| Initially high saturation but no prior safe level | Bounded legal reductions; no fabricated known-safe state. Reports unresolved saturation if minimum power is still insufficient. |
| Sudden scene becomes more saturating, B falls from 8 to 3 | Previously safe power is reconsidered downward promptly; learned ceiling is not treated as permanently safe under new evidence. |
| Scene becomes less saturating, B rises from 3 to 9 | No ceiling reset merely because lowering power removed saturation; an approved explicit reset/synthetic independent scene-change event permits controlled requalification. |
| Two-frame effect delay, settling and old-generation frames | No multiple in-flight increases or decisions attributed to the wrong power. |
| Repeated/out-of-order/stale frame IDs and configuration generations | Reject observations without advancing clear counts or updating safe ceilings. |
| Empty sector, all invalid samples, IR/unsupported detector | No increase and no sentinel-to-power conversion. |
| Four different boundaries, e.g. 2/5/8/12 | Independent operation only where independence is assumed by the synthetic model; unaffected sectors remain stable. |
| Crosstalk fixture: raising sector 0 saturates sector 1 | Does not falsely certify independent safety; reports/freezes the ambiguous affected adaptation according to the explicit policy. |
| Legal minimum/maximum, negative intermediate calculation | No overflow/wrapping or out-of-range emitted command. |
| Long deterministic run (e.g. 10,000 virtual frames) | Bounded memory; unchanged-scene unsafe probe count remains bounded; no persistent steady-state limit cycle. |

Count reversals, failed probes and commanded power transitions. Define convergence
in assertions (not eyeballed plots): after settling/requalification in a constant
scene, power becomes constant at an observed-safe level, or the controller reports
an unresolved fault/unknown state without increasing. Do not require finding the
maximum safe level when validity/crosstalk does not support that guarantee.

## Stage D — Fake SDK and production adapter tests

Use a narrow injected register interface taking an explicit four-value tuple and
returning the real status/acknowledgment semantics. The production implementation
wraps `DeviceInterface::writeAfeRegisters`; a fake records calls. If testing the
full interface, derive a fake `aditof::DeviceInterface` implementing required
methods with no hardware access, following the repository's stub/fake patterns.
Do not copy test sensor behavior into the production SDK stub unnoticed.

The fake supports:

- Scripted OK, BUSY, GENERIC_ERROR, UNAVAILABLE and uncertain/partial application.
- Known actual tuple separate from requested tuple; optional documented-style
  readback for recovery tests only.
- Delayed physical application/observation effect as modeled by the synthetic
  sensor; distinguish SDK acknowledgment from physical settling.
- Capture start/stop/reinitialization, external register writes and alternate
  initial state.
- A bounded call log with frame ID, addresses, encoded values and returned status.

Required adapter/integration cases:

1. Verify addresses 0x4011..0x4014 and the approved sector-to-register byte mapping.
   Never derive physical orientation solely from the current shifts.
2. Software default values do not prove initial hardware state. First eligible
   behavior follows the documented initialization contract and selection policy.
3. Successful write updates acknowledged tuple/generation; failed write does not.
4. Partial/uncertain write invalidates assumed hardware state; no optimistic
   continuation or increase using stale acknowledged knowledge.
5. Acknowledgment does not make old frames eligible; delayed observations respect
   command generations and settling.
6. Preserve existing write opportunities (`frame % 3 == 1`) and repeated-write
   policy. The controller may HOLD the desired tuple while the adapter still
   performs contract-required writes. Rewriting the same acknowledged tuple must
   not restart adaptation settling forever unless hardware really requires it.
7. Modes 0/1/3/4/5/6 feed the controller from current observations independent of
   display SLAM history ordering. Mode 0 must not read its old history slot.
8. Calibration/smoothing and display selection do not change the saturation
   detector's documented units/meaning or introduce stale processing feedback.
9. Capture restart, SDK initialization, mode/exposure changes, dynamic toggle and
   generic writes affecting these registers invalidate the appropriate state.
   Clearly distinguish invalidating hardware knowledge from clearing learned
   ceilings; toggling must not bypass the unsafe-probe restriction accidentally.
10. Audit every register writer. If generic writes cannot be tracked/blocked for
    experimental operation, do not enable the controller. Test collisions under
    the existing camera ownership mutex.
11. Legacy/fixed policy with experiment disabled preserves SDK call ordering,
    register cadence/values, capture behavior, metadata and output contracts.
12. No new stop/start normalization, blind BUSY adoption, retry loop, or writes
    from HTTP/metrics readers. Preserve first-frame verification at startup.

## Stage E — Small production integration and diagnostics

Only after stages A-D establish usable contracts:

- Extract observations from the currently acquired frame/SDK metadata before
  display/calibration smoothing/SLAM filtering. Never mutate measurement buffers
  to create a control signal. Preserve immutable JPEG ownership and frame metadata.
- Invoke the bounded policy on the camera worker while preserving device ownership.
  Perform register operations only through the existing owner/adapter.
- Add observational `/stream/metrics` fields: selected policy, supported detector,
  frame/command generations, saturation/eligible counts or unknown reasons,
  requested/acknowledged tuple, ceilings, state/reason, settling/clear counts,
  failed probes and write success/failure counters. Bound all storage.
- Browser diagnostics may display this state but must not implicitly select the
  experimental mode, reset ceilings or change registers. Explicit operator reset
  actions, if approved, need their own validated mutation route/contract.
- No per-frame logging, raw-frame histories or unbounded probe records.
- Add the option to the source/build/launcher path deliberately, record policy
  selection in the manifest/runtime diagnostics, and package matching UI/tools.
  Preserve the default accepted release behavior when the option is absent.

## Change boundaries and deliverables

Each is a separate reviewed checkpoint, not one large cleanup:

1. Contract/evidence report and repeatable legacy synthetic failure fixture.
2. Pure controller plus deterministic response tests (not hardware-connected).
3. Register adapter/fake SDK plus lifecycle/failure/mapping/cadence tests.
4. Observation extraction/current-frame integration, default-off selection and
   bounded observational diagnostics.
5. Separately authorized external builds/audits and operator hardware qualification.

Deliver source changes, parameter/state-machine documentation, commands for
external tests/builds, explicit unresolved prerequisites, test expectations and
rollback instructions. Do not report an unbuilt test as passing or claim measured
hardware stabilization from a synthetic model.

## Acceptance, external validation and rollback

Before production activation, externally run focused pure/fake/integration tests,
full simulator/UI/measurement regressions and appropriate ASan/UBSan/TSan runs.
Compare deterministic processed uint16 values, calibration/smoothing histories,
SLAM, pixel mapping, JPEG metadata/ownership and observational exports for the
experiment-disabled path. Host tests must not open a device or execute cross code.

Cross-build a **new** versioned full bundle using the recovered Yocto runtime;
audit AArch64/native NEON, strong symbols, dependency closure, runtime versions
and absence of distribution runtime/RPATH leakage. Never overwrite `bin/chiton`,
an accepted artifact, the working release or shared device state. All builds
remain external unless permission is explicitly renewed.

Hardware qualification requires a resolved detector/register/latency contract,
a documented recovery procedure, backed-up shared device state, and a known-good
single-owner acquisition baseline (the current startup timeout must be addressed
separately). Start with a controlled static scene, then operator-qualified dynamic
scenes, varied reflectance/range, exposure/mode/calibration settings and restart
cases. Compare previous/new releases with identical settings and clients, 30-second
warmup and at least 120-second repeated A/B/A runs. Verify actual saturation,
stable power, depth/image quality, 29-30 FPS, latency/control response and recovery;
CPU improvement alone is not acceptance. More illumination is not automatically
better measurement quality.

On neutral/worse/ambiguous results, leave the replacement off. Rollback means stop
the new camera owner and run the prior accepted release; transport rollback alone
is insufficient. A previous binary may not restore hardware state changed by the
new controller. Use only the documented hardware recovery procedure; code rollback
or blindly forcing full power is not a substitute.

### Suggested first instruction to the new agent

> Read this document and the required references, inspect the current code and git
> state, and report the unresolved saturation/register/latency contracts before
> editing production control behavior. Then implement the pure bounded controller,
> deterministic synthetic responses and fake-SDK tests as separately reviewable,
> default-off changes. Do not build, execute ARM code, deploy, write the SD card or
> test real registers. Stop and ask when an unknown contract would require guessing.
