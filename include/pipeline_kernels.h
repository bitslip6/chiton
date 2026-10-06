#pragma once
#include "neon_compat.h"
#include <cstddef>
#include <cstdint>

// Experimental kernels. Callers retain AOI/runline boundaries and output
// ownership. No saturation, rounding, fast-math, FP16 or fused operations.
namespace pipeline_kernels {
inline void grey16_reference(const uint16_t *src, uint8_t *dst, size_t count, bool invert) {
    for(size_t i=0;i<count;++i) dst[i]=invert ? 255-src[i]/16 : src[i]/16;
}
template<bool Invert>
inline void grey16(const uint16_t *src, uint8_t *dst, size_t count) {
    size_t i=0;
    for(;count-i>=8;i+=8) {
        // Shift is exact unsigned /16. Non-saturating narrowing preserves the
        // existing uint8 wrapping for inputs above the normal sensor range.
        uint8x8_t bytes=vmovn_u16(vshrq_n_u16(vld1q_u16(src+i),4));
        if(Invert) bytes=vsub_u8(vdup_n_u8(255),bytes);
        vst1_u8(dst+i,bytes);
    }
    grey16_reference(src+i,dst+i,count-i,Invert);
}

// Explicit multiply/add stages must remain non-fused. Do not force a global
// FP policy on the existing scalar application. Qualification compares these
// paths with its actual scalar results; scalar compiler contraction may differ.
#if defined(__GNUC__) && !defined(__clang__)
#define CHITON_ORDERED_FLOAT __attribute__((optimize("fp-contract=off")))
#else
#define CHITON_ORDERED_FLOAT
#endif

template<bool Smooth>
CHITON_ORDERED_FLOAT inline void average(const uint16_t *src,uint16_t *dst,size_t count) {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
    static_assert(Smooth,"Unsmoothed averaging retains the caller's memcpy");
    size_t i=0;
    for(;count-i>=4;i+=4) {
        const auto source=vcvtq_f32_u32(vmovl_u16(vld1_u16(src+i)));
        const auto previous=vcvtq_f32_u32(vmovl_u16(vld1_u16(dst+i)));
        const auto old_part=vmulq_n_f32(previous,.8f);
        const auto new_part=vmulq_n_f32(source,.2f);
        float values[4]; vst1q_f32(values,vaddq_f32(old_part,new_part));
        // Keep the original C++ float-to-uint16 truncation/invalid-input policy,
        // rather than adding SIMD saturation or a different conversion policy.
        for(size_t lane=0;lane<4;++lane) dst[i+lane]=values[lane];
    }
    for(;i<count;++i) dst[i]=dst[i]*.8f+src[i]*.2f;
}

template<bool Smooth>
CHITON_ORDERED_FLOAT inline void calibrate(const uint16_t *src,uint16_t *dst,
                                         const float *gain,const float *offset,size_t count) {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
    size_t i=0;
    for(;count-i>=4;i+=4) {
        const auto source=vcvtq_f32_u32(vmovl_u16(vld1_u16(src+i)));
        const auto scaled=vmulq_f32(source,vld1q_f32(gain+i));
        auto corrected=vaddq_f32(scaled,vld1q_f32(offset+i));
        if(Smooth) {
            const auto previous=vcvtq_f32_u32(vmovl_u16(vld1_u16(dst+i)));
            const auto old_part=vmulq_n_f32(previous,.7f);
            const auto new_part=vmulq_n_f32(corrected,.3f);
            corrected=vaddq_f32(old_part,new_part);
        }
        float values[4]; vst1q_f32(values,corrected);
        for(size_t lane=0;lane<4;++lane) dst[i+lane]=values[lane];
    }
    for(;i<count;++i) {
        const float corrected=src[i]*gain[i]+offset[i];
        dst[i]=Smooth ? dst[i]*.7f+corrected*.3f : corrected;
    }
}
#undef CHITON_ORDERED_FLOAT
}
