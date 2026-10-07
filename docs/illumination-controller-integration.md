# Default-off experimental depth-proxy illumination integration

## Status and operator-approved assumptions

Source implementation only: the agent did NOT build, run tests, execute ARM,
access a camera/register, deploy, write an SD card or alter saved configuration.
The operator reported passing tests for the earlier two pure/fake fixtures; that
report does not cover the new integration or updated sources. New validation is
required. No hardware stabilization/performance claim is made.

The operator subsequently supplied these contracts/explicit assumptions:

- Codes **1..12** are permitted; larger is more illumination. Zero is forbidden
  on this experimental path (the old synthetic 0..12 fixture stays fictional).
- Existing sectors correspond to independent physical emitters; reversed mapping
  is approved: logical sectors 0/1/2/3 go to registers 4014/4013/4012/4011.
- SDK OK means the entire four-value tuple applied. **Non-OK is assumed to leave
  the previous tuple unchanged**, by explicit operator instruction. This is NOT
  independently verified SDK failure/atomicity evidence.
- The next acquisition should reflect a changed command, but may be one frame
  behind. Discard the first post-write acquisition; the second is eligible.
- Repeated writes are unnecessary, but suppression remains a separate reviewed
  change. This integration retains the old `% 3 == 1` opportunities and repeats
  acknowledged held tuples; successful identical writes do not restart settling.
- Simultaneous depth+IR acquisition is a future change. For now **every acquired
  depth sample <250, including zero**, is treated as a saturation *proxy*.
  This is an experimental assumption, not proof of physical overexposure. Zero
  invalid returns can therefore look saturated. Missing/empty geometry is unknown.
- Approved starting policy: high >=10%, low <=2%, five eligible clear frames,
  decrease two codes, one-code single-sector upward probes. Learned ceilings
  persist within the process until explicit controlled reset/requalification.
  There is no timer, scene-change inference or reset HTTP route.

The exact matching SDK/sensor documentation and recovery procedure still matter
for hardware qualification, especially the non-OK unchanged assumption and proxy
validity. Old partial/uncertain fake tests remain on the conservative uncertainty
policy to demonstrate why this assumption is a limitation.

## Selection: two independent opt-ins

Default compilation and runtime are legacy, preserving the old dynamic switch
and ordinary fixed behavior. CMake `CHITON_EXPERIMENTAL_ILLUMINATION=ON` compiles
availability; it does NOT select the controller. The application additionally
requires:

```text
--illumination-policy experimental-depth-proxy
--illumination-initial p0,p1,p2,p3
```

All four initial codes must be explicitly chosen in 1..12. There is deliberately
no initial hardware tuple guess, automatic full-power fallback or undocumented
readback. At the first existing write opportunity the initial command is issued;
only SDK OK establishes applied knowledge. A failed initial write leaves startup
unknown and suspends writes/adaptation until an explicit acknowledged capture
restart. Failure after knowledge exists retains the old acknowledged tuple,
discards any rejected proposal and clears upward qualification. There is no
immediate SDK retry; fresh eligible observations may later propose another command.
Successful identical held writes retain settling/clear state.

The CLI refuses an unavailable compiled policy, missing/illegal initial tuple,
unknown policy, an initial tuple with legacy selection, and experimental startup
with IR configured. No fallback pretends the experiment is running. Selection is
immutable for the process and not saved in device configuration. No environment
variable or existing dynamic checkbox selects it.

## Current frame / lifecycle / ownership

On the camera worker, immediately after requestFrame/getData and BEFORE
calibration, smoothing, plugins, angle overlays, SLAM history or display conversion:

1. Use the existing AOI runlines and angle-bucket map. Denominator is all acquired
   samples within each approved sector [128,320), [320,512), [512,704), [704,896).
   Numerator is depth <250. Nothing mutates the SDK measurement buffer.
2. Feed frame/time, lifecycle, acknowledged command association, support and
   conservative eligibility to the pure controller. Adaptation is unsupported
   during calibration sampling or IR; unsupported/empty data cannot increase.
3. Apply only through the owner-held SDK adapter at the unchanged opportunities.
   Multiple in-flight increases are impossible. No legacy register call is also
   issued while this policy is selected.

Depth-proxy units mean the current SDK DEPTH array, not calibrated display output
or display range units. Modes 0/1/3/4/5/6 share the same current-frame observation;
mode 0 never uses its old SLAM slot for experimental feedback. Other display modes
likewise cannot reinterpret the detector. Metadata/JPEG ownership and acquisition
start/stop/first-frame verification code are unchanged.

Acknowledged capture initialization/start/stop advances the experimental ownership
and hardware generation. The HTTP path only updates generations/observational
cache, never the controller or registers. The worker invalidates hardware knowledge
on its next acquisition, retains learned ceilings and re-establishes the last owned
acknowledged tuple capped by those ceilings before adapting. Capping prevents a
restart after a failed retreat from re-emitting a known-unsafe level; it does not
claim the cap is automatically safe. A successful re-establishment gets the same
two-acquisition eligibility rule. Display mode changes requalify clear/safe state
without pretending to be a hardware writer; ceilings and the owned tuple remain.
Smoothing/calibration-display/quality changes do not change detector units.

Under the existing recursive camera ownership mutex, experimental operation
rejects IR selection, legacy dynamic mutations and **all generic AFE writes** with
409. Blocking only the four illumination addresses would not account for generic
exposure/firmware changes. Generic reads remain observational. SDK initialization
and setMode/setControl currently happen before policy construction; there is no
new runtime SDK initialization route. All active application register writers are
main's owner write and the blocked settings route. Do not run another camera owner
or external register writer concurrently; application locking cannot track them.

