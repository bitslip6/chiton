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

#define DRAW(x, y, c) disp->buffer1[(y*disp->width)+x]=c
#define ANDDRAW(x, y, c) disp->buffer1[(y*disp->width)+x]&=c
#define ORDRAW(x, y, c) disp->buffer1[(y*disp->width)+x]|=c



#ifndef db_t
typedef unsigned short int db_t;
#endif

#ifndef u16
typedef unsigned short int u16;
#endif
#ifndef s16
typedef short int s16;
#endif
#ifndef u8
typedef unsigned char u8;
#endif



// generic display interface
typedef struct DisplayBuffers {
    db_t *raw;
    db_t *buffer1;
    db_t *buffer2;
    db_t *buffert;
    int16_t *zbuffer;
    db_t *draw_ptr;
    u_int16_t sz;
    uint16_t width;
    uint16_t height;
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
    int16_t x;
    int16_t y;
    int16_t xoffset;
    int16_t yoffset;
} Point2D_t;

// simple signed 16bit x and y point
typedef struct xy_t {
    int16_t x;
    int16_t y;
} xy_t;


// simple float x and y point
typedef struct xyf_t {
    float x;
    float y;
} xyf_t;



// averaged 4 point sample
typedef struct xy_samp_t {
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
    float xf;
    float yf;
} xy_samp_t;

// averaged 4 point sample
typedef struct xyf_samp_t {
    float x1;
    float y1;
    float x2;
    float y2;
    float xf;
    float yf;
} xyf_samp_t;



// simple signed 16bit x and y point
typedef struct line_t {
    xy_t p1;
    xy_t p2;
} line_t;

// simple float x and y point
typedef struct linef_t {
    xyf_t p1;
    xyf_t p2;
} linef_t;



typedef void (*VoidCallback)();
typedef void (*AdiCallback)();
typedef void (*PixelFunc)(uint16_t*, RGB8*);
typedef uint16_t (*CalibrateFunc)(uint16_t*);

extern DisplayBuffers *GDISP;
#endif
