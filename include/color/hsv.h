#ifndef __HSV_H
#define __HSV_H 1

#include <stdint.h>

// integer based HSV 
typedef struct HSV8 {
    uint8_t h;       // angle in degrees 0 - 255
    uint8_t s;       // a number between 0 and 255
    uint8_t v;       // a number 0 and 255
} HSV8;

// integer based RGB
typedef struct RGB8 {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} RGB8;

struct RGB8_s {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

// integer based RGB
typedef struct RGBA8 {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    float a;
} RGBA8;


#define RGBf(x) {x*255, x*255, x*255}
#define RGBfff(x,y,z) {x*255, y*255, z*255}
#define RGBu(x) {x, x, x}
#define RGBuuu(x,y,z) {x, y, z}


#define RGBAf(x) {x*255, x*255, x*255, 1.0f}
#define RGBAfff(x,y,z) {x*255, y*255, z*255, 1.0f}
#define RGBAffff(x,y,z,a) {x*255, y*255, z*255, a}


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

/**
 * fill draw_ptr with HSV color
 */
#define FILL_HSV(disp, hsv8) SetDisp(disp, HSV8To32(hsv8))

#endif
