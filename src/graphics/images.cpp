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
#include <map>
#include <math.h>

#include "demos.h"
#include "core.h"
#include "system.h"
#include "color/hsv.h"
#include "utils.hpp"
#include "math.h"
#include "data.h"

#include <sys/types.h>
#include <sys/stat.h>

static u8   *led_mapping = new u8[CAM_WIDTH*CAM_HEIGHT];
static u8   *mask_mapping = new u8[CAM_WIDTH*CAM_HEIGHT];
static u16  *offset_mapping = new u16[6000*8];
extern RGB8 *hsv_cache;






/**
 * find the laser position on the image
 * atan2 calculates a degree in radians from -PI to +PI based on rotational degrees from center at 0,240
 * we att PI to that to give a value between 0 2PI, we then divide by 2PI to get from 0-1.0, 
 * we subtract 0.025f to rotate the value to align with image center.
 * we then split the value into 12ths and rotate to laser position number by 3
 * we then modulo by 11 using a while loop and force clipping.
 * 
 * speed is not factor here since these values are cached in led_mapping
 * 
 * PURE
 */
s8 find_laser(u16 x, u16 y) {
    float delta_x = x - GOPTS->center_x;
    float delta_y = y - GOPTS->center_y;
    
    float theta_radians = ((float)(atan2f(delta_y, delta_x) + PI) / (PI * 2))-0.025f;
    int result = ((((int)floorf((theta_radians * 11))))) - 3;

    while (result < 0) { result += 11; }
    if (result > 8) { result = 0; }
    return result;
}



/**
 * initialize cache distance and led offset calculations
 * TODO: combine mask & led map into a single u16 map with bitmasking
 * 
 * SIDE EFFECT: mask_mapping and led_mapping are initialized
 */
