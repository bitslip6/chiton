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

#define CAM_WIDTH  640
#define CAM_HEIGHT 480



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


typedef struct resolution_t {
    uint16_t width;
    uint16_t height;
} resolution_t;


// averaged 4 point sample
typedef struct xy_samp_t {
    xy_t p1;
    xy_t p2;
    float xf;
    float yf;
    float angle; // angle in radians
    s16 offset;
} xy_samp_t;

// averaged 4 point sample
typedef struct offset_samp_t {
    uint src_off;
    uint dst_off;
    float xf;
    float yf;
    float angle; // angle in radians
    float gain; // pixel gain
    s16 gain_offset;
} offset_samp_t;


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




typedef void (*VoidCallback)();
typedef void (*AdiCallback)();
typedef void (*PixelFunc)(uint16_t*, RGB8*);
typedef void (*PixelFunc2)(uint16_t, unsigned int);
typedef uint16_t (*CalibrateFunc)(uint16_t*);

extern DisplayBuffers *GDISP;
#endif
