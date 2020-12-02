#ifndef DEMOS_H
#define DEMOS_H 1

//#define __2D_DEBUG 1
//#define __3D_DEBUG 1

#define SDLDRIVER 1
//#define NDEBUG 1
//#define LEDDRIVER 1

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include "color/hsv.h" 
#include "util.h"

#ifdef SDLDRIVER
#include <SDL2/SDL.h>
#endif
#ifdef LEDDRIVER
#include "ws2811.h"
#endif



#define UNUSED __attribute__((unused))
#define MAX_SIGNALS 4
#define MAX_RUNLEN 64
#define DRAW(x, y, c) disp->buffer1[(y*disp->width)+x]=c
#define ANDDRAW(x, y, c) disp->buffer1[(y*disp->width)+x]&=c
#define ORDRAW(x, y, c) disp->buffer1[(y*disp->width)+x]|=c

static inline int min(int a, int b) { return (a>b) ? b : a; }
static inline int max(int a, int b) { return (a>b) ? a : b; }

typedef struct AnimationOptions {
    u_int16_t seconds;
    u_int8_t fps;
    u_int8_t elements;

    u_int8_t brightness;
    float speed1;
    float speed2;

    u_int8_t hue;
    float hue_mul;
    u_int8_t sat;
    float sat_mul;

    u8 debug;
} AnimationOptions;

/*
typedef struct AnimationFilter {
    AnimationFilterOut (*filter_ptr)(AnimationFilterOut *, DisplayBuffers *);
} AnimationFilter;
*/

typedef struct AnimationFilterOut {
    float xoff;
    float yoff;
    float mul;
    int frames;
} AnimationFilterOut;



// generic display interface
typedef struct DisplayBuffers {
    u_int32_t *raw;
    u_int32_t *buffer1;
    u_int32_t *buffer2;
    u_int32_t *buffert;
    int16_t *zbuffer;
    u32 *draw_ptr;
    u_int16_t sz;
    u8 width;
    u8 height;
    float invx;
    float invy;

    u8 widths[MAX_SIGNALS];
    u8 heights[MAX_SIGNALS];

#ifdef SDLDRIVER
    SDL_Texture *texture;
    SDL_Renderer *renderer;
    SDL_Window *window;
#endif

#ifdef LEDDRIVER
    /*
    int height_map1[LWIDTH] = {16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29};
    float height_offset1[LWIDTH] = {7.0, 6.5, 6.0, 5.5, 5.0, 4.5, 4.0, 3.5, 3.0, 2.5, 2.0, 1.5, 1.0, 0.5};
    int height_map2[RWIDTH] = {30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18};
    float height_offset2[RWIDTH] = {0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 6.0};//, 6.5, 7.0};
    */

    int runlen[MAX_SIGNALS][MAX_RUNLEN];
    float offset[MAX_SIGNALS][MAX_RUNLEN];

    ws2811_t ledstring;
    /*
     = {
        .freq = TARGET_FREQ,
        .dmanum = DMA,
        .channel = {
            [0] = {
                .gpionum = GPIO_PIN,
                .count = 0,
                .invert = 0,
                .brightness = 96,
                .strip_type = STRIP_TYPE,
            },
            [1] = {
                .gpionum = GPIO_PIN2,
                .count = 0,
                .invert = 0,
                .brightness = 96,
                .strip_type = STRIP_TYPE,
            },
        },
    };
    */
#endif

} DisplayBuffers;


// global system configuration
typedef struct GlobalOpts {
    u_int8_t clear_exit;
    bool running;
} GlobalOpts;
//GlobalOpts *GOPTS;

// basic frame buffer info
/*
typedef struct ScreenInfo {
    int width;
    int height;
    int bpp;
    int buffer_sz;
    int fd;
} ScreenInfo;
*/


typedef struct Point2D_t {
    s16 x;
    s16 y;
    s16 xoffset;
    s16 yoffset;
} Point2D_t;

typedef struct HSV_t {
    u8 h;
    u8 s;
    u8 v;
} HSV_t;



typedef struct Color_t {
    u8 r;
    u8 g;
    u8 b;
} Color_t;


typedef struct Costume_t {
    u8 width;
    u8 height;
    u8 end;
    u8 toff[64];
    u8 boff[64];
    u16 leds;
    u8 center_x;
    u8 center_y;
    u16 offset;
    RGB8 *image;
} Costume_t;


typedef void (*VoidCallback)();



DisplayBuffers *GDISP;
#endif
