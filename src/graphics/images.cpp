#include <stdint.h>
#include "measurement.h"
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
#include <limits>
#include <new>
#include <stdexcept>
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

    large_fonts[10] = FontChar{0, 200, 16, 22, 0, 0};
    large_fonts[32] = FontChar{0, 100, 16, 22, 0, 0};
    large_fonts[33] = FontChar{370, 0, 6, 24, 6, 1};
    large_fonts[34] = FontChar{339, 56, 11, 9, 3, 1};
    large_fonts[35] = FontChar{0, 56, 18, 22, 0, 3};
    large_fonts[36] = FontChar{110, 0, 16, 30, 1, -1};
    large_fonts[37] = FontChar{398, 0, 21, 24, -1, 1};
    large_fonts[38] = FontChar{419, 0, 19, 24, 0, 1};
    large_fonts[39] = FontChar{505, 34, 5, 9, 6, 1};
    large_fonts[40] = FontChar{5, 0, 11, 33, 4, -1};
    large_fonts[41] = FontChar{16, 0, 12, 33, 2, -1};
    large_fonts[42] = FontChar{281, 56, 16, 15, 1, 1};
    large_fonts[43] = FontChar{264, 56, 17, 17, 0, 7};
    large_fonts[44] = FontChar{297, 56, 10, 13, 3, 17};
    large_fonts[45] = FontChar{388, 56, 11, 5, 3, 13};
    large_fonts[46] = FontChar{381, 56, 7, 7, 5, 18};
    large_fonts[47] = FontChar{145, 0, 17, 28, 0, 1};
    large_fonts[48] = FontChar{488, 34, 17, 22, 0, 3};
    large_fonts[49] = FontChar{345, 34, 16, 22, 1, 3};
    large_fonts[50] = FontChar{361, 34, 15, 22, 1, 3};
    large_fonts[51] = FontChar{376, 34, 15, 22, 2, 3};
    large_fonts[52] = FontChar{391, 34, 18, 22, 0, 3};
    large_fonts[53] = FontChar{409, 34, 15, 22, 1, 3};
    large_fonts[54] = FontChar{424, 34, 16, 22, 1, 3};
    large_fonts[55] = FontChar{440, 34, 16, 22, 1, 3};
    large_fonts[56] = FontChar{456, 34, 16, 22, 1, 3};
    large_fonts[57] = FontChar{472, 34, 16, 22, 1, 3};
    large_fonts[58] = FontChar{257, 56, 7, 18, 5, 7};
    large_fonts[59] = FontChar{388, 0, 10, 24, 2, 7};
    large_fonts[60] = FontChar{18, 56, 15, 21, 1, 5};
    large_fonts[61] = FontChar{324, 56, 15, 10, 1, 10};
    large_fonts[62] = FontChar{33, 56, 15, 21, 2, 5};
    large_fonts[63] = FontChar{376, 0, 12, 24, 4, 1};
    large_fonts[64] = FontChar{90, 0, 20, 30, 0, 1};
    large_fonts[65] = FontChar{438, 0, 20, 22, -1, 3};
    large_fonts[66] = FontChar{458, 0, 15, 22, 1, 3};
    large_fonts[67] = FontChar{473, 0, 16, 22, 0, 3};
    large_fonts[68] = FontChar{489, 0, 17, 22, 1, 3};
    large_fonts[69] = FontChar{0, 34, 14, 22, 2, 3};
    large_fonts[70] = FontChar{14, 34, 14, 22, 2, 3};
    large_fonts[71] = FontChar{28, 34, 17, 22, 0, 3};
    large_fonts[72] = FontChar{45, 34, 16, 22, 1, 3};
    large_fonts[73] = FontChar{61, 34, 14, 22, 2, 3};
    large_fonts[74] = FontChar{75, 34, 13, 22, 2, 3};
    large_fonts[75] = FontChar{88, 34, 16, 22, 1, 3};
    large_fonts[76] = FontChar{104, 34, 14, 22, 3, 3};
    large_fonts[77] = FontChar{118, 34, 17, 22, 0, 3};
    large_fonts[78] = FontChar{135, 34, 16, 22, 1, 3};
    large_fonts[79] = FontChar{151, 34, 18, 22, 0, 3};
    large_fonts[80] = FontChar{169, 34, 15, 22, 1, 3};
    large_fonts[81] = FontChar{126, 0, 19, 28, 0, 3};
    large_fonts[82] = FontChar{184, 34, 16, 22, 2, 3};
    large_fonts[83] = FontChar{200, 34, 16, 22, 1, 3};
    large_fonts[84] = FontChar{216, 34, 17, 22, 0, 3};
    large_fonts[85] = FontChar{233, 34, 16, 22, 1, 3};
    large_fonts[86] = FontChar{249, 34, 20, 22, -1, 3};
    large_fonts[87] = FontChar{269, 34, 19, 22, -1, 3};
    large_fonts[88] = FontChar{288, 34, 20, 22, -1, 3};
    large_fonts[89] = FontChar{308, 34, 21, 22, -2, 3};
    large_fonts[90] = FontChar{329, 34, 16, 22, 1, 3};
    large_fonts[91] = FontChar{28, 0, 10, 32, 4, -1};
    large_fonts[92] = FontChar{162, 0, 16, 28, 1, 1};
    large_fonts[93] = FontChar{38, 0, 10, 32, 3, -1};
    large_fonts[94] = FontChar{307, 56, 17, 12, 0, 3};
    large_fonts[95] = FontChar{399, 56, 20, 5, -1, 26};
    large_fonts[96] = FontChar{350, 56, 13, 9, -1, 1};
    large_fonts[97] = FontChar{48, 56, 15, 18, 1, 7};
    large_fonts[98] = FontChar{193, 0, 16, 24, 1, 1};
    large_fonts[99] = FontChar{63, 56, 14, 18, 1, 7};
    large_fonts[100] = FontChar{209, 0, 15, 24, 1, 1};
    large_fonts[101] = FontChar{77, 56, 16, 18, 1, 7};
    large_fonts[102] = FontChar{224, 0, 18, 24, -1, 1};
    large_fonts[103] = FontChar{242, 0, 17, 24, 0, 7};
    large_fonts[104] = FontChar{259, 0, 15, 24, 1, 1};
    large_fonts[105] = FontChar{274, 0, 15, 24, 2, 1};
    large_fonts[106] = FontChar{48, 0, 14, 30, 1, 1};
    large_fonts[107] = FontChar{289, 0, 16, 24, 2, 1};
    large_fonts[108] = FontChar{305, 0, 15, 24, 2, 1};
    large_fonts[109] = FontChar{93, 56, 17, 18, 0, 7};
    large_fonts[110] = FontChar{110, 56, 15, 18, 1, 7};
    large_fonts[111] = FontChar{125, 56, 17, 18, 0, 7};
    large_fonts[112] = FontChar{320, 0, 16, 24, 1, 7};
    large_fonts[113] = FontChar{178, 0, 15, 25, 1, 6};
    large_fonts[114] = FontChar{142, 56, 15, 18, 2, 7};
    large_fonts[115] = FontChar{157, 56, 14, 18, 2, 7};
    large_fonts[116] = FontChar{336, 0, 16, 24, 0, 1};
    large_fonts[117] = FontChar{171, 56, 15, 18, 1, 7};
    large_fonts[118] = FontChar{186, 56, 18, 18, 0, 7};
    large_fonts[119] = FontChar{204, 56, 20, 18, -1, 7};
    large_fonts[120] = FontChar{224, 56, 18, 18, 0, 7};
    large_fonts[121] = FontChar{352, 0, 18, 24, 0, 7};
    large_fonts[122] = FontChar{242, 56, 15, 18, 2, 7};
    large_fonts[123] = FontChar{62, 0, 14, 30, 1, 1};
    large_fonts[124] = FontChar{0, 0, 5, 34, 7, -3};
    large_fonts[125] = FontChar{76, 0, 14, 30, 2, 1};
    large_fonts[126] = FontChar{363, 56, 18, 9, 0, 11};

    return large_fonts;
}

