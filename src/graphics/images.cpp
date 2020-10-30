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
#include <map>
#include <math.h>

#include "system.h"
#include "color/hsv.h"
#include "utils.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#define PI 3.1415926f

static u8  *led_mapping = new u8[640*480];
static u8  *mask_mapping = new u8[640*480];
static u16 *offset_mapping = new u16[6000*8];
//std::list<float> noise;

/**
 * calculate distance between 2 2d points using sqrt (no cache)
 */
int calc_distance2(int x1, int y1, int x2, int y2) {
    float dist = std::sqrt((float)(x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
    return (int)dist;
}

/**
 * find the laser position on the image
 * atan2 calculates a degree in radians from -PI to +PI based on rotational degrees from center at 0,240
 * we att PI to that to give a value between 0 2PI, we then divide by 2PI to get from 0-1.0, 
 * we subtract 0.025f to rotate the value to align with image center.
 * we then split the value into 12ths and rotate to laser position number by 3
 * we then modulo by 11 using a while loop and force clipping.
 * 
 * speed is not factor here since these values are cached in led_mapping
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
 * todo: combine mask & led map into a single u16 map with bitmasking
 */
float init_mappings() {
    float fill_per = 0.0f;

    for (u16 y=1; y<479; y++) {
        for (u16 x=0; x<640; x++) {
            int d = calc_distance2(GOPTS->center_x, GOPTS->center_y, x, y);
            // mask mapping clips image data not viewable by the lens
            mask_mapping[(y*640)+x] = (d < 125 || d > 300) ? 0 : 1;
            // led mapping laser position of each pixel
            led_mapping[(y*640)+x] = find_laser(x, y);
        }
    }

    return fill_per;
}


/**
 * convert 12bit value to greyscale
 */
void depth_to_grayscale(uint16_t *value, RGB8 *ptr) {
    uint16_t v = *value >> 4; // clip 12 bit values to 8 bit values
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
        GOPTS->min_cam_value;
        memset(cache, 0, 10000 * sizeof(RGB8));
        for (uint16_t i=GOPTS->min_cam_value; i<GOPTS->max_cam_value; i++) {
            HsvToRgb_f(i, GOPTS->min_cam_value, GOPTS->max_cam_value, GOPTS->color_rotation, &cache[i]);
        }
        return;
    }
    if (value != NULL) {
        if (v <= GOPTS->max_cam_value) {
            ptr->r = cache[v].r;
            ptr->g = cache[v].g;
            ptr->b = cache[v].b;
        } else {
            ptr->r = 0;
            ptr->g = 0;
            ptr->b = 0;
        }
    }
}

float calc_temporal_noise(float std)
{
    static uint32_t frames=0; 
    static std::vector<float> noise(20);
    noise[ frames++ % noise.size() ] = std;
    
    if(frames>noise.size()*3)
      frames=noise.size()+1;

    if (frames > noise.size() ) {
         std::cout << std::setprecision(2) << std::fixed;
         std::copy(noise.begin(), noise.end(), std::ostream_iterator<float>(std::cout, " "));
         std::cout << std::endl;
         
         std::vector<float> noise_dup=noise;
         std::sort(noise_dup.begin(),noise_dup.end());

         std::copy(noise_dup.begin(), noise_dup.end(), std::ostream_iterator<float>(std::cout, " "));
         std::cout << std::endl;
        return noise_dup[noise_dup.size()/2];
    }

    return 0;
}

float calc_rms(std::vector<float> window, float mean) {
    float sum = 0;
    std::for_each(window.begin(), window.end(), [&](float i) { sum += pow((i - mean), 2);} );
    return sqrt(sum/window.size());
}

void process_statistics(uint16_t *raw_image) {
	int total = 0;
    float mean = 0, std_dev = 0;

    std::vector<float> window;

    std::cout << GOPTS->stats_x1 << "," << GOPTS->stats_y1 << " / " << GOPTS->stats_x2 << "," << GOPTS->stats_y2 << std::endl;
    std::cout << std::fixed << std::setw(8) << std::setprecision(2);


	for (int j = std::min(GOPTS->stats_y1,GOPTS->stats_y2); j <= std::max(GOPTS->stats_y1,GOPTS->stats_y2); j++)
	{
		for (int i = std::min(GOPTS->stats_x1,GOPTS->stats_x2); i <= std::max(GOPTS->stats_x1,GOPTS->stats_x2); i++)
		{
                   float pixel = raw_image[(640 * j) + i];
                   window.push_back(pixel);
                   std::cout << pixel << " ";
		}
		std::cout << std::endl;
	}
    
    GOPTS->area_mean_value = calc_mean(window);
    GOPTS->area_std_deviation = calc_std_deviation(window, GOPTS->area_mean_value);
    GOPTS->area_temporal_noise = calc_temporal_noise(GOPTS->area_std_deviation);
    GOPTS->area_rms_value = calc_rms(window, GOPTS->area_mean_value);
    std::cout << "area_mean: " << GOPTS->area_mean_value << std::endl;
    std::cout << "area_std dev: " << GOPTS->area_std_deviation << std::endl;
    std::cout << "area_tnoise: " << GOPTS->area_temporal_noise << std::endl;
    std::cout << "area_rms: " << GOPTS->area_rms_value << std::endl;
}


/**
 * calibrate image *src inplace, as well as update jpeg_data
 */ 
void process_image(uint16_t *src, uint16_t *dst, JSAMPLE *jpeg_data) {

    u32 soffset, doffset = 0;
    u16 center_x = GOPTS->center_x;
    u16 center_y = GOPTS->center_y;
    RGB8 c;

    doffset = 640*3;
    soffset = 640;

    // find all of the vixel calibration data from the map and pre-load into the calibration array
    VixelCalibration *calibration[10];
    for (int i=1; i<=8; i++) {
        std::map<int, VixelCalibration*>::iterator vit = GOPTS->cal->find(i);
        calibration[i] = vit->second;
    }

    u32 smooth = 0;
    for (u16 y=1; y<479; y++) {
        for (u16 x=0; x<640; x++) {
            s8 led = led_mapping[soffset];

            if (!mask_mapping[soffset]) {
                jpeg_data[doffset++] = 0;
                jpeg_data[doffset++] = 100;
                jpeg_data[doffset++] = 100;
            }
            else if (led > 0) {
                //dst[soffset] = (src[soffset] * gain) + offset;
                //dst[soffset] = led * 15;
                //int l2 = led -2;
                //if ( l2 < 1 || l2 > 8) { printf("led=%d\n", l2); };
                dst[soffset] = (GOPTS->chitoncal) ?
                    utils::calibrate_pixel(src[soffset], calibration[led]->gain, calibration[led]->offset) :
                    src[soffset];
                    
                depth_to_hsv(&dst[soffset], &c, false);

                jpeg_data[doffset++] = c.r;
                jpeg_data[doffset++] = c.g;
                jpeg_data[doffset++] = c.b;
            }
            else {
                jpeg_data[doffset++] = 100;
                jpeg_data[doffset++] = 100;
                jpeg_data[doffset++] = 0;
            }

            soffset++;
        }
    }
}




/**
 * convert from raw source to jpeg out 
 * src: raw IR or DEPTH data
 * dst: at least len*3 bytes of allocated memory
 * len: length of src data
 * method: 1 = greyscale, 2=hsv
 */
bool convert_to_rgb(uint16_t *src, JSAMPLE *jpeg_data, int len, int method, std::function<int(int, int, int, int)> calc_dist) {
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

    if (method == 3) {
        int total1 = 0;
        int total2 = 0;
        int total3 = 0;
        int diff1=0;
        int diff2=0;
        int off=0;
        for (int y=0; y<480; y++) {
            
            //total1=0;
            //int cnt=0;
            for (int x=0; x<640; x++) {

                int d = calc_dist(312, 174, x, y);
                if (d < 125 || d > 300) {
                    jpeg_data[offset++] = 0;
                    jpeg_data[offset++] = 0;
                    jpeg_data[offset++] = 0;
                } else {
                    depth_to_hsv(&src[off], &c, false);
                    jpeg_data[offset++] = c.r;
                    jpeg_data[offset++] = c.g;
                    jpeg_data[offset++] = c.b;
                }
                off++;
            }

/*
            if (total3 > 0) {
                diff1 = total3-total1;
            }
            
            if (std::abs(diff2-diff1) > 30000) {
                if (diff1 > cnt*350) {
                    printf("BAD block start: Y:%d [%d(%d)] [%d] = %d\n", y, diff1, diff2, cnt, (diff1/cnt));
                    return false;
                }
                else if (diff1 < (cnt*-350)) {
                    printf("BAD block end: Y:%d [%d(%d)] [%d] = %d\n", y, diff1, diff2, cnt, (diff1/cnt));
                    return false;
                }
            }
            total3=total2;
            total2=total1;
            diff2 = diff1;
            */
        }
        //printf("GOOD FRAME!\n");
    }


    return true;
}



/**
 * writes to jpeg_data_wptr;
 * sets length in mem_size ptr
 * jpeg data will be stored in dest_ptr, this memory must be freed
 * free() any previously allocated memory (that is if dest_ptr != NULL)
 */
void write_jpeg(JSAMPLE *image_buffer, long dest_ptr, unsigned long *mem_size)
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

    cinfo.image_width = 640;
    cinfo.image_height = 480;
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
uint8_t *get_data_u8(std::string filename, int *out_sz) {


	std::string fn = utils::get_self_path() + "/" + filename;
	struct stat sb;

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

