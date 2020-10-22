#ifndef __IMAGE_H
#define __IMAGE_H 1

#include "color/hsv.h"
#include "util.h"
#include <unistd.h>
#include <string>
#include <jpeglib.h>

void depth_to_grayscale(uint16_t *value, RGB8 *ptr);
void depth_to_hsv(uint16_t *value, RGB8 *ptr, bool init);
bool convert_to_rgb(uint16_t *src, JSAMPLE *jpeg_data, int len, int method);
void write_jpeg(JSAMPLE *image_buffer, long dest_ptr_adr, unsigned long *mem_size);
uint16_t *get_data_u16(std::string filename, int *out_sz);
uint8_t *get_data_u8(std::string filename, int *out_sz);
//u8 find_laser(u16 x, u16 y);
u8 find_laser(u16 x, u16 y);

bool convert_to_rgb(uint16_t *src, JSAMPLE *jpeg_data, int len, int method, std::function<int(int, int, int, int)> calc_dist);
//void process_image(uint16_t *src, uint16_t *dst, JSAMPLE *jpeg_data, std::function<int(int, int, int, int)> calc_dist);
void process_image(uint16_t *src, uint16_t *dst, JSAMPLE *jpeg_data);
float init_mappings();

void process_statistic(uint16_t *raw_image, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

#endif