float init_mappings() {
    TRACE("begin");
    float fill_per = 0.0f;

    for (u16 y=1; y<479; y++) {
        for (u16 x=0; x<CAM_WIDTH; x++) {
            int d = core::calc_distance(GOPTS->center_x, GOPTS->center_y, x, y);
            // mask mapping clips image data not viewable by the lens
            mask_mapping[(y*CAM_WIDTH)+x] = (d < GOPTS->inner_dist || d > GOPTS->outer_dist) ? 0 : 1;
            // led mapping laser position of each pixel
            led_mapping[(y*CAM_WIDTH)+x] = find_laser(x, y);
        }
    }

    TRACE("end");
    return fill_per;
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
void map_image(uint16_t *data, int x1, int y1, int x2, int y2, std::function<void(uint16_t)> fn) {
    uint offset;
    // min/max x/y
    int ya = std::min(y1, y2);
    int yb = std::max(y1, y2);
    int xa = std::min(x1, x2);
    int xb = std::max(x1, x2);

    // iterate over the bounding box
    for (int y = ya; y < yb; y++) {
        // find first pixel in the row (y)
        offset = (y*CAM_WIDTH) + xa;
        // call fn for each pixel in the row (x)
        for (int x = xa; x < xb; x++) {
            fn(data[offset++]);
        }
    }
}


/**
 * get all values of *data inside the bounding box returned as a vector if uint16_t
 */
std::vector<uint16_t> get_window(uint16_t *data, int x1, int y1, int x2, int y2) {
    // allocate memory for result
    int width = MAX(1, core::absdiff(x1, x2));
    int height = MAX(1, core::absdiff(y1, y2));
    std::vector<uint16_t> result(width * height);
    uint16_t *ptr = result.data();

    int ptr_idx = 0;
    map_image(data, x1, y1, x2, y2, [&ptr, &ptr_idx](uint16_t point) {
        ptr[ptr_idx++] = point;
    });

    return result;
}




/**
 * calibrate image *src inplace, as well as update jpeg_data
 */ 
//void process_image(uint16_t *src, uint16_t *dst, JSAMPLE *jpeg_data, std::function<void(uint16_t*, RGB8 *)> color_fn) {
void process_image(uint16_t *src, uint16_t *dst, JSAMPLE *jpeg_data, PixelFunc color_fn) {

    u32 soffset, doffset = 0;
    RGB8 c;

    doffset = 0;
    soffset = 0;

    // find all of the vixel calibration data from the map and pre-load into the calibration array
    VixelCalibration *calibration[10];
    for (int i=1; i<=8; i++) {
        std::map<int, VixelCalibration*>::iterator vit = GOPTS->cal->find(i);
        if (vit != GOPTS->cal->end()) {
            calibration[i] = vit->second;
        }

        // if this is an IR frame, no calibration 
        if (!GOPTS->depth_frame) {
            calibration[i]->gain = 1.0f;
            calibration[i]->offset = 0;
        }
    }

    /*
    // select the color mapping function one time and store a pointer
    // this way we don't have a conditional in the inner loop

    TODO: 
    for (u_int32_t z=0; z<CAM_WIDTH*CAM_HEIGHT; z++) {
        dst[z] = utils::calibrate_pixel(src[soffset++], calibration[3]->gain, calibration[3]->offset);
        color_fn(&dst[z], &c);
        
        jpeg_data[doffset++] = c.r;
        jpeg_data[doffset++] = c.g;
        jpeg_data[doffset++] = c.b;
    }

    return;
    */

    // u32 smooth = 0;
    for (u16 y=1; y<479; y++) {
        for (u16 x=0; x<CAM_WIDTH; x++) {
            s8 led = led_mapping[soffset];

            // inner and outer ring masking
            if (!mask_mapping[soffset]) {
                jpeg_data[doffset++] = 100;
                jpeg_data[doffset++] = 100;
                jpeg_data[doffset++] = 100;
            }
            else if (led > 0) {
                // calibrate
                dst[soffset] = (data::get_chiton_calibration()) ?
                    utils::calibrate_pixel(src[soffset], calibration[led]->gain, calibration[led]->offset) :
                    src[soffset];
                    
                // draw jpeg pixel
                color_fn(&dst[soffset], &c);
                jpeg_data[doffset++] = c.r;
                jpeg_data[doffset++] = c.g;
                jpeg_data[doffset++] = c.b;
            }
            // led is hidden
            else {
                jpeg_data[doffset++] = 100;
                jpeg_data[doffset++] = 100;
                jpeg_data[doffset++] = 50;
            }

            soffset++;
        }
    }
}



/**
 * calibrate image *src inplace, as well as update jpeg_data
 */ 
//void process_image(uint16_t *src, uint16_t *dst, JSAMPLE *jpeg_data, std::function<void(uint16_t*, RGB8 *)> color_fn) {
void process_wide_image(uint16_t *src, uint16_t *dst, JSAMPLE *jpeg_data, PixelFunc color_fn) {

    u32 soffset, doffset = 0;
    RGB8 c;

    doffset = 0;
    soffset = 0;

    // find all of the vixel calibration data from the map and pre-load into the calibration array
    VixelCalibration *calibration[10];
    for (int i=1; i<=8; i++) {
        std::map<int, VixelCalibration*>::iterator vit = GOPTS->cal->find(i);
        if (vit != GOPTS->cal->end()) {
            calibration[i] = vit->second;
        }

        // if this is an IR frame, no calibration 
        if (!GOPTS->depth_frame) {
            calibration[i]->gain = 1.0f;
            calibration[i]->offset = 0;
        }
    }

    /*
    // select the color mapping function one time and store a pointer
    // this way we don't have a conditional in the inner loop

    TODO: 
    for (u_int32_t z=0; z<CAM_WIDTH*CAM_HEIGHT; z++) {
        dst[z] = utils::calibrate_pixel(src[soffset++], calibration[3]->gain, calibration[3]->offset);
        color_fn(&dst[z], &c);
        
        jpeg_data[doffset++] = c.r;
        jpeg_data[doffset++] = c.g;
        jpeg_data[doffset++] = c.b;
    }

    return;
    */

    // u32 smooth = 0;
    for (u16 y=1; y<479; y++) {
        for (u16 x=0; x<CAM_WIDTH; x++) {
            s8 led = led_mapping[soffset];

            // inner and outer ring masking
            if (!mask_mapping[soffset]) {
                jpeg_data[doffset++] = 100;
                jpeg_data[doffset++] = 100;
                jpeg_data[doffset++] = 100;
            }
            else if (led > 0) {
                // calibrate
                dst[soffset] = (data::get_chiton_calibration()) ?
                    utils::calibrate_pixel(src[soffset], calibration[led]->gain, calibration[led]->offset) :
                    src[soffset];
                    
                // draw jpeg pixel
                color_fn(&dst[soffset], &c);
                jpeg_data[doffset++] = c.r;
                jpeg_data[doffset++] = c.g;
                jpeg_data[doffset++] = c.b;
            }
            // led is hidden
            else {
                jpeg_data[doffset++] = 100;
                jpeg_data[doffset++] = 100;
                jpeg_data[doffset++] = 50;
            }

            soffset++;
        }
    }
}




/**
 * writes to jpeg_data_wptr;
 * sets length in mem_size ptr
 * jpeg data will be stored in dest_ptr, this memory must be freed
 * free() any previously allocated memory (that is if dest_ptr != NULL)
 */
void write_jpeg(JSAMPLE *image_buffer, long dest_ptr, unsigned long *mem_size, resolution_t resolution)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    int row_stride;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    //if (dest_ptr != NULL) { free(dest_ptr); }
    //printf("jpeg write [%ld]\n", dest_ptr);
    jpeg_mem_dest(&cinfo, (unsigned char**)dest_ptr, mem_size);

    cinfo.image_width = resolution.width;
    cinfo.image_height = resolution.height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, GOPTS->quality, TRUE);
    //jpeg_set_quality(&cinfo, 96, TRUE);
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
    std::cout << " send data: " << fn << " : " << filename << " _ " << std::endl;
    struct stat sb;

    if (stat(fn.c_str(), &sb) == -1) { out_sz = 0; return NULL; }
	if (stat(fn.c_str(), &sb) == -1) { out_sz = 0; return NULL; }

    std::ifstream myfile(fn, std::ios::in | std::ios::binary | std::ios::ate);

    size_t sz = myfile.tellg();
    uint8_t *data1 = new uint8_t[sz];
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

