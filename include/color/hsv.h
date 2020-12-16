#ifndef __HSV_H
#define __HSV_H 1

#include <stdint.h>
#include "demos.h"


/**
 * Convert uint color to RGB 8 bit color space
 */
static inline void U32RGB8(uint32_t c, RGB8 *rgb) {
    rgb->r = (c & 0x00FF0000) >> 16;
    rgb->g = (c & 0x0000FF00) >> 8;
    rgb->b = (c & 0x000000FF);
}

/**
 * Convert  RGB 8 bit color space to uint color
 */
static inline uint32_t RGB8U32(RGB8 *c1) {
    return c1->r << 16 | c1->g << 8 | c1->b;
}

static inline uint32_t RGBA8U32(RGBA8 *c1) {
    return (uint8_t)(c1->r * c1->a) << 16 | (uint8_t)(c1->g * c1->a) << 8 | (uint8_t)(c1->b * c1->a);
}


void HsvToRGB8(HSV8 *h, RGB8 *c);
uint32_t HsvToRgb(uint8_t h, uint8_t s, uint8_t v);
uint32_t HSV8To32(HSV8 *hsv);
void HsvToRgb_f(unsigned short in, unsigned short min, unsigned short max, float rotation, RGB8 *out);


RGB8* init_rgb_cache(void);
//void depth_to_grayscale(uint16_t *value, RGB8 *ptr);
void depth_to_grayscale2(uint16_t depth, unsigned int rgb_offset);
//void depth_to_grayscale3(uint16_t depth, unsigned int rgb_offset);
//PixelFunc2 depth_to_grayscale_curry(unsigned char *rgb_data);
std::function <void(uint16_t, uint)> depth_to_grayscale_curry(unsigned char *rgb_data);
// void depth_to_hsv(uint16_t *value, RGB8 *ptr, RGB8 *cache);
void depth_to_hsv(uint16_t *value, RGB8 *ptr);
void depth_to_hsv_raw(uint16_t *value, unsigned char *ptr);
void init_depth_to_hsv(int min_value, int max_value, float rotation, RGB8 *cache);

/**
 * fill draw_ptr with HSV color
 */
#define FILL_HSV(disp, hsv8) SetDisp(disp, HSV8To32(hsv8))

#endif
