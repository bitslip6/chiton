# Pure illumination policy and fictional host fixtures

**Original pure/fake checkpoint.** The operator later reported those two fixtures
passing externally. Newly changed sources and the third SDK/production-policy
fixture remain unbuilt/unexecuted by the agent. The production path is independently
compiled/selected, default-off; see
[illumination-controller-integration.md](illumination-controller-integration.md).
Original prerequisite inspection is in
[illumination-controller-contracts.md](illumination-controller-contracts.md).

## Parameters and interface contract

`Parameters` has no defaults. The constructor validates signed logical power
bounds within uint16 encoding capacity, positive bounded step sizes, ordered
integer parts-per-million thresholds and nonzero clear-frame count. This numeric
capacity is NOT a sensor range specification. Real use additionally requires a
qualified monotonic logical-level-to-register encoding; the fixture encodes levels
identically. All arithmetic remains signed until checked final encoding.

Only `tests/illumination_fixture.h` chooses fictional values: 0..12, high 10%,
low 2%, decrease 2, probe 1, five clear frames, independent sectors. The sensor
classifies 25 of 100 fictional samples as saturated above the boundary and zero
below; missing means unknown/zero eligible, never zero saturation evidence.
Fixed replay sequences supply noise; there is no random entropy, sleep, wall
clock, device call or depth-based control. The distorted 400/2000 depth exists
only for the legacy diagnostic. Boundary and crosstalk changes are test inputs.

Observations carry frame/time, lifecycle, command generation, support/eligibility
and four known/count pairs. The caller must supply documented exposure association
and eligibility. This implementation deliberately does NOT infer latency from
receipt time. Matching command, eligibility flag and `eligible_from` are all
required. Duplicate/out-of-order frames, decreasing time and mismatched lifecycle
are rejected before advancing counts. Ineligible/wrong-command/unsupported frames
clear consecutive eligibility. A lifecycle change permits restarting IDs only
through explicit owner invalidation/recovery; command generations never restart.

`recover` requires a legal externally established tuple and newer command ID;
software defaults do not establish it. It is not an SDK recovery algorithm.
`invalidate` discards pending/known hardware state, safe certifications and clear
counts, retaining ceilings and coupling freeze. Regressing lifecycle generations
cannot restore knowledge. `reset_learning` is a separate explicit approved action,
never triggered by time, disappearing saturation, toggle or invalidation. It does
not recover hardware knowledge or cancel an outstanding probe.

## State machine

- Unknown hardware: no command. Unsupported/empty/malformed observations: no
  increases. Known, fresh high evidence may reduce a valid sector even when
  another sector is unknown; otherwise hold. There is no invented fallback tuple.
- Pending command: do not adapt until certain acknowledgment. Stale acknowledgments
  cannot update state. Failed/uncertain calls invalidate hardware association;
  no retries. The old acknowledged tuple remains diagnostic only, marked unknown.
- Settling: ignore old-generation/early frames. Certain acknowledgment advances
  applied tuple/generation, NOT physical effect. Only caller-qualified future
  frames can qualify safety.
- High evidence: clear upward eligibility, lower within legal bounds, tighten the
  ceiling excluding the observed unsafe level, invalidate safe certificates at
  or above that level. High at minimum reports `SaturatedAtMinimum`, not safety.
- Band: hold and reset the relevant clear counter. Low: count eligible consecutive
  frames, certify an observed-safe level after qualification. Safe is evidence in
  the current synthetic scene, never an unconditional hardware guarantee.
- Require all sectors clear/qualified before one round-robin small probe. One
  desired command at a time; no speculative baseline from a pending tuple.
- Probe success requires all sectors qualified together, certifies the tuple and
  returns without immediately probing again. Failure retreats the probed sector
  to its pre-probe qualified-safe level, tightens its ceiling, reduces other high
  sectors and counts the failed probe. No failed level is retried in unchanged
  scene. More light/maximal safe power is not an acceptance requirement.
- Saturation in another sector during a probe is ambiguous attribution: freeze
  ALL upward adaptation, retreat and conservatively reduce high sectors. No
  independent safety claim is made from it. If independence was not explicitly
  enabled, there are no upward probes at all. Freeze persists until approved reset.

State is fixed arrays/scalars. Diagnostic counters saturate rather than wrap;
command generation exhaustion discards hardware knowledge and stops commands.
Transitions/reversals count acknowledged changed tuples and sector directions;
failed probes count observed failed probes. No histories or per-frame logging.

