#ifndef DEMOS_H
#define DEMOS_H 1

#define SDLDRIVER 1

#include <sys/types.h>
#ifdef SDLDRIVER
#include <SDL2/SDL.h>
#endif
#ifdef LEDDRIVER
#include "ws2811.h"
#endif

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#define NEWMEM(sz) memset(malloc(sz), 0, sz);




typedef struct AnimationOptions {
    u_int16_t seconds;
    u_int8_t fps;
    u_int8_t elements;

    u_int8_t brightness;
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
    u_int16_t sz;
    unsigned char width;
    unsigned char height;

#ifdef SDLDRIVER
    SDL_Texture *texture;
    SDL_Renderer *renderer;
    SDL_Window *window;
#endif

#ifdef LEDDRIVER
    ws2811_t ledstring = {
        .freq = TARGET_FREQ,
        .dmanum = DMA,
        .channel =
        {
            [0] =
            {
                .gpionum = GPIO_PIN,
                .count = 0,
                .invert = 0,
                .brightness = 255,
                .strip_type = STRIP_TYPE,
            },
            [1] =
            {
                .gpionum = 0,
                .count = 0,
                .invert = 0,
                .brightness = 0,
            },
        },
    };
#endif

} DisplayBuffers;


// global system configuration
typedef struct GlobalOpts {
    u_int8_t clear_exit;
    u_int8_t running;
} GlobalOpts;
GlobalOpts *GOPTS;

// basic frame buffer info
typedef struct ScreenInfo {
    int width;
    int height;
    int bpp;
    int buffer_sz;
    int fd;
} ScreenInfo;


DisplayBuffers *GDISP;
#endif
