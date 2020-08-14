#ifndef DEMOS_H
#define DEMOS_H 1

//#define __2D_DEBUG 1
//#define __3D_DEBUG 1

#define FBDRIVER 1
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



#define UNUSED __attribute__((unused))
// #define NEWMEM(sz) memset(malloc(sz), 0, sz);
#define DRAW(x, y, c) disp->buffer1[(y*disp->width)+x]=c
#define ANDDRAW(x, y, c) disp->buffer1[(y*disp->width)+x]&=c
#define ORDRAW(x, y, c) disp->buffer1[(y*disp->width)+x]|=c

static inline int min(int a, int b) { return (a<b) ? b : a; }
static inline int max(int a, int b) { return (a>b) ? a : b; }



// generic display interface
typedef struct DisplayBuffers {
    u_int32_t *raw;
    u_int32_t *buffer1;
    u_int32_t *buffer2;
    u_int32_t *buffert;
    int16_t *zbuffer;
    u32 *draw_ptr;
    u_int16_t sz;
    u16 width;
    u16 height;
    float invx;
    float invy;

#ifdef SDLDRIVER
    SDL_Texture *texture;
    SDL_Renderer *renderer;
    SDL_Window *window;
#endif

} DisplayBuffers;


// global system configuration
typedef struct GlobalOpts {
    u_int8_t clear_exit;
    bool running;
    bool enable;

    u_int16_t max_cam_value;
    u_int16_t min_cam_value;
    bool depth_frame;
    int color_mode;
    float color_rotation;
    int quality;
    char mode[32];
} GlobalOpts;
extern GlobalOpts *GOPTS;


typedef struct Point2D_t {
    s16 x;
    s16 y;
    s16 xoffset;
    s16 yoffset;
} Point2D_t;

/*
typedef struct Color_t {
    u8 r;
    u8 g;
    u8 b;
} Color_t;
*/

typedef void (*VoidCallback)();

extern DisplayBuffers *GDISP;
#endif