## Fake register boundary

At the original pure/fake checkpoint, `RegisterPort` takes four explicit addresses/values and returns SDK status,
explicit certain-tuple acknowledgment and eligible-frame bound. The adapter
requires an explicit permutation and availability flag; no production mapping
or acknowledgment semantics are selected by default. It preserves `% 3 == 1`
write opportunities and rewrites held acknowledged tuples. Fixture successful
identical rewrites do not restart settling. Non-OK or uncertain OK invalidates
state, including failed refreshes. No real `DeviceInterface` instance is created by these fixtures. The later
DevicePort source wrapper is described in the integration document.

The fake's requested call, acknowledged policy state, queued effect and actual
sensor tuple are distinct. Physical effect is two frames after write; eligibility
is four frames after write (two effect plus two settling). The 32-entry ring log
wraps; it stores frame/generation, addresses, values and status. Failure scripts
include BUSY, GENERIC_ERROR, UNAVAILABLE and uncertain OK; partial failures can
change one physical register. External writes/restarts/initialization/mode,
exposure and toggle events are modeled as owner invalidation plus optional
fictional readback recovery. This tests interfaces, NOT installed production hooks.

## External validation commands (operator only)

The agent did not run these. Use a NEW native host directory. This standalone
configuration avoids camera/application/PNG/JPEG/SIMDe dependencies:

```sh
cmake -S tests/illumination -B /tmp/chiton-illumination-NEW -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/chiton-illumination-NEW --target illumination_controller_test illumination_register_adapter_test
ctest --test-dir /tmp/chiton-illumination-NEW --output-on-failure
```

Alternatively the full native configuration can opt in via
`-DCHITON_ILLUMINATION_FIXTURE_TESTS=ON` (default OFF). Cross configuration of
these fixtures is rejected. Do not execute cross-built tests.

Repeat in fresh host directories for ASan/UBSan/TSan, using both compile and
link flags, e.g. `-DCMAKE_CXX_FLAGS='-g -O1 -fno-omit-frame-pointer
-fsanitize=address' -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address`, then substitute
`undefined` or `thread`. The fixtures are single-owner/single-thread; TSan does
not establish the production camera ownership contract.

Expected assertions include:

- High-start B=6 convergence; low-start successful probes then failed B+1;
  unchanged tuples/counter stability across 10,000 frames; bounded failures and
  reversals versus over 900 reversals in the fictional legacy diagnostic.
- Different independent boundaries 2/5/8/12, legal extrema/large negative
  intermediates, parameter rejection, command exhaustion and minimum saturation.
- Boundary drops 8->3 prompt downward requalification, rises 3->9 retaining the
  ceiling until explicit reset; coupling fixture freezes rather than certifies.
- Replay around both thresholds/band, stale/duplicate/time/lifecycle/command
  rejection, unknown/missing/unsupported input, one in-flight probe, delayed
  effect distinct from acknowledgment and no clear counts during settling.
- Fake permutation/address encoding (fictional approval only), unavailable
  adapter/no assumed initial tuple, acknowledged/failure/partial states, failed
  refresh, no retry, readback recovery, retained ceilings across lifecycle events,
  repeated writes at original opportunities and bounded wrapping call log.

Convergence means: after 1000 requalification frames in a constant supported
independent fixture, the tuple is observed safe and remains identical with no
additional transitions for another 1000 or 10,000 frames. Alternatively minimum
saturation remains an unresolved fault with no further transitions. Unknown or
ambiguous cases may remain frozen without finding maximum safe power.

These are source assertions, **not passing test results**. Full simulator/UI,
measurement/calibration/JPEG/lifetime/metadata regressions and production adapter,
current-frame extraction, all-writer collision/ownership tests remain external
and/or blocked by contracts. No hardware stabilization or performance claim.

## Rollback

For this original pure/fake checkpoint, leave the fixture option OFF. The later
production integration has two separate opt-ins; leave both OFF and follow its
integration document for rollback.
Review/revert only the new files and optional CMake test block, preserving the
operator's handoff and accepted artifacts. Any eventual connected experiment
requires a new versioned release, external audits and operator qualification.
Stop the new owner and restore the prior accepted release on ambiguous/worse
results; code rollback does not restore changed hardware state. Use only a
qualified hardware recovery procedure, never a guessed full-power write.
