#ifndef __IMAGE_H
#define __IMAGE_H 1

#include "color/hsv.h"
#include "util.h"
#include "demos.h"
#include <unistd.h>
#include <string>
#include <jpeglib.h>

//#define PROCESS_IMAGE_CALC

// TODO: clean this up and add to an img namespace

void write_jpeg(JSAMPLE *image_buffer, long dest_ptr_adr, unsigned long *mem_size);

void put_data_u16(std::string filename, char *data, int out_sz);
uint16_t *get_data_u16(std::string filename, int *out_sz);
uint8_t *get_data_u8(std::string filename, int *out_sz);
u8 find_laser(u16 x, u16 y);

void process_image(uint16_t *src, uint16_t *dst, JSAMPLE *jpeg_data, PixelFunc color_fn);
std::vector<xy_t> circle_points(uint16_t center_x, uint16_t center_y, uint16_t radius, uint16_t points);

void map_image(uint16_t *data, int x1, int y1, int x2, int y2, std::function<void(uint16_t)> fn);
std::vector<uint16_t> get_window(uint16_t *data, int x1, int y1, int x2, int y2);

uint16_t calc_rms(std::vector<uint16_t> window, uint16_t mean);
uint16_t calc_temporal_noise(uint16_t std);

float init_mappings();
uint16_t *read_raw_img(std::string image);

void process_statistics(uint16_t *raw_image);

#endif
