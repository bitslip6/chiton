
#include "chitontypes.h"

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
#include <vector>
#include <functional>
#include "color/hsv.h" 
#include "util.h"
#include "json.hpp"

using json = nlohmann::json;


#ifdef SDLDRIVER
#include <SDL2/SDL.h>
#endif



#define FB_PIXEL_SZ 5
#define UNUSED __attribute__((unused))
// #define NEWMEM(sz) memset(malloc(sz), 0, sz);
#define DRAW(x, y, c) disp->buffer1[(y*disp->width)+x]=c
#define ANDDRAW(x, y, c) disp->buffer1[(y*disp->width)+x]&=c
#define ORDRAW(x, y, c) disp->buffer1[(y*disp->width)+x]|=c

static inline int min(int a, int b) { return (a<=b) ? a : b; }
static inline int max(int a, int b) { return (a>b) ? a : b; }



// generic display interface
typedef struct DisplayBuffers {
    db_t *raw;
    db_t *buffer1;
    db_t *buffer2;
    db_t *buffert;
    int16_t *zbuffer;
    db_t *draw_ptr;
    u_int16_t sz;
    u16 width;
    u16 height;
    float invx;
    float invy;

#ifdef SDLDRIVER
    SDL_Texture *texture;
    SDL_Renderer *renderer;
    SDL_Window *window;
#elif FBDRIVER
    int fd;
#endif

} DisplayBuffers;


typedef struct Point2D_t {
    s16 x;
    s16 y;
    s16 xoffset;
    s16 yoffset;
} Point2D_t;

// simple signed 16bit x and y point
typedef struct xy_t {
    int16_t x;
    int16_t y;
} xy_t;


// simple signed 16bit x and y point
typedef struct line_t {
    xy_t p1;
    xy_t p2;
} line_t;


typedef void (*VoidCallback)();
typedef void (*AdiCallback)();
typedef void (*PixelFunc)(uint16_t*, RGB8*);
typedef uint16_t (*CalibrateFunc)(uint16_t*);

extern DisplayBuffers *GDISP;
#endif
