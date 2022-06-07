#include <stdint.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <stdio.h>
#include <vector>
#include <fstream>
#include <unistd.h>
#include <string.h>
#include <jpeglib.h>
#include <functional>
#include <algorithm>
#include <vector>
#include <memory>
#include <map>
#include <math.h>
#include <strings.h>

#include <linux/fb.h>

#include "demos.h"
#include "core.h"
#include "system.h"
#include "color/hsv.h"
#include "utils.hpp"
#include "math.h"
#include "data.h"
#include "shapes.h"
#include "image.h"
#include "system.h"
#include "calibration.h"
#include "utils.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <png.h>
#include <fcntl.h>
#include <sys/mman.h>

static u16  *offset_mapping = new u16[6000*8];
extern RGB8 *hsv_cache;

/*
struct FBPngImage FB_LARGE_FONT;
FontChar FB_LARGE_FONT_OFF[128];
struct FBPngImage FB_TINY_FONT;
FontChar FB_TINY_FONT_OFF[128];
*/



std::unique_ptr<FontChar[]> init_large_fonts() {
    std::unique_ptr<FontChar[]> large_fonts = std::make_unique<FontChar[]>(128);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    large_fonts[10] = (FontChar) {.x = 0, .y = 200, .width = 16, .height = 22, .xoffset = 0, .yoffset = 0};
    large_fonts[32] = (FontChar) {.x = 0, .y = 100, .width = 16, .height = 22, .xoffset = 0, .yoffset = 0};
    large_fonts[33] = (FontChar) {.x = 370, .y = 0, .width = 6, .height = 24, .xoffset = 6, .yoffset = 1};
    large_fonts[34] = (FontChar) {.x = 339, .y = 56, .width = 11, .height = 9, .xoffset = 3, .yoffset = 1};
    large_fonts[35] = (FontChar) {.x = 0, .y = 56, .width = 18, .height = 22, .xoffset = 0, .yoffset = 3};
    large_fonts[36] = (FontChar) {.x = 110, .y = 0, .width = 16, .height = 30, .xoffset = 1, .yoffset = -1};
    large_fonts[37] = (FontChar) {.x = 398, .y = 0, .width = 21, .height = 24, .xoffset = -1, .yoffset = 1};
    large_fonts[38] = (FontChar) {.x = 419, .y = 0, .width = 19, .height = 24, .xoffset = 0, .yoffset = 1};
    large_fonts[39] = (FontChar) {.x = 505, .y = 34, .width = 5, .height = 9, .xoffset = 6, .yoffset = 1};
    large_fonts[40] = (FontChar) {.x = 5, .y = 0, .width = 11, .height = 33, .xoffset = 4, .yoffset = -1};
    large_fonts[41] = (FontChar) {.x = 16, .y = 0, .width = 12, .height = 33, .xoffset = 2, .yoffset = -1};
    large_fonts[42] = (FontChar) {.x = 281, .y = 56, .width = 16, .height = 15, .xoffset = 1, .yoffset = 1};
    large_fonts[43] = (FontChar) {.x = 264, .y = 56, .width = 17, .height = 17, .xoffset = 0, .yoffset = 7};
    large_fonts[44] = (FontChar) {.x = 297, .y = 56, .width = 10, .height = 13, .xoffset = 3, .yoffset = 17};
    large_fonts[45] = (FontChar) {.x = 388, .y = 56, .width = 11, .height = 5, .xoffset = 3, .yoffset = 13};
    large_fonts[46] = (FontChar) {.x = 381, .y = 56, .width = 7, .height = 7, .xoffset = 5, .yoffset = 18};
    large_fonts[47] = (FontChar) {.x = 145, .y = 0, .width = 17, .height = 28, .xoffset = 0, .yoffset = 1};
    large_fonts[48] = (FontChar) {.x = 488, .y = 34, .width = 17, .height = 22, .xoffset = 0, .yoffset = 3};
    large_fonts[49] = (FontChar) {.x = 345, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    large_fonts[50] = (FontChar) {.x = 361, .y = 34, .width = 15, .height = 22, .xoffset = 1, .yoffset = 3};
    large_fonts[51] = (FontChar) {.x = 376, .y = 34, .width = 15, .height = 22, .xoffset = 2, .yoffset = 3};
    large_fonts[52] = (FontChar) {.x = 391, .y = 34, .width = 18, .height = 22, .xoffset = 0, .yoffset = 3};
    large_fonts[53] = (FontChar) {.x = 409, .y = 34, .width = 15, .height = 22, .xoffset = 1, .yoffset = 3};
    large_fonts[54] = (FontChar) {.x = 424, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    large_fonts[55] = (FontChar) {.x = 440, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    large_fonts[56] = (FontChar) {.x = 456, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    large_fonts[57] = (FontChar) {.x = 472, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    large_fonts[58] = (FontChar) {.x = 257, .y = 56, .width = 7, .height = 18, .xoffset = 5, .yoffset = 7};
    large_fonts[59] = (FontChar) {.x = 388, .y = 0, .width = 10, .height = 24, .xoffset = 2, .yoffset = 7};
    large_fonts[60] = (FontChar) {.x = 18, .y = 56, .width = 15, .height = 21, .xoffset = 1, .yoffset = 5};
    large_fonts[61] = (FontChar) {.x = 324, .y = 56, .width = 15, .height = 10, .xoffset = 1, .yoffset = 10};
    large_fonts[62] = (FontChar) {.x = 33, .y = 56, .width = 15, .height = 21, .xoffset = 2, .yoffset = 5};
    large_fonts[63] = (FontChar) {.x = 376, .y = 0, .width = 12, .height = 24, .xoffset = 4, .yoffset = 1};
    large_fonts[64] = (FontChar) {.x = 90, .y = 0, .width = 20, .height = 30, .xoffset = 0, .yoffset = 1};
    large_fonts[65] = (FontChar) {.x = 438, .y = 0, .width = 20, .height = 22, .xoffset = -1, .yoffset = 3};
    large_fonts[66] = (FontChar) {.x = 458, .y = 0, .width = 15, .height = 22, .xoffset = 1, .yoffset = 3};
    large_fonts[67] = (FontChar) {.x = 473, .y = 0, .width = 16, .height = 22, .xoffset = 0, .yoffset = 3};
    large_fonts[68] = (FontChar) {.x = 489, .y = 0, .width = 17, .height = 22, .xoffset = 1, .yoffset = 3};
    large_fonts[69] = (FontChar) {.x = 0, .y = 34, .width = 14, .height = 22, .xoffset = 2, .yoffset = 3};
    large_fonts[70] = (FontChar) {.x = 14, .y = 34, .width = 14, .height = 22, .xoffset = 2, .yoffset = 3};
    large_fonts[71] = (FontChar) {.x = 28, .y = 34, .width = 17, .height = 22, .xoffset = 0, .yoffset = 3};
    large_fonts[72] = (FontChar) {.x = 45, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    large_fonts[73] = (FontChar) {.x = 61, .y = 34, .width = 14, .height = 22, .xoffset = 2, .yoffset = 3};
    large_fonts[74] = (FontChar) {.x = 75, .y = 34, .width = 13, .height = 22, .xoffset = 2, .yoffset = 3};
    large_fonts[75] = (FontChar) {.x = 88, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    large_fonts[76] = (FontChar) {.x = 104, .y = 34, .width = 14, .height = 22, .xoffset = 3, .yoffset = 3};
    large_fonts[77] = (FontChar) {.x = 118, .y = 34, .width = 17, .height = 22, .xoffset = 0, .yoffset = 3};
    large_fonts[78] = (FontChar) {.x = 135, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    large_fonts[79] = (FontChar) {.x = 151, .y = 34, .width = 18, .height = 22, .xoffset = 0, .yoffset = 3};
    large_fonts[80] = (FontChar) {.x = 169, .y = 34, .width = 15, .height = 22, .xoffset = 1, .yoffset = 3};
    large_fonts[81] = (FontChar) {.x = 126, .y = 0, .width = 19, .height = 28, .xoffset = 0, .yoffset = 3};
    large_fonts[82] = (FontChar) {.x = 184, .y = 34, .width = 16, .height = 22, .xoffset = 2, .yoffset = 3};
    large_fonts[83] = (FontChar) {.x = 200, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    large_fonts[84] = (FontChar) {.x = 216, .y = 34, .width = 17, .height = 22, .xoffset = 0, .yoffset = 3};
    large_fonts[85] = (FontChar) {.x = 233, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    large_fonts[86] = (FontChar) {.x = 249, .y = 34, .width = 20, .height = 22, .xoffset = -1, .yoffset = 3};
    large_fonts[87] = (FontChar) {.x = 269, .y = 34, .width = 19, .height = 22, .xoffset = -1, .yoffset = 3};
    large_fonts[88] = (FontChar) {.x = 288, .y = 34, .width = 20, .height = 22, .xoffset = -1, .yoffset = 3};
    large_fonts[89] = (FontChar) {.x = 308, .y = 34, .width = 21, .height = 22, .xoffset = -2, .yoffset = 3};
    large_fonts[90] = (FontChar) {.x = 329, .y = 34, .width = 16, .height = 22, .xoffset = 1, .yoffset = 3};
    large_fonts[91] = (FontChar) {.x = 28, .y = 0, .width = 10, .height = 32, .xoffset = 4, .yoffset = -1};
    large_fonts[92] = (FontChar) {.x = 162, .y = 0, .width = 16, .height = 28, .xoffset = 1, .yoffset = 1};
    large_fonts[93] = (FontChar) {.x = 38, .y = 0, .width = 10, .height = 32, .xoffset = 3, .yoffset = -1};
    large_fonts[94] = (FontChar) {.x = 307, .y = 56, .width = 17, .height = 12, .xoffset = 0, .yoffset = 3};
    large_fonts[95] = (FontChar) {.x = 399, .y = 56, .width = 20, .height = 5, .xoffset = -1, .yoffset = 26};
    large_fonts[96] = (FontChar) {.x = 350, .y = 56, .width = 13, .height = 9, .xoffset = -1, .yoffset = 1};
    large_fonts[97] = (FontChar) {.x = 48, .y = 56, .width = 15, .height = 18, .xoffset = 1, .yoffset = 7};
    large_fonts[98] = (FontChar) {.x = 193, .y = 0, .width = 16, .height = 24, .xoffset = 1, .yoffset = 1};
    large_fonts[99] = (FontChar) {.x = 63, .y = 56, .width = 14, .height = 18, .xoffset = 1, .yoffset = 7};
    large_fonts[100] = (FontChar) {.x = 209, .y = 0, .width = 15, .height = 24, .xoffset = 1, .yoffset = 1};
    large_fonts[101] = (FontChar) {.x = 77, .y = 56, .width = 16, .height = 18, .xoffset = 1, .yoffset = 7};
    large_fonts[102] = (FontChar) {.x = 224, .y = 0, .width = 18, .height = 24, .xoffset = -1, .yoffset = 1};
    large_fonts[103] = (FontChar) {.x = 242, .y = 0, .width = 17, .height = 24, .xoffset = 0, .yoffset = 7};
    large_fonts[104] = (FontChar) {.x = 259, .y = 0, .width = 15, .height = 24, .xoffset = 1, .yoffset = 1};
    large_fonts[105] = (FontChar) {.x = 274, .y = 0, .width = 15, .height = 24, .xoffset = 2, .yoffset = 1};
    large_fonts[106] = (FontChar) {.x = 48, .y = 0, .width = 14, .height = 30, .xoffset = 1, .yoffset = 1};
    large_fonts[107] = (FontChar) {.x = 289, .y = 0, .width = 16, .height = 24, .xoffset = 2, .yoffset = 1};
    large_fonts[108] = (FontChar) {.x = 305, .y = 0, .width = 15, .height = 24, .xoffset = 2, .yoffset = 1};
    large_fonts[109] = (FontChar) {.x = 93, .y = 56, .width = 17, .height = 18, .xoffset = 0, .yoffset = 7};
    large_fonts[110] = (FontChar) {.x = 110, .y = 56, .width = 15, .height = 18, .xoffset = 1, .yoffset = 7};
    large_fonts[111] = (FontChar) {.x = 125, .y = 56, .width = 17, .height = 18, .xoffset = 0, .yoffset = 7};
    large_fonts[112] = (FontChar) {.x = 320, .y = 0, .width = 16, .height = 24, .xoffset = 1, .yoffset = 7};
    large_fonts[113] = (FontChar) {.x = 178, .y = 0, .width = 15, .height = 25, .xoffset = 1, .yoffset = 6};
    large_fonts[114] = (FontChar) {.x = 142, .y = 56, .width = 15, .height = 18, .xoffset = 2, .yoffset = 7};
    large_fonts[115] = (FontChar) {.x = 157, .y = 56, .width = 14, .height = 18, .xoffset = 2, .yoffset = 7};
    large_fonts[116] = (FontChar) {.x = 336, .y = 0, .width = 16, .height = 24, .xoffset = 0, .yoffset = 1};
    large_fonts[117] = (FontChar) {.x = 171, .y = 56, .width = 15, .height = 18, .xoffset = 1, .yoffset = 7};
    large_fonts[118] = (FontChar) {.x = 186, .y = 56, .width = 18, .height = 18, .xoffset = 0, .yoffset = 7};
    large_fonts[119] = (FontChar) {.x = 204, .y = 56, .width = 20, .height = 18, .xoffset = -1, .yoffset = 7};
    large_fonts[120] = (FontChar) {.x = 224, .y = 56, .width = 18, .height = 18, .xoffset = 0, .yoffset = 7};
    large_fonts[121] = (FontChar) {.x = 352, .y = 0, .width = 18, .height = 24, .xoffset = 0, .yoffset = 7};
    large_fonts[122] = (FontChar) {.x = 242, .y = 56, .width = 15, .height = 18, .xoffset = 2, .yoffset = 7};
    large_fonts[123] = (FontChar) {.x = 62, .y = 0, .width = 14, .height = 30, .xoffset = 1, .yoffset = 1};
    large_fonts[124] = (FontChar) {.x = 0, .y = 0, .width = 5, .height = 34, .xoffset = 7, .yoffset = -3};
    large_fonts[125] = (FontChar) {.x = 76, .y = 0, .width = 14, .height = 30, .xoffset = 2, .yoffset = 1};
    large_fonts[126] = (FontChar) {363, 56, 18, 9, 0, 11};

#pragma GCC diagnostic pop
    return large_fonts;
}

std::unique_ptr<FontChar[]> init_small_fonts() {
    std::unique_ptr<FontChar[]> small_fonts = std::make_unique<FontChar[]>(128);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
    //FB_LARGE_FONT_OFF[32] = (FontChar) {.x = 0, .y = 40, .width = 6, .height = 7, .xoffset = 1, .yoffset = 0};
    //for (int i=65; i<=90; i++) {
    for (int i=0; i<=25; i++) {
        small_fonts[65+i] = (FontChar) { .x = i*6, .y = 22, .width = 6, .height = 7, .xoffset = 1, .yoffset = 0 };
    }
    for (int i=0; i<=25; i++) {
        small_fonts[97+i] = (FontChar) { .x = i*6, .y = 22, .width = 6, .height = 7, .xoffset = 1, .yoffset = 0 };
        //FB_TINY_FONT_OFF[97+i] = (FontChar) { .x = i*4, .y = 1, .width = 3, .height = 5, .xoffset = 1, .yoffset = 1 };
    }
    for (int i=0; i<=9; i++) {
        small_fonts[48+i] = (FontChar) { .x = i*6, .y = 30, .width = 6, .height = 7, .xoffset = 1, .yoffset = 0 };
    }
    small_fonts[58] = (FontChar) { .x = 61, .y = 30, .width = 6, .height = 7, .xoffset = 1, .yoffset = 0 };
    small_fonts[45] = (FontChar) { .x = 66, .y = 30, .width = 6, .height = 7, .xoffset = 1, .yoffset = 0 };
    small_fonts[124] = (FontChar) { .x = 71, .y = 30, .width = 6, .height = 7, .xoffset = 1, .yoffset = 0 };
    small_fonts[34] = (FontChar) { .x = 77, .y = 30, .width = 6, .height = 7, .xoffset = 1, .yoffset = 0 };
    small_fonts[39] = (FontChar) { .x = 84, .y = 30, .width = 6, .height = 7, .xoffset = 1, .yoffset = 0 };
    small_fonts[91] = (FontChar) { .x = 90, .y = 30, .width = 6, .height = 7, .xoffset = 1, .yoffset = 0 };
    small_fonts[93] = (FontChar) { .x = 96, .y = 30, .width = 6, .height = 7, .xoffset = 1, .yoffset = 0 };
    small_fonts[40] = (FontChar) { .x = 102, .y = 30, .width = 6, .height = 7, .xoffset = 1, .yoffset = 0 };
    small_fonts[41] = (FontChar) { .x = 108, .y = 30, .width = 6, .height = 7, .xoffset = 1, .yoffset = 0 };
    small_fonts[43] = (FontChar) { .x = 114, .y = 30, .width = 6, .height = 7, .xoffset = 1, .yoffset = 0 };
    small_fonts[44] = (FontChar) { .x = 132, .y = 30, .width = 6, .height = 7, .xoffset = 1, .yoffset = 0 };
    small_fonts[45] = (FontChar) { .x = 120, .y = 30, .width = 6, .height = 7, .xoffset = 1, .yoffset = 0 };
    small_fonts[46] = (FontChar) { .x = 134, .y = 30, .width = 6, .height = 7, .xoffset = 3, .yoffset = 0 };
    small_fonts[47] = (FontChar) { .x = 126, .y = 30, .width = 6, .height = 7, .xoffset = 1, .yoffset = 0 };
    small_fonts[92] = (FontChar) { .x = 132, .y = 30, .width = 6, .height = 7, .xoffset = 1, .yoffset = 0 };
    small_fonts[92] = (FontChar) { .x = 138, .y = 30, .width = 6, .height = 7, .xoffset = 1, .yoffset = 0 };
    small_fonts[60] = (FontChar) { .x = 144, .y = 30, .width = 6, .height = 7, .xoffset = 1, .yoffset = 0 };
    small_fonts[62] = (FontChar) { .x = 150, .y = 30, .width = 6, .height = 7, .xoffset = 1, .yoffset = 0 };
    small_fonts[37] = (FontChar) { .x = 162, .y = 30, .width = 6, .height = 7, .xoffset = 1, .yoffset = 0 };

#pragma GCC diagnostic pop
    return small_fonts;
}

void fb_write_font_string(const ChitonFB *disp, const char *msg, const xy_t point, const u16 fgcolor, const ChitonPngImage *src, const FontChar *font_offsets) {
    int l = strlen(msg);
    int char_offset = 0;
    //printf("write string len [%d] @ %d.%d\n", l, point.x, point.y);
    // loop over each char
    for (int i = 0; i < l; i++) {
        //int off = msg[i];
        //printf("have [%d]\n", off);
        FontChar fontc = font_offsets[(int)msg[i]];
        //printf("font char height: [%d] width: [%d]\n", fontc.height, fontc.width);

        // loop over each row
        for(int y = 0; y < fontc.height; y++) {
            //printf("ROW [%d] .. [%d]\n", y, y+fontc.y);
            png_bytep row = src->row_pointers[y+fontc.y];
            // loop over each column
            for(int x = 0; x <fontc.width; x++) {
                // map out the alpha level
                png_bytep px = &(row[(fontc.x + x) * 4]);
                //printf("pixel: [%d] alpha: [%d]\n", px[0], px[3]);
                // calculate the color as white
                //u_int16_t color = ((px[3] >> 3)<<11) + ((px[3] >> 2)<<5) + (px[3] >> 3);
                // plot the pixel
                //printf(" offset: [%d]\n", offset);
                //printf("color: [%d]\n", color);
                //disp->buffer1[disp->width*y + x] = HsvToRgb(hue, sat, 250);//color;
                if (px[3] != 0) {
                    int offset = ((y+point.y+fontc.yoffset)*disp->width)+x+point.x+char_offset+fontc.xoffset;
                    disp->buffer1[offset] = fgcolor;
                }
            }
        }

        // skip to the next character
        char_offset += fontc.width + 1;
    }
}



/**
 * calculate the mode of the last 30 standard deviations
 */
uint16_t calc_temporal_noise(uint16_t std) {
    static uint32_t frames = 0; 
    static std::vector<uint16_t> noise(30);
    noise[frames++ % noise.size()] = std;
    
    std::vector<uint16_t> noise_dup = noise;
    std::sort(noise_dup.begin(), noise_dup.end());
    return noise_dup[noise_dup.size() / 2];
}


/**
 * 
 */
uint16_t calc_rms(std::vector<uint16_t> window, uint16_t mean) {

    Return_IfNot((window.size() > 0), "rms window empty", 0);

    uint32_t sum = 0;
    std::for_each(window.begin(), window.end(), [&](uint16_t i) {
        sum += (i - mean) * (i - mean);
    });
    return sqrt(sum / window.size());
}


/**
 * call fn for each pixel in data between the bounding box x1,x2/y1,y2
 */
void map_image(uint16_t *data, int width, int x1, int y1, int x2, int y2, std::function<void(uint16_t)> fn) {
    uint offset;
    // min/max x/y
    int ya = std::min(y1, y2);
    int yb = std::max(y1, y2);
    int xa = std::min(x1, x2);
    int xb = std::max(x1, x2);

    // iterate over the bounding box
    for (int y = ya; y < yb; y++) {
        // find first pixel in the row (y)
        offset = (y*width) + xa;
        // call fn for each pixel in the row (x)
        for (int x = xa; x < xb; x++) {
            fn(data[offset++]);
        }
    }
}


/**
 * get all values of *data inside the bounding box returned as a vector if uint16_t
 */
std::vector<uint16_t> get_window(uint16_t *data, int width, int x1, int y1, int x2, int y2) {
    // allocate memory for result
    int xd = MAX(1, core::absdiff(x1, x2));
    int yd = MAX(1, core::absdiff(y1, y2));
    std::vector<uint16_t> result(xd * yd);
    uint16_t *ptr = result.data();

    int ptr_idx = 0;
    map_image(data, width, x1, y1, x2, y2, [&ptr, &ptr_idx](uint16_t point) {
        ptr[ptr_idx++] = point;
    });

    return result;
}









/**
 * writes to jpeg_data_wptr;
 * sets length in mem_size ptr
 * jpeg data will be stored in dest_ptr, this memory must be freed
 * free() any previously allocated memory (that is if dest_ptr != NULL)
 */
/*
void write_jpeg(JSAMPLE *image_buffer, long dest_ptr, unsigned long *mem_size, resolution_t resolution)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    int row_stride;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    jpeg_mem_dest(&cinfo, (unsigned char**)dest_ptr, mem_size);

    cinfo.image_width = resolution.width;
    cinfo.image_height = resolution.height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, GOPTS->quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    row_stride = cinfo.image_width * 3;
    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = &image_buffer[cinfo.next_scanline * row_stride];
        (void)jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
}
*/

    /**
     * @brief generate calibrated data and render a 3d point cloud using vectorized math
     * requires calibraion::make_calibration_data() to be called first...
     * 
     * @param src the pointer to raw data
     * @param dst the pointer to the calibrated data 
     */
    void create_slam_jpeg(u16 *__restrict__ slam, u16 slam_idx, JSAMPLE *__restrict__ jpeg_data) {
    
        memset(jpeg_data, 0, 640*480*3);

        u16 *new_slam = slam+(slam_idx * SLAM_POINTS);
        u16 *slam_info = slam+(99 * SLAM_POINTS);



        float inc = PI2 / 1024;
        float a = -PI;
        float aoi_unit = (float)slam_info[1024] / 6500.0f;

        uint color_off = slam_info[0] < 800 ? 0 : 1;

        for (int i=0; i<SLAM_POINTS; i++) {
            a += inc;
            float unit = (float)new_slam[i] / 6500.0f;
            int x = CLAMP640(((unit * sinf(a)) * 512)+ GOPTS->center_x);
            int y = CLAMP480(((unit * -cosf(a)) * 512)+ GOPTS->center_y+100);
            int offset = (y*CAM_WIDTH)+x;
            if (slam_info[i] & (1 << SLAM_STATIC_BIT)) {
                jpeg_data[(offset*3)+color_off] = 230;
            } else {
                jpeg_data[(offset*3)+3] = 230;
            }

            // draw AREA OF INTEREST LINE
            /*
            if (slam_info[i] & (1 << SLAM_AOI_BIT)) {
                jpeg_data[(offset*3)+2] = 230;

                int x = CLAMP640(((aoi_unit * sinf(a)) * 600)+ GOPTS->center_x);
                int y = CLAMP480(((aoi_unit * -cosf(a)) * 600)+ GOPTS->center_y+100);
                int offset = (y*CAM_WIDTH)+x;
                jpeg_data[(offset*3)] = 250;
            }
            */
            /* else {
                jpeg_data[(offset*3)] = 230;
            }
            if (slam_info[i] & (1 << SLAM_SPIKE_BIT)) {
                jpeg_data[(offset*3)+2] = 230;
            }
            */
        }
    }

    int calc_src_dst_points(int *src_ptr, int *dst_ptr, int width, int height) {

        const float min_angle = 0.90f;
        const float max_angle = 5.36f;
        const float angle_inc = (max_angle - min_angle) / (float)width;

        INFO("inner / outer dist [%d / %d]", GOPTS->inner_dist, GOPTS->outer_dist);

        float angle = min_angle;
        int ctr = 0;
        for (int x=0; x<width; x++) {

            xyf_t p1 = gfx::calculate_circle_point_float(GOPTS->inner_dist, angle);
            xyf_t p2 = gfx::calculate_circle_point_float(GOPTS->outer_dist, angle);

            float x1 = p1.x + GOPTS->center_x;
            float x2 = p2.x + GOPTS->center_x;
            float y1 = p1.y + GOPTS->center_y;
            float y2 = p2.y + GOPTS->center_y;

            linef_t line2 = {x1, y1, x2, y2};

            std::vector<Point2D_t> points = gfx::line_to_point_vector(line2, height);

            for (int i=0; i<points.size() && i < height; i++) {
                dst_ptr[ctr] = x + (i * width);
                src_ptr[ctr] = (points[i].y * CAM_WIDTH) + points[i].x;
                PANIC_IF(ctr > width * height, "line mapping went past end of memory", -1);
                ctr++;
            }
            angle += angle_inc;
        }

        return ctr;
    }


unsigned long create_planar_jpeg(u16 *__restrict__ src, JSAMPLE *__restrict__ jpeg_data, int *src_ptr, int *dst_ptr, int points) {

    int div = (GOPTS->calibration_enabled) ? 26 : 16;

    for (int i=0; i<points; i++) {
        jpeg_data[dst_ptr[i]] = 255 - (src[src_ptr[i]] / div); // CREATE IMAGE FROM 16 bit input (0-6000)
    }
   return 0;
}

unsigned long create_planar_color_jpeg(u16 *__restrict__ src, JSAMPLE *__restrict__ jpeg_data, int *src_ptr, int *dst_ptr, int points) {

    uint rgb_offset = 0;
    for (int i=0; i<points; i++) {
        depth_to_hsv_raw(src+src_ptr[i], jpeg_data + rgb_offset);
        rgb_offset += 3;
    }

    //rgb_offset += 3;
    //depth_to_hsv_raw(src+i, jpeg_data + rgb_offset);
    return 0;
}



unsigned long create_greyscale_jpeg(u16 *__restrict__ src, JSAMPLE *__restrict__ jpeg_data, bool chitoncal) { //}, long dest_ptr, resolution_t resolution) {

    calibration::runline *runlines = calibration::get_run_lines();

    if (GOPTS->depth_frame) {
        // GCC vectorizer can not vectorize > 2 loops, so must move the conditional here...
        if (chitoncal) {
            for (int j=0; j<calibration::get_num_runlines(); j++) {
                const uint srcstart = runlines[j].srcstart;
                const uint runlen = runlines[j].runlen;

                for (uint i=srcstart; i<(runlen); i++) {
                    jpeg_data[i] = 255 - (src[i] / 26); // CREATE IMAGE FROM 16 bit input (0-6000)
                }
            }
        }
        else {
            for (int j=0; j<calibration::get_num_runlines(); j++) {
                const uint srcstart = runlines[j].srcstart;
                const uint runlen = runlines[j].runlen;
                for (uint i=srcstart; i<(runlen); i++) {
                    jpeg_data[i] = 255 - (src[i] / 16); // CREATE IMAGE FROM 16 bit input (0-6000)
                }
            }
        }
    } else {
        // GCC vectorizer can not vectorize > 2 loops, so must move the conditional here...
        if (chitoncal) {
            for (int j=0; j<calibration::get_num_runlines(); j++) {
                const uint srcstart = runlines[j].srcstart;
                const uint runlen = runlines[j].runlen;

                for (uint i=srcstart; i<(runlen); i++) {
                    jpeg_data[i] = (src[i] / 24); // CREATE IMAGE FROM 16 bit input (0-6000)
                }
            }
        }
        else {
            for (int j=0; j<calibration::get_num_runlines(); j++) {
                const uint srcstart = runlines[j].srcstart;
                const uint runlen = runlines[j].runlen;
                for (uint i=srcstart; i<(runlen); i++) {
                    jpeg_data[i] = (src[i] / 16); // CREATE IMAGE FROM 16 bit input (0-6000)
                }
            }
        }

    }

    return 0;
}

unsigned long create_color_jpeg(uint16_t *__restrict__ src, JSAMPLE *__restrict__ jpeg_data) {

    calibration::runline *runlines = calibration::get_run_lines();
    for (int j=0; j<calibration::get_num_runlines(); j++) {
        const uint srcstart = runlines[j].srcstart;
        const uint runlen = runlines[j].runlen;

        uint rgb_offset = srcstart * 3;
        for (uint i=srcstart; i<(runlen); i++) {
            rgb_offset += 3;
            depth_to_hsv_raw(src+i, jpeg_data + rgb_offset);
        }
    }

    return 0;
}



unsigned long write_jpeg(JSAMPLE *src_data, long dest_ptr, struct jpeg_compress_struct cinfo) {
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    int row_stride;
    unsigned long mem_size;

    struct jpeg_compress_struct cinfo2;
    cinfo2.err = jpeg_std_error(&jerr);

    jpeg_create_compress(&cinfo2);

    jpeg_mem_dest(&cinfo2, (unsigned char**)dest_ptr, &mem_size);

    cinfo2.input_components = cinfo.input_components;
    cinfo2.in_color_space = cinfo.in_color_space;
    cinfo2.image_width = cinfo.image_width;
    cinfo2.image_height = cinfo.image_height;
    
    jpeg_set_defaults(&cinfo2);
    jpeg_set_quality(&cinfo2, GOPTS->quality, TRUE);
    jpeg_start_compress(&cinfo2, TRUE);

    row_stride = cinfo.image_width * cinfo.input_components;
    while (cinfo2.next_scanline < cinfo2.image_height) {
        row_pointer[0] = &src_data[cinfo2.next_scanline * row_stride];
        (void)jpeg_write_scanlines(&cinfo2, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo2);
    jpeg_destroy_compress(&cinfo2);

    return mem_size;
}

/**
 * writes to dest_ptr;
 * returns the size of the image data
 * jpeg data will be stored in dest_ptr, this memory must be freed
 * free() any previously allocated memory (that is if dest_ptr != NULL)
 */
unsigned long write_greyscale_jpeg(JSAMPLE *image_buffer, long dest_ptr, resolution_t resolution) {
    struct jpeg_compress_struct cinfo;
    cinfo.input_components = 1;
    cinfo.in_color_space = JCS_GRAYSCALE;
    cinfo.image_width = resolution.width;
    cinfo.image_height = resolution.height;

    return write_jpeg(image_buffer, dest_ptr, cinfo);
}

/**
 * writes to dest_ptr;
 * returns the size of the image data
 * jpeg data will be stored in dest_ptr, this memory must be freed
 * free() any previously allocated memory (that is if dest_ptr != NULL)
 */
unsigned long write_rgb_jpeg(JSAMPLE *image_buffer, long dest_ptr, resolution_t resolution) {
    struct jpeg_compress_struct cinfo;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    cinfo.image_width = resolution.width;
    cinfo.image_height = resolution.height;
    //printf("dest ptr %x\n", dest_ptr);

    return write_jpeg(image_buffer, dest_ptr, cinfo);
}




/**
 * load  a binary file of uint16 from filename
 * store the size in out_sz
 * return u16 ptr that must be deleted
 */
uint16_t *get_data_u16(std::string filename, int *out_sz) {
    std::ifstream myfile(filename, std::ios::in | std::ios::binary | std::ios::ate);
    size_t sz = myfile.tellg();
    *out_sz = sz;
    Return_IfNot(sz > 0, "load empty file", NULL);
    uint16_t *data1 = new uint16_t[sz];
    if (sz >= 1 && myfile.is_open()) {
        myfile.seekg(0, std::ios::beg);
        myfile.read((char *)data1, sz);
        myfile.close();
    } else {
        *out_sz = 0;
    }
    return data1;
}


/**
 * load  a binary file of uint16 from filename
 * store the size in out_sz
 * return u16 ptr that must be deleted
 */
void put_data_u16(std::string filename, char *data, int out_sz) {
    std::ofstream myfile(filename, std::ios::out | std::ios::binary | std::ios::ate);
    myfile.write(data, out_sz);
    myfile.close();
}



/**
 * load  a binary file of uint16 from filename
 * store the size in out_sz
 * return u16 ptr that must be deleted
 */
uint8_t *get_data_u8(std::string filename, int *out_sz) {


    std::string fn = utils::get_self_path() + "/" + filename;
    // std::cout << " send data: " << fn << " : " << filename << " _ " << std::endl;
    struct stat sb;

    if (stat(fn.c_str(), &sb) == -1) { out_sz = 0; return NULL; }
	if (stat(fn.c_str(), &sb) == -1) { out_sz = 0; return NULL; }

    std::ifstream myfile(fn, std::ios::in | std::ios::binary | std::ios::ate);

    size_t sz = myfile.tellg();
    INFO("ALLOCATE %d bytes", sz);
    uint8_t *data1 = NEWMEM(uint8_t, sz);
    INFO("ALLOCATE %d bytes", sz);
    if (sz >= 1 && myfile.is_open()) {
        myfile.seekg(0, std::ios::beg);
        myfile.read((char *)data1, sz);
        myfile.close();
        *out_sz = sz;
    } else {
        *out_sz = 0;
    }

    return data1;
}

u16 rgba_to_u16(unsigned char *input) {
    //printf("%d ", input[3]);
    u16 result = (((input[0] >>3) << 11)) | (((input[1] >>2) << 5)) | (((input[2] >>3)));
    //u16 result = 0 | ((input[1] >>2) << 5);// | ((input[2] >>3));
    return result;
}


/**
 * reads the png image pointed to by filename and returns
 * memory allocated to display the image.  This does multiple
 * mallocs, seems like the implementation could be optimized
 * Ripped directly from libpng site...
 */
std::unique_ptr<ChitonPngImage> read_png_file(const char *filename, bool keep_rowptr) {
    struct stat stat_buf;
    PANIC_IF(lstat(filename, &stat_buf) < 0, "Error: unable to open png file", EXIT_FILE_OPEN_ERR);

    FILE *fp = fopen((char *)filename, "rb");

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    PANIC_IFNOT(png, "unable to read png", EXIT_FILE_OPEN_ERR);

    png_infop info = png_create_info_struct(png);
    PANIC_IFNOT(info, "unable to create png info", EXIT_FILE_OPEN_ERR);

    if(setjmp(png_jmpbuf(png))) { Panic(8, "png jmp abort?"); }

    png_init_io(png, fp);

    png_read_info(png, info);

    std::unique_ptr<ChitonPngImage> image = std::make_unique<ChitonPngImage>();
    image->width      = png_get_image_width(png, info);
    image->height     = png_get_image_height(png, info);
    image->color_type = png_get_color_type(png, info);
    image->bit_depth  = png_get_bit_depth(png, info);

    // Read any color_type into 8bit depth, RGBA format.
    // See http://www.libpng.org/pub/png/libpng-manual.txt

    if (image->bit_depth == 16) {
        png_set_strip_16(png);
    }

    if (image->color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    }

    // PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
    if(image->color_type == PNG_COLOR_TYPE_GRAY && image->bit_depth < 8) {
        png_set_expand_gray_1_2_4_to_8(png);
    }

    if(png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(png);
    }

    // These color_type don't have an alpha channel then fill it with 0xff.
    if(image->color_type == PNG_COLOR_TYPE_RGB || image->color_type == PNG_COLOR_TYPE_GRAY || image->color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    }

    if(image->color_type == PNG_COLOR_TYPE_GRAY || image->color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
    }

    png_read_update_info(png, info);

    //if (image->row_pointers) { free(image->row_pointers); } //Panic(9, "row pointers init already!"); }


    image->row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * image->height);
    for(int y = 0; y < image->height; y++) {
        image->row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png,info));
    }
    png_read_image(png, image->row_pointers);


    //ChitonImage *cimage = std::make_unique<ChitonImage>(1);
    image->image = (u16*) malloc(image->height * image->width * sizeof(u16));

    // printf("read image [%dx%d]\n", image->width, image->height);
    for(int y = 0; y < image->height; y++) {
        png_bytep row = image->row_pointers[y];
        // loop over each column
        for(int x = 0; x <image->width; x++) {
            // map out the alpha level
            png_bytep px = &(row[x * 4]);
            image->image[(y*image->width)+x] = rgba_to_u16(px);
        }
        if (!keep_rowptr) {
            free(row);
        }
    }
    if (image->row_pointers != NULL && !keep_rowptr) { free(image->row_pointers); }
    

    fclose(fp);
    png_destroy_read_struct(&png, &info, NULL);

    return image;
}

/**
 * returns a single memory allocation of RGBA 4 byte / pixel data for an image
 */
/*
ChitonImage* load_png(const char *filename) {
    struct FBPngImage image = read_png_file(filename);

    //ChitonImage *cimage = std::make_unique<ChitonImage>(1);
    unsigned char *buffer = (unsigned char*) malloc(image.height * image.width);

    for(int y = 0; y < image.height; y++) {
        png_bytep row = image.row_pointers[y];
        // loop over each column
        for(int x = 0; x <image.width; x++) {
            // map out the alpha level
            png_bytep px = &(row[x * 4]);
            //buffer[(y*image.width)+x] = (row[(x*4)]);
            //buffer[(y*image.width)+x+1] = (row[(x*4)+1]);
            //buffer[(y*image.width)+x+2] = (row[(x*4)+2]);
            //buffer[(y*image.width)+x+3] = (row[(x*4)+3]);
            buffer[(y*image.width)+x] = px[3];
        }
        free(row);
    }
    
    cimage->width = image.wi
    return buffer;
}
*/


u16 convert32to16(u32 input) {
    u8 r = input & 0x00FF0000;
    u8 g = input & 0x0000FF00;
    u8 b = input & 0x000000FF;

    u16 result = ((r >>3) << 11) | (((g >>2) << 5)) | ((b >>3));
    //u16 result = (input & 0x1F) << 11 | (input & 0x3F) << 6 | (input & 0x1F);
    return result;
}

std::unique_ptr<ChitonFB> open_fb(char *fb_device) {
    struct fb_var_screeninfo vinfo;
    std::unique_ptr<ChitonFB> fb = std::make_unique<ChitonFB>();
    fb->fd = open(fb_device, O_RDWR);
    // Get variable screen information
    PANIC_IF(ioctl(fb->fd, FBIOGET_VSCREENINFO, &vinfo) == -1, "error reading screen info", EXIT_FB_ERROR);
    fb->width = vinfo.xres;
    fb->height = vinfo.yres;
    fb->bpp  = vinfo.bits_per_pixel;
    fb->sz  = fb->width * fb->height * (fb->bpp / 8);

    PANIC_IF((fb->raw = (u16 *)mmap(0, fb->sz, PROT_READ | PROT_WRITE, MAP_SHARED, fb->fd, 0)) == MAP_FAILED, "unable mmap memort", EXIT_FB_ERROR);
    bzero(fb->raw, fb->sz); 
    fb->buffer1 = NEWMEM(u16, fb->width * fb->height);
    return fb;
}



