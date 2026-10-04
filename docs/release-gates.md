# F20 desktop release gates

## Current implementation status

**Desktop release-gate implementation is complete:** local checks, hosted CI,
Ubuntu SIMDe compatibility and required-status enforcement on `master` are all
verified. F20 remains partial only for full hardware/support qualification; the
operator's successful hardware application run is recorded as a smoke test.
No CI activation or branch-protection setup is still pending.

**Recommended next coding item: F09**, observational frame retrieval, one capture
lifecycle and bounded freshness. Then F08 can address connection/job concurrency.
The scope, presentation-mode decision and acceptance checks are in
[web-server-review.md](web-server-review.md). Neither recommendation authorizes
implementation or privileged hardware operations.

## Required checks

`.github/workflows/ubuntu1804_gcc_7.5.0_intel.yml` now runs real checks on pushes
(including tags), pull requests, and manual dispatch. Its historical filename
is retained; the runner is **Ubuntu 22.04**, not Ubuntu 18.04. The two historical
Yocto workflows are checkout-only placeholders and now run manually only, without
recursive submodule initialization; they are not release gates.

- Separate headless simulator builds: normal, ASan, UBSan, and TSan.
- Twelve CTest entries per variant: framing, wall/SLAM boundaries, socket/assets/
  validation/locking, measurement references, calibration/serialization/config,
  display, simulated SDK, simulator HTTP safety, simulator measurements,
  both shipped UI copies, fixture generator, and gate-wiring tests.
- Required Chromium recovery and native PNG/raw/OBJ download checks against a
  local mock, for **both** `release/files` and `bin/files`. Missing Chromium is a
  failure, not a skipped test. Production exports are independently checked over
  simulator HTTP; the browser mock is not an end-to-end hardware test.
- A final **Web release gate** fails if any dependency failed, skipped, or was
  cancelled and prints each dependency's result. Failed commands/timeouts expose
  bounded log excerpts directly in the Actions console; full logs and CTest
  results are uploaded even on test failure.
- Official checkout/setup-node/upload-artifact actions use their Node 24-based
  v7 releases. Their internal runtime is separate from Node 22 selected for the
  project's UI tests. A pre-check prints the runner's Git path/version.

**Required-status enforcement is enabled** on `bitslip6/chiton`'s default branch,
`master`, and verified through the GitHub API:

- Required check: `Web release gate`, bound to the GitHub Actions app (15368).
- Strict/up-to-date checks: enabled.
- Administrator enforcement: enabled; unchecked admin pushes/merges cannot bypass it.
- No review approvals required. There were no existing branch protections or
  active rulesets to overwrite. Other branches/tags are not protected by this rule.

Make changes on a feature branch, push it, open a pull request against `master`,
and merge after the gate passes. The workflow alone did not enable this policy;
it was applied using administrator access. Policy request/response, prior state,
and verification evidence are in `/tmp/chiton-web-gate-policy-8NRmLH/`.

`mkrelease.sh` now runs the full desktop gate **before** copying `bin/chiton` to
`release/chiton`. It never executes that target binary. Direct file copies or
other deployment scripts are not an enforceable security boundary; release
operators must use the gate and the hardware signoff below.

## Local reproduction

