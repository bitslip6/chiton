
#ifndef __MAIN_H__
#define __MAIN_H__


#include <aditof/camera.h>
#include <sys/types.h>
#include <png.h>
#include "geometry.h"

#define FB_WIDTH 320
#define FB_HEIGHT 240

#define PIXEL(fbm, x, y, color) *((unsigned short int*)(fbm + (y*FB_WIDTH) + x)) = color
#define PIXEL_AND(fbm, x, y, color) *((unsigned short int*)(fbm + (y*FB_WIDTH) + x)) &= color
#define PIXEL_OR(fbm, x, y, color) *((unsigned short int*)(fbm + (y*FB_WIDTH) + x)) |= color
#define PIXEL2(fbm, y, x, color) *((unsigned short int*)(fbm + (y*FB_WIDTH) + x)) = color
#define PIXEL_OFF(x, y) ((y*FB_WIDTH) + x)

#define FLOAT_TO_INT(in,out)  __asm__ __volatile__ ("fistpl %0" : "=m" (out) : "t" (in) : "st") ;

const u_int16_t BLACK = 0;
const u_int16_t BLUE = 0b0000000000011111;
const u_int16_t GREEN = 0b0000011111100000;
const u_int16_t RED = 0b1111100000000000;
const u_int16_t TURQ = BLUE|GREEN;
const u_int16_t PURPLE = BLUE|RED;
const u_int16_t WHITE = BLUE|RED|GREEN;
const u_int16_t YELLOW = RED|GREEN;

// a single point on screen
struct ScreenPoint {
    int x;
    int y;
};

// basic frame buffer info
struct ScreenInfo {
    int width;
    int height;
    int bpp;
    int buffer_sz;
    int fd;
};

struct FBPngImage {
    int width;
    int height;
    png_byte color_type;
    png_byte bit_depth;
    png_bytep *row_pointers;
};

struct FontChar {
    int x;
    int y;
    int width;
    int height;
    int xoffset;
    int yoffset;
};

typedef struct DemoConf {
    bool iron;
    bool depthon;
    bool bbon;
    bool fpson;
    bool senseon;
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
} DemoConf;

void fb_cleanup();
u_int16_t* fb_init(const char* file_name, const char *font_file);
ScreenInfo fb_get_buffer_info();
void fb_line(int x0, int y0, int x1, int y1, u_int16_t color, u_int16_t *   fbm);
void fb_line4(Vec2i p0, Vec2i p1, u_int16_t color, u_int16_t *fbm);
void fb_triangle(Vec2i t0, Vec2i t1, Vec2i t2, u_int16_t color, u_int16_t *fbm);
void fb_fast_triangle(Vec2i t0, Vec2i t1, Vec2i t2, u_int16_t color,  u_int16_t *buffer);
void fb_quad_fill(Vec2i *pts, u_int16_t color, u_int16_t *fbm);
void fb_my_triangle(ScreenPoint v1, ScreenPoint v2, ScreenPoint v3, u_int16_t color,  u_int16_t *buffer);
//void fb_vixel(int x0, int y0, u_int8_t sz, u_int16_t color, u_int16_t *buffer);
void fb_vixel(int x0, int y0, u_int8_t szx, u_int8_t szy, u_int16_t color, u_int16_t *buffer);
void fb_vixel_add(int x0, int y0, u_int8_t szx, u_int8_t szy, u_int16_t color, u_int16_t *buffer);
void Panic(const char *msg);

void draw_image_half_slow(int src_width, int src_height, int dx, int dy, u_int16_t *src, u_int16_t *dst);
void draw_image_half_fast(int src_width, int src_height, int dx, int dy, u_int16_t *src, u_int16_t *dst);
void draw_image_half_fast_skip(int src_width, int src_height, int dx, int dy, u_int16_t *src, u_int16_t *dst);
FBPngImage read_png_file(const char *filename);

void fb_write_string(char *msg, int buffx, int buffy, u_int16_t bgcolor, u_int16_t *buffer);

void fb_aa_line(int x0 , int y0 , int x1 , int y1, u_int16_t *buffer);
void init_fonts();





// ADI WRAPPER
aditof::Camera *get_tof_camera();
void set_camera_mode(aditof::Camera *camera, char *mode);
void get_depth_frame(aditof::Camera *camera, uint16_t **data1);
#endif