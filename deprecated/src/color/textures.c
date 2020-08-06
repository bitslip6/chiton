#include "demos.h"
#include "draw.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * returns a pointer to newly allocated memory 
 * TODO: MEM free Texture_Checker
 * TODO: fix the off by one error at y=/12 x=0;
 */
u32 *Texture_Checker(u8 sz, u8 hue, u8 sat, u8 hue2, u8 sat2) {
    u32 c1=HsvToRgb(hue, sat, 250);
    u32 c2=HsvToRgb(hue2, sat2, 250);
    u32 c3=0;

    u32 *tex = NEWMEM(u32, sz*sz);
    u8 sz2 = sz/2, x=0, y=0;
    u16 off=0;

    for (x=0; x<sz; x++) {
        for (y=0; y<sz; y++) {
            if (y<sz2) {
                c3 = (x<=sz2) ? c1 : c2;
            } else {
                c3 = (x<=sz2) ? c2 : c1;
            }
            tex[off++]= c3;
        }
    }

    return tex;
}

/**
 * returns a pointer to newly allocated memory 
 * TODO: MEM free Texture_XOR
 */
u32 *Texture_XOR(u8 sz, u8 hue, u8 sat) {
    float scale = 255 / sz;
    u32 *tex = NEWMEM(u32, sz*sz);
    u8 x=0, y=0, v=0;
    u32 c;

    
    u16 off = 0;
    for (x=0; x<sz; x++) {
        for (y=0; y<sz; y++) {
            v = (((x * sz / sz) ^ (y * sz / sz))*scale);
            c = HsvToRgb(hue, sat, v);
            tex[off++] = c;
        }
    }

    return tex;
}


void Buffer_Set(u8 width, u8 height, u32 *buffer, u32(*method)(u8, u8)) {
    u16 off = 0;
    for (int y=0; y<height; y++) {
        for (int x=0; x<width; x++) {
            buffer[off++] = method(x, y);
        }
    }
}

#ifdef __cplusplus
}
#endif
