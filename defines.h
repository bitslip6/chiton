#ifndef __DEFINE_H__
#define __DEFINE_H__

#include <sys/types.h>
#include <png.h>

#define FB_WIDTH 320
#define FB_HEIGHT 240

#define TOF_WIDTH 640
#define TOF_HEIGHT 480
#define TOF_HALF_WIDTH 320
#define TOF_HALF_HEIGHT 240

#define TOF_FULL_HEIGHT 960
#define TOF_MIN_DIST 390
#define TOF_MAX_DIST 16000
#define VIXEL_W 160
#define VIXEL_H 120
#define VIXEL_SZ 4
#define VIXEL_HALF 2
#define VIXEL_HALF2 1

#define MAX_UI_ELM 4

#define BTN_SEL_UP 1
#define BTN_SEL_DOWN 2

#define BTN_VAL_UP 3
#define BTN_VAL_DOWN 4

#define MIN_X 180
#define MAX_X 460
#define MIN_Y 160
#define MAX_Y 300





#define PIXEL(fbm, x, y, color) *((unsigned short int*)(fbm + (y*FB_WIDTH) + x)) = color
#define PIXEL_AND(fbm, x, y, color) *((unsigned short int*)(fbm + (y*FB_WIDTH) + x)) &= color
#define PIXEL_OR(fbm, x, y, color) *((unsigned short int*)(fbm + (y*FB_WIDTH) + x)) |= color
#define PIXEL2(fbm, y, x, color) *((unsigned short int*)(fbm + (y*FB_WIDTH) + x)) = color
#define PIXEL_OFF(x, y) ((y*FB_WIDTH) + x)

#define FLOAT_TO_INT(in,out)  __asm__ __volatile__ ("fistpl %0" : "=m" (out) : "t" (in) : "st") ;

const u_int16_t BLACK = 0;
//const u_int16_t BLUE = 0b0000000000011111;
const u_int16_t BLUE = 0x1F;
const u_int16_t GREEN = 0x7E0;//0b0000011111100000;
const u_int16_t RED = 0xF800;//0b1111100000000000;
const u_int16_t TURQ = BLUE|GREEN;
const u_int16_t PURPLE = BLUE|RED;
const u_int16_t WHITE = BLUE|RED|GREEN;
const u_int16_t YELLOW = RED|GREEN;


struct input_event {
    struct timeval time;
    unsigned short type;
    unsigned short code;
    unsigned int value; 
};

// a structure  to hold information about a single line of data from the floor
// used to detect objects that are not the floor
struct FloorLine {
    uint exepected_height;
    uint max_error_value;
    uint num_errors;
    uint max_error_len;
    uint line_no;
    uint line_avg;
};


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
    bool flooron;


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


#endif