std::unique_ptr<FontChar[]> init_small_fonts() {
    std::unique_ptr<FontChar[]> small_fonts = std::make_unique<FontChar[]>(128);

    //FB_LARGE_FONT_OFF[32] = FontChar{0, 40, 6, 7, 1, 0};
    //for (int i=65; i<=90; i++) {
    for (int i=0; i<=25; i++) {
        small_fonts[65+i] = FontChar{i*6, 22, 6, 7, 1, 0};
    }
    for (int i=0; i<=25; i++) {
        small_fonts[97+i] = FontChar{i*6, 22, 6, 7, 1, 0};
        //FB_TINY_FONT_OFF[97+i] = FontChar{i*4, 1, 3, 5, 1, 1};
    }
    for (int i=0; i<=9; i++) {
        small_fonts[48+i] = FontChar{i*6, 30, 6, 7, 1, 0};
    }
    small_fonts[58] = FontChar{61, 30, 6, 7, 1, 0};
    small_fonts[45] = FontChar{66, 30, 6, 7, 1, 0};
    small_fonts[124] = FontChar{71, 30, 6, 7, 1, 0};
    small_fonts[34] = FontChar{77, 30, 6, 7, 1, 0};
    small_fonts[39] = FontChar{84, 30, 6, 7, 1, 0};
    small_fonts[91] = FontChar{90, 30, 6, 7, 1, 0};
    small_fonts[93] = FontChar{96, 30, 6, 7, 1, 0};
    small_fonts[40] = FontChar{102, 30, 6, 7, 1, 0};
    small_fonts[41] = FontChar{108, 30, 6, 7, 1, 0};
    small_fonts[43] = FontChar{114, 30, 6, 7, 1, 0};
    small_fonts[44] = FontChar{132, 30, 6, 7, 1, 0};
    small_fonts[45] = FontChar{120, 30, 6, 7, 1, 0};
    small_fonts[46] = FontChar{134, 30, 6, 7, 3, 0};
    small_fonts[47] = FontChar{126, 30, 6, 7, 1, 0};
    small_fonts[92] = FontChar{132, 30, 6, 7, 1, 0};
    small_fonts[92] = FontChar{138, 30, 6, 7, 1, 0};
    small_fonts[60] = FontChar{144, 30, 6, 7, 1, 0};
    small_fonts[62] = FontChar{150, 30, 6, 7, 1, 0};
    small_fonts[37] = FontChar{162, 30, 6, 7, 1, 0};

    return small_fonts;
}

