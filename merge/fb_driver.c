#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/fb.h>

#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <png.h>
#include <stdbool.h>

#ifdef __cplusplus
    extern "C" {
        #include "system.h"
        #include "fb_driver.h"
    }
#else
    #include "fb_driver.h"
    #include "system.h"
#endif

#define FB_WIDTH 320
#define FB_HEIGHT 200

#define min(X, Y) (((X) < (Y)) ? (X) : (Y))
#define max(X, Y) (((X) > (Y)) ? (X) : (Y))

// #define RGB(r, g, b) (r << 16) + (g << 8) + (b)
#define RGB(r, g, b) ((r & 0x1F) << 11) + ((g & 0x3F) << 6) + (b & 0x1F)



// frame bufffer info
//u_int16_t *FBM;
//struct ScreenInfo FB_INFO;
struct FBPngImage FB_FONT_IMAGE;
FontChar fontData[128];

void init_fonts() {
    fontData[10] = (FontChar) {.x = 0, .y = 0, .width = 0, .height = 0, .xoffset = 0, .yoffset = 0};
    fontData[32] = (FontChar) {.x = 0, .y = 0, .width = 0, .height = 0, .xoffset = -1, .yoffset = 23};
    fontData[33] = (FontChar) {.x = 370, .y = 0, .width = 6, .height = 24, .xoffset = 6, .yoffset = 1};
    fontData[34] = (FontChar) {.x = 339, .y = 56, .width = 11, .height = 9, .xoffset = 3, .yoffset = 1};
    fontData[35] = (FontChar) {.x = 0, .y = 56, .width = 18, .height = 22, .xoffset = 0, .yoffset = 3};
    fontData[36] = (FontChar) {.x = 110, .y = 0, .width = 16, .height = 30, .xoffset = 1, .yoffset = -1};
    fontData[37] = (FontChar) {.x = 398, .y = 0, .width = 21, .height = 24, .xoffset = -1, .yoffset = 1};
    fontData[38] = (FontChar) {.x = 419, .y = 0, .width = 19, .height = 24, .xoffset = 0, .yoffset = 1};
    fontData[39] = (FontChar) {.x = 505, .y = 34, .width = 5, .height = 9, .xoffset = 6, .yoffset = 1};
    fontData[40] = (FontChar) {.x = 5, .y = 0, .width = 11, .height = 33, .xoffset = 4, .yoffset = -1};
    fontData[41] = (FontChar) {.x = 16, .y = 0, .width = 12, .height = 33, .xoffset = 2, .yoffset = -1};
    fontData[42] = (FontChar) {.x = 281, .y = 56, .width = 16, .height = 15, .xoffset = 1, .yoffset = 1};
    fontData[43] = (FontChar) {.x = 264, .y = 56, .width = 17, .height = 17, .xoffset = 0, .yoffset = 7};
    fontData[44] = (FontChar) {.x = 297, .y = 56, .width = 10, .height = 13, .xoffset = 3, .yoffset = 17};
    fontData[45] = (FontChar) {.x = 388, .y = 56, .width = 11, .height = 5, .xoffset = 3, .yoffset = 13};
    fontData[46] = (FontChar) {.x = 381, .y = 56, .width = 7, .height = 7, .xoffset = 5, .yoffset = 18};
    fontData[47] = (FontChar) {.x = 145, .y = 0, .width = 17, .height = 28, .xoffset = 0, .yoffset = 1};
    fontData[48] = (FontChar) {.x = 488, .y = 34, .width = 17, .height = 22, .xoffset = 0, .yoffset = 3};
    fontData[49] = (FontChar) {.x = 345, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[50] = (FontChar) {.x = 361, .y = 34, .width = 15, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[51] = (FontChar) {.x = 376, .y = 34, .width = 15, .height = 22, .xoffset = 2, .yoffset = 3};
    fontData[52] = (FontChar) {.x = 391, .y = 34, .width = 18, .height = 22, .xoffset = 0, .yoffset = 3};
    fontData[53] = (FontChar) {.x = 409, .y = 34, .width = 15, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[54] = (FontChar) {.x = 424, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[55] = (FontChar) {.x = 440, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[56] = (FontChar) {.x = 456, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[57] = (FontChar) {.x = 472, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[58] = (FontChar) {.x = 257, .y = 56, .width = 7, .height = 18, .xoffset = 5, .yoffset = 7};
    fontData[59] = (FontChar) {.x = 388, .y = 0, .width = 10, .height = 24, .xoffset = 2, .yoffset = 7};
    fontData[60] = (FontChar) {.x = 18, .y = 56, .width = 15, .height = 21, .xoffset = 1, .yoffset = 5};
    fontData[61] = (FontChar) {.x = 324, .y = 56, .width = 15, .height = 10, .xoffset = 1, .yoffset = 10};
    fontData[62] = (FontChar) {.x = 33, .y = 56, .width = 15, .height = 21, .xoffset = 2, .yoffset = 5};
    fontData[63] = (FontChar) {.x = 376, .y = 0, .width = 12, .height = 24, .xoffset = 4, .yoffset = 1};
    fontData[64] = (FontChar) {.x = 90, .y = 0, .width = 20, .height = 30, .xoffset = 0, .yoffset = 1};
    fontData[65] = (FontChar) {.x = 438, .y = 0, .width = 20, .height = 22, .xoffset = -1, .yoffset = 3};
    fontData[66] = (FontChar) {.x = 458, .y = 0, .width = 15, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[67] = (FontChar) {.x = 473, .y = 0, .width = 16, .height = 22, .xoffset = 0, .yoffset = 3};
    fontData[68] = (FontChar) {.x = 489, .y = 0, .width = 17, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[69] = (FontChar) {.x = 0, .y = 34, .width = 14, .height = 22, .xoffset = 2, .yoffset = 3};
    fontData[70] = (FontChar) {.x = 14, .y = 34, .width = 14, .height = 22, .xoffset = 2, .yoffset = 3};
    fontData[71] = (FontChar) {.x = 28, .y = 34, .width = 17, .height = 22, .xoffset = 0, .yoffset = 3};
    fontData[72] = (FontChar) {.x = 45, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[73] = (FontChar) {.x = 61, .y = 34, .width = 14, .height = 22, .xoffset = 2, .yoffset = 3};
    fontData[74] = (FontChar) {.x = 75, .y = 34, .width = 13, .height = 22, .xoffset = 2, .yoffset = 3};
    fontData[75] = (FontChar) {.x = 88, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[76] = (FontChar) {.x = 104, .y = 34, .width = 14, .height = 22, .xoffset = 3, .yoffset = 3};
    fontData[77] = (FontChar) {.x = 118, .y = 34, .width = 17, .height = 22, .xoffset = 0, .yoffset = 3};
    fontData[78] = (FontChar) {.x = 135, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[79] = (FontChar) {.x = 151, .y = 34, .width = 18, .height = 22, .xoffset = 0, .yoffset = 3};
    fontData[80] = (FontChar) {.x = 169, .y = 34, .width = 15, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[81] = (FontChar) {.x = 126, .y = 0, .width = 19, .height = 28, .xoffset = 0, .yoffset = 3};
    fontData[82] = (FontChar) {.x = 184, .y = 34, .width = 16, .height = 22, .xoffset = 2, .yoffset = 3};
    fontData[83] = (FontChar) {.x = 200, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[84] = (FontChar) {.x = 216, .y = 34, .width = 17, .height = 22, .xoffset = 0, .yoffset = 3};
    fontData[85] = (FontChar) {.x = 233, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[86] = (FontChar) {.x = 249, .y = 34, .width = 20, .height = 22, .xoffset = -1, .yoffset = 3};
    fontData[87] = (FontChar) {.x = 269, .y = 34, .width = 19, .height = 22, .xoffset = -1, .yoffset = 3};
    fontData[88] = (FontChar) {.x = 288, .y = 34, .width = 20, .height = 22, .xoffset = -1, .yoffset = 3};
    fontData[89] = (FontChar) {.x = 308, .y = 34, .width = 21, .height = 22, .xoffset = -2, .yoffset = 3};
    fontData[90] = (FontChar) {.x = 329, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[91] = (FontChar) {.x = 28, .y = 0, .width = 10, .height = 32, .xoffset = 4, .yoffset = -1};
    fontData[92] = (FontChar) {.x = 162, .y = 0, .width = 16, .height = 28, .xoffset = 1, .yoffset = 1};
    fontData[93] = (FontChar) {.x = 38, .y = 0, .width = 10, .height = 32, .xoffset = 3, .yoffset = -1};
    fontData[94] = (FontChar) {.x = 307, .y = 56, .width = 17, .height = 12, .xoffset = 0, .yoffset = 3};
    fontData[95] = (FontChar) {.x = 399, .y = 56, .width = 20, .height = 5, .xoffset = -1, .yoffset = 26};
    fontData[96] = (FontChar) {.x = 350, .y = 56, .width = 13, .height = 9, .xoffset = -1, .yoffset = 1};
    fontData[97] = (FontChar) {.x = 48, .y = 56, .width = 15, .height = 18, .xoffset = 1, .yoffset = 7};
    fontData[98] = (FontChar) {.x = 193, .y = 0, .width = 16, .height = 24, .xoffset = 1, .yoffset = 1};
    fontData[99] = (FontChar) {.x = 63, .y = 56, .width = 14, .height = 18, .xoffset = 1, .yoffset = 7};
    fontData[100] = (FontChar) {.x = 209, .y = 0, .width = 15, .height = 24, .xoffset = 1, .yoffset = 1};
    fontData[101] = (FontChar) {.x = 77, .y = 56, .width = 16, .height = 18, .xoffset = 1, .yoffset = 7};
    fontData[102] = (FontChar) {.x = 224, .y = 0, .width = 18, .height = 24, .xoffset = -1, .yoffset = 1};
    fontData[103] = (FontChar) {.x = 242, .y = 0, .width = 17, .height = 24, .xoffset = 0, .yoffset = 7};
    fontData[104] = (FontChar) {.x = 259, .y = 0, .width = 15, .height = 24, .xoffset = 1, .yoffset = 1};
    fontData[105] = (FontChar) {.x = 274, .y = 0, .width = 15, .height = 24, .xoffset = 2, .yoffset = 1};
    fontData[106] = (FontChar) {.x = 48, .y = 0, .width = 14, .height = 30, .xoffset = 1, .yoffset = 1};
    fontData[107] = (FontChar) {.x = 289, .y = 0, .width = 16, .height = 24, .xoffset = 2, .yoffset = 1};
    fontData[108] = (FontChar) {.x = 305, .y = 0, .width = 15, .height = 24, .xoffset = 2, .yoffset = 1};
    fontData[109] = (FontChar) {.x = 93, .y = 56, .width = 17, .height = 18, .xoffset = 0, .yoffset = 7};
    fontData[110] = (FontChar) {.x = 110, .y = 56, .width = 15, .height = 18, .xoffset = 1, .yoffset = 7};
    fontData[111] = (FontChar) {.x = 125, .y = 56, .width = 17, .height = 18, .xoffset = 0, .yoffset = 7};
    fontData[112] = (FontChar) {.x = 320, .y = 0, .width = 16, .height = 24, .xoffset = 1, .yoffset = 7};
    fontData[113] = (FontChar) {.x = 178, .y = 0, .width = 15, .height = 25, .xoffset = 1, .yoffset = 6};
    fontData[114] = (FontChar) {.x = 142, .y = 56, .width = 15, .height = 18, .xoffset = 2, .yoffset = 7};
    fontData[115] = (FontChar) {.x = 157, .y = 56, .width = 14, .height = 18, .xoffset = 2, .yoffset = 7};
    fontData[116] = (FontChar) {.x = 336, .y = 0, .width = 16, .height = 24, .xoffset = 0, .yoffset = 1};
    fontData[117] = (FontChar) {.x = 171, .y = 56, .width = 15, .height = 18, .xoffset = 1, .yoffset = 7};
    fontData[118] = (FontChar) {.x = 186, .y = 56, .width = 18, .height = 18, .xoffset = 0, .yoffset = 7};
    fontData[119] = (FontChar) {.x = 204, .y = 56, .width = 20, .height = 18, .xoffset = -1, .yoffset = 7};
    fontData[120] = (FontChar) {.x = 224, .y = 56, .width = 18, .height = 18, .xoffset = 0, .yoffset = 7};
    fontData[121] = (FontChar) {.x = 352, .y = 0, .width = 18, .height = 24, .xoffset = 0, .yoffset = 7};
    fontData[122] = (FontChar) {.x = 242, .y = 56, .width = 15, .height = 18, .xoffset = 2, .yoffset = 7};
    fontData[123] = (FontChar) {.x = 62, .y = 0, .width = 14, .height = 30, .xoffset = 1, .yoffset = 1};
    fontData[124] = (FontChar) {.x = 0, .y = 0, .width = 5, .height = 34, .xoffset = 7, .yoffset = -3};
    fontData[125] = (FontChar) {.x = 76, .y = 0, .width = 14, .height = 30, .xoffset = 2, .yoffset = 1};
    fontData[126] = (FontChar) {363, 56, 18, 9, 0, 11};
}

// swaps two numbers 
void swap(int* a , int*b) { 
    int temp = *a; 
    *a = *b; 
    *b = temp; 
} 

// returns absolute value of number 
static inline float absolute(float x ) { 
    if (x < 0) return -x; 
    else return x; 
} 
  
//returns integer part of a floating point number 
static inline int iPartOfNumber(float x) { 
    return (int)x; 
} 
  
//rounds off a number 
static inline int roundNumber(float x) { 
    return iPartOfNumber(x + 0.5) ; 
} 
  
//returns fractional part of a number 
static inline float fPartOfNumber(float x) { 
    if (x>0) return x - iPartOfNumber(x); 
    else return x - (iPartOfNumber(x)+1); 
  
} 
  
//returns 1 - fractional part of number 
static inline float rfPartOfNumber(float x) { 
    return 1 - fPartOfNumber(x); 
} 
  

dc_t HsvToRgb(unsigned char h, unsigned char s, unsigned char v) {
    //RgbColor rgb;

	//unsigned char r, g, b;
    unsigned char region, remainder, p, q, t;

    if (s <= 0) {
        return 0;
    }

    region = h / 43;
    remainder = (h - (region * 43)) * 6; 

    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region)
    {
        case 0:
            return RGB(v, t, p);
			//return v << 16 | t << 8 | p;
            //rgb.r = hsv.v; rgb.g = t; rgb.b = p;
            break;
        case 1:
            return RGB(q, v, p);
			//return q << 16 | v << 8 | p;
            //rgb.r = q; rgb.g = hsv.v; rgb.b = p;
            break;
        case 2:
            return RGB(p, v, t);
			//return p << 16 | v << 8 | t;
            //rgb.r = p; rgb.g = hsv.v; rgb.b = t;
            break;
        case 3:
            return RGB(p, q, v);
			//return p << 16 | q << 8 | v;
            //rgb.r = p; rgb.g = q; rgb.b = hsv.v;
            break;
        case 4:
            return RGB(t, p, v);
			//return t << 16 | p << 8 | v;
            //rgb.r = t; rgb.g = p; rgb.b = hsv.v;
            break;
        default:
            return RGB(v, p, q);
			//return v << 16 | p << 8 | q;
            //rgb.r = hsv.v; rgb.g = p; rgb.b = q;
            break;
    }

    u8 r = 255;
    u8 g = 255;
    u8 b = 255;
    return RGB(r, g, b);
}




/**
 * multiply a color by amount (2 = 2x brighter, .5 = 1/2 bright)
 */
static inline u32 color_mul(u32 c, float amount) {
    if (amount > 1) {
        return 
            min((((0xFF << 16) & c) * amount), 0xFF) +
            min((((0xFF << 8) & c) * amount),  0xFF) +
            min((((0xFF) & c) * amount),       0xFF);
    } else {
        return 
            max((((0xFF << 16) & c) * amount), 0x00) +
            max((((0xFF << 8) & c) * amount), 0x00) +
            max((((0xFF) & c) * amount), 0x00);
    }
}

/**
 * add a color to the existing color, dont go over or under
 * on RG or B 
 */
static inline u32 color_add(u32 c, u32 amount) {
    return 
        min(max(((0xFF << 16) & c) + (amount << 16), 0xFF), 0) +
        min(max(((0xFF << 8) & c) + (amount << 8), 0xFF), 0) +
        min(max(((0xFF) & c) + amount, 0xFF), 0);
}





/**
 * todo: attach signal handler and call this function.
 * todo: remove global variables
 */
void fb_cleanup(DisplayBuffers *disp) {
    if (disp->raw != NULL) {
        munmap(disp->raw, disp->info->buffer_sz);
    }

    if (disp->buffer1 != NULL) {
        free(disp->buffer1);
    }

    if (disp->buffer2 != NULL) {
        free(disp->buffer2);
    }

    if (disp->buffert != NULL) {
        free(disp->buffert);
    }

    if (disp->info->fd) {
        close(disp->info->fd);
    }
}


/**
 * set every pixel in a buffer to black (0)
 * FB_WIDTH * FB_HEIGHT * sizeof(db_t)
 */
void clear_buffer(db_t *buffer) {
    memset(buffer, 0, FB_WIDTH * FB_HEIGHT * sizeof(db_t));
}


/**
 * initialize the frame buffer, display some info to the console
 * Panic if we can't
 */ 
DisplayBuffers* fb_init(const char *file_name, const char *font_file) {
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;


    DisplayBuffers *disp = (DisplayBuffers *) malloc(sizeof(DisplayBuffers));
    ScreenInfo *infz = (ScreenInfo *) malloc(sizeof(ScreenInfo));
    disp->info = infz;
    init_fonts();


    disp->info->fd = open(file_name, O_RDWR);
    // Open the file for reading and writing
    if (disp->info->fd == -1) {
        Panic(1, "Error: cannot open framebuffer device: %s", file_name);
    } 

    // Get fixed screen information
    if (ioctl(disp->info->fd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        Panic(2, "Error reading fixed information");
    }

    // Get variable screen information
    if (ioctl(disp->info->fd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        Panic(3, "Error reading variable information");
    }

    // display some stats
    printf("Frame Buffer Rez: %dx%d, Virtual Rez: %dx%d, %dbpp\n", vinfo.xres, vinfo.yres, vinfo.xres_virtual, vinfo.yres_virtual, vinfo.bits_per_pixel);
    printf("%d Xoff, %d Yoff, %d line len\n", vinfo.xoffset, vinfo.yoffset, finfo.line_length);
    printf("accel: [%d] smem_len: [%d]\n", finfo.accel, finfo.smem_len);

    disp->sz = finfo.smem_len;
    disp->info->bpp = vinfo.bits_per_pixel;
    disp->info->buffer_sz = finfo.smem_len;
    disp->info->width = vinfo.xres;
    disp->info->height = vinfo.yres;
    disp->width = vinfo.xres;
    disp->height = vinfo.yres;

    printf("just mapped: SZ:%d, W%d.IW%d.IBZ%d\n", disp->sz, disp->width, disp->info->width, disp->info->buffer_sz);
    // error on unexpected video format
    /*
    if (vinfo->bits_per_pixel != 16) {
        Panic("currently only support for 16BPP", 4);
    }
    */

    // Map the device to memory
    disp->raw = (db_t *)mmap(0, disp->info->buffer_sz, PROT_READ | PROT_WRITE, MAP_SHARED, disp->info->fd, 0);
    if ((int)disp->raw == -1) {
        Panic(5, "Error: failed to map framebuffer device to memory");
    }

    FB_FONT_IMAGE = read_png_file(font_file);
    //init_fonts();
    //ioctl(FB_INFO.fd, FBIOBLANK, VESA_POWERDOWN: VESA_NO_BLANKING);
    ioctl(disp->info->fd, FBIOBLANK, VESA_NO_BLANKING);
    bzero(disp->raw, disp->info->buffer_sz);

    disp->buffer1 = (db_t*) malloc(disp->sz);
    disp->buffer2 = (db_t*) malloc(disp->sz);
    disp->buffert = (db_t*) malloc(disp->sz);

 
    printf("cleared screen: SZ:%d, W%d.IW%d.VW%d\n", disp->sz, disp->width, disp->info->width, vinfo.xres);


    return disp;
}

/**
 * copy buffer1 to the display
 */
void fb_draw_buffer1(DisplayBuffers *disp) {
    printf("write sz bytes: %d", disp->sz);
    memcpy(disp->raw, disp->buffer1, disp->sz);
}


/**
 * draw a line, no multiply
 **/
void fb_line(DisplayBuffers *disp, int x0, int y0, int x1, int y1, u8 hue, u8 sat) {
    bool steep = false; 
    if (absolute(x0-x1)<absolute(y0-y1)) { 
        swap(&x0, &y0); 
        swap(&x1, &y1); 
        steep = true; 
    } 
    if (x0>x1) { 
        swap(&x0, &x1); 
        swap(&y0, &y1); 
    } 
    int dx = x1-x0; 
    int dy = y1-y0; 
    int dx2 = dx * 2;
    int derror2 = absolute(dy)*2; 
    int error2 = 0; 
    int y = y0; 
    int off = disp->info->width * y;
    //printf("fb_line draw line color :%d, %x\n", color, color); 
    //printf("STEEP DP %d,%d -- O:%d = %d,%x\n", x0, x1, off, color, color);
    for (int x=x0; x<=x1; x++) {
        if (steep) { 
            u16 off = (disp->info->width * x) + y;
            //printf("STEEP DP [%d,%d]O:%d = %d,%x\n", x, y, off, color, color);
            disp->buffer1[disp->info->width*y + x] = HsvToRgb(hue, sat, 250);//color;
        } else { 
            //printf("NS\n");
            u16 off = (disp->info->width * x) + y;
            //printf("DP [%d,%d]O:%d = %d,%x\n", x, y, off, color, color);
            disp->buffer1[disp->info->width*y + x] = HsvToRgb(hue, sat, 250);//color;
            //disp->buffer1[disp->info->width*x + y] = color;
            //disp->raw[off] = color;
        } 
        error2 += derror2; 
        if (error2 > dx) { 
            y += (y1>y0?1:-1);
            error2 -= dx2;
        }
    }
} 






/**
 * draw image
 */
void draw_image(uint src_width, uint src_height, uint dx, uint dy, db_t *src, DisplayBuffers *disp) {

    int dst_off, src_off = 0;
    dst_off = dy * disp->info->width + dx; 

    for (uint y=0; y < src_height; y++) {

        // skip 1 vertical line in src
        src_off = (src_width * y);

        //for (uint x=0; x<src_width; x+=2) {
        for (uint x=src_width - 2; x != 0; x--) {
            disp->buffer1[dst_off++] = src[src_off++];
        }

        // skip 1 vertical line in dst, 
        // need to check src_width in case it is not FB_WIDTH*2
        dst_off += disp->info->width - (src_width);
    }
}


/**
 * reads the png image pointed to by filename and returns
 * memory alocated to display the image.  This does multiple
 * mallocs, seems like the implementation could be optomized
 * Ripped directly from libpng site...
 */
FBPngImage read_png_file(const char *filename) {
  FILE *fp = fopen((char *)filename, "rb");

  png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if(!png) { Panic(6, "unable to read PNG"); }

  png_infop info = png_create_info_struct(png);
  if(!info) { Panic(7, "unable to create PNG info struct"); }

  if(setjmp(png_jmpbuf(png))) { Panic(8, "png jmp abort?"); }

  png_init_io(png, fp);

  png_read_info(png, info);

  FBPngImage image;

  image.width      = png_get_image_width(png, info);
  image.height     = png_get_image_height(png, info);
  image.color_type = png_get_color_type(png, info);
  image.bit_depth  = png_get_bit_depth(png, info);

  // Read any color_type into 8bit depth, RGBA format.
  // See http://www.libpng.org/pub/png/libpng-manual.txt

  if(image.bit_depth == 16)
    png_set_strip_16(png);

  if(image.color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb(png);

  // PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
  if(image.color_type == PNG_COLOR_TYPE_GRAY && image.bit_depth < 8)
    png_set_expand_gray_1_2_4_to_8(png);

  if(png_get_valid(png, info, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha(png);

  // These color_type don't have an alpha channel then fill it with 0xff.
  if(image.color_type == PNG_COLOR_TYPE_RGB ||
     image.color_type == PNG_COLOR_TYPE_GRAY ||
     image.color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

  if(image.color_type == PNG_COLOR_TYPE_GRAY ||
     image.color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png);

  png_read_update_info(png, info);

  if (image.row_pointers) { Panic(9, "row pointers init already!"); }

  image.row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * image.height);
  for(int y = 0; y < image.height; y++) {
    image.row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png,info));
  }

  png_read_image(png, image.row_pointers);

  printf("loaded image file [%s]\n", filename);
  fclose(fp);
  png_destroy_read_struct(&png, &info, NULL);

  return image;
}


/**
 * writes the string to the display using the font previously
 * loaded at FB_FONT_IMAGE
 */
void fb_write_string(DisplayBuffers *disp, char *msg, int buffx, int buffy, u8 fgcolor) {
    int l = strlen(msg);
    // loop over each char
    for (int i = 0; i < l; i++) {
        // map the char to the font offset
        FontChar fontc = fontData[(int)msg[i]];

        // loop over each row
        for(int y = 0; y < fontc.height; y++) {
            png_bytep row = FB_FONT_IMAGE.row_pointers[y+fontc.y];
            // loop over each column
            for(int x = 0; x <fontc.width; x++) {
                // map out the alpha level
                png_bytep px = &(row[(fontc.x + x) * 4]);
                // calculate the color as white
                //u_int16_t color = ((px[3] >> 3)<<11) + ((px[3] >> 2)<<5) + (px[3] >> 3);
                // plot the pixel
                int offset = ((y+buffy+fontc.yoffset)*FB_WIDTH)+x+buffx+fontc.xoffset;
                dc_t color = HsvToRgb(200, 250, px[3]);
                //disp->buffer1[disp->info->width*y + x] = HsvToRgb(hue, sat, 250);//color;
                if (color > 0) {
                    color = (fgcolor > 0) ? fgcolor : color;
                    disp->buffer1[offset] = color;
                }
            }
        }

        // skip to the next character
        buffx += 18;
    }
}

  
/**
 * draw an antialiased line using wu algorithm
 */
void fb_aa_line(DisplayBuffers *disp, int x0 , int y0 , int x1 , int y1, u_int32_t bcolor) {
    int steep = absolute(y1 - y0) > absolute(x1 - x0) ; 

    printf("steep: %d\n", steep);
    // swap the co-ordinates if slope > 1 or we 
    // draw backwards 
    if (steep) 
    { 
        swap(&x0 , &y0); 
        swap(&x1 , &y1); 
    } 
    if (x0 > x1) 
    { 
        swap(&x0 ,&x1); 
        swap(&y0 ,&y1); 
    } 
  
    //compute the slope 
    float dx = x1-x0; 
    float dy = y1-y0; 
    float gradient;
    if (dx == 0.0) 
        gradient = 1; 
    else 
        gradient = dy/dx; 
    //printf("gradient: [%f] [%d]\n", gradient, steep);
  
    int xpxl1 = (x0 <= 0) ? 1 : x0; 
    int xpxl2 = x1; 
    float intersectY = (y0 <= 0) ? 1 : y0; 

    printf("xpxl1 %d / xpxl2 %d W%d.H%d\n", xpxl1, xpxl2, disp->info->width, disp->info->height);
    // main loop 
    if (steep) 
    {
        int x; 
        // TODO optomize this loop!
        for (x = xpxl1 ; x <=xpxl2 && x < disp->info->height; x++) 
        { 
            // pixel coverage is determined by fractional 
            // part of y co-ordinate 
            int px = 255 * fPartOfNumber(intersectY);
            // TODO: HSV THIS
            u_int16_t color = ((px >> 3)<<11) + ((px >> 2)<<5) + (px >> 3);
            //PIXEL_OR(buffer, iPartOfNumber(intersectY), x, color); 
            disp->buffer1[iPartOfNumber(intersectY) + x] = color;
            printf("STEEP PIX X%d.Y%d C%d\n", iPartOfNumber(intersectY)*FB_WIDTH, x, color);

            px = 255 * rfPartOfNumber(intersectY);
            color = ((px >> 3)<<11) + ((px >> 2)<<5) + (px >> 3);
            disp->buffer1[(iPartOfNumber(intersectY)-1) + x] = color;
            //PIXEL_OR(buffer, iPartOfNumber(intersectY)-1, x, color); 
            printf("STEEP PIX X%d.Y%d C%d\n", (iPartOfNumber(intersectY)-1)*FB_WIDTH, x, color);

            intersectY += gradient; 
        } 
    } 
    else
    {
        printf("W%d / Y%f - H%d\n", disp->info->width, intersectY, disp->height);
        for (u16 x = xpxl1 ; x <=xpxl2 && x < disp->info->width && intersectY < disp->info->height; x++) 
        { 
            int px = 255 * fPartOfNumber(intersectY);
            u_int16_t color = ((px >> 3)<<11) + ((px >> 2)<<5) + (px >> 3);
            disp->buffer1[(iPartOfNumber(intersectY)*FB_WIDTH)+x] = 0xFFFF; //color;
            printf("PIX X%d.Y%d C%d\n", iPartOfNumber(intersectY)*FB_WIDTH, x, color);

            px = 255 * rfPartOfNumber(intersectY);
            color = ((px >> 3)<<11) + ((px >> 2)<<5) + (px >> 3);
            disp->buffer1[((iPartOfNumber(intersectY)-1)*FB_WIDTH)+x] = 0xFFFF;//color;
            printf("PIX X%d.Y%d C%d\n", (iPartOfNumber(intersectY)-1)*FB_WIDTH, x, color);

            intersectY += gradient; 
        } 
        printf("W.%d, IY.%f, H.%d\n", disp->info->width, intersectY, disp->info->height);
    }
} 
 

