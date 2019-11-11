#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/fb.h>

#include <vector>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <strings.h>
#include <png.h>
#include "main.h"
#include "geometry.h"
#include "font.h"

// frame bufffer info
u_int16_t *FBM;
ScreenInfo FB_INFO;
FBPngImage FB_FONT_IMAGE;
struct FontChar fontData[128];

void init_fonts()
{
    fontData[10] = {.x = 0, .y = 0, .width = 0, .height = 0, .xoffset = 0, .yoffset = 0};
    fontData[32] = {.x = 0, .y = 0, .width = 0, .height = 0, .xoffset = -1, .yoffset = 23};
    fontData[33] = {.x = 370, .y = 0, .width = 6, .height = 24, .xoffset = 6, .yoffset = 1};
    fontData[34] = {.x = 339, .y = 56, .width = 11, .height = 9, .xoffset = 3, .yoffset = 1};
    fontData[35] = {.x = 0, .y = 56, .width = 18, .height = 22, .xoffset = 0, .yoffset = 3};
    fontData[36] = {.x = 110, .y = 0, .width = 16, .height = 30, .xoffset = 1, .yoffset = -1};
    fontData[37] = {.x = 398, .y = 0, .width = 21, .height = 24, .xoffset = -1, .yoffset = 1};
    fontData[38] = {.x = 419, .y = 0, .width = 19, .height = 24, .xoffset = 0, .yoffset = 1};
    fontData[39] = {.x = 505, .y = 34, .width = 5, .height = 9, .xoffset = 6, .yoffset = 1};
    fontData[40] = {.x = 5, .y = 0, .width = 11, .height = 33, .xoffset = 4, .yoffset = -1};
    fontData[41] = {.x = 16, .y = 0, .width = 12, .height = 33, .xoffset = 2, .yoffset = -1};
    fontData[42] = {.x = 281, .y = 56, .width = 16, .height = 15, .xoffset = 1, .yoffset = 1};
    fontData[43] = {.x = 264, .y = 56, .width = 17, .height = 17, .xoffset = 0, .yoffset = 7};
    fontData[44] = {.x = 297, .y = 56, .width = 10, .height = 13, .xoffset = 3, .yoffset = 17};
    fontData[45] = {.x = 388, .y = 56, .width = 11, .height = 5, .xoffset = 3, .yoffset = 13};
    fontData[46] = {.x = 381, .y = 56, .width = 7, .height = 7, .xoffset = 5, .yoffset = 18};
    fontData[47] = {.x = 145, .y = 0, .width = 17, .height = 28, .xoffset = 0, .yoffset = 1};
    fontData[48] = {.x = 488, .y = 34, .width = 17, .height = 22, .xoffset = 0, .yoffset = 3};
    fontData[49] = {.x = 345, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[50] = {.x = 361, .y = 34, .width = 15, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[51] = {.x = 376, .y = 34, .width = 15, .height = 22, .xoffset = 2, .yoffset = 3};
    fontData[52] = {.x = 391, .y = 34, .width = 18, .height = 22, .xoffset = 0, .yoffset = 3};
    fontData[53] = {.x = 409, .y = 34, .width = 15, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[54] = {.x = 424, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[55] = {.x = 440, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[56] = {.x = 456, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[57] = {.x = 472, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[58] = {.x = 257, .y = 56, .width = 7, .height = 18, .xoffset = 5, .yoffset = 7};
    fontData[59] = {.x = 388, .y = 0, .width = 10, .height = 24, .xoffset = 2, .yoffset = 7};
    fontData[60] = {.x = 18, .y = 56, .width = 15, .height = 21, .xoffset = 1, .yoffset = 5};
    fontData[61] = {.x = 324, .y = 56, .width = 15, .height = 10, .xoffset = 1, .yoffset = 10};
    fontData[62] = {.x = 33, .y = 56, .width = 15, .height = 21, .xoffset = 2, .yoffset = 5};
    fontData[63] = {.x = 376, .y = 0, .width = 12, .height = 24, .xoffset = 4, .yoffset = 1};
    fontData[64] = {.x = 90, .y = 0, .width = 20, .height = 30, .xoffset = 0, .yoffset = 1};
    fontData[65] = {.x = 438, .y = 0, .width = 20, .height = 22, .xoffset = -1, .yoffset = 3};
    fontData[66] = {.x = 458, .y = 0, .width = 15, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[67] = {.x = 473, .y = 0, .width = 16, .height = 22, .xoffset = 0, .yoffset = 3};
    fontData[68] = {.x = 489, .y = 0, .width = 17, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[69] = {.x = 0, .y = 34, .width = 14, .height = 22, .xoffset = 2, .yoffset = 3};
    fontData[70] = {.x = 14, .y = 34, .width = 14, .height = 22, .xoffset = 2, .yoffset = 3};
    fontData[71] = {.x = 28, .y = 34, .width = 17, .height = 22, .xoffset = 0, .yoffset = 3};
    fontData[72] = {.x = 45, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[73] = {.x = 61, .y = 34, .width = 14, .height = 22, .xoffset = 2, .yoffset = 3};
    fontData[74] = {.x = 75, .y = 34, .width = 13, .height = 22, .xoffset = 2, .yoffset = 3};
    fontData[75] = {.x = 88, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[76] = {.x = 104, .y = 34, .width = 14, .height = 22, .xoffset = 3, .yoffset = 3};
    fontData[77] = {.x = 118, .y = 34, .width = 17, .height = 22, .xoffset = 0, .yoffset = 3};
    fontData[78] = {.x = 135, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[79] = {.x = 151, .y = 34, .width = 18, .height = 22, .xoffset = 0, .yoffset = 3};
    fontData[80] = {.x = 169, .y = 34, .width = 15, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[81] = {.x = 126, .y = 0, .width = 19, .height = 28, .xoffset = 0, .yoffset = 3};
    fontData[82] = {.x = 184, .y = 34, .width = 16, .height = 22, .xoffset = 2, .yoffset = 3};
    fontData[83] = {.x = 200, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[84] = {.x = 216, .y = 34, .width = 17, .height = 22, .xoffset = 0, .yoffset = 3};
    fontData[85] = {.x = 233, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[86] = {.x = 249, .y = 34, .width = 20, .height = 22, .xoffset = -1, .yoffset = 3};
    fontData[87] = {.x = 269, .y = 34, .width = 19, .height = 22, .xoffset = -1, .yoffset = 3};
    fontData[88] = {.x = 288, .y = 34, .width = 20, .height = 22, .xoffset = -1, .yoffset = 3};
    fontData[89] = {.x = 308, .y = 34, .width = 21, .height = 22, .xoffset = -2, .yoffset = 3};
    fontData[90] = {.x = 329, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    fontData[91] = {.x = 28, .y = 0, .width = 10, .height = 32, .xoffset = 4, .yoffset = -1};
    fontData[92] = {.x = 162, .y = 0, .width = 16, .height = 28, .xoffset = 1, .yoffset = 1};
    fontData[93] = {.x = 38, .y = 0, .width = 10, .height = 32, .xoffset = 3, .yoffset = -1};
    fontData[94] = {.x = 307, .y = 56, .width = 17, .height = 12, .xoffset = 0, .yoffset = 3};
    fontData[95] = {.x = 399, .y = 56, .width = 20, .height = 5, .xoffset = -1, .yoffset = 26};
    fontData[96] = {.x = 350, .y = 56, .width = 13, .height = 9, .xoffset = -1, .yoffset = 1};
    fontData[97] = {.x = 48, .y = 56, .width = 15, .height = 18, .xoffset = 1, .yoffset = 7};
    fontData[98] = {.x = 193, .y = 0, .width = 16, .height = 24, .xoffset = 1, .yoffset = 1};
    fontData[99] = {.x = 63, .y = 56, .width = 14, .height = 18, .xoffset = 1, .yoffset = 7};
    fontData[100] = {.x = 209, .y = 0, .width = 15, .height = 24, .xoffset = 1, .yoffset = 1};
    fontData[101] = {.x = 77, .y = 56, .width = 16, .height = 18, .xoffset = 1, .yoffset = 7};
    fontData[102] = {.x = 224, .y = 0, .width = 18, .height = 24, .xoffset = -1, .yoffset = 1};
    fontData[103] = {.x = 242, .y = 0, .width = 17, .height = 24, .xoffset = 0, .yoffset = 7};
    fontData[104] = {.x = 259, .y = 0, .width = 15, .height = 24, .xoffset = 1, .yoffset = 1};
    fontData[105] = {.x = 274, .y = 0, .width = 15, .height = 24, .xoffset = 2, .yoffset = 1};
    fontData[106] = {.x = 48, .y = 0, .width = 14, .height = 30, .xoffset = 1, .yoffset = 1};
    fontData[107] = {.x = 289, .y = 0, .width = 16, .height = 24, .xoffset = 2, .yoffset = 1};
    fontData[108] = {.x = 305, .y = 0, .width = 15, .height = 24, .xoffset = 2, .yoffset = 1};
    fontData[109] = {.x = 93, .y = 56, .width = 17, .height = 18, .xoffset = 0, .yoffset = 7};
    fontData[110] = {.x = 110, .y = 56, .width = 15, .height = 18, .xoffset = 1, .yoffset = 7};
    fontData[111] = {.x = 125, .y = 56, .width = 17, .height = 18, .xoffset = 0, .yoffset = 7};
    fontData[112] = {.x = 320, .y = 0, .width = 16, .height = 24, .xoffset = 1, .yoffset = 7};
    fontData[113] = {.x = 178, .y = 0, .width = 15, .height = 25, .xoffset = 1, .yoffset = 6};
    fontData[114] = {.x = 142, .y = 56, .width = 15, .height = 18, .xoffset = 2, .yoffset = 7};
    fontData[115] = {.x = 157, .y = 56, .width = 14, .height = 18, .xoffset = 2, .yoffset = 7};
    fontData[116] = {.x = 336, .y = 0, .width = 16, .height = 24, .xoffset = 0, .yoffset = 1};
    fontData[117] = {.x = 171, .y = 56, .width = 15, .height = 18, .xoffset = 1, .yoffset = 7};
    fontData[118] = {.x = 186, .y = 56, .width = 18, .height = 18, .xoffset = 0, .yoffset = 7};
    fontData[119] = {.x = 204, .y = 56, .width = 20, .height = 18, .xoffset = -1, .yoffset = 7};
    fontData[120] = {.x = 224, .y = 56, .width = 18, .height = 18, .xoffset = 0, .yoffset = 7};
    fontData[121] = {.x = 352, .y = 0, .width = 18, .height = 24, .xoffset = 0, .yoffset = 7};
    fontData[122] = {.x = 242, .y = 56, .width = 15, .height = 18, .xoffset = 2, .yoffset = 7};
    fontData[123] = {.x = 62, .y = 0, .width = 14, .height = 30, .xoffset = 1, .yoffset = 1};
    fontData[124] = {.x = 0, .y = 0, .width = 5, .height = 34, .xoffset = 7, .yoffset = -3};
    fontData[125] = {.x = 76, .y = 0, .width = 14, .height = 30, .xoffset = 2, .yoffset = 1};
    fontData[126] = {363, 56, 18, 9, 0, 11};
}

/**
 * return info about the current frame buffer
 */
ScreenInfo fb_get_buffer_info()
{
    return FB_INFO;
}

/**
 * todo: attach signal handler and call this function.
 * todo: remove global variables
 */
void fb_cleanup()
{
    munmap(FBM, FB_INFO.buffer_sz);
    close(FB_INFO.fd);
}

/**
 * initialize the frame buffer, display some info to the console
 * Panic if we can't
 */
u_int16_t *fb_init(const char *file_name, const char *font_file)
{
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

    FB_INFO.fd = open(file_name, O_RDWR);
    // Open the file for reading and writing
    if (FB_INFO.fd == -1)
    {
        Panic("Error: cannot open framebuffer device");
    }

    // Get fixed screen information
    if (ioctl(FB_INFO.fd, FBIOGET_FSCREENINFO, &finfo) == -1)
    {
        Panic("Error reading fixed information");
    }

    // Get variable screen information
    if (ioctl(FB_INFO.fd, FBIOGET_VSCREENINFO, &vinfo) == -1)
    {
        Panic("Error reading variable information");
    }

    // display some stats
    printf("Frame Buffer Rez: %dx%d, Virtual Rez: %dx%d, %dbpp\n", vinfo.xres, vinfo.yres,
           vinfo.xres_virtual, vinfo.yres_virtual, vinfo.bits_per_pixel);
    printf("%d Xoff, %d Yoff, %d line len\n", vinfo.xoffset, vinfo.yoffset, finfo.line_length);
    printf("accel: [%d] smem_len: [%d]\n", finfo.accel, finfo.smem_len);
    FB_INFO.bpp = vinfo.bits_per_pixel;
    FB_INFO.buffer_sz = finfo.smem_len;
    FB_INFO.width = vinfo.xres;
    FB_INFO.height = vinfo.yres;

    // error on unexpected video format
    if (vinfo.bits_per_pixel != 16)
    {
        Panic("currently only support for 16BPP");
    }

    // Map the device to memory
    FBM = (u_int16_t *)mmap(0, FB_INFO.buffer_sz, PROT_READ | PROT_WRITE, MAP_SHARED, FB_INFO.fd, 0);
    if ((int)FBM == -1)
    {
        Panic("Error: failed to map framebuffer device to memory");
    }

    FB_FONT_IMAGE = read_png_file(font_file);
    printf("font image read\n");
    init_fonts();
    printf("fonts initialized\n");
    //ioctl(FB_INFO.fd, FBIOBLANK, VESA_POWERDOWN: VESA_NO_BLANKING);
    ioctl(FB_INFO.fd, FBIOBLANK, VESA_NO_BLANKING);
    bzero(FBM, FB_INFO.buffer_sz);
    printf("cleared screen\n");
    return FBM;
}

/**
 * display an iamge fullscreen (must be 320x240)
 */
void display_image(const char *logo) {
    //FBPngImage image = read_png_file(font_file);

    char *cmdargs[] = {"/home/pi/fbv/fbv", "logo.png", "-i", "-c", NULL};
    execvp("/home/pi/fbv/fbv", cmdargs);
}

/**
 * frame buffer draw line
 */
void fb_line(int x0, int y0, int x1, int y1, u_int16_t color, u_int16_t *buffer)
{
    bool steep = false;
    if (std::abs(x0 - x1) < std::abs(y0 - y1))
    {
        std::swap(x0, y0);
        std::swap(x1, y1);
        steep = true;
    }
    if (x0 > x1)
    {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }
    int dx = x1 - x0;
    int dy = y1 - y0;
    int derror2 = std::abs(dy) * 2;
    int error2 = 0;
    int y = y0;
    for (int x = x0; x <= x1; x++)
    {
        if (steep)
        {
            PIXEL(buffer, y, x, color);
        }
        else
        {
            PIXEL(buffer, x, y, color);
        }
        error2 += derror2;
        if (error2 > dx)
        {
            y += (y1 > y0 ? 1 : -1);
            error2 -= dx * 2;
        }
    }
}

void fb_line4(Vec2i p0, Vec2i p1, u_int16_t color, u_int16_t *buffer)
{
    bool steep = false;
    if (std::abs(p0.x - p1.x) < std::abs(p0.y - p1.y))
    {
        std::swap(p0.x, p0.y);
        std::swap(p1.x, p1.y);
        steep = true;
    }
    if (p0.x > p1.x)
    {
        std::swap(p0, p1);
    }

    for (int x = p0.x; x <= p1.x; x++)
    {
        float t = (x - p0.x) / (float)(p1.x - p0.x);
        int y = p0.y * (1. - t) + p1.y * t;
        if (steep)
        {
            PIXEL(buffer, y, x, color);
        }
        else
        {
            PIXEL(buffer, x, y, color);
        }
    }
}

/**
 * line draw a triangle polygon
 **/
void fb_triangle(Vec2i t0, Vec2i t1, Vec2i t2, u_int16_t color, u_int16_t *buffer)
{
    // sort the vertices, t0, t1, t2 lower−to−upper (bubblesort yay!)
    if (t0.y > t1.y)
        std::swap(t0, t1);
    if (t0.y > t2.y)
        std::swap(t0, t2);
    if (t1.y > t2.y)
        std::swap(t1, t2);
    fb_line4(t0, t1, color, buffer);
    fb_line4(t1, t2, color, buffer);
    fb_line4(t2, t0, RED, buffer);
}

/**
 * draw a filled quad polygon
 */
/*
void fb_quad_fill(Vec2i *pts, u_int16_t color, u_int16_t *buffer) { 
} 
*/

void fb_fast_triangle(Vec2i t0, Vec2i t1, Vec2i t2, u_int16_t color, u_int16_t *buffer)
{
    // sort the vertices, t0, t1, t2 lower−to−upper (bubblesort yay!)
    if (t0.y > t1.y)
        std::swap(t0, t1);
    if (t0.y > t2.y)
        std::swap(t0, t2);
    if (t1.y > t2.y)
        std::swap(t1, t2);
    int total_height = t2.y - t0.y;
    for (int y = t0.y; y <= t1.y; y++)
    {
        int segment_height = t1.y - t0.y + 1;
        float alpha = (float)(y - t0.y) / total_height;
        float beta = (float)(y - t0.y) / segment_height; // be careful with divisions by zero
        Vec2i A = t0 + (t2 - t0) * alpha;
        Vec2i B = t0 + (t1 - t0) * beta;
        if (A.x > B.x)
            std::swap(A, B);
        memset(buffer + (y * 320) + A.x, color, B.x - A.x);
        /*
        for (int j=A.x; j<=B.x; j++) { 
            PIXEL(buffer, j, y, color);
            //image.set(j, y, color); // attention, due to int casts t0.y+i != A.y 
        } 
        */
    }
    for (int y = t1.y; y <= t2.y; y++)
    {
        int segment_height = t2.y - t1.y + 1;
        float alpha = (float)(y - t0.y) / total_height;
        float beta = (float)(y - t1.y) / segment_height; // be careful with divisions by zero
        Vec2i A = t0 + (t2 - t0) * alpha;
        Vec2i B = t1 + (t2 - t1) * beta;
        if (A.x > B.x)
            std::swap(A, B);
        memset(buffer + (y * 320) + A.x, color, B.x - A.x);
        /*
        for (int j=A.x; j<=B.x; j++) { 
            PIXEL(buffer, j, y, color);
            //image.set(j, y, color); // attention, due to int casts t0.y+i != A.y 
        } 
        */
    }
}

void triangle_top(ScreenPoint v1, ScreenPoint v2, ScreenPoint v3, u_int16_t color, u_int16_t *buffer)
{
    //printf("TT\n");
    float invslope1, invslope2;
    float divor = v3.y - v1.y;
    if (divor == 0)
    {
        invslope1 = 0.0;
    }
    else
    {
        invslope1 = (v3.x - v1.x) / divor;
    }

    divor = v3.y - v2.y;
    if (divor == 0)
    {
        invslope2 = 0.0;
    }
    else
    {
        invslope2 = (v3.x - v2.x) / divor;
    }

    //printf("inv1: %f, inv2: %f\n", invslope1, invslope2);
    //printf("v3x: [%d,%d], [%d, %d]\n", v3.x, v2.x, v3.y, v2.y);

    float curx1 = v3.x;
    float curx2 = v3.x;

    //int xoff;
    for (int scanlineY = v3.y; scanlineY > v1.y; scanlineY--)
    {
        int tlen = abs((int)(curx2 - curx1) << 1);
        memset(buffer + (scanlineY * 320) + (int)curx1, color, tlen);
        curx1 -= invslope1;
        curx2 -= invslope2;
    }
}

void triangle_bottom(ScreenPoint v1, ScreenPoint v2, ScreenPoint v3, u_int16_t color, u_int16_t *buffer)
{
    float invslope1, invslope2;
    float divor = v2.y - v1.y;
    if (divor == 0)
    {
        invslope1 = 0.0;
    }
    else
    {
        invslope1 = (v2.x - v1.x) / divor;
    }

    divor = v3.y - v1.y;
    if (divor == 0)
    {
        invslope2 = 0.0;
    }
    else
    {
        invslope2 = (v3.x - v1.x) / divor;
    }

    //printf("inv1: %f, inv2: %f\n", invslope1, invslope2);
    //printf("v3x: [%d,%d], [%d, %d]\n", v3.x, v2.x, v3.y, v2.y);

    float curx1 = v1.x;
    float curx2 = v1.x;

    for (int scanlineY = v1.y; scanlineY <= v2.y; scanlineY++)
    {
        //FLOAT_TO_INT(curx1, xoff);
        //memset(buffer+(scanlineY*320)+lrintf(curx1), color, lrintf(curx2)-lrintf(curx1));
        int toff = scanlineY * 320 + (int)curx1;
        int tlen = abs((int)(curx2 - curx1) << 1);
        //printf("toff: [%d][%d]\n", toff, tlen);
        memset(buffer + toff, color, tlen);
        //fb_line((int)curx1, scanlineY, (int)curx2, scanlineY, color, buffer);
        //drawLine((int)curx1, scanlineY, (int)curx2, scanlineY);
        curx1 += invslope1;
        curx2 += invslope2;
    }
}

void fb_my_triangle(ScreenPoint v1, ScreenPoint v2, ScreenPoint v3, u_int16_t color, u_int16_t *buffer)
{
    // sort the vertices, t0, t1, t2 lower−to−upper (bubblesort yay!)
    if (v1.y > v2.y)
        std::swap(v1, v2);
    if (v1.y > v3.y)
        std::swap(v1, v3);
    if (v2.y > v3.y)
        std::swap(v2, v3);

    v1.x = std::max((int)0, v1.x);
    v2.x = std::max((int)0, v2.x);
    v3.x = std::max((int)0, v3.x);
    v1.x = std::min((int)320, v1.x);
    v2.x = std::min((int)320, v2.x);
    v3.x = std::min((int)320, v3.x);
    v1.y = std::max((int)0, v1.y);
    v2.y = std::max((int)0, v2.y);
    v3.y = std::max((int)0, v3.y);
    v1.y = std::min((int)240, v1.y);
    v2.y = std::min((int)240, v2.y);
    v3.y = std::min((int)240, v3.y);

    ScreenPoint v4;
    if (v3.y - v1.y == 0)
    {
        v4.x = v3.x;
    }
    else
    {
        v4.x = ((int)(v1.x + ((float)(v2.y - v1.y) / (float)(v3.y - v1.y)) * (v3.x - v1.x)));
    }
    v4.y = v2.y;
    //printf("p1:%d,%d  p2:%d,%d  p3:%d,%d  p4:%d,%d\n", v1.x, v1.y, v2.x, v2.y, v3.x, v3.y, v4.x, v4.y);

    triangle_bottom(v1, v2, v4, color, buffer);
    triangle_top(v2, v4, v3, color, buffer);

    /*
    PIXEL(buffer, v1.x, v1.y, RED);
    PIXEL(buffer, v2.x, v2.y, BLUE);
    PIXEL(buffer, v3.x, v3.y, GREEN);
    PIXEL(buffer, v4.x, v4.y, WHITE);
    */
    //fb_line(v1.x, v1.y, v2.x, v2.y, WHITE, buffer);
}

/**
 * fastest possible scale blit, just skips pixels, no averaging
 */
void draw_image_half_fast(int src_width, int src_height, int dx, int dy, u_int16_t *src, u_int16_t *dst)
{

    int dst_off, src_off = 0;
    dst_off = PIXEL_OFF(dx, dy);
    for (int y = 0; y < src_height; y += 2)
    {

        for (int x = 0; x < src_width; x += 2)
        {
            dst[dst_off++] = src[src_off + x];
            // skip 1 pixel
        }

        // skip 1 vertical line in src
        src_off += (src_width * 2);

        // skip 1 vertical line in dst
        dst_off += FB_WIDTH - (src_width / 2);
    }
}

void draw_image_half_slow(int src_width, int src_height, int dx, int dy, u_int16_t *src, u_int16_t *dst)
{

    int dst_off, src_off = 0;
    dst_off = PIXEL_OFF(dx, dy);
    for (int y = 480; y < src_height; y += 2)
    {

        // skip 1 vertical line in src
        src_off = (src_width * y);

        for (int x = 0; x < src_width; x += 2)
        {
            uint col = src[src_off + x];
            col += src[src_off + x + 1];
            col += src[src_off + x + src_width];
            col += src[src_off + x + src_width + 1];
            //col /= 4 ;
            col /= 256; // change 0-2048 to 0-32 and average
            //printf("[%d]\n", col);

            //col &= 0b11111000;
            //col = col >> 3;
            //if (col < 0) { col = BLUE + 1; }
            if (col > BLUE)
            {
                col = BLUE;
            }

            dst[dst_off] = col;
            dst_off++;
        }

        // skip 1 vertical line in dst
        dst_off += FB_WIDTH - (src_width / 2);
    }
}

/**
 * fastest possible scale blit, just skips pixels, no averaging
 */
void draw_image_half_fast_skip(int src_width, int src_height, int dx, int dy, u_int16_t *src, u_int16_t *dst)
{

    int dst_off, src_off = 0;
    dst_off = PIXEL_OFF(dx, dy);

    int line_skip = FB_WIDTH - (src_width / 2);
    for (int y = 0; y < src_height; y += 2)
    {

        for (int x = 0; x < src_width; x += 2)
        {
            dst[dst_off++] = src[src_off + x];
            // skip 1 pixel
        }

        // skip 2 vertical lines in src
        src_off += (src_width * 4);

        // skip 1 vertical line in dst
        dst_off += line_skip;
    }
}

FBPngImage read_png_file(const char *filename)
{
    FILE *fp = fopen((char *)filename, "rb");

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png)
    {
        Panic("unable to read PNG");
    }

    png_infop info = png_create_info_struct(png);
    if (!info)
    {
        Panic("unable to create PNG info struct");
    }

    if (setjmp(png_jmpbuf(png)))
    {
        Panic("png jmp abort?");
    }

    png_init_io(png, fp);

    png_read_info(png, info);

    FBPngImage image;

    image.width = png_get_image_width(png, info);
    image.height = png_get_image_height(png, info);
    image.color_type = png_get_color_type(png, info);
    image.bit_depth = png_get_bit_depth(png, info);

    // Read any color_type into 8bit depth, RGBA format.
    // See http://www.libpng.org/pub/png/libpng-manual.txt

    if (image.bit_depth == 16)
        png_set_strip_16(png);

    if (image.color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    // PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
    if (image.color_type == PNG_COLOR_TYPE_GRAY && image.bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);

    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    // These color_type don't have an alpha channel then fill it with 0xff.
    if (image.color_type == PNG_COLOR_TYPE_RGB ||
        image.color_type == PNG_COLOR_TYPE_GRAY ||
        image.color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

    if (image.color_type == PNG_COLOR_TYPE_GRAY ||
        image.color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

/*
    if (image.row_pointers)
    {
        Panic("row pointers init already!");
    }
    */

    image.row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * image.height);
    for (int y = 0; y < image.height; y++)
    {
        image.row_pointers[y] = (png_byte *)malloc(png_get_rowbytes(png, info));
    }

    png_read_image(png, image.row_pointers);

    printf("loaded font file [%s]\n", filename);
    fclose(fp);
    png_destroy_read_struct(&png, &info, NULL);

    return image;
}




/**
 *  writes the string to the display
 */
void fb_write_string(char *msg, int buffx, int buffy, u_int16_t fgcolor, u_int16_t *buffer)
{
    int l = strlen(msg);
    // loop over each char
    for (int i = 0; i < l; i++)
    {
        // map the char to the font offset
        FontChar fontc = fontData[(int)msg[i]];

        // loop over each row
        for (int y = 0; y < fontc.height; y++)
        {
            png_bytep row = FB_FONT_IMAGE.row_pointers[y + fontc.y];
            // loop over each column
            for (int x = 0; x < fontc.width; x++)
            {
                // map out the alpha level
                png_bytep px = &(row[(fontc.x + x) * 4]);
                // calculate the color as white
                u_int16_t color = ((px[3] >> 3) << 11) + ((px[3] >> 2) << 5) + (px[3] >> 3);
                // plot the pixel
                int offset = ((y + buffy + fontc.yoffset) * FB_WIDTH) + x + buffx + fontc.xoffset;
                if (color > 0)
                {
                    color = (fgcolor > 0) ? fgcolor : color;
                    buffer[offset] = color;
                }
            }
        }

        // skip to the next character
        buffx += 18;
    }
}

// swaps two numbers
void swap(int *a, int *b)
{
    int temp = *a;
    *a = *b;
    *b = temp;
}

// returns absolute value of number
float absolute(float x)
{
    if (x < 0)
        return -x;
    else
        return x;
}

//returns integer part of a floating point number
int iPartOfNumber(float x)
{
    return (int)x;
}

//rounds off a number
int roundNumber(float x)
{
    return iPartOfNumber(x + 0.5);
}

//returns fractional part of a number
float fPartOfNumber(float x)
{
    if (x > 0)
        return x - iPartOfNumber(x);
    else
        return x - (iPartOfNumber(x) + 1);
}

//returns 1 - fractional part of number
float rfPartOfNumber(float x)
{
    return 1 - fPartOfNumber(x);
}

// draws a pixel on screen of given brightness
// 0<=brightness<=1. We can use your own library
// to draw on screen
/*
void drawPixel( int x , int y , float brightness) 
{ 
    int c = 255*brightness; 
    SDL_SetRenderDrawColor(pRenderer, c, c, c, 255); 
    SDL_RenderDrawPoint(pRenderer, x, y); 
} 
*/

void fb_vixel(int x0, int y0, u_int8_t szx, u_int8_t szy, u_int16_t color, u_int16_t *buffer)
{

    // bounding...
    x0 = (x0 < 0) ? 0 : x0;
    y0 = (y0 < 0) ? 0 : y0;
    if (x0 > FB_WIDTH)
    {
        return;
    }
    if (y0 > FB_HEIGHT)
    {
        return;
    }
    szx = (x0 + szx < FB_WIDTH) ? szx : FB_WIDTH - x0 - 1;
    szy = (y0 + szy < FB_HEIGHT) ? szy : FB_HEIGHT - y0 - 1;

    for (int y = y0; y < y0 + szy; y++)
    {
        for (int x = x0; x < x0 + szx; x++)
        {
            buffer[(y * FB_WIDTH) + x] = color;
        }
    }
}

void fb_vixel_add(int x0, int y0, u_int8_t szx, u_int8_t szy, u_int16_t color, u_int16_t *buffer)
{

    // bounding...
    x0 = (x0 < 0) ? 0 : x0;
    y0 = (y0 < 0) ? 0 : y0;
    if (x0 > FB_WIDTH)
    {
        return;
    }
    if (y0 > FB_HEIGHT)
    {
        return;
    }
    szx = (x0 + szx < FB_WIDTH) ? szx : FB_WIDTH - x0 - 1;
    szy = (y0 + szy < FB_HEIGHT) ? szy : FB_HEIGHT - y0 - 1;

    for (int y = y0; y < y0 + szy; y++)
    {
        for (int x = x0; x < x0 + szx; x++)
        {
            buffer[(y * FB_WIDTH) + x] += color;
        }
    }
}

/**
 * todo: calculate color based from lineColor
 */
void fb_aa_line(int x0, int y0, int x1, int y1, u_int16_t lineColor, u_int16_t *buffer)
{
    int steep = absolute(y1 - y0) > absolute(x1 - x0);
    if (1 == 2) {
        printf("%u ", lineColor);
    }

    // swap the co-ordinates if slope > 1 or we
    // draw backwards
    if (steep)
    {
        swap(&x0, &y0);
        swap(&x1, &y1);
    }
    if (x0 > x1)
    {
        swap(&x0, &x1);
        swap(&y0, &y1);
    }

    //compute the slope
    float dx = x1 - x0;
    float dy = y1 - y0;
    float gradient;
    if (dx == 0.0)
        gradient = 1;
    else
        gradient = dy / dx;
    //printf("gradient: [%f] [%d]\n", gradient, steep);

    int xpxl1 = (x0 <= 0) ? 1 : x0;
    int xpxl2 = x1;
    float intersectY = (y0 <= 0) ? 1 : y0;

    // main loop
    if (steep)
    {
        int x;
        for (x = xpxl1; x <= xpxl2 && x < FB_HEIGHT; x++)
        {
            // pixel coverage is determined by fractional
            // part of y co-ordinate
            int px = 255 * fPartOfNumber(intersectY);
            u_int16_t color = ((px >> 3) << 11) + ((px >> 2) << 5) + (px >> 3);
            PIXEL(buffer, iPartOfNumber(intersectY), x, color);

            px = 255 * rfPartOfNumber(intersectY);
            color = ((px >> 3) << 11) + ((px >> 2) << 5) + (px >> 3);
            PIXEL(buffer, iPartOfNumber(intersectY) - 1, x, color);

            intersectY += gradient;
        }
    }
    else
    {
        int x;
        for (x = xpxl1; x <= xpxl2 && x < FB_WIDTH && intersectY < FB_HEIGHT; x++)
        {
            int px = 255 * fPartOfNumber(intersectY);
            u_int16_t color = ((px >> 3) << 11) + ((px >> 2) << 5) + (px >> 3);
            buffer[(iPartOfNumber(intersectY) * FB_WIDTH) + x] = color;

            px = 255 * rfPartOfNumber(intersectY);
            color = ((px >> 3) << 11) + ((px >> 2) << 5) + (px >> 3);
            buffer[((iPartOfNumber(intersectY) - 1) * FB_WIDTH) + x] = color;

            intersectY += gradient;
        }
    }
}
