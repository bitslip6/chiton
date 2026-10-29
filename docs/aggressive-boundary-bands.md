# Aggressive boundary bands · algorithm 3

Explicit opt-in display classification, **Off by default**. This is not sensor
confidence, invalid-depth detection, or a calibration/optical correction. It flags
original records for Highlight/Hide; geometry, count/order, measurements and raw,
OBJ and binary exports remain unchanged. Algorithms 1 Exact and 2 Conservative
Early Exit keep their previous behavior. See [ownership/API](console-boundary-cleanup.md)
and [producer contracts](sampled-boundary-cleanup.md).

## Exact policy

At each original emitted center, independently, from the same immutable processed
source frame:

1. Require processed-domain membership, a nonzero center, and inclusive configured
   evidence min/max. Zero is missing evidence even if the interval includes zero.
2. Scan radii **1 through 6**, in ascending order; at each radius scan horizontal,
   vertical, then the two diagonals `(1,1)` and `(1,-1)`. Read opposite pairs only
   after checking **both** X/Y bounds and actual runline-domain membership. Both
   endpoints must be nonzero and in the evidence interval.
3. For each usable pair, `N=min(a,b)`, `F=max(a,b)`. Any pair with
   `F-N > boundary_gap`, `center-N > center_margin`, and
   `F-center > center_margin` establishes **MixedCandidate**. Stop on success.
   A center outside both endpoints cannot qualify. No later point depends on this
   decision or on any previously written flag.
4. Without a match: BoundaryRisk if any usable pair passed the gap gate; otherwise
   Retained if there was a usable pair, or Unclassified if none. Unsupported source
   contexts remain Unclassified under the existing producer gates.

There are **no** support-group/refinement/half-plane tests, radius-one profile
validity gate, smooth-ramp rescue, same-depth/third-surface rescue, island fallback,
mask erosion or temporal state. Center neighbors 50 has no special meaning here.
A coherent strip, ramp or real narrow object bracketed by two depths may disappear;
a broad stable interior without a discontinuity is retained. One noisy pair can
flag a real surface: inspect **Highlight before Hide**.

Active existing parameters: `evidence_min`, `evidence_max`, `boundary_gap`,
`center_margin`. Anchor tolerance, ramp residual, surface neighbors and center
neighbors are **not applicable**, disabled/labeled in the UI, echoed and preserved
unchanged for rollback. All fields retain their complete unsigned storage ranges;
no new empirical bounds, coupled limits, clamping or automatic tuning were added.
Reversed evidence intervals supply no evidence. The fixed radius is part of ID 3,
not a hidden reuse of a configuration field.

`gated` counts centers with any usable pair exceeding the gap (including suspects),
`detailed = gated`, `early_retained = 0`. Final classes partition the record prefix;
`mixed + boundary <= detailed`. No wire-layout/length change: the existing version
field carries 3. Config API capabilities advertise `[1,2,3]`. New UI clients hide
unadvertised choices; old decoders reject ID 3, rather than silently claiming active
filtering on an unfiltered stream. Full applied config/generation matching remains
mandatory, including the preserved inactive values.

CLI `--sampled-boundary-cleanup aggressive-mm-v1` enables ID 3 with ordinary default
thresholds **250..6000 / gap 200 / margin 60**. It does not adopt a previous live
process's tuning. The console can select ID 3 without changing any threshold.
The one-shot diagnostic records actual applied settings and retains its separate
schema-2 exclusions, including complete all-invalid output.

## Work and safety bounds

At most **24 opposite pairs / 49 scalar source loads** per emitted center,
including center; at most a 13x13 footprint, no copied/staged depth window. Each
probe is checked; the old radius-two interior bit is deliberately NOT used to
skip checks. Signed depth differences are in `[-65535,65535]`; coordinates are
bounded by x `[-6,645]`, y `[-6,485]`. Only checked coordinates are indexed.
No depth sums, floating-point conversions, dynamic scratch, allocations/free,
source/XYZD copies, additional persistent memory, new threads or queues.
The fixed four-direction table and scalar locals are the entire kernel scratch.
Work is O(24 pairs) per center, with an early success exit (not a separate
`early_retained` shortcut). Off never enters any neighborhood classifier.
Topology invalidation, units/plugin/sampling gates, publication leases and CAS
ownership are unchanged. Multi-core work was not needed for this implementation.

