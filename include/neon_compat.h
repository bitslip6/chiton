#ifndef CHITON_NEON_COMPAT_H
#define CHITON_NEON_COMPAT_H

// Preserve the NEON API while using native SIMD where available (including ARM)
// or SIMDe's portable implementations on other architectures.
#ifndef SIMDE_ENABLE_NATIVE_ALIASES
#define SIMDE_ENABLE_NATIVE_ALIASES
#endif
#include <simde/arm/neon.h>

// SIMDe 0.7.2 (Ubuntu 22.04) has no portable FMA-lane implementation.
// Native AArch64 already supplies this intrinsic; newer SIMDe supplies its
// alias. On older non-AArch64 builds, preserve fused arithmetic with std::fma
// rather than substituting a separately rounded multiply followed by add.
#if !defined(SIMDE_ARM_NEON_FMA_LANE_H) && !defined(SIMDE_ARM_NEON_A64V8_NATIVE) && !defined(vfmaq_laneq_f32)
#include <cmath>
#define CHITON_NEON_FMA_LANE_FALLBACK 1
namespace chiton_neon {
template<int Lane>
inline float32x4_t fmaq_laneq_f32(float32x4_t accumulator, float32x4_t factors, float32x4_t lanes) {
    static_assert(Lane >= 0 && Lane < 4, "invalid FMA lane");
    float result[4], values[4], scales[4];
    vst1q_f32(result, accumulator);
    vst1q_f32(values, factors);
    vst1q_f32(scales, lanes);
    for (int i = 0; i < 4; ++i) result[i] = std::fma(values[i], scales[Lane], result[i]);
    return vld1q_f32(result);
}
}
#define vfmaq_laneq_f32(a, b, v, lane) chiton_neon::fmaq_laneq_f32<(lane)>((a), (b), (v))
#endif

#endif
