#include <stdint.h>
#include <jpeglib.h>
#include <iostream>
#include <string>
#include <color/hsv.h>
#include "demos.h"
#include <stdio.h>

/**
 * convert 12bit value to greyscale
 */
void depth_to_grayscale(uint16_t *value, RGB8 *ptr) {
    uint16_t v = *value >> 4;
    ptr->r = (unsigned char)v;
    ptr->g = ptr->r;
    ptr->b = ptr->r;
}

/**
 * cached hsv to rgb color
 */
void depth_to_hsv(uint16_t *value, RGB8 *ptr, bool init) {
    uint16_t v = *value;

    static RGB8* cache = new RGB8[10000];
    if (init) {
        for (int i=0; i<GOPTS->max_cam_value; i++) {
            HsvToRgb_f(i, GOPTS->max_cam_value, GOPTS->color_rotation, &cache[i]);
        }
        return;
    }
    if (value != NULL && *value <= GOPTS->max_cam_value) {
        ptr->r = cache[v].r;
        ptr->g = cache[v].g;
        ptr->b = cache[v].b;
    }
}

/**
 * convert from raw source to jpeg out 
 * src: raw IR or DEPTH data
 * dst: at least len*3 bytes of allocated memory
 * len: length of src data
 * method: 1 = greyscale, 2=hsv
 */
void convert_to_rgb(uint16_t *src, JSAMPLE *jpeg_data, int len, int method) {
    int i=0, offset=0;;
    RGB8 c;

    if (method == 1) {
        while(i++<len) {
           depth_to_grayscale(&src[i], &c);
           jpeg_data[offset++] = c.r;
           jpeg_data[offset++] = c.g;
           jpeg_data[offset++] = c.b;
        }
    }
    if (method == 2) {
        while(i++<len) {
           depth_to_hsv(&src[i], &c, false);
           jpeg_data[offset++] = c.r;
           jpeg_data[offset++] = c.g;
           jpeg_data[offset++] = c.b;
        }
    }
}


/**
 * writes to jpeg_data_wptr;
 * sets length in mem_size ptr
 * jpeg data will be stored in dest_ptr, this memorty must be freed
 */
void write_jpeg(JSAMPLE *image_buffer, unsigned char *dest_ptr, unsigned long *mem_size)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    int row_stride;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    jpeg_mem_dest(&cinfo, &dest_ptr, mem_size);

    cinfo.image_width = 640;
    cinfo.image_height = 480;
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

/**
 * load  a binary file of uint16 from filename
 * store the size in out_sz
 * return u16 ptr that must be deleted
 */
uint16_t *get_data_u16(std::string filename, int *out_sz) {
    std::ifstream myfile(filename, std::ios::in | std::ios::binary | std::ios::ate);
    size_t sz = myfile.tellg();
    uint16_t *data1 = new uint16_t[sz];
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


/**
 * load  a binary file of uint16 from filename
 * store the size in out_sz
 * return u16 ptr that must be deleted
 */
uint8_t *get_data_u8(std::string filename, int *out_sz)
{
    std::ifstream myfile(filename, std::ios::in | std::ios::binary | std::ios::ate);
    size_t sz = myfile.tellg();
    uint8_t *data1 = new uint8_t[sz];
    if (sz >= 1 && myfile.is_open())
    {
        myfile.seekg(0, std::ios::beg);
        myfile.read((char *)data1, sz);
        myfile.close();
        *out_sz = sz;
    } else {
        *out_sz = 0;
    }
    return data1;
}