## Saved-frame evidence and reproducibility

Phase evidence (ignored/private):
`_build/aggressive-boundary-band/phase-20261026-144138/`.
Original `arm2/` remains read-only and passes the existing validator. Source
mapping, processed depths and projection factors are taken from the capture,
never regenerated from repository calibration defaults.

The operator identifies the broad green/cyan scatter behind the person in
`top2.png` as unwanted, except the actual vertical wall. **That screenshot is not
registered to the `arm2` numeric capture.** We cannot honestly claim exact
per-record coverage of it, or physical preservation of that wall, from this replay.
The report's source-space regions are analyst examples, not a bad-pixel mask;
particularly, not every point in the hand/context region is wrong.

Reference tuning: Exact, evidence 250..6000, gap 120, tolerance 20, margin 30,
ramp residual 15, surface support 1, center support **50**. The center-50 flag
prefix is byte-identical to the saved pre-phase center-25 prefix (both disable
the old 24-neighbor third-surface veto).

| Candidate | Suspects / 36,692 |
| --- | ---: |
| Existing Exact / center 50 | 11,337 |
| Radius 4 / any pair | 22,910 |
| **Selected radius 6 / any pair** | **27,012** |
| Radius 6 / two agreeing pairs | 23,562 |
| Radius 6 / any pair + 75% outer island votes | 27,567 |
| ID 3 with ordinary default gap 200 / margin 60 | 19,212 |

The selected reference-tuned policy adds 15,679 suspects and retains four old
suspects (the old algorithm refines anchors; the new one does not). It is not a
strict superset. Larger suspect counts are **not accuracy**. The paired top/front
and exact source maps show much less intermediate scatter, alongside visible
scene erosion. Island rejection added only 555 beyond bracketing and further
removed outlines; it was not integrated. One-sided/unbracketed islands may remain.
No unconditional AOI-edge/background deletion was introduced.

| Analyst source region (inclusive) | Records | Exact center50 suspects | ID3 reference-tuned suspects |
| --- | ---: | ---: | ---: |
| Finger transition x315..350, y390..425 | 324 | 154 | 268 |
| Hand context x140..365, y350..440 (contains real surfaces) | 4,816 | 1,547 | 3,334 |
| Left mid-scene x60..90, y180..260 | 632 | 121 | 291 |
| Right mid-scene x530..570, y160..240 | 827 | 197 | 423 |
| Numerical stable left x80..95, y312..327 | 64 | 0 | 1 |
| Numerical stable right x568..583, y232..247 | 64 | 0 | 4 |
| Numerical stable foreground x144..159, y384..399 | 64 | 0 | 0 |

The three small controls have full processed-depth spans of 78 mm; probes can
reach beyond their edges. ID3 defaults flag zero in all three. These are numerical
stability controls, **not** operator-labeled broad wall/body retention proof.
Both previously illustrative finger-edge records (32928/33870) are flagged by
both center50 and ID3; they are not counted as newly solved misses.

Reproduce on a little-endian host, using a **new** output directory:

```sh
python3 tools/pointcloud_capture.py arm2 --validate-only
mkdir -p NEW/replay
c++ -O2 -std=c++17 -Iinclude tools/aggressive_cleanup_replay.cpp -o NEW/replay/replay
NEW/replay/replay arm2 NEW/replay > NEW/replay/sweep.txt
python3 tools/aggressive_cleanup_report.py arm2 NEW/replay NEW/images
python3 -m http.server --bind 127.0.0.1 --directory NEW/images 8769
# Open http://127.0.0.1:8769/comparison.html locally.
```

