#include <stdint.h>
#include <stdbool.h>
#include "demos.h"
#include "color/blends.h"
#include "color/hsv.h"
#include "mathcore.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * blend 2 RGB pixels (src,dst) using blend mode (constant begins with BM_)
 * store result in dst
 */
void BlendPixel(RGB8 *dst, RGB8 *src, uint8_t mode) {
    switch(mode) {
        case BM_NORMAL:
            dst->r = ChannelBlend_Normal(src->r, dst->r);
            dst->g = ChannelBlend_Normal(src->g, dst->g);
            dst->b = ChannelBlend_Normal(src->b, dst->b);
            break;
        case BM_LIGHTEN:
            dst->r = ChannelBlend_Lighten(src->r, dst->r);
            dst->g = ChannelBlend_Lighten(src->g, dst->g);
            dst->b = ChannelBlend_Lighten(src->b, dst->b);
            break;
        case BM_DARKEN:
            dst->r = ChannelBlend_Darken(src->r, dst->r);
            dst->g = ChannelBlend_Darken(src->g, dst->g);
            dst->b = ChannelBlend_Darken(src->b, dst->b);
            break;
        case BM_MULTIPLY:
            dst->r = ChannelBlend_Multiply(src->r, dst->r);
            dst->g = ChannelBlend_Multiply(src->g, dst->g);
            dst->b = ChannelBlend_Multiply(src->b, dst->b);
            break;
        case BM_COLORBURN:
            dst->r = ChannelBlend_ColorBurn(src->r, dst->r);
            dst->g = ChannelBlend_ColorBurn(src->g, dst->g);
            dst->b = ChannelBlend_ColorBurn(src->b, dst->b);
            break;
        case BM_SOFTLIGHT:
            dst->r = ChannelBlend_SoftLight(src->r, dst->r);
            dst->g = ChannelBlend_SoftLight(src->g, dst->g);
            dst->b = ChannelBlend_SoftLight(src->b, dst->b);
            break;
        case BM_HARDLIGHT:
            dst->r = ChannelBlend_HardLight(src->r, dst->r);
            dst->g = ChannelBlend_HardLight(src->g, dst->g);
            dst->b = ChannelBlend_HardLight(src->b, dst->b);
            break;
        case BM_GLOW:
            dst->r = ChannelBlend_Glow(src->r, dst->r);
            dst->g = ChannelBlend_Glow(src->g, dst->g);
            dst->b = ChannelBlend_Glow(src->b, dst->b);
            break;
        case BM_OVERLAY:
            dst->r = ChannelBlend_Overlay(src->r, dst->r);
            dst->g = ChannelBlend_Overlay(src->g, dst->g);
            dst->b = ChannelBlend_Overlay(src->b, dst->b);
            break;
    }
}

/**
 * 
 */
void BlendPixelAlpha(RGB8 *dst, RGBA8 *src, uint8_t mode) {
    if (mode == BM_NORMAL) {
        dst->r = MIX(dst->r, src->r, src->a);
        dst->g = MIX(dst->g, src->g, src->a);
        dst->b = MIX(dst->b, src->b, src->a);
    } else {
        RGB8 s;
        s.r = MIX(src->r, 0, src->a);
        s.g = MIX(src->g, 0, src->a);
        s.b = MIX(src->b, 0, src->a);
        BlendPixel(dst, &s, mode);
    }
}



/**
 * blends buffer2 into buffer1 using mode as the blend mode
 * if ignore_black is true, buffer2 black pixels are not blended
 */
void Blend(DisplayBuffers *disp, uint8_t mode, bool ignore_black) {

    u8 r=0, g=0, b=0;
    for (u16 offset=0; offset<disp->sz; offset++) {
        if (ignore_black == true && disp->buffer2[offset] == 0) { continue; }

        RGB8 src, dst;
        U32RGB8(disp->buffer2[offset], &src);
        U32RGB8(disp->buffer1[offset], &dst);

        switch(mode) {
            case BM_NORMAL:
                r = ChannelBlend_Normal(src.r, dst.r);
                g = ChannelBlend_Normal(src.g, dst.g);
                b = ChannelBlend_Normal(src.b, dst.b);
                break;
            case BM_LIGHTEN:
                r = ChannelBlend_Lighten(src.r, dst.r);
                g = ChannelBlend_Lighten(src.g, dst.g);
                b = ChannelBlend_Lighten(src.b, dst.b);
                break;
            case BM_DARKEN:
                r = ChannelBlend_Darken(src.r, dst.r);
                g = ChannelBlend_Darken(src.g, dst.g);
                b = ChannelBlend_Darken(src.b, dst.b);
                break;
            case BM_MULTIPLY:
                r = ChannelBlend_Multiply(src.r, dst.r);
                g = ChannelBlend_Multiply(src.g, dst.g);
                b = ChannelBlend_Multiply(src.b, dst.b);
                break;
            case BM_COLORBURN:
                r = ChannelBlend_ColorBurn(src.r, dst.r);
                g = ChannelBlend_ColorBurn(src.g, dst.g);
                b = ChannelBlend_ColorBurn(src.b, dst.b);
                break;
            case BM_SOFTLIGHT:
                r = ChannelBlend_SoftLight(src.r, dst.r);
                g = ChannelBlend_SoftLight(src.g, dst.g);
                b = ChannelBlend_SoftLight(src.b, dst.b);
                break;
            case BM_HARDLIGHT:
                r = ChannelBlend_HardLight(src.r, dst.r);
                g = ChannelBlend_HardLight(src.g, dst.g);
                b = ChannelBlend_HardLight(src.b, dst.b);
                break;
            case BM_GLOW:
                r = ChannelBlend_Glow(src.r, dst.r);
                g = ChannelBlend_Glow(src.g, dst.g);
                b = ChannelBlend_Glow(src.b, dst.b);
                break;
            case BM_OVERLAY:
                r = ChannelBlend_Overlay(src.r, dst.r);
                g = ChannelBlend_Overlay(src.g, dst.g);
                b = ChannelBlend_Overlay(src.b, dst.b);
                break;

        }
        //printf("R: [%d.%d=%d] G: [%d.%d=%d] B: [%d.%d=%d]\n", src.r, dst.r, r, src.g, dst.g, g, src.b, dst.b, b);
        disp->buffer1[offset] = r << 16 | g << 8 | b;
    }
}

#ifdef __cplusplus
}
#endif
