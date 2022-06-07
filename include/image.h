#ifndef __IMAGE_H
#define __IMAGE_H 1

#include "demos.h"
#include "util.h"
#include <unistd.h>
#include <string>
#include <jpeglib.h>
#include <png.h>
#include <memory>


typedef struct ChitonPngImage {
    u16 width;
    u16 height;
    png_byte color_type;
    png_byte bit_depth;
    png_bytep *row_pointers;
    u16 *image;
} ChitonPngImage;

typedef struct ChitonImage {
    u16 width;
    u16 height;
    unsigned char *image;
} ChitonImage;

typedef struct ChitonFB {
    void *raw;
    u16 *buffer1;
    int fd;
    int width;
    int height;
    int bpp;
    int sz;
} ChitonFB;

typedef struct FontChar {
    int x;
    int y;
    int width;
    int height;
    int xoffset;
    int yoffset;
} FontChar;



//#define PROCESS_IMAGE_CALC

// TODO: clean this up and add to an img namespace

//void write_jpeg(JSAMPLE *image_buffer, long dest_ptr, unsigned long *mem_size, resolution_t resolution);

void create_slam_jpeg(u16 *__restrict__ slam, u16 slam_idx, JSAMPLE *__restrict__ jpeg_data); 
unsigned long create_color_jpeg(u16 *__restrict__ src, JSAMPLE *__restrict__ jpeg_data);
unsigned long create_greyscale_jpeg(u16 *__restrict__ src, JSAMPLE *__restrict__ jpeg_data, bool chitoncal);
unsigned long write_greyscale_jpeg(JSAMPLE *image_buffer, long dest_ptr, resolution_t resolution);
unsigned long write_rgb_jpeg(JSAMPLE *image_buffer, long dest_ptr, resolution_t resolution);

void put_data_u16(std::string filename, char *data, int out_sz);
uint16_t *get_data_u16(std::string filename, int *out_sz);
uint8_t *get_data_u8(std::string filename, int *out_sz);

void process_image(uint16_t *src, uint16_t *dst, JSAMPLE *jpeg_data, PixelFunc color_fn);
std::vector<xy_t> circle_points(uint16_t center_x, uint16_t center_y, uint16_t radius, uint16_t points);

void map_image(uint16_t *data, int width, int x1, int y1, int x2, int y2, std::function<void(uint16_t)> fn);
std::vector<uint16_t> get_window(uint16_t *data, int width, int x1, int y1, int x2, int y2);

uint16_t calc_rms(std::vector<uint16_t> window, uint16_t mean);
uint16_t calc_temporal_noise(uint16_t std);

float init_mappings();
uint16_t *read_raw_img(std::string image);

void process_statistics(uint16_t *raw_image);

//ChitonPngImage read_png_file(const char *filename);
u16 convert32to16(u32 input);
std::unique_ptr<ChitonPngImage> read_png_file(const char *filename, bool keep_rowptr);
ChitonImage* load_png(const char *filename);
std::unique_ptr<ChitonFB> open_fb(char *fb_device);
std::unique_ptr<FontChar[]> init_small_fonts();
std::unique_ptr<FontChar[]> init_large_fonts();
void fb_write_font_string(const ChitonFB *disp, const char *msg, const xy_t point, const u16 fgcolor, const ChitonPngImage *src, const FontChar *font_offsets);

unsigned long create_planar_jpeg(u16 *__restrict__ src, JSAMPLE *__restrict__ jpeg_data, int *src_ptr, int *dst_ptr, int points);
unsigned long create_planar_color_jpeg(u16 *__restrict__ src, JSAMPLE *__restrict__ jpeg_data, int *src_ptr, int *dst_ptr, int points);
int calc_src_dst_points(int *src_ptr, int *dst_ptr, int width, int height);

#endif