void fb_write_font_string(const ChitonFB *disp, const char *msg, const xy_t point, const u16 fgcolor, const ChitonPngImage *src, const FontChar *font_offsets) {
    if (!src || !src->row_pointers || !disp || !disp->buffer1) return;
    int char_offset = 0;
    for (const unsigned char *c = reinterpret_cast<const unsigned char *>(msg); *c; ++c) {
        FontChar fontc = font_offsets[*c < 128 ? *c : '?'];
        for (int y = 0; y < fontc.height; ++y) {
            int sy = fontc.y + y, dy = y + point.y + fontc.yoffset;
            if (sy < 0 || sy >= src->height || dy < 0 || dy >= disp->height) continue;
            for (int x = 0; x < fontc.width; ++x) {
                int sx = fontc.x + x, dx = x + point.x + char_offset + fontc.xoffset;
                if (sx < 0 || sx >= src->width || dx < 0 || dx >= disp->width) continue;
                if (src->row_pointers[sy][sx * 4 + 3])
                    disp->buffer1[dy * disp->width + dx] = fgcolor;
            }
        }
        char_offset += fontc.width + 1;
        if (point.x + char_offset >= disp->width) break;
    }
}



/**
 * calculate the mode of the last 30 standard deviations
 */
uint16_t calc_temporal_noise(uint16_t std) {
    // Compatibility helper only: no shared cross-request temporal state.
    // Actual temporal measurements now come from explicit per-client histories.
    return std;
}


/**
 * 
 */
uint16_t calc_rms(const std::vector<uint16_t> &window, double mean) {
    return static_cast<uint16_t>(measurement::deviation(window, mean));
}


/**
 * call fn for each pixel in data between the bounding box x1,x2/y1,y2
 */
void map_image(uint16_t *data, int width, int height, size_t capacity,
               int x1, int y1, int x2, int y2, std::function<void(uint16_t)> fn) {
    if (!data || width <= 0 || height <= 0 ||
        static_cast<size_t>(width) * height > capacity ||
        x1 < 0 || x2 < 0 || y1 < 0 || y2 < 0 ||
        x1 > width || x2 > width || y1 > height || y2 > height)
        throw std::invalid_argument("invalid image window");
    for (int y = std::min(y1, y2); y < std::max(y1, y2); ++y)
        for (int x = std::min(x1, x2); x < std::max(x1, x2); ++x)
            fn(data[static_cast<size_t>(y) * width + x]);
}