A process restart discards learned state and is an explicit requalification/reset,
not an automatic retry. Do not toggle/restart repeatedly to bypass learned ceilings.
The controller does not invent a driver recovery sequence or persist calibration.

## Observational diagnostics

`/stream/metrics` adds `pipeline.illumination`: compiled availability, selected
policy (fixed / legacy_dynamic / experimental_depth_proxy), proxy identity/support,
frame/configuration/lifecycle/command generations, sample counts, requested and
acknowledged tuples (unknown is null), safe levels, ceilings, clear counts,
settling eligibility, reason, startup failure, coupling freeze, failed probes,
transitions/reversals, SDK status and bounded success/failure counters.

A fixed-size worker snapshot is copied under a separate short mutex; HTTP JSON
construction performs no camera/register access and cannot mutate control state.
Capture invalidation immediately marks published hardware knowledge unknown.
The existing browser diagnostics reader exposes these fields via
`window.CHITON_METRICS`. The source UI's diagnostic formatter labels fixed, legacy
dynamic and EXPERIMENTAL depth<250 proxy explicitly, including unknown acknowledged
state/knowledge faults. No browser mutation/reset/policy selection is added. The
accepted `bin/files` asset copy is intentionally untouched; new bundles package
matching `release/files` source assets.
Detailed illumination fields are NOT added to WebSocket status or JPEG metadata.
No raw histories, per-frame logs or unbounded probe records are retained.

## External native tests (operator-run only)

Build the focused fixtures in a NEW native directory:

```sh
cmake -S tests/illumination -B /tmp/chiton-illumination-integration-NEW -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/chiton-illumination-integration-NEW
ctest --test-dir /tmp/chiton-illumination-integration-NEW --output-on-failure
```

There are now **three** fixtures. `illumination_camera_policy_test` additionally
wraps a fake DeviceInterface through the actual SDK adapter and tests codes 1..12,
initial unknown state, successful establishment, immediate-next-frame and one-frame-
behind responses, successful identical refresh, unchanged failures, qualification
reset, no initial-failure retry, restart ceiling retention, proxy boundaries/zero,
geometry validation, CLI tuple parsing and bounded diagnostics. No real SDK is
linked. Existing uncertainty/partial-application tests remain separate.

Review checkpoints remain separable: (1) pure failure/requalification APIs;
(2) extractor/camera-owned adapter and fake DeviceInterface tests;
(3) guarded worker/lifecycle/settings integration and observational metrics/UI;
(4) default-off build/packager/manifest wiring and mocked packaging tests.
No commits or accepted artifact replacements were made by the agent.

Repeat focused tests in fresh ASan/UBSan/TSan directories using compile AND link
sanitizer flags documented in `illumination-controller-tests.md`. Then externally
configure the full native simulator in another fresh directory with:

```text
-DUSE_ADITOF_STUB=ON -DUSE_SDL2=OFF -DCHITON_RELEASE_TESTS=ON
-DCHITON_EXPERIMENTAL_ILLUMINATION=ON -DCHITON_ILLUMINATION_FIXTURE_TESTS=ON
```

Run the full regression suite, including `camera_lifecycle`, `cross_build_tools`
and new `http_illumination`, plus `web_performance` for read-only policy labels.
The HTTP test is also registered in the default-OFF simulator variant (the script
gets `available`/`unavailable` explicitly). It rejects ARM and non-simulator binaries
BEFORE launching; it runs sequential owners and checks unavailable/invalid choices,
legacy/fixed selection, blocked generic/dynamic/IR mutations, current proxy counts
across modes and smoothing, cadence, and capture invalidation/re-establishment.
Also run the default-OFF full simulator variant and compare deterministic processed
uint16/calibration/smoothing/SLAM/pixel/JPEG/metadata/export results. Do not weaken
output equivalence tests or execute cross-built tests. Source assertions are not
passing test results. Makefile remains legacy-only; use CMake for this experiment.

## External cross bundle / activation / rollback

After native review gates, externally build a NEW versioned bundle:

```sh
./cross-build.sh --name rel_illumination_REVIEW_VERSION --experimental-illumination
```

The packager explicitly resets compiled availability OFF when this flag is absent,
preventing stale workspace-option inheritance. The manifest records compiled
availability, default legacy runtime, selection, proxy/failure/latency assumptions;
matching integration instructions are packaged under `docs/`. The existing
launcher forwards arguments but never implicitly enables the policy.

This command builds/packages, not activation. An incremental unchanged build can
still produce a new archive with the same executable hash. Compare manifest/hash
and compilation option, not just the release filename. Never overwrite accepted
artifacts or `bin/chiton`.

Only after operator hardware qualification, with a documented recovery route,
backed-up shared state and one known-good camera owner, may the operator use:

```text
/new/versioned/release/run.sh --display headless
    --illumination-policy experimental-depth-proxy --illumination-initial CHOSEN_TUPLE
```

`CHOSEN_TUPLE` is four explicit approved codes, not a literal argument or a claimed
universally safe tuple. Fix the existing startup/acquisition baseline separately;
this integration adds no capture normalization/retry. Qualify saturation/depth
quality, stable power, latency and 29-30 FPS with controlled repeated A/B/A runs.
Neither fixture convergence nor proxy percentages establish real IR overexposure.

Rollback: stop the new owner, run the previous accepted release (or omit runtime
selection for legacy behavior). Restore hardware only through the documented
operator recovery procedure. Code rollback alone does not restore registers; no
blind full-power fallback or SD/device write is authorized by these source changes.
