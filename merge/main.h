
#ifndef __MAIN_H__
#define __MAIN_H__


#include <sys/types.h>
#include <png.h>

#define FB_WIDTH 37
#define FB_HEIGHT 20

#define PIXEL(fbm, x, y, color) *((unsigned short int*)(fbm + (y*FB_WIDTH) + x)) = color
#define PIXEL_AND(fbm, x, y, color) *((unsigned short int*)(fbm + (y*FB_WIDTH) + x)) &= color
#define PIXEL_OR(fbm, x, y, color) *((unsigned short int*)(fbm + (y*FB_WIDTH) + x)) |= color
#define PIXEL2(fbm, y, x, color) *((unsigned short int*)(fbm + (y*FB_WIDTH) + x)) = color
#define PIXEL_OFF(x, y) ((y*FB_WIDTH) + x)

#define max(a,b)  ({ __typeof__ (a) _a = (a);  __typeof__ (b) _b = (b);  _a > _b ? _a : _b; })
#define min(a,b)  ({ __typeof__ (a) _a = (a);  __typeof__ (b) _b = (b);  _a > _b ? _b : _a; })

#define FLOAT_TO_INT(in,out)  __asm__ __volatile__ ("fistpl %0" : "=m" (out) : "t" (in) : "st") ;

/*
const u_int32_t BLACK = 0;
const u_int32_t BLUE = 0x000000FF;
const u_int32_t GREEN = 0x00FF0000;
const u_int32_t RED = 0x00FF0000;
const u_int32_t TURQ = BLUE|GREEN;
const u_int32_t PURPLE = BLUE|RED;
const u_int32_t WHITE = BLUE|RED|GREEN;
const u_int32_t YELLOW = RED|GREEN;
*/

// a single point on screen
struct ScreenPoint {
    int x;
    int y;
};

// basic frame buffer info
typedef struct ScreenInfo {
    int width;
    int height;
    int bpp;
    int buffer_sz;
    int fd;
} ScreenInfo;
/*

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
*/


void fb_cleanup();
u_int32_t* fb_init(const char *filename);
struct ScreenInfo fb_get_buffer_info();
void Panic(const char *msg, int code);
void put_pixel(int x, int y, u_int32_t color, u_int32_t *buf); 

void draw_image(int src_width, int src_height, int dx, int dy, u_int16_t *src, u_int16_t *dst);
struct FBPngImage read_png_file(const char *filename);
void fb_write_string(char *msg, int buffx, int buffy, u_int16_t bgcolor, u_int16_t *buffer);
void fb_aa_line(int x0 , int y0 , int x1 , int y1, u_int32_t bcolor, u_int16_t *buffer);
void init_fonts();

#endif
