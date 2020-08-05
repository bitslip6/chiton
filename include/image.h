#ifndef __IMAGE_H
#define __IMAGE_H 1

#include "color/hsv.h"
#include <unistd.h>
#include <string>
#include <jpeglib.h>

void depth_to_grayscale(uint16_t *value, RGB8 *ptr);
void depth_to_hsv(uint16_t *value, RGB8 *ptr, bool init);
void convert_to_rgb(uint16_t *src, JSAMPLE *jpeg_data, int len, int method);
void write_jpeg(JSAMPLE *image_buffer, unsigned char *dest_ptr, unsigned long *mem_size);
uint16_t *get_data_u16(std::string filename, int *out_sz);
uint8_t *get_data_u8(std::string filename, int *out_sz);

#endif
