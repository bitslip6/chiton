# Plan 2 — Quadrant-local reduction and recovery

## Scope and status

Planning only; no production edits, builds, executions, register experiments,
deployment, SD writes or saved-state changes. Preserve existing work/artifacts.
Use the slow bounded recovery mechanism from
[illumination-slow-recovery-plan.md](illumination-slow-recovery-plan.md), but apply
its evidence, goals and budgets LOCALLY rather than requiring all quadrants clear.
The plans are separately reviewable; adopting local recovery needs both local
attribution rules and an approved bounded-recovery policy.

"Quadrant" means one approved physical emitter/angle sector, NOT a rectangular
quarter of the displayed image:

| Logical quadrant | Angle buckets | Register |
| --- | --- | --- |
| 0 | [128,320) | 0x4014 |
| 1 | [320,512) | 0x4013 |
| 2 | [512,704) | 0x4012 |
| 3 | [704,896) | 0x4011 |

Existing source has a per-sector `ceiling[i]`, not a shared scalar ceiling. Its
all-clear gate, clearing all counters and global `upward_frozen` behavior are
shared adaptation restrictions. Also determine from metrics whether multiple
quadrants genuinely satisfy the current depth proxy simultaneously; correct local
logic will still reduce each quadrant that independently supplies high evidence.

## Contracts and observation

Retain 1..12, approved independent-emitter/reverse mapping, SDK OK/all-four
acknowledgment, operator-approved non-OK unchanged assumption, second-post-write
eligibility, current register opportunities and explicit initial tuple.

Extract current SDK depth before calibration/smoothing/SLAM/display changes. For
quadrant i, numerator is its own depth <250 count (including zero); denominator
is its own eligible AOI sample count. Never use whole-frame totals, another
quadrant's fraction, a smoothed history slot or a display image as its feedback.
Missing/unsupported samples remain unknown for that quadrant. The proxy limitation
remains: absent/underexposed returns can be confused with physical saturation.
Replacing it with simultaneous IR/validity metadata is a separate future change.

## Independent bounded state

For EACH quadrant keep:

- Acknowledged power, last qualified-safe power and learned unsafe bound.
- Local clear qualification and local settling eligibility.
- `reduced_by_saturation` flag and pre-reduction recovery goal.
- Probe from/to, recovery-failure budget, last increase and local reason.

The full tuple/SDK command generation, ownership and pending write remain shared,
because the real API writes all four values. That does not justify shared optical
qualification or shared ceilings. State is fixed four-element arrays/scalars.

## Local decisions

1. **Reduction:** if i meets its own high threshold, lower ONLY tuple[i], clear
   ONLY i's upward qualification and tighten ONLY i's boundary. Save the original
   pre-reduction goal on the FIRST reduction of an episode, not on every descending
   step. The goal is a requested recovery limit, NOT proof it was safe.
2. If several quadrants independently saturate, combine their individual decreases
   into one tuple. Do not reduce a healthy quadrant to match the lowest power.
3. **Unknown/band/minimum:** unknown or band observations hold i and inhibit i's
   upward eligibility. High at code 1 reports i's unresolved saturation; it must
   not automatically stop a different known-clear reduced quadrant recovering.
4. **Recovery candidate:** i can increase only when `reduced_by_saturation[i]` is
   true, its own observations satisfy the approved clear window, its own pacing/
   latency criteria are met, and its local recovery budget allows the probe.
   No `all_clear` requirement across other quadrants.
5. **Local goal:** recommended default proposal is to recover no higher than the
   pre-reduction goal (always <=12). Once reached and qualified, clear i's reduced
   flag and stop opportunistic upward exploration. An initially low but never-
   reduced quadrant therefore does not climb merely because it is clear. Confirm
   this interpretation with the operator before implementation.
6. **Probe:** change only i by +1, with all other tuple entries copied from the
   last acknowledged tuple. At most one upward probe/command is pending initially.
   Use fair round-robin selection among locally eligible reduced quadrants, not
   a global lowest-power or common-target rule.
7. **Success/failure:** certify or retreat based on i's own qualified observations.
   Use the bounded historical-ceiling requalification mechanism from Plan 1;
   merely preserving the current permanent ceiling would reproduce the floor lock.
   A failed probe consumes ONLY i's failure budget and caps ONLY i. Other reduced,
   clear quadrants remain eligible to recover.
