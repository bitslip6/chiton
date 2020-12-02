#include <stdint.h>
#include <algorithm>
#include <cassert>

#include "util.h"
#include "utils.hpp"
#include "system.h"
#include "color/hsv.h"

extern GlobalOpts *GOPTS;
extern RGB8 *hsv_cache;

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
 * pre-initializing pixel cache, returns pointer to new memory
 * 
 * SIDE EFFECT: new memory
 */
RGB8* init_rgb_cache(void) {
    TRACE("");
    RGB8* depth_cache = (RGB8*)NEWMEM(RGB8, MAX64K);
    init_depth_to_hsv(GOPTS->min_cam_value, GOPTS->max_cam_value, GOPTS->color_rotation, depth_cache);
    return depth_cache;
}



/**
 * floating point hsv calc for reference
 * hsv to rgb with floats...
 */ 
void HsvToRgb_f(unsigned short in, unsigned short minv, unsigned short maxv, float rotation, RGB8 *out) {

    float      hh, p, q, t, ff;
    int        i;

    // make the value darker for distance values... ?? looks a bit ugly...
    //uint16_t bit8 = in >> 4; // clip 12 bit values to 8 bit values
    uint16_t bit8 = 255; // clip 12 bit values to 8 bit values

    // TODO: underflow here...
    in -= minv;
    maxv -= minv;
    //in = std::min(in, maxv);
    //in = (in < minv) ? 1 : in - minv;


    hh = ((float)in/(float)maxv)*360.0f;
    hh += rotation;
    while (hh >= 360.0f) {
        hh -= 360.0f;
    }
    hh /= 60.0f;
    i = (int)hh;
    ff = hh - i;
    p = 1.0f * (1.0f - 1.0f);
    q = 1.0f * (1.0f - (1.0f * ff));
    t = 1.0f * (1.0f - (1.0f * (1.0f - ff)));

    switch(i) {
    case 0:
        out->r = bit8;
        out->g = t * 255;
        out->b = p * 255;
        break;
    case 1:
        out->r = q * 255;
        out->g = bit8;
        out->b = p * 255;
        break;
    case 2:
        out->r = p * 255;
        out->g = bit8;
        out->b = t * 255;
        break;
    case 3:
        out->r = p * 255;
        out->g = q * 255;
        out->b = bit8;
        break;
    case 4:
        out->r = t * 255;
        out->g = p * 255;
        out->b = bit8;
        break;
    case 5:
    default:
        out->r = bit8;
        out->g = p * 255;
        out->b = q * 255;
        break;
    }
}
/**
 * 
 * TODO: perhaps we should return  
 * SIDE EFFECT: cache has 65K initialized cache values
 */
void init_depth_to_hsv(int min_value, int max_value, float rotation, RGB8 *cache) {
    // lower bounded out of range values
    for (int i = 0; i < min_value; i++) {
        cache[i].r = 250;
        cache[i].g = 50;
        cache[i].b = 50;
    }

    // TODO: recalculate the "spread" here (ratio of i to max_value)
    for (int i = min_value; i < max_value; i++) {
        HsvToRgb_f(i, min_value, max_value, rotation, &cache[i]);
    }

    // upper bounded out of range values
    for (int i = max_value; i < MAX64K-1; i++) {
        cache[i].r = 0;
        cache[i].g = 0;
        cache[i].b = 0;
    }

    printf("id to hsv 4\n");
}



/**
 * convert 12bit value to greyscale
 * 
 * SIDE EFFECT: maniuplates 1 RGB8 struct pointed to by ptr
 */
void depth_to_grayscale(uint16_t *value, RGB8 *ptr) {
    uint16_t v = *value >> 4; // clip 12 bit values to 8 bit values
    ptr->r = ptr->g = ptr->b = (unsigned char)v;
}


/**
 * cached hsv to rgb color
 * 
 * NOTE: requires cache to be at least 2^16 length
 * SIDE EFFECT: ptr is updated
 */
void depth_to_hsv(uint16_t *value, RGB8 *ptr) {
    ptr->r = hsv_cache[*value].r;
    ptr->g = hsv_cache[*value].g;
    ptr->b = hsv_cache[*value].b;
 }

