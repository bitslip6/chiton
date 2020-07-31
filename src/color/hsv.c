#include <stdint.h>
#include "demos.h"
#include "color/hsv.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * convert HSV8 value to RGB8 struct
 */ 
void HsvToRGB8(HSV8 *hsv, RGB8 *c) {

    if (hsv->s <= 0 || hsv->v <= 0) {
        c->r = 0;
        c->g = 0;
        c->b = 0;
        return;
    }

    uint8_t region=0, remainder=0, p=0, q=0, t=0;
    region = hsv->h / 43;
    remainder = (hsv->h - (region * 43)) * 6; 

    p = (hsv->v * (255 - hsv->s)) >> 8;
    q = (hsv->v * (255 - ((hsv->s * remainder) >> 8))) >> 8;
    t = (hsv->v * (255 - ((hsv->s * (255 - remainder)) >> 8))) >> 8;

    switch (region)
    {
        case 0:
            c->r = hsv->v;
            c->g = t;
            c->b = p;
            break;
        case 1:
            c->r = q;
            c->g = hsv->v;
            c->b = p;
            break;
        case 2:
            c->r = p;
            c->g = hsv->v;
            c->b = t;
            break;
        case 3:
            c->r = p;
            c->g = q;
            c->b = hsv->v;
            break;
        case 4:
            c->r = t;
            c->g = p;
            c->b = hsv->v;
            break;
        default:
            c->r = hsv->v;
            c->g = p;
            c->b = q;
            break;
    }
}

/**
 * convert HSV value to u32 RGB value (ARGB);
 */ 
uint32_t HSV8To32(HSV8 *hsv) {

    // bounds checking
    if (hsv->s <= 0 || hsv->v <= 0) {
        return 0;
    }

    uint8_t region=0, remainder=0, p=0, q=0, t=0;
    region = hsv->h / 43;
    remainder = (hsv->h - (region * 43)) * 6; 

    p = (hsv->v * (255 - hsv->s)) >> 8;
    q = (hsv->v * (255 - ((hsv->s * remainder) >> 8))) >> 8;
    t = (hsv->v * (255 - ((hsv->s * (255 - remainder)) >> 8))) >> 8;

    switch (region)
    {
        case 0:
			return hsv->v << 16 | t << 8 | p;
            break;
        case 1:
			return q << 16 | hsv->v << 8 | p;
            break;
        case 2:
			return p << 16 | hsv->v << 8 | t;
            break;
        case 3:
			return p << 16 | q << 8 | hsv->v;
            break;
        case 4:
			return t << 16 | p << 8 | hsv->v;
            break;
        default:
			return hsv->v << 16 | p << 8 | q;
            break;
    }

    return 0;
}


/**
 * convert HSV value to u32 RGB value (ARGB);
 */ 
uint32_t HsvToRgb(uint8_t h, uint8_t s, uint8_t v) {

    // bounds checking
    if (s <= 0 || v <= 0) {
        return 0;
    }

    uint8_t region=0, remainder=0, p=0, q=0, t=0;
    region = h / 43;
    remainder = (h - (region * 43)) * 6; 

    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region)
    {
        case 0:
			return v << 16 | t << 8 | p;
            break;
        case 1:
			return q << 16 | v << 8 | p;
            break;
        case 2:
			return p << 16 | v << 8 | t;
            break;
        case 3:
			return p << 16 | q << 8 | v;
            break;
        case 4:
			return t << 16 | p << 8 | v;
            break;
        default:
			return v << 16 | p << 8 | q;
            break;
    }

    return 0;
}


/**
 * floating point hsv calc for reference
 * hsv to rgb with floats...
 */ 
void HsvToRgb_f(u16 in, u16 max, RGB8 *out) {

    //static RGB8 cache = new RGB8[max];

    float      hh, p, q, t, ff;
    int        i;

    hh = ((float)in/(float)max)*360.0f;
    if (hh >= 360.0f) {
        hh = 0.0;
    }
    hh /= 60.0f;
    i = (int)hh;
    ff = hh - i;
    p = 1.0f * (1.0f - 1.0f);
    q = 1.0f * (1.0f - (1.0f * ff));
    t = 1.0f * (1.0f - (1.0f * (1.0f - ff)));

    switch(i) {
    case 0:
        out->r = 255;
        out->g = t * 255;
        out->b = p * 255;
        break;
    case 1:
        out->r = q * 255;
        out->g = 255;
        out->b = p * 255;
        break;
    case 2:
        out->r = p * 255;
        out->g = 255;
        out->b = t * 255;
        break;

    case 3:
        out->r = p * 255;
        out->g = q * 255;
        out->b = 255;
        break;
    case 4:
        out->r = t * 255;
        out->g = p * 255;
        out->b = 255;
        break;
    case 5:
    default:
        out->r = 255;
        out->g = p * 255;
        out->b = q * 255;
        break;
    }
}

#ifdef __cplusplus
}
#endif
