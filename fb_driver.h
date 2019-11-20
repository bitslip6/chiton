#ifndef FB_DRIVER_H
#define FB_DRIVER_H 1

#include <png.h>
#include <sys/types.h>
#include "system.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

// the type of the display buffer, 32bpp for normal HDMI out, 16BPP for TFT displays
typedef unsigned short db_t;
typedef unsigned short dc_t;

// basic frame buffer info
typedef struct ScreenInfo {
    int width;
    int height;
    int bpp;
    int buffer_sz;
    int fd;
} ScreenInfo;


// a single point on screen
struct ScreenPoint {
    u16 x;
    u16 y;
};


typedef struct FBPngImage {
    u16 width;
    u16 height;
    png_byte color_type;
    png_byte bit_depth;
    png_bytep *row_pointers;
} FBPngImage;

typedef struct FontChar {
    int x;
    int y;
    int width;
    int height;
    int xoffset;
    int yoffset;
} FontChar;


typedef struct DemoConf {
    u8 iron;
    u8 depthon;
    u8 bbon;
    u8 fpson;
    u8 senseon;
    u8 flooron;


    uint sense;
    uint max_depth;
    uint min_depth;
    uint mid_depth;

    uint bb_min_x;
    uint bb_max_x;

    uint bb_min_y;
    uint bb_max_y;

    uint vixel_w;
    uint vixel_h;

    u8 running;
} DemoConf;




// generic display interface
typedef struct DisplayBuffers {
    db_t *raw;
    db_t *buffer1;
    db_t *buffer2;
    db_t *buffert;
    int sz;

    u16 width;
    u16 height;
    ScreenInfo *info;

#ifdef SDLDRIVER
    SDL_Texture *texture;
    SDL_Renderer *renderer;
    SDL_Window *window;
#endif

} DisplayBuffers;



void init_fonts();
DisplayBuffers* fb_init(const char *file_name, const char *font_file);
void clear_buffer(db_t *buffer);
void fb_cleanup(DisplayBuffers *disp);
void draw_image(uint src_width, uint src_height, uint dx, uint dy, db_t *src, DisplayBuffers *disp);
void fb_line(DisplayBuffers *disp, int x0, int y0, int x1, int y1, u8 hue, u8 sat);
void fb_write_string(DisplayBuffers *disp, char *msg, int buffx, int buffy, u8 fgcolor);
dc_t HsvToRgb(unsigned char h, unsigned char s, unsigned char v);

void fb_aa_line(DisplayBuffers *disp, int x0 , int y0 , int x1 , int y1, u_int32_t bcolor);

FBPngImage read_png_file(const char *filename);


#endif
