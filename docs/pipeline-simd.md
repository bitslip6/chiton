# Experimental image/calibration SIMD paths

These are independently selectable **default-off** changes. No build, ARM
execution, sanitizer run, assembly inspection or target performance acceptance
was performed by the agent. The current camera startup failure must be diagnosed
before live capture A/B measurements are meaningful. Source implementation is
not evidence of a speedup or acceptance.

## Greyscale first (Phase 4b, /16 only)

`CHITON_SIMD_GREYSCALE=ON` replaces only the regular uncalibrated /16 greyscale
loops, for depth (inverted) and IR (non-inverted). Eight unsigned uint16 pixels
are loaded, shifted right by four, narrowed **without saturation**, and optionally
inverted with wrapping uint8 subtraction. Scalar tails preserve short/odd runs.
Runline endpoints, untouched AOI pixels and byte mapping are unchanged.

The /24, /26 and planar paths remain scalar. Colour, preview, JPEG settings and
compressor ownership are unchanged. Explicit SIMD is not assumed faster than
GCC's existing vectorization; inspect reports and generated ARM assembly before
acceptance, then compare actual benchmark/live timings.

Build externally as a new artifact:

```sh
./cross-build.sh --name rel_simd_grey_v1 --simd-greyscale
```

Stop the existing camera owner before any operator target comparison. On the
compatible target run `./benchmarks/run.sh image grey16 1000`, compare the previous
accepted fixture checksum/timings, and measure `conversion` and camera CPU through
`/stream/metrics` with identical settings. The benchmark manifest/output records
which implementation flags were selected. Revert by running the prior accepted
release; a normal `cross-build.sh` invocation explicitly restores scalar options
rather than inheriting a prior workspace cache selection.

## Calibration/smoothing second (Phase 4c)

Only **after greyscale is accepted**, separately qualify
`CHITON_SIMD_CALIBRATION=ON`. Four lanes are widened from uint16 to unsigned uint32
then float32. Gain/offset and optional smoothing use explicit multiply/add stages;
float results retain C++ float-to-uint16 casts rather than adding saturation,
rounding or a new invalid-input policy. Unsmoothed averaging retains its existing
full-frame memcpy. Smoothed averaging uses the existing AOI and .8/.2 weights;
calibration smoothing retains .7/.3 weights and its existing history.

SLAM scatter remains scalar and follows the original runline/pixel order,
including repeated buckets and the `src > 250` rule. Arithmetic and scatter are
two passes **per runline**, adding memory traffic that must earn its cost.

The new float kernels disable contraction locally (GCC per-function option or
Clang pragma); they use no new FMA, FP16 or fast-math. A calibration build with
`__FAST_MATH__` is rejected. Existing scalar code and its compiler options are
not globally changed.

**Important numerical gate:** the previous scalar compiler may already contract
expressions into FMA. Ordered SIMD results can then differ near truncation
boundaries even without changing source formulas. The tests compare against the
original scalar expressions under the same build flags and intentionally fail
on any difference. Do not weaken tests, silently enable contraction, or accept
changed calibration values. Leave the calibration path off if equivalence cannot
be established; any arithmetic-policy change needs a separately approved
correctness decision. Calibration inputs outside the scalar cast's defined
finite uint16 domain are not given an invented SIMD validity policy.

After that gate passes, create a **separate** artifact (adding the accepted
greyscale flag only if it was previously accepted):

```sh
./cross-build.sh --name rel_simd_cal_v1 --simd-greyscale --simd-calibration
```

Compare `average` and `calibrate` benchmark kernels with smoothing 0/1, all fixture
and SLAM checksums, and live `calibration_smoothing`/processing/camera CPU metrics.
Keep instrumentation and compiler flags constant. Reject any output, latency,
control-response, memory or capture-rate regression. Revert kernels independently
by omitting their flags in a new versioned artifact. Defaults remain scalar.

## External host validation

Use new host build directories, `USE_ADITOF_STUB=ON`, `USE_SDL2=OFF`, and enable
`BUILD_PIPELINE_BENCHMARKS` for diagnostics. The corresponding CMake options are
`CHITON_SIMD_GREYSCALE` and `CHITON_SIMD_CALIBRATION`. Do not execute cross-built
CTest programs on the host.

Added targets:

- `pipeline_kernels_test`: exhaustive /16 inputs in both directions, guard bytes,
  SIMD-unaligned starts, zero/short/odd spans, scalar tails, varied calibration
  gains/positive and negative offsets in the defined cast domain, and repeated
  smoothing histories.
- `pipeline_kernels_portable_test`: the same with `SIMDE_NO_NATIVE`.
- `pipeline_integration_test`: real image/calibration entry points with both
  experimental paths compiled in, full-frame untouched-pixel comparisons,
  exhaustive /16 mapping, unchanged /24 and /26 mapping, irregular/short AOI
  runlines, capture-threshold values, repeated SLAM buckets and history equality.
- `pipeline_integration_portable_test`: identical integration coverage with
  portable SIMDe fallback.

Run those externally in release, ASan and UBSan host configurations, plus the
full simulator/UI/measurement/lifetime and appropriate TSan suites. Compare
native ARM assembly and run independently qualified target artifacts; host
SIMDe results/timings are not proof of native NEON correctness/performance.