The HTML compares unchanged orthographic API XZ/XY and exact emitted source pixels;
clicking identifies record/source/XYZD and flags. It is not a GPU depth-tested pick
or an exact screenshot camera registration. CSV and group JSONs preserve each
region's rejected/visible record IDs; provenance records capture/mask hashes.
Saved PNGs in phase evidence were exported from those same canvases. Original
capture and unfiltered images are preserved. Native replay checks every ID3
rejection against the independent all-pairs prototype before reporting timings.

## Host performance (not target qualification)

AMD Ryzen 5 5600X, GCC 16.2.1, Release, pinned logical CPU 0; two rounds of
300 On frames, Off/On/Off, 30 warmups per phase. Values below are total production
emitter **thread CPU mean ms/frame**, including XYZD/flags/publication, not the
whole application or camera acquisition. All geometry/source/counter checks pass.

| Layout / synthetic scene | Off (about) | Exact | Aggressive |
| --- | ---: | ---: | ---: |
| AOI / flat | 0.10 | 0.515 | 3.046 |
| AOI / dense mixed | 0.10 | 2.402 | 0.533 |
| AOI / dense noisy | 0.10 | 2.108 | 2.173 |
| Full domain / flat | 0.20 | 1.122 | 6.865 |
| Full domain / dense mixed | 0.20 | 5.675 | 0.681 |
| Full domain / dense noisy | 0.20 | 4.744 | 4.817 |

Flat scenes are a deliberate worst case for this rule: it must inspect the larger
footprint before retaining a center. It is **not universally faster** than Exact.
Aggressive full-domain flat CPU p95 was 6.977 ms, versus 0.706 ms for dense mixed;
raw JSON retains wall, p95/p99/max and Off-before/after values for all six scenes.
The saved `arm2` reference-tuned kernel-only replay (`-O2`, 300 iterations, pinned
CPU 0) measured Off loop 0.008 ms, center50 Exact 3.991 ms, Aggressive 2.294 ms.
That replay excludes geometry, publication, acquisition and transport; its Off
loop is not a measured live Off producer cost. No ARM timing is inferred.

Host release suite: 45/45; focused ASan and UBSan: 7/7 each; focused TSan delivery:
1/1. Real WebGL1/2 tests cover ID3 Highlight/Hide, clipping **and EDL color/depth**,
no hidden foreground depth silhouettes in either record order, pending/stale
configurations, draft preservation, unadvertised modes and PNG provenance.
Native browser/API tests preserve raw/OBJ/binary; diagnostic tests include ID3
preset geometry/source parity and all-invalid schema-2 output. The old decoder
rejects ID3; the old classifier template body is byte-identical to the baseline.
Fresh-context read-only critic found no actionable defects in the implementation
or supplied evidence; final timing/handoff artifacts and the subsequent test-only
EDL extension were parent-validated, not independently re-reviewed.

The full `./cross-build.sh` recovered-Yocto build/package passed the 15-file ELF
closure/strong-symbol/RPATH/runtime-leak audit, old GCC 7.3-header compile probe and
archive validation. Packaged `chiton.bin` matches the rebuilt ELF; current console
assets match the archive. No ARM executable was run.

Artifact: phase `cross/rel_aggressive_bands_20261026_v1.tar.gz`.
SHA-256 archive: `74fc1c8088f661bba1c0f031cf3a1053d39651815f4497a0922903484eab74d6`.
SHA-256 binary: `01bbcd2a6705e5a50d2d2f2afe8949f1235a61c0cc498d4c857b10123ce6beff`.

## Qualification boundary

Runtime goal: **20 FPS on i.MX8 Mini, preferably one core** (50 ms for the entire
frame pipeline, not an extra 50 ms for cleanup). Host kernel-only arm2 measurements
and full production-emitter synthetic Off/Exact/Aggressive CPU/wall comparisons
are retained under phase `replay/` and `performance/`. They are not ARM cadence or
whole-process CPU qualification. Allocation/copy guards and sanitizer tests are
separate; exact commands/results, package hashes and review disposition are in the
phase handoff report. Target capture, deployment and ARM execution are unperformed
and require separate approval. Final operator acceptance must inspect the actual
screenshot scene and broad wall/person retention; the vast-majority visual target
and target 20 FPS remain **unverified**, not inferred from count increases.