std::vector<uint16_t> get_window(uint16_t *data, int width, int height, size_t capacity,
                                 int x1, int y1, int x2, int y2) {
    std::vector<uint16_t> result;
    map_image(data, width, height, capacity, x1, y1, x2, y2,
              [&](uint16_t point) { result.push_back(point); });
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
                jpeg_data[(offset*3)+2] = 230;
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

    int calc_src_dst_points(int *src_ptr, int *dst_ptr, int width, int height, size_t capacity) {

        if (!src_ptr || !dst_ptr || width <= 0 || height <= 0) return 0;
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

            for (int i = 0; i < height && static_cast<size_t>(i) < points.size(); ++i) {
                if (static_cast<size_t>(ctr) >= capacity) return ctr;
                if (points[i].x < 0 || points[i].x >= CAM_WIDTH ||
                    points[i].y < 0 || points[i].y >= CAM_HEIGHT) continue;
                dst_ptr[ctr] = x + (i * width);
                src_ptr[ctr] = (points[i].y * CAM_WIDTH) + points[i].x;
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

unsigned long create_planar_color_jpeg(u16 *__restrict__ src, JSAMPLE *__restrict__ jpeg_data, int *src_ptr, int points) {

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
// Loaders return new[] ownership; out_sz is always a byte count.
template<class T>
static T *load_binary(const std::string &filename, int *out_sz) {
    if (!out_sz) return nullptr;
    *out_sz = 0;
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) return nullptr;
    const auto end = file.tellg();
    // Bound externally supplied assets and reject truncated 16-bit samples.
    if (end <= 0 || end > 64 * 1024 * 1024 || end % sizeof(T) != 0) return nullptr;
    const size_t bytes = static_cast<size_t>(end);
    std::unique_ptr<T[]> data(new (std::nothrow) T[bytes / sizeof(T)]);
    if (!data) return nullptr;
    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char *>(data.get()), bytes)) return nullptr;
    *out_sz = static_cast<int>(bytes);
    return data.release();
}
uint16_t *get_data_u16(std::string filename, int *out_sz) {
    return load_binary<uint16_t>(filename, out_sz);
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
    return load_binary<uint8_t>(filename, out_sz);
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
ChitonPngImage::~ChitonPngImage() {
    if (row_pointers) {
        for (int y = 0; y < height; ++y) free(row_pointers[y]);
        free(row_pointers);
    }
    free(image);
}

std::unique_ptr<ChitonPngImage> read_png_file(const char *filename, bool keep_rowptr) {
    png_image png{};
    png.version = PNG_IMAGE_VERSION;
    std::unique_ptr<png_image, decltype(&png_image_free)> png_cleanup(&png, png_image_free);
    if (!png_image_begin_read_from_file(&png, filename)) {
        std::cerr << "Chiton: cannot read optional PNG " << filename << ": " << png.message << std::endl;
        return nullptr;
    }
    // Reject oversized/malformed optional images without terminating the app.
    if (!png.width || !png.height || png.width > 4096 || png.height > 4096) {
        std::cerr << "Chiton: unsupported PNG dimensions: " << filename << std::endl;
        return nullptr;
    }
    png.format = PNG_FORMAT_RGBA;
    std::vector<png_byte> rgba(PNG_IMAGE_SIZE(png));
    if (!png_image_finish_read(&png, nullptr, rgba.data(), 0, nullptr)) {
        std::cerr << "Chiton: invalid optional PNG " << filename << ": " << png.message << std::endl;
        return nullptr;
    }
    auto image = std::make_unique<ChitonPngImage>();
    image->width = png.width;
    image->height = png.height;
    image->color_type = PNG_COLOR_TYPE_RGBA;
    image->bit_depth = 8;
    image->image = static_cast<u16 *>(malloc(size_t(image->width) * image->height * sizeof(u16)));
    if (!image->image) throw std::bad_alloc();
    if (keep_rowptr) {
        image->row_pointers = static_cast<png_bytep *>(calloc(image->height, sizeof(png_bytep)));
        if (!image->row_pointers) throw std::bad_alloc();
    }
    for (int y = 0; y < image->height; ++y) {
        auto row = rgba.data() + size_t(y) * image->width * 4;
        if (keep_rowptr) {
            image->row_pointers[y] = static_cast<png_bytep>(malloc(image->width * 4));
            if (!image->row_pointers[y]) throw std::bad_alloc();
            memcpy(image->row_pointers[y], row, image->width * 4);
        }
        for (int x = 0; x < image->width; ++x)
            image->image[y * image->width + x] = rgba_to_u16(row + x * 4);
    }
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