8. **Other-quadrant evidence during a probe:** if j saturates while i is probed,
   reduce j using j's evidence. Under the approved independence contract, do NOT
   automatically classify i's probe as failed or freeze all upward adaptation.
   i's outcome depends on i. If actual observations contradict independence,
   report the contradiction and stop qualification for review rather than silently
   introduce an unmodeled coupled-control policy or claim local attribution proven.

## SDK scheduling and lifecycle

- Preserve `% 3 == 1`; write a complete four-value tuple, even when only one entry
  changes. "Only affect quadrant i" means other encoded values and requested
  levels remain identical, not issuing an unqualified single-register write.
- Downward commands take priority over an UNISSUED upward proposal. Cancel/replace
  that proposal using acknowledged state if needed; never treat it as applied.
  Do not invent a second in-flight command or asynchronous acknowledgment semantics.
- Respect the second-post-change acquisition eligibility contract. Initially it is
  acceptable to discard the first whole acquisition conservatively, but PAUSE rather
  than erase unaffected quadrants' clear progress solely because another quadrant
  changed power. Define "consecutive" over qualified eligible observations; local
  high/band/unknown evidence breaks only the relevant quadrant's streak. Clear/safe
  certification must never advance from an exposure whose association is unknown.
- Do not restart settling because the adapter rewrites an identical tuple.
- Failed writes do not advance any acknowledged value; preserve the approved old
  tuple and discard rejected changes. No immediate blind retries. Genuine unknown
  hardware state is a shared HARD fault and must suspend increases; this is distinct
  from one quadrant's optical saturation or exhausted recovery budget.
- Capture/reinitialization/generic-write rules retain ownership and knowledge
  invalidation. Preserve all local learned bounds/budgets across ordinary toggles;
  re-establishment cannot re-emit a known-unsafe level. Explicitly approved process/
  operator qualification reset is different from routine configuration change.
- Generic AFE writes and incompatible IR selection stay blocked during experiment.

## Diagnostics / assertions

Expose one bounded record per quadrant: counts/fraction, acknowledged/desired
power, safe level, unsafe bound, reduced flag, recovery goal, clear count, local
reason, failed recovery count/budget and settling/probe state. Keep full command
status/generation separately. The UI can identify the active probing/reducing
quadrant; it must not mutate control/reset state.

Required deterministic pure/fake-SDK assertions:

- Only quadrant 0 saturates: quadrant 0 decreases; requested/acknowledged values,
  ceilings and optical state for quadrants 1..3 do not change from that event.
- Quadrant 0 becomes locally clear: only it climbs slowly toward its approved
  goal, even if a different quadrant remains saturated, band-limited or unknown.
- Clear quadrants never reduced by saturation do NOT increase.
- Two independently saturated quadrants decrease individually in one tuple; the
  other two values remain byte-for-byte unchanged at approved reverse addresses.
- A failed probe in 0 does not veto locally clear reduced 1; qualify both fairly
  under one pending tuple and the original write opportunities.
- Different boundaries/goals, missing sectors, mixed noise and independent scene
  changes preserve local attribution and never convert absent data to clearance.
- Same tuple rewrites and other-quadrant writes do not continually reset local
  qualification and starve recovery. Both latency scenarios reject old effects.
- Partial/unknown-state fixtures remain distinguishable from the explicit unchanged
  failure contract. Hardware knowledge cannot be fabricated from a software tuple.
- Constant-scene 10,000-frame runs have bounded failed probes, stable operating
  tuples (or local explicit unresolved/blocked states), bounded memory and no
  persistent limit cycle. No test needs to find maximum safe power.

Review and externally validate pure/local scheduling changes first, then the
fake adapter and default-off current-frame integration. Run full disabled-path
measurement/calibration/SLAM/JPEG/UI/metadata and sanitizer regressions externally.
Hardware tests remain operator-only, single-owner and separately qualified with
recovery/rollback instructions. No concurrent cleanup/write suppression/transport/
SIMD/calibration/startup changes.

## Decisions before implementation

Use the same four operator questions as Plan 1: clear definition, slow cadence,
pre-reduction versus explicit/max recovery goal, and bounded retry/reset policy.
Recommendation: this local plan plus Plan 1's bounded +1 recovery, a pre-reduction
recovery goal, and no timer that continuously erases unsafe boundaries. Do not
change production defaults until those choices and external gates are approved.
