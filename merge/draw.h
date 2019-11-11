#include "sys/types.h"
#include "demos.h"

#ifndef DRAW_H
#define DRAW_H 1


// the pixel dimensions
#define WIDTH 25
#define HEIGHT 36 
// #define WIDTH 37
// #define HEIGHT 22 

// framebuffer and sdl rendering info
#define FBWIDTH 1920
#define FBPIXEL_SZ 20 

// ws2811 lib ccode... (TODO rewrite this and optomize)
// defaults for cmdline options
#define TARGET_FREQ             WS2811_TARGET_FREQ
#define GPIO_PIN                12
#define DMA                     10

// #define STRIP_TYPE            WS2811_STRIP_RGB        // WS2812/SK6812RGB integrated chip+leds
#define STRIP_TYPE            WS2811_STRIP_GRB        // WS2812/SK6812RGB integrated chip+leds


//int height_map[WIDTH] = {19, 20, 20, 20, 20, 20, 19, 19, 20, 20, 20, 20, 20, 20,  19, 17, 17, 17, 16, 16, 16, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 17, 17};

typedef struct HSV {
    float h;       // angle in degrees
    float s;       // a fraction between 0 and 1
    float v;       // a fraction between 0 and 1
} HSV;

typedef struct RGB {
    float r;       // a fraction between 0 and 1
    float g;       // a fraction between 0 and 1
    float b;       // a fraction between 0 and 1
} RGB;

typedef struct RGB8 {
    u8 r;
    u8 g;
    u8 b;
} RGB8;

typedef u32 (DrawMethod)(u32, u32);

u_int32_t HsvToRgb(u_int8_t h, u_int8_t s, u_int8_t v);
RGB HsvToRgb_f(HSV in);

// Pixel functions
void inline PPixelSet(DisplayBuffers *disp, int x, int y, u_int32_t color);
void HPixelSet(DisplayBuffers *disp, u_int8_t x, u_int8_t y, u_int8_t h, u_int8_t v, u_int8_t s);
void inline HPixelOr(DisplayBuffers *disp, int x, int y, u_int8_t h, u_int8_t v, u_int8_t s);
void inline HPixelAnd(DisplayBuffers *disp, int x, int y, u_int8_t h, u_int8_t v, u_int8_t s);
RGB8 inline U32RGB8(u32 c) {
    RGB8 difuse = { .r = (c & 0x00FF0000) >> 16, .g = (c & 0x0000FF00) >> 8, .b = c & 0x000000FF };
    return difuse;
}
u32 inline RGB8U32(RGB8 c1) {
    u32 c2 = c1.r << 16 | c1.g << 8 | c1.b;
    return c2;
}
u32 inline GetPixel(DisplayBuffers *disp, int x, int y) {
    return(disp->buffer1[(y*disp->width)+x]);
}

inline u8 SatMulF(u8 x, float y) {
	u16 res = (u16) x * (float) y;
	u8 hi = res >> 8;
	u8 lo = res;
	
	return lo | -!!hi;
}
inline u8 SatAdd(u8 a1, u8 a2) { u8 m1 = (a1 + a2); m1 |= -(m1 < a1); return m1; }
inline u8 SatSub(u8 s1, u8 s2) { u8 m1 = (s1 - s2); m1 &= -(m1 <= s1); return m1; }

inline u32 SatAdd32(u32 a1, u32 a2) {
    RGB8 c1 = U32RGB8(a1);
    RGB8 c2 = U32RGB8(a2);
    RGB8 c3 = {
        .r = SatAdd(c1.r, c2.r),
        .g = SatAdd(c1.g, c2.g),
        .b = SatAdd(c1.b, c2.b)
    };
    return RGB8U32(c3);
}

inline u32 SatSub32(u32 a1, u32 a2) {
    RGB8 c1 = U32RGB8(a1);
    RGB8 c2 = U32RGB8(a2);
    RGB8 c3 = {
        .r = SatSub(c1.r, c2.r),
        .g = SatSub(c1.g, c2.g),
        .b = SatSub(c1.b, c2.b)
    };
    return RGB8U32(c3);
}



void ClearDisp(DisplayBuffers *disp);
void RenderDisp(DisplayBuffers *disp);
void Decay(DisplayBuffers *disp, u_int32_t amount);
u_int32_t* ReadPNG(char* file_name, u_int32_t trans, int *width, int *height);


#ifdef FBDRIVER
u_int32_t* fb_init(const char *file_name);
#endif

#endif
