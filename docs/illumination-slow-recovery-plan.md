# Plan 1 — Slow, bounded recovery after saturation clears

## Scope and status

Planning only; no controller/source behavior changes, builds, device/register
experiments, deployment or SD writes are authorized/performed by this plan.
Preserve the existing accepted release, original default-off selection and all
current unrelated work. Review this independently from quadrant isolation in
[illumination-quadrant-isolation-plan.md](illumination-quadrant-isolation-plan.md).

This plan adds a recovery mechanism to the existing four-value policy without
redesigning sector attribution/global qualification. It does NOT force the tuple
uniform or introduce a shared scalar ceiling. If a genuinely uniform global-power
policy is desired, that is a separate operator decision, not an inferred mapping.

Current source already stores four ceilings and reduces only sectors whose own
proxy fraction is high. However, all-sector clearance, clearing all qualification
counts and a persistent coupling freeze can make recovery appear globally blocked.
Confirm the running `pipeline.illumination` snapshot before attributing the observed
hardware behavior to a literal shared ceiling. Plan 2 specifically removes these
normal-operation shared decisions.

## Existing contracts retained

- Legal codes 1..12, larger means more light; approved reverse register mapping.
- Current acquired SDK depth <250, including zero, is the TEMPORARY proxy.
  Count within existing sector/AOI geometry before smoothing/calibration/display.
  Not verified IR overexposure; low-light invalid zeros may poison this signal.
- High >=10%, clear <=2% currently; unknown/empty input is not clear.
- SDK OK acknowledges all four; operator assumes non-OK leaves old tuple applied.
- First post-change acquisition discarded; second conservatively eligible.
- Existing `% 3 == 1` register opportunities/repeated-write behavior unchanged.
- Explicit initial tuple required; no initial-state/readback/full-power guess.

## Proposed recovery behavior

1. **Reduce promptly:** maintain the current bounded two-code reductions and
   record the unsafe level. Saturation at code 1 reports an unresolved fault.
2. **Wait for sustained clearance:** start a recovery window only on fresh,
   eligible clear observations. Unknown/band/high observations break that window;
   old/ineligible frames cannot qualify it. Keep hardware applied and proposed
   tuples separate throughout.
3. **Probe slowly:** propose only +1 in one sector, using the last acknowledged
   tuple as baseline. Never stack increases, infer success from disappearing
   depth artifacts or immediately climb again after acknowledgment.
4. **Requalify a learned boundary rather than erase it:** allow a specifically
   budgeted trial at `applied + 1`, even when a historical ceiling excludes it.
   The permission is a pending experiment, not a raised safe ceiling. Record
   `probe_from`, `probe_to`, command generation and recovery budget.
5. **Success:** only fresh eligible clear frames at the new level certify it.
   Update the observed-safe level and the requalified bound one code at a time;
   retain unrelated learned bounds. Enforce the inter-probe interval before another
   step. Successful steps can continue toward an explicitly approved recovery goal.
6. **Failure:** retreat to the last qualified-safe `probe_from`, exclude the
   failed level, clear upward eligibility and consume the recovery-failure budget.
   Do not claim a desired retreat applied until SDK OK. Under the approved unchanged
   failure contract, failed writes retain the old acknowledged tuple; no immediate
   blind retry or invented fallback.
7. **Bound retries:** recommended starting design is at most ONE unsuccessful
   recovery probe per sector per explicit qualification epoch. Successes do not
   consume that failure budget; saturation-driven decreases do not replenish it.
   After budget exhaustion, hold/reduce as needed and report `recovery_blocked`.
   A timer, disappearing saturation, ordinary mode/capture toggle or HTTP reader
   must not reset budgets/ceilings. Epoch reset needs an explicit approved operator
   action or independently qualified scene-change signal; no such automatic signal
   exists today. Reset-route authorization/ownership must be reviewed separately.

### Candidate timing, NOT approved production defaults

- 30 consecutive eligible clear observations before a recovery probe.
- At least 60 acquired frames between acknowledged upward commands.
- Five eligible clear observations at the probed level to certify success.
- At 30 FPS, approximately one second of clearance and no faster than +1 per two
  seconds. Frame rules must remain correct at lower/variable acquisition rates;
  if time-based pacing is preferred, inject monotonic observation time into the
  pure policy, never sleeps/wall-clock reads inside it.

Keep 10%/2%, -2/+1 and latency rules unless the operator approves changes. Exact
clear definition, timing, recovery goal and exhausted-budget policy are open below.

## Why this prevents a persistent limit cycle

Slow probing alone only makes oscillation slower. If the only signal is saturation
which disappears when power is lowered, an unchanged safe boundary and a genuinely
improved scene can look identical at low power. There is no guaranteed autonomous
way to distinguish them from that evidence alone.

The bounded failure budget makes the distinction explicit: a constant scene may
experience a finite exploratory overshoot/retreat, then stable power. A later scene
improvement after the budget is exhausted cannot guarantee autonomous recovery
without a separate signal/reset. Unlimited periodic retries would sacrifice that
no-persistent-cycle guarantee and require explicit operator approval; do not hide
that tradeoff behind a cooldown.

## Bounded diagnostics and tests

Add bounded fields for recovery state, goal, clear-window count, last increase,
probe from/to, per-sector failed-recovery budget, exhausted reason and qualification
reset generation. Show requested versus acknowledged tuples and keep proxy identity
visible. Diagnostics/readers cannot select policy, reset budgets or write registers.

Externally test source fixtures before hardware qualification:

- Initial high saturation drives code 1; later-clear synthetic scene permits
  gradual 1->2->3... recovery rather than being trapped by ceiling 1.
- Constant boundary: each unsafe level cannot be retried indefinitely; transitions
  become constant after finite qualification/failure, over at least 10,000 frames.
- Scene improves after reduction: successful steps requalify boundaries slowly;
  improvement after exhausted budget remains explicitly blocked until approved reset.
- Short clear bursts, threshold-band noise and alternating saturation do not cause
  rapid direction reversals. Unknown/empty/unsupported samples cannot recover power.
- Immediate-next-frame and one-frame-behind effects, failed writes and delayed
  acknowledgment cannot produce speculative multi-step increases.
- Legal minima/maxima, generation invalidation and capture/configuration changes
  preserve the failure budget; only approved reset creates a new qualification epoch.

Review checkpoints: pure recovery policy/tests; adapter/diagnostics tests; separately
selected default-off integration; external native/sanitizer/output-regression gates;
operator-only hardware qualification with documented recovery and rollback. Do not
combine with write suppression, SIMD, calibration, transport or startup changes.

## Operator decisions needed

1. Does "saturation removed" mean the existing <=2% clear threshold, or literally
   zero proxy-saturated samples?
2. Is +1 no faster than every two seconds after about one second of clearance an
   appropriate initial definition of "slow"?
3. Is the goal the pre-reduction level, an explicit operator target, or exploration
   toward 12? A pre-reduction target is NOT automatically known safe.
4. Approve bounded failure/explicit reset, or prefer ongoing periodic retries with
   the acknowledged possibility of recurring small oscillations?