Linux dependencies: CMake >= 3.20 for the gate's CLI, a C++17 compiler, make,
pkg-config, PNG/JPEG/zlib/I2C/SIMDe development files, Python >= 3.8, Node >= 22,
and Chromium/Google Chrome. SIMDe 0.7.2 (Ubuntu 22.04's package) and newer headers
are supported. The library's ordinary CMake minimum remains 3.10.
Initialize only the recorded build dependency
(`git submodule update --init -- FunctionalPlus`); the restored `.gitmodules`
mapping makes clean CI checkout reproducible. CI intentionally does not initialize
the unrelated unmapped legacy `tools/librover` gitlink.

```sh
# Fresh output tree; existing paths are deliberately rejected.
python3 tests/release_gate.py --all --output /tmp/chiton-release-checks --jobs 2

# Individual gates for diagnosis:
python3 tests/release_gate.py --sanitizer thread --output /tmp/chiton-thread-checks
CHITON_CHROMIUM=/path/to/chromium python3 tests/release_gate.py --browser-only
```

Each build gets separate objects, executable-relative configuration, and an
unused install root. The runner clears inherited SDK/resource/sanitizer overrides,
forces `USE_ADITOF_STUB=ON`, `USE_SDL2=OFF`, and refuses hardware runtime-test
registration. It does not clean/rebuild `bin/chiton`, install software, or change
host settings. Runtime fixtures/calibration/plugins are disposable; logs survive
failures in the reported artifact directory. HTTP suites share an advisory local
lock because the current server still uses ports 80/8080. Other services must not
occupy both ports. There is no privileged invocation of the application.

Tests assert simulator identity before valid register/administrative HTTP calls.
Register access is simulated; the opt-in `ADITOF_STUB_FAIL_REGISTER=1` fault makes
only address `c0ff` fail, exercising checked read/write errors and recovery without
breaking capture. PATH tripwires block sudo/reboot/network/I2C utilities and fail
the test even if a command failure was ignored. These are regression safeguards,
not a sandbox for an arbitrary untrusted executable. HTTP simulator operations
must still return 501 for embedded-only provisioning, gateway, update, diagnostics,
and placeholder rover actions.

Sanitizers halt on the first diagnostic. Full-app ASan leak checking remains off
for existing module allocations; focused native helpers retain leak checking.
The gate also scans retained logs, including the legacy nonzero restart exit,
so a sanitizer report cannot be mistaken for a successful restart. There are no
race suppressions, retries that conceal failures, or allowed-to-fail TSan jobs.
CI lowers address-space randomization on **its disposable sanitizer runner only**
(`vm.mmap_rnd_bits=28`). Local incompatible sanitizer runtimes fail closed; the
runner never changes a desktop sysctl or skips a variant automatically.

## Added coverage and repaired failures

`http_modes.py` covers modes 0–6 in both transition directions for distinct IR/
depth fixtures, actual JPEG dimensions (640x480 or 1500x165), cloud alignment,
raw metadata/shape, invalid-mode rejection, capture-disabled error responses and
re-enablement. UI tests cover the same mode sequence plus HTTP/network/JSON/
acknowledgement failures, one-attempt rollback, and subsequent recovery. This
extends existing deadlines, fragmented framing, client isolation, exports,
settings and download inventory tests; it does not add F01 authentication or F16
capability endpoints that do not yet exist.

HTTP file-failure tests cover a directory at the calibration target, failed load/
atomic-save, temporary-sibling cleanup, unchanged saved data, and recovery. SDK
faults and forbidden host-command attempts have explicit assertions. Gate-wiring
unit tests verify nonzero tool results, sanitizer diagnostics, reused build trees,
missing browsers, both browser asset roots, environment isolation and all five
required sub-gates.

The new ASan mode-zero test originally failed on SLAM's AOI metadata sample at
`99*1024+1024`, just past a 100-bank allocation. Both application allocations now
use the existing 101-bank allocator. Focused tests also retain the spike-filter
lower-neighbour bound, bounded geometry/bucket reads, and correct final-pixel RGB
channel write. Full-mode testing stays enabled rather than excluding mode zero.

## Local implementation evidence

Passed `python3 tests/release_gate.py --all --output /tmp/chiton-f20-complete-final --jobs 4`:
all **12/12 CTest entries** in each normal/ASan/UBSan/TSan build, and both Chromium
recovery/native-download runs. Evidence is under `/tmp/chiton-f20-complete-final/`, with
per-variant `logs/` and `build/Testing/` directories. Local tools: GCC 16.2.1,
CMake 3.31.12, Node 26.7.0, Chromium 152.0.7977.82, Arch Linux. CMake was downloaded
to `/tmp` and checked against the upstream SHA-256 manifest; no system installation
or sysctl change was performed. CI instead selects Ubuntu 22.04/GCC from its apt
repository, Node 22, and the runner's Google Chrome. This is a test matrix, not a
promised hardware/browser support matrix.

### Hosted SIMDe compatibility failure

The supplied `web-gate-undefined.zip` artifact identified a compilation failure
with GCC 11.4.0: Ubuntu 22.04's SIMDe 0.7.2-6 package lacks `vmovq_n_f32` and portable
`vfmaq_laneq_f32`. Matrix helpers now use the equivalent `vdupq_n_f32` zero
constructor. `neon_compat.h` supplies a narrowly guarded FMA-lane fallback for old
non-AArch64 headers using `std::fma`, preserving fused arithmetic rather than
silently replacing it with multiply-then-add. Newer SIMDe and native AArch64 keep
their existing intrinsic path.

`web_correctness` now checks real column-major matrix multiplication against
identity/zero/nonsymmetric references, all lanes, and fused cancellation on the
fallback path. **Passed locally:** full UBSan gates, 12/12 CTest entries each,
against the extracted official Ubuntu `libsimde-dev_0.7.2-6_all.deb` headers
(`/tmp/chiton-ci-simde072-ubsan`) and current SIMDe 0.8.2 headers
(`/tmp/chiton-ci-simde082-ubsan`). The local compiler remains GCC 16.2.1, not
Ubuntu's GCC 11. Hosted success was subsequently independently verified through
the GitHub API: [run 37233153411](https://github.com/bitslip6/chiton/actions/runs/37233153411)
on commit `84b658b91df18bca463bb57a946c75623d4d2a7e` completed successfully,
including all four simulator variants, Chromium, and the final gate. Job
metadata was retrieved, not the full successful build logs/artifacts. Required
status enforcement on `master` is also verified. No CI gate was removed or weakened.

## Hardware signoff — manual qualification outstanding

The operator reports that the application runs successfully on hardware. This
records a hardware smoke test, not full target qualification: target/SDK identity,
measurement references, failure/recovery cases and checklist evidence have not
been supplied. No automated hardware operations were performed by this gate.

F20 remains partial for hardware qualification. Before claiming a device release,
the owner must name the supported embedded target/SDK/firmware/browser versions,
approve an isolated target and recovery plan, and record evidence for:

1. Capture startup, IR/depth discrimination, all supported output modes, calibrated
   units/reference measurements, real frame timing, shutdown and thermal health.
2. Save/load/restart calibration round-trips with backups and known-good recovery.
3. Physical display, SDK/register behavior and multi-client operation appropriate
   to that target, using only approved harmless register addresses.
4. Only under **separate explicit approval**, provisioning/connectivity rollback,
   diagnostics/privacy, firmware authenticity/interruption/recovery and reboot.
   F03/F14/F15 remain separate backlog work; passing desktop gates does not repair
   or approve those operations.

Record target identity, build revision, approved scope, results, artifact paths,
known limitations and owner signoff. No automatic CI job connects to hardware,
changes desktop networking, writes real registers, provisions Wi-Fi or upgrades/
reboots a host. Hardware credentials and an unsafe “enable hardware” switch are
not part of this gate.
