/******************************************************************************
 *
 * @file            main.cpp
 *
 * @brief           Main Chiton application front-end.
 *
 * @details
 *
 * @author(s)       Stephen Stauts, Cory Marsh
 *
 * @created         06/15/2020   (MM/DD/YYYY)
 *
 * @copyright       © 2020 SWYM, LLC and BitSlip6. All Rights Reserved.
 *
 *****************************************************************************/

//-----------------------------------------------------------------------------
//  Includes
//-----------------------------------------------------------------------------

#include <sys/cdefs.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <thread>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <climits>
#include <cstring>
#include <algorithm>
#include <functional>
#include <numeric>
#include <mutex>
#include <unistd.h>
#include <zlib.h>
#include <sys/ioctl.h>

#include "fplus/fplus.hpp"
#include <jpeglib.h>
// #include "app.hpp"
#include "image.h"
#include "util.h"
#include "utils.hpp"
#include "demos.h"
#include "system.h"
#include "system.hpp"
#include "webserver.h"
#include "shapes.h"
#include "data.h"
#include "camera.h"
#include "calibration.h"
#include "zlib.h"
#include "CL/cl.h"
#include "rover.h"
#include "color/hsv.h"

#include <stdio.h>
#include <string.h>
#include <zlib.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <execinfo.h>

#include <iostream>
#include <fstream>
#include <png.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>

#include <aditof/camera.h>
#include <aditof/device_interface.h>
#include <aditof/frame.h>
#include <aditof/system.h>

#include "system.h"
#define PORT 80
#define CHITON_VER 94


// TODO: find a better place for these...
RGB8 *hsv_cache;
//uint16_t *calibrated = NULL;
uint16_t *raw_data1 = NULL;
unsigned char *jpeg_ptr = NULL;
unsigned char *jpeg_rgb = NULL;
unsigned long jpeg_len = 0;

int *src_ptr = NULL;
int *dst_ptr = NULL;

// is calibration enabled
/*
int vixel_index = 0;
int autocal_samples = 0;
std::vector<calibration_status> vixel_autocal_status;
double **autocalibrated_values;
calibration_status autocal_status = calibration_status::NONE;
int n_samples = 100;
std::vector<xy_t> autocal_points;
*/

// TODO: move to common file for testing.  remove dead code
GlobalOpts *GOPTS = NULL;
std::shared_ptr<aditof::Camera> camera = NULL;


/**
 * get a function pointer from a dl loaded library
 */
void *getFunctionPointer(void *lib, const char *funcName)
{
    // Get the function pointer to the function
    void *fptr = dlsym(lib, funcName);
    if (!fptr)
    {
        fprintf(stderr, "Could not get function pointer for %s\n  error is: %s\n\n", funcName, dlerror());
        return NULL;
    }
    return fptr;
}

/**
 * @brief  UNUSED.  Zlib default compression.
 * 
 * @param in_data pointer to allocated memory to compress
 * @param in_data_size  size of in_data
 * @param out_data  pointer to allocated memory to store compressed data
 * @return int size of compressed data
 */
int compress_memory(u8 *in_data, size_t in_data_size, u8 *out_data)
{
    z_stream strm = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    strm.zalloc = 0;
    strm.zfree = 0;
    strm.next_in = in_data;
    strm.avail_in = strm.total_in = in_data_size;
    strm.next_out = out_data;
    strm.avail_out = strm.total_out = in_data_size;
    int deflate_res = Z_OK;

    deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    deflate_res = deflate(&strm, Z_FINISH);
    WARN_IFNOT(deflate_res == Z_OK, "error compressing data");
    int sz = strm.total_out;
    deflateEnd(&strm);
    return sz;
}


/**
 * @brief draw a line from inner_dist-1 to outer_dist+2 along angle radian
 * @todo move to shapes
 * 
 * @param radians angle in radian to draw line. 0 is top, counter clockwise to 2pi
 * @return std::vector<Point2D_t> of points along line
 */
std::vector<Point2D_t> draw_angle_line(float radians) {

    xyf_t p1 = gfx::calculate_circle_point_float(GOPTS->inner_dist-1, radians);
    xyf_t p2 = gfx::calculate_circle_point_float(GOPTS->outer_dist+2, radians);

    float x1 = p1.x + GOPTS->center_x;
    float x2 = p2.x + GOPTS->center_x;
    float y1 = p1.y + GOPTS->center_y;
    float y2 = p2.y + GOPTS->center_y;

    linef_t line2 = {x1, y1, x2, y2};

    std::vector<Point2D_t> points = gfx::line_to_point_vector(line2, 200);

    return points;
} 


/**
 * @brief plot a line in image data of color along each point of points
 * @todo move to shapes
 * 
 * @param points 
 * @param image_data 
 * @param color 
 */

void plot_points(std::vector<Point2D_t> points, uint16_t *image_data, uint16_t color) {
    for (Point2D_t p : points) {
        int index = (p.y * CAM_WIDTH) + p.x;
        image_data[index] = color;
    }
}


bool sample_line(std::vector<Point2D_t> points, int physical_dist, float radian) {

    const int samples = 20;
    static int last_sum = 1;
    static int this_sum = 0;
    int max_diff = 0;
    uint16_t *calibrated = data::get_calibrated_ptr();
    calibration::point *calibration = calibration::get_calibration();
    float *vert_angle = calibration::get_vert_angle();

    u16 *data = NULL;//NEWMEM(u16, points.size() * samples*2);
    int totals[samples] = {0};



    // direct return...
    for (Point2D_t p : points) {
        int index = (p.y * CAM_WIDTH) + p.x;
        //int avg_sample = data[idx++] / count;
        if (calibrated[index] > 250) {
            uint16_t calc_dist = physical_dist / sinf(1.571f - vert_angle[index]);
            if (physical_dist < 1024) {
                // if we have a sample for this same distance, then average them
                if (calibration[index].near_dist != 0 && calibration[index].near_dist == calc_dist) {
                    calibration[index].near_point += calibrated[index];
                    calibration[index].near_point /= 2;
                } else {
                    calibration[index].near_dist = calc_dist;
                    calibration[index].near_point = calibrated[index];
                }
            } else {
                if (calibration[index].far_dist != 0 && calibration[index].far_dist == calc_dist) {
                    calibration[index].far_point += calibrated[index];
                    calibration[index].far_point /= 2;
                } else {
                    calibration[index].far_dist = calc_dist;
                    calibration[index].far_point = calibrated[index];
                }
            }
            calibration[index].center_angle = radian;
        }
    }

    return true;





    int slice = 0;
    // take samples along the line, wait 33 ms between samples for frame capture to update
    for (int i=0; i<samples; i++) {
        int idx = i*points.size();
        totals[i] = 0;
        for (Point2D_t p : points) {
            int index = (p.y * CAM_WIDTH) + p.x;
            data[idx++] = calibrated[index];
            totals[i] += calibrated[index];
        }

        totals[i] /= points.size();
        // wait 1 frame...
        usleep(33333);
    }


    // find min/max samples
    int max = 0;
    int min = 9000;
    for (int i=0; i<samples; i++) {
        if (totals[i] > max) { max = totals[i]; }
        if (totals[i] < min) { min = totals[i]; }
    }


    // count samples closer to min/max
    int min_total, max_total = 0;
    for (int i=0; i<samples; i++) {
        int diff1 = abs(totals[i] - max);
        int diff2 = abs(totals[i] - min);
        if (diff1 < diff2) {
            max_total++;
        } else {
            min_total++;
        }
    }


    // include samples closer to min/max
    int count = 0;
    for (int i=0; i<samples; i++) {
        int diff1 = std::abs(totals[i] - max);
        int diff2 = std::abs(totals[i] - min);
        // INFO("diff1: %d, diff2: %d, total: %d, min: %d, max: %d, points: %d", diff1, diff2, totals[i], min, max, points.size());
        if (max_total > min_total) {
            if (diff1 < diff2) {
                int idx = samples*points.size();
                for (Point2D_t p : points) {
                    int index = (p.y * CAM_WIDTH) + p.x;
                    data[idx++] = calibrated[index];
                }
                count++;
            }
            else if (diff1 > diff2) {
                int idx = samples*points.size();
                for (Point2D_t p : points) {
                    int index = (p.y * CAM_WIDTH) + p.x;
                    data[idx++] = calibrated[index];
                }
                count++;
            }
        }
    }
    int sum_diff = std::abs(last_sum - min);
    INFO("samples [%d], max [%d], min [%d], last [%d/%d]count [%d]", samples, max, min, last_sum, sum_diff, count);

    // take the average of the samples
    int idx = samples*points.size();
    for (Point2D_t p : points) {
        int index = (p.y * CAM_WIDTH) + p.x;
        int avg_sample = data[idx++] / count;
        if (avg_sample > 500) {
            if (physical_dist < 1024) {
                calibration[index].near_dist = physical_dist / sinf(1.571f - vert_angle[index]);
                calibration[index].near_point = avg_sample;
            } else {
                calibration[index].far_dist = physical_dist / sinf(1.571f - vert_angle[index]);
                calibration[index].far_point = avg_sample;
            }
            calibration[index].center_angle = radian;
        }
    }

    //free(data);
    // sample is not within tolerance
    /*
    if (sum_diff > 4)  {
        WARN ("FAILED SAMPLE - last num");
        if (last_sum == 1 ) { last_sum = min; }
        // adjust last sum by 20% of the difference
        last_sum -= ((last_sum - min)*0.2f); 
        return false;
    }
    */
    last_sum = min;
    bool result = (max - min) < 5;
    if (!result) { WARN("FAILED SAMPLE, min/max"); }
    return result;
}


/**
 * camera functions
 * params vector of url parts 
 * socket a connected socket
 */
int calibrate_fn(std::vector<std::string> params, int socket)
{
    static int last_sum = 1;
    static int this_sum = 0;
    static u16 all_points[1024];
    std::string response = "NA";

    //INFO("calibrate_fn");

    if (params[0].compare(std::string("sample2")) == 0) {

        uint16_t *calibrated = data::get_calibrated_ptr();
        calibration::point *calibration = calibration::get_calibration();
        int physical_dist = utils::clamp(params[2], 400, 3000);
        int angle_idx = std::stoi(params[3]);
        float radian = std::stof(params[4]) / 10000.0f;
        GOPTS->calibrate_angle = radian;

        WARN("calibrating at angle %d @%d, radian(%f)", angle_idx, physical_dist, radian);

        std::vector<Point2D_t> points = draw_angle_line(radian);


        std::string raw = "HTTP/1.1 200 OK\nContent-Type: application/json\nAccess-Control-Allow-Origin: *\nContent-Length: 0\n\n";
        float *vert_angle = calibration::get_vert_angle();
        u16* dist = calibration::get_pixel_dist();

        sample_line(points, physical_dist, radian);


/*
        this_sum = 0;
        int max_diff = 0;
        int ctr = 0;
        for (Point2D_t p : points) {
            int index = (p.y * CAM_WIDTH) + p.x;
            // center angle
            if (dist[index] == 232) {
                if (calibrated[index] > physical_dist) {
                    PANIC("calibrating with chiton calibration enagled.  enable smoothing and disable chiton cal");
                }
            }
          
            // near calibration point
            if (physical_dist < 1024) {
                // calculate the hypotenuse (physical distance from the camera at vertical angle)
                // hypotenuse = b / sin(ang b);  vert_angle is angle a, so angle b is 90(1.571) - angle a.  physical_dist is b
                calibration[index].near_dist = physical_dist / sinf(1.571f - vert_angle[index]);
                calibration[index].near_point = calibrated[index];
                INFO("<1024  index %dx%d  -  (%d) / (%d)", p.x, p.y, calibration[index].near_dist, calibrated[index]);
            } else {
                calibration[index].far_dist = physical_dist / sinf(1.571f - vert_angle[index]);
                calibration[index].far_point = calibrated[index];
                INFO(">1024  index %dx%d  -  (%d) / (%d)", p.x, p.y, calibration[index].far_dist, calibrated[index]);
            }
            calibration[index].center_angle = radian;
            this_sum += calibrated[index];

            max_diff = std::max(max_diff, abs(calibrated[index] - all_points[ctr++]));
            all_points[ctr-1] = calibrated[index];
        }

        if (last_sum != 1) {
            int five_percent = this_sum * 0.03;
            int diff = std::abs(this_sum - last_sum);
            if (diff < five_percent) {
                INFO("calibration is good");
                response = "{\"status\":\"ok\", \"value\": " + std::to_string(this_sum) + ", \"max_diff\": " + std::to_string(max_diff) + "}";
            } else {
                PANIC("calibration is bad");
                response = "{\"status\":\"fail\", \"value\": " + std::to_string(this_sum) + "}";
            }
        }
        last_sum = this_sum;
        */
        web::send_headers(raw, socket);

        
        /*
        for (Point2D_t p : points) {
            int index = (p.y * CAM_WIDTH) + p.x;
            calibrated[index] = 100;
        }
        */
    }
    else if (params[0].compare(std::string("save")) == 0) {
        WARN("saving calibration");
        utils::write_calibration(calibration::get_calibration(), "/tmp/calibration.bin", CAM_WIDTH * CAM_HEIGHT);
    }
    else if (params[0].compare(std::string("load")) == 0) {
        WARN("loading calibration");
        std::string file = "calibration2.bin";
        utils::read_calibration(file);
    }
    else if (params[0].compare(std::string("process")) == 0) {
        WARN("process calibration");
        calibration::process_calibration();
    }
    else if (params[0].compare(std::string("infill")) == 0) {
        WARN("infill calibration begin");
        calibration::fill_missing_calibration_data();
        WARN("infill calibration complete");
    } else if (params[0].compare(std::string("smooth")) == 0) {
        WARN("calibration smooth begin");
        calibration::point *p = calibration::get_calibration();
        int ksz = 1;
        gfx::apply_kernel(ksz, gfx::make_gausian_kernel(ksz), p);
        WARN("calibration smooth complete");
    }


    std::string raw = "HTTP/1.1 200 OK\nContent-Type: application/json; charset=utf-8\nAccess-Control-Allow-Origin: *\nContent-Length: "+std::to_string(response.length())+"\n\n"+response;
    web::send_headers(raw, socket);

    return 200;
}



/**
 * camera functions
 * params vector of url parts 
 * socket a connected socket
 */
int camera_fn(std::vector<std::string> params, int socket)
{

    //INFO("camera_fn");
    if (params[0].compare(std::string("enable")) == 0)
    {
        WARN("camera enable\n");
        utils::adi_call([&]()
                        {
                            printf("camera start\n");
                            return camera->start();
                        });
        GOPTS->enable = true;
        return 200;
    }

    if (params[0].compare(std::string("disable")) == 0)
    {
        WARN("camera disable\n");
        GOPTS->enable = false;
        utils::adi_call([&]()
                        {
                            printf("camera stop\n");
                            return camera->stop();
                        });
        return 200;
    }

    if (!data::is_ready())
    {
        return 404;
    }

    if (params[0].compare(std::string("raw")) == 0)
    {
        DEBUG("send raw image");
        uint16_t *calibrated = data::get_calibrated_ptr();
        bool okay = web::send_data(socket, std::string("application/octet-stream"), (unsigned char *)calibrated, data::get_frame_size_bytes());

        WARN_IFNOT(okay, "unable to send raw data");
        return okay ? 0 : -1;
    }
    else if (params[0].compare(std::string("pointcloud")) == 0)
    {
        TRACE("send pointcloud data [%d]", data::get_point_cloud_size());
        //float *pc = data::get_point_cloud_f();
        //bool okay = web::send_data(socket, std::string("application/octet-stream"), (unsigned char *)pc, data::get_point_cloud_size() * 4);
        GOPTS->color_mode = 4;
        GOPTS->frame_request = true;
        utils::Wait_Until([]() { return GOPTS->frame_request == false; }, 1);
        
        s16 *pc = data::get_point_cloud();
        //u8 *data = data::get_point_cloud_compressed();
        //int size = compress_memory((u8*)pc, data::get_point_cloud_size() * 4, data);

        bool okay = web::send_data(socket, std::string("application/octet-stream"), (unsigned char *)pc, data::get_point_cloud_size() * 2);
        //bool okay = web::send_precompressed_data(socket, std::string("application/octet-stream"), (unsigned char *)data, size);
        WARN_IFNOT(okay, "unable to send point cloud data");
        return okay ? 0 : -1;
    }
    else if (params[0].compare(std::string("pointcloud_obj")) == 0)
    {
        int tmp = GOPTS->color_mode;
        GOPTS->color_mode = 4;
        GOPTS->frame_request = true;
        utils::Wait_Until([]() { return GOPTS->frame_request == false; }, 2);
        GOPTS->frame_request = true;
        utils::Wait_Until([]() { return GOPTS->frame_request == false; }, 3);

        
        s16 *pc = data::get_point_cloud();
        int sz = data::get_point_cloud_size();
        int numsz = sz * 8;
        INFO("1 send pointcloud obj data [%d]", sz);
        u8 *data = NEWMEM(u8, numsz);

        
        int off=0;
        for (int i=0; i<sz && off<numsz; i+=4) {
            float x = pc[i] / 6000.0f;
            float y = pc[i+1] / 6000.0f;
            float z =  pc[i+2] / 6000.0f;
            off += sprintf((char *)data+off, "v %.4f %.4f %.4f\n", x, y, z);
        }
        INFO("sending obj file of [%d] bytes", off);
        
        bool okay = web::send_data(socket, std::string("text/plain"), (unsigned char *)data, off);
        WARN_IFNOT(okay, "unable to send obj point cloud data");
        free(data);
        GOPTS->color_mode = tmp;
        return okay ? 0 : -1;
    }

    else if (params[0].compare(std::string("image")) == 0)
    {
        char timer_name[128];
        snprintf(timer_name, 128, "send [%lu] jpeg bytes", jpeg_len);
        bool okay = false;
        utils::Wait_Until([]() { return GOPTS->waiting != 3; }, 4);
        GOPTS->waiting = 2;
        //TIMER2(timer_name, 30, okay = web::send_data(socket, std::string("image/jpeg"), jpeg_ptr, jpeg_len));
        okay = web::send_data(socket, std::string("image/jpeg"), jpeg_ptr, jpeg_len);
        GOPTS->waiting = 0;

        WARN_IFNOT(okay, "unable to send jpeg data");
        return okay ? 0 : -1;
    }

    else if (params[0].compare(std::string("dimension")) == 0)
    {

        INFO("dimension! [%d] [%d]", GOPTS->center_x, GOPTS->center_y);
        std::string data = "{\"inner_dist\":" + std::to_string(GOPTS->inner_dist) + ", \"outer_dist\":"
         + std::to_string(GOPTS->outer_dist) + ", \"center_x\": " + std::to_string(GOPTS->center_x) + 
         ", \"center_y\": " + std::to_string(GOPTS->center_y) + "}\n\n";
        bool okay = web::send_data(socket, std::string("text/plain"), (unsigned char *)data.c_str(), data.length());

        WARN_IFNOT(okay, "unable to send image dimension data");
        return okay ? 0 : -1;
    }
    else if (params[0].compare(std::string("wall_radian")) == 0)
    {
        float g_angle = calibration::find_closest_wall_radian(raw_data1, 600);
        INFO("wall radian: %f", g_angle);
    }
    /*
    else if (params[0].compare(std::string("slam")) == 0)
    {
        u16 *new_slam = slam+(slam_idx * SLAM_POINTS);
    }
    */
    return 404;
}

/**
 * get the value of a pixel in the current data pointer 
 * TODO: move std deviation out of GOPTS and move it here
 */
int value_fn(std::vector<std::string> params, int socket)
{
    TRACE("value_fn");

    static u16 saved_x, saved_y = 0;

    // input validation
    Return_IfNot((params.size() >= 4), "requires at least 2 params", 500);

    // result map
    std::map<std::string, int> result;

    // get the value at the current position
    std::shared_ptr<Point2D_t> ptr1 = web::url_point_to_2d(utils::slice(params, 2, 3));

    uint16_t *calibrated = data::get_calibrated_ptr();

    GOPTS->point.x = ptr1->x;
    GOPTS->point.y = ptr1->y;
    INFO("get point: %d/%d width:[%d]", ptr1->x, ptr1->y, data::get_frame_width());
    int value;
    if (GOPTS->color_mode < 5) {
        value = (int)calibrated[(ptr1->y * data::get_frame_width()) + ptr1->x];
    } else {
        ptr1->x = CLAMP(ptr1->x, 0, 1500);
        ptr1->y = CLAMP(ptr1->y, 0, 165);
        int idx = (ptr1->x * 165) + ptr1->y;
        INFO("idx: %d", idx);
        value = (int)calibrated[src_ptr[idx]];
    }

    // calculate over an area
    if (GOPTS->color_mode < 5 && params.size() > 4)
    {
        std::shared_ptr<Point2D_t> ptr2 = web::url_point_to_2d(utils::slice(params, 4, 5));

        // TODO: could be a problem with 2 threads accessing this memory at the same time?
        std::vector<uint16_t> window = get_window(calibrated, data::get_frame_width(), ptr1->x, ptr1->y, ptr2->x, ptr2->y);

        // calculate the statistics
        if (window.size() > 0)
        {
            uint16_t mean = (int)utils::calc_mean(window);
            float stdd = calc_std_deviation(window, mean);
            uint16_t noise = (int)calc_temporal_noise(stdd);
            uint16_t rms = (int)calc_rms(window, mean);
            // keep only items that are within 1 standard deviation
            auto window2 = fplus::keep_if([&](uint16_t point)
                                          { return std::abs(point - mean) <= stdd; },
                                          window);
            uint16_t mean2 = (int)utils::calc_mean(window2);

            // assign to result map
            result["mean"] = mean;
            result["1std_mean"] = mean2;
            result["stdd"] = (int)(stdd * 1000.0f);
            result["temporal"] = noise;
            result["rms"] = rms;

            std::sort(window.begin(), window.end());
            result["diff"] = window.back() - window.front();
        }
        else
        {
            WARN(" window size <= 0 [%d] width:%d", window.size(), data::get_frame_width());
            result["diff"] = 0;
            result["rms"] = 0;
            result["stdd"] = 0;
            result["rms"] = 0;
            result["1std_mean"] = 0;
            result["mean"] = 0;
        }

        // adjust the center point from the upper left to the center
        ptr1->x = ((ptr2->x - ptr1->x) / 2) + ptr1->x;
        ptr1->y = ((ptr2->y - ptr1->y) / 2) + ptr1->y;
        value = (int)calibrated[(ptr1->y * data::get_frame_width()) + ptr1->x];
    }
    else
    {
        data::add_point(value);
        result["mean"] = (int)data::point_mean();
        result["stdd"] = (int)(data::point_stdd() * 1000.0f);
        result["diff"] = (int)data::point_range();
    }

    // someone selected a new point, clear the standard deviation point
    if (ptr1->x != saved_x || ptr1->y != saved_y)
    {
        saved_x = ptr1->x;
        saved_y = ptr1->y;
        data::clear_points();
    }

    result["value"] = value;
    result["width"] = data::get_frame_width();
    result["x"] = ptr1->x;
    result["y"] = ptr1->y;
    float rads = gfx::pixel_to_radians(xy_t{(s16)GOPTS->center_x, (s16)GOPTS->center_y}, xy_t{ptr1->x, ptr1->y});
    rads = (rads >= 0) ? rads : (PI * 2)+rads;
    result["radians"] = rads * 1000;

    std::string response_json = utils::map_to_json_int(result);

    std::string raw = "HTTP/1.1 200 OK\nContent-Type: application/json\nAccess-Control-Allow-Origin: *\nContent-Length: " + std::to_string(response_json.length()) + "\n\n" + response_json;

    web::send_headers(raw, socket);
    return 200;
}


/**
 * auto calibration process
 */
void check_auto_calibration(u16 *raw)
{

    return;
    static int frame_ctr[NUMBER_OF_VIXELS + 10];
    static int accumulator[NUMBER_OF_VIXELS + 10];
    static int ctr;

    //if (!GOPTS->is_calibration_enabled)
    if (GOPTS->calibration_enabled > 1)
    {
        for (int i = 0; i < NUMBER_OF_VIXELS; i++)
        {
            frame_ctr[i] = accumulator[i] = 0;
        }
        return;
    }

    // printf("check: %d / %d\n");
    std::string dist = "";
    for (const auto &[index, cal] : *GOPTS->cal)
    {
        if (GOPTS->calibrate_angle > 0 && GOPTS->calibrate_angle != index)
        {
            continue;
        }

        xy_t p = gfx::calculate_circle_point(238, cal->radian);
        xy_t p2 = {(s16)std::clamp(p.x + GOPTS->center_x, 1, 640), (s16)std::clamp(GOPTS->center_y + p.y, 1, 480)};

        //printf("%d.%d %d.%d\n", p.x, p.y, p2.x, p2.y);

        // INFO("%f = %d.%d x %d.%d", cal->radian, p.x, p.y, p2.x, p2.y);
        //u16 avg_point = raw[(p2.y*FRAME_POLAR_WIDTH_DEFAULT)+p2.x];
        u16 avg_point = utils::calc_mean(get_window(raw, FRAME_POLAR_WIDTH_DEFAULT, p2.x - 5, p2.y - 5, p2.x + 5, p2.y + 5));
        //printf("avg point: %d\n", avg_point);
        dist += " . " + std::to_string(avg_point);

        u16 calibrated = avg_point * 1.38f;
        //printf("calibrate vixel: %d  EST: (%d/%d)  (%f/%f)\n", GOPTS->calibrate_vixel, avg_point, calibrated, GOPTS->far_calibration * 0.9f, GOPTS->far_calibration * 1.4f);
        if ((calibrated < GOPTS->near_calibration * .9f || calibrated > GOPTS->near_calibration * 1.4f) &&
            (calibrated < GOPTS->far_calibration * .9f || calibrated > GOPTS->far_calibration * 1.4f))
        {
            frame_ctr[index] = 0;
            accumulator[index] = 0;
            continue;
        }
        frame_ctr[index]++;
        if (frame_ctr[index] < 200)
        {
            if (frame_ctr[index] % 10 == 1)
            {
                printf("waiting... calibrate vixel [#%d]\n", index);
            }
            accumulator[index] = 0;
            ctr = 0;
        }
        else if (frame_ctr[index] >= 200 && frame_ctr[index] <= 300)
        {
            accumulator[index] += avg_point;
            ctr++;
            INFO("! calibrate sampling vixel:%d, [%d] @%dx%d = %d\n", ctr, index, p.x, p.y, avg_point);
        }
        else if (frame_ctr[index] == 301)
        {
            double avg_value = accumulator[index] / ctr;
            bool is_near = (std::abs(calibrated - GOPTS->near_calibration) < std::abs(calibrated - GOPTS->far_calibration)) ? true : false;
            WARN("!!! update calibration vixel:[%d] = sampled: [%f] avg_point: [%d] (%d) \n", index, avg_value, avg_point, calibrated);
            utils::update_json_calibration(index, is_near, (u16)round(avg_value));
        }
    }
}

/**
 * run the camera and convert data into jpeg data
 */
int run_camera()
{

    INFO("run camera");
    aditof::System system;
    std::vector<std::shared_ptr<aditof::Camera>> cameras;
    std::vector<std::string> frameTypes;

    INFO("loading api.so [%s]", dlerror());
    //void (*custom_frame)(uint16_t * frame, int width, int height, std::vector<AngleDist> &dist) = NULL;
    void (*custom_frame)(uint16_t * frame, float *horiz_angle, float *vert_angle, u16 *angle_bucket) = NULL;
    void (*set_runlines)(calibration::runline *lines, unsigned short num_lines) = NULL;

    GOPTS->api_lib = dlopen("/usr/local/chiton/current/api.so", RTLD_LAZY | RTLD_GLOBAL);
    if (GOPTS->api_lib == NULL) {
        WARN("api.so failed to load [%s]", dlerror());
    } else {
        INFO("found api.so. loaded");
        //custom_frame = (void (*)(uint16_t * frame, int width, int height, std::vector<AngleDist> &distances))
        custom_frame = (void (*)(uint16_t * frame, float *horiz_angle, float *vert_angle, u16 *angle_bucket))
            getFunctionPointer(GOPTS->api_lib, "process_standard_frame");

        if (custom_frame == NULL) {
            INFO("process_standard_frame not found");
        } else {
            INFO("process_standard_frame callback found");
        }

        set_runlines = (void (*)(calibration::runline *lines, unsigned short num_lines))
            getFunctionPointer(GOPTS->api_lib, "set_runlines");

        if (set_runlines == NULL) {
            INFO("set_runlines not found");
        } else {
            INFO("set_runlines callback found");
            set_runlines(calibration::get_run_lines(), calibration::get_num_runlines());
        }

    }

    // DELETE ME
    /*
    std::vector<xy_t> inner_points = gfx::circle_points(GOPTS->center_x, GOPTS->center_y, 1024, GOPTS->inner_dist);
    int zr = 0;
    for (const auto &p : inner_points)
    {
        INFO("inner point: (%d) - %d.%d", ++zr, p.x, p.y);
    }
    */

    // system initialize
    TRACE("camera sys init");
    utils::adi_call([&]() { return system.initialize(); });
    TRACE("camera get list");
    utils::adi_call([&]() {
                        system.getCameraList(cameras);
                        camera = cameras.front();
                        return aditof::Status::OK;
                    });
    TRACE("camera init");
    utils::adi_call([&]() { return camera->initialize(); });
    //camera->getDevice()->writeAFERegisters()
    TRACE("camera get frame type");
    utils::adi_call([&]() {
                        camera->getAvailableFrameTypes(frameTypes);
                        return frameTypes.empty() ? aditof::Status::GENERIC_ERROR : aditof::Status::OK;
                    });

    // set frame type and mode
    TRACE("camera set frame type");
    utils::adi_call([&]() { return camera->setFrameType(frameTypes.front()); });
    TRACE("camera set mode");
    utils::adi_call([&]() { return camera->setMode(GOPTS->mode); });

    // turn off adi camera calibration and geometry correction
    INFO("adi geometry correction off");
    camera->setControl("geometry_correction", "off");
    camera->setControl("noise_reduction_threshold", std::to_string(GOPTS->noise_reduction));

    uint16_t frame_number = 0;
    INFO("camera running");
    jpeg_rgb = (unsigned char *)NEWMEM(uint8_t, data::get_frame_size_bytes() * 3);
    src_ptr = NEWMEM(int, 1500*165);
    dst_ptr = NEWMEM(int, 1500*165);
    INFO("jpeg_rgb bytes [%d]", data::get_frame_size_bytes() * 3);

    uint16_t *calibrated = data::get_calibrated_ptr();

    std::vector<AngleDist> distances = gfx::create_distance_map();

    aditof::Frame frame;
    u16 regs[4] = {0x4011, 0x4012, 0x4013, 0x4014};
    u16 values[4] = {0x0C, 0x0C, 0x0C, 0x0C};
    u32 beta_ctl = 0;
    int last_color_mode = 0;

    u16 *slam = NEWMEM(u16, SLAM_POINTS * 100);
    u16 slam_idx;

    // get references to pixel angles and pixel buckets
    float *horiz_angle = calibration::get_horiz_angle();
    float *vert_angle = calibration::get_vert_angle();
    u16 *angle_bucket = calibration::get_pixel_bucket();

    data::set_frame_format(FRAME_FORMAT_POLAR);

    std::unordered_map<int, std::vector<offset_samp_t>>* mapping = data::get_point_mapping();


    calc_src_dst_points(src_ptr, dst_ptr, 1500, 165);







    while (GOPTS->running > 0)
    {
        // if we are point cloud data, wait until we get a request...
        utils::Wait_Until([]() { return GOPTS->color_mode != 4 || GOPTS->frame_request == true; }, 5);
        //if (GOPTS->color_mode == 4 && !GOPTS->frame_request == true) { continue; }

        // if the camera is disabled, wait 1/30 second and loop again..
        if (GOPTS->enable == false) {
            usleep(33333);
            continue;
        }

        // add new atomic for adi chip locking
        //utils::Wait_Until([]() { return GOPTS->waiting != 1; }, -1);
        //GOPTS->waiting = 3;

        cam::run_with_lock([&]() {
                               // get a reference to the internal API data pointer
                               aditof::FrameDataType type = (GOPTS->depth_frame) ? aditof::FrameDataType::DEPTH : aditof::FrameDataType::IR;
                               TIMER2("request_frame", 500, utils::adi_call([&]() { return camera->requestFrame(&frame); }));
                               utils::adi_call([&]() { return frame.getData(type, &raw_data1); });
                               return 0;
                           });




/*
        if (GOPTS->dynamic_illumination && GOPTS->depth_frame)
        {
            if (((beta_ctl & 0x000000FF) != values[0]) ||
                ((beta_ctl & 0x0000FF00) != values[1]) ||
                ((beta_ctl & 0x00FF0000) != values[2]) ||
                ((beta_ctl & 0xFF000000) != values[3]))
            {
                values[0] = beta_ctl & 0x000000FF;
                values[1] = (beta_ctl & 0x0000FF00) >> 8;
                values[2] = (beta_ctl & 0x00FF0000) >> 16;
                values[3] = (beta_ctl & 0xFF000000) >> 24;

                camera->getDevice()->writeAfeRegisters(regs, values, 4);
            }
        }
        else if (values[0] != 0x0C || values[1] != 0x0C || values[2] != 0x0C || values[3] != 0x0C)
        {
            values[0] = 0x0C;
            values[1] = 0x0C;
            values[2] = 0x0C;
            values[3] = 0x0C;
            camera->getDevice()->writeAfeRegisters(regs, values, 4);
        }
       */ 



        // clear image buffer when switching from rgb to greyscale
        // TODO: 
        if (last_color_mode != GOPTS->color_mode) {
            INFO("CLEAR IMAGE BUFFER, new mode: %d", (int)GOPTS->color_mode);
            memset(jpeg_rgb, 0, CAM_WIDTH*CAM_HEIGHT*3);
            memset(calibrated, 0, CAM_WIDTH*CAM_HEIGHT*2);
        }


        // chiton cal enabled, AND we are a depth frame
        bool chitoncal = data::get_chiton_calibration() && GOPTS->depth_frame;
        // first calibration averaging sample, 0 out the calibrated buffer (may contain IR data)
        if (GOPTS->calibration_enabled == CALIBRATION_SAMPLE_FRAMES) { memcpy(calibrated, raw_data1, CAM_WIDTH*CAM_HEIGHT*2);}

        // do calibration and extended averaging on data until time to take the sample
        if (GOPTS->calibration_enabled > 1) {
            WARN("SAMPLING number [%d]", GOPTS->calibration_enabled);
            TIMER2("average", 5, calibration::average_frame(raw_data1, calibrated));
            GOPTS->calibration_enabled--;
        } else if (chitoncal) {
            TIMER2("calibrate", 240, calibration::calibrate_frame(raw_data1, calibrated));
        } else {
            TIMER2("average", 240, calibration::average_frame(raw_data1, calibrated));
        }

        
        // do calibration
        if (GOPTS->calibration_enabled == 1) {
            WARN("SAMPLING CALIBRATION!");
            TIMER2("calibrate_sample", 1, calibration::save_calibration_frame(calibrated, GOPTS->near_calibration, GOPTS->calibrate_angle));
            WARN("SAMPLING COMPLETE! [%d], [%f]", GOPTS->near_calibration, GOPTS->calibrate_angle);
            GOPTS->calibration_enabled--;
            //if (GOPTS->near_calibration > 1500) {
                calibration::process_calibration();
                printf("CALIBRATION PROCESSED\n");
                calibration::point *p = calibration::get_calibration();
                utils::write_calibration(p, "calibration.bin", CAM_WIDTH * CAM_HEIGHT);
            //}
        }

        // set frame number in data
        calibrated[(data::get_frame_height() * data::get_frame_width()) - 1] = frame_number;
        raw_data1[(data::get_frame_height() * data::get_frame_width()) - 1] = frame_number++;

        // CALL USER API
        if (custom_frame != NULL) {
            custom_frame(calibrated, horiz_angle, vert_angle, angle_bucket);
        }

        if (GOPTS->calibrate_angle != 0.0f) {
            //int index = GOPTS->calibrate_angle;
            std::vector<Point2D_t> points = draw_angle_line(GOPTS->calibrate_angle);
            //INFO("draw line at angle index: %f  [%d]", GOPTS->calibrate_angle, points.size());
            plot_points(points, raw_data1, 5500);
            //plot_points(points, raw_data1, 0);
            points.clear();
            points.shrink_to_fit();
        }


        // calculate dynamic illumination value every 5 frames if enabled
        if (GOPTS->dynamic_illumination && GOPTS->depth_frame) {
            if (frame_number % 3 == 1) {
                // calculate slam dataset if we are not in slam output mode
                if (GOPTS->color_mode != 0) {
                    TIMER2("vector_slam", 180, calibration::vector_slam(calibrated, slam, slam_idx));
                }
                beta_ctl = calibration::dynamic_illumination(slam, slam_idx, beta_ctl);
           }
        }
        // if disabled, set to full illumination
        else {
            beta_ctl = 0x0C0C0C0C;
            // HANDLE reducing illumination on vixels not being calibrated
            /*
            if (GOPTS->calibrate_angle != 0.0f) {
                if (GOPTS->calibrate_angle > -3.14f) {
                    beta_ctl = 0x000C0C00;
                    if (GOPTS->calibrate_angle > -1.7f) { beta_ctl = 0x0000000C; }
                    else if (GOPTS->calibrate_angle > -2.4f) { beta_ctl = 0x000C000C; }
                    else if (GOPTS->calibrate_angle > -2.7f) { beta_ctl = 0x0000000C; }
                }
                else {
                    beta_ctl = 0x000C0C00;
                    if (GOPTS->calibrate_angle < -3.58f) { beta_ctl = 0x00000C00; }
                    else if (GOPTS->calibrate_angle < -4.02f) { beta_ctl = 0x0C000C00; }
                    else if (GOPTS->calibrate_angle < -4.46f) { beta_ctl = 0x0C000000; }
                }
            }
            */
        }

        // every 5 frames, update the illumination value on the camera
        if (frame_number % 3 == 1) {
            values[3] = beta_ctl & 0x000000FF;
            values[2] = (beta_ctl & 0x0000FF00) >> 8;
            values[1] = (beta_ctl & 0x00FF0000) >> 16;
            values[0] = (beta_ctl & 0xFF000000) >> 24;

            camera->getDevice()->writeAfeRegisters(regs, values, 4);
        }


        resolution_t planar_rez = {CAM_WIDTH, CAM_HEIGHT};

        if (GOPTS->color_mode == 6) {
            INFO("create color planar ");
            planar_rez = {1500, 165};
            TIMER2("create_planar_color_jpeg", 420, create_planar_color_jpeg(raw_data1, jpeg_rgb, src_ptr, dst_ptr, 1500*165));
        }
        else if (GOPTS->color_mode == 5) {
            planar_rez = {1500, 165};
            TIMER2("create_planar_jpeg", 420, create_planar_jpeg(raw_data1, jpeg_rgb, src_ptr, dst_ptr, 1500*165));
        }
        else if (GOPTS->color_mode == 4) {
            TIMER2("point_cloud", 420, calibration::vector_point_cloud(calibrated));
            GOPTS->frame_request = false;
        }
        // GREYSCALE IMAGE
        else if (GOPTS->color_mode == 1) {
            TIMER2("greyscale_jpeg_create", 420, create_greyscale_jpeg(raw_data1, jpeg_rgb, chitoncal));
        }
        // COLOR IMAGE
        else if (GOPTS->color_mode == 3) {
            TIMER2("rgb_jpeg_create", 420, create_color_jpeg(calibrated, jpeg_rgb));
        }
        // GAIN IMAGE
        else if (GOPTS->color_mode == 2) {
            TIMER2("gain_map", 420, calibration::gain_mapping(jpeg_rgb));
        }
        // COLOR IMAGE
        else if (GOPTS->color_mode == 0) {
            /*
            if (frame_number % 5 != 1 || !GOPTS->dynamic_illumination) {
                slam_idx = (slam_idx +1) % 10;
                TIMER2("vector_slam", 180, calibration::vector_slam(calibrated, slam, slam_idx));
                //beta_ctl = calibration::dynamic_illumination(slam, slam_idx, beta_ctl);
            }
            */

            TIMER2("vector_slam", 180, calibration::vector_slam(calibrated, slam, slam_idx));
            create_slam_jpeg(slam, slam_idx, jpeg_rgb);
        } else {
            memset(jpeg_rgb, 0, 640*480*3);
        }



        // TODO: ADD ME!
        //calibration::dynamic_illumination(&raw_data1)


        // TODO: improve JPEG encoding here...
        utils::Wait_Until([]() { return GOPTS->waiting != 2; }, 6);
        GOPTS->waiting = 3;
        if (jpeg_ptr != NULL)
        {
            free(jpeg_ptr);
            jpeg_ptr = NULL;
        }
        if (GOPTS->color_mode == 3 || GOPTS->color_mode == 2 || GOPTS->color_mode == 0 || GOPTS->color_mode == 6)
        {
            jpeg_len = write_rgb_jpeg(jpeg_rgb, (long)&jpeg_ptr, planar_rez);
        }
        else if (GOPTS->color_mode <= 1 || GOPTS->color_mode == 5)
        {
            jpeg_len = write_greyscale_jpeg(jpeg_rgb, (long)&jpeg_ptr, planar_rez);
        }
        GOPTS->waiting = 0;

        data::set_ready(true);
        last_color_mode = GOPTS->color_mode;
        slam_idx = (slam_idx +1) % 10;
    }

    INFO("camera thread done");
    data::set_ready(true);
    return 1;
}

/**
 * move to a new file, something like background.cpp or tasks.cpp
 * return true if temperature is too high to start
 */
bool monitor_update()
{
    GOPTS->vixel_temperature = utils::get_vixel_temp_c();
    GOPTS->cpu_temperature = utils::get_cpu_temp_c();

    // temp warning
    if (GOPTS->cpu_temperature > 75 || GOPTS->vixel_temperature > 70) {
        GOPTS->message = "TEMP WARNING";
        WARN("TEMP WARNING cpu %.2fc vixel %.2fc", GOPTS->cpu_temperature, GOPTS->vixel_temperature);
    }

    // temp shutdown
    if (GOPTS->cpu_temperature > 80 || GOPTS->vixel_temperature > 80)
    {
        GOPTS->message = "TEMP SHUTDOWN";
        PANIC("TEMP SHUTDOWN CPU: %.2fc VIXEL: %.2fc", GOPTS->cpu_temperature, GOPTS->vixel_temperature);
        GOPTS->running = EXIT_TEMP_ERROR; // exit
        sys::shutdown_now();
        return true;
    }

    return (GOPTS->cpu_temperature > 75 || GOPTS->vixel_temperature > 70) ? true : false;
}

/**
 * temperature monitor thread
 */
void run_monitor()
{
    while (GOPTS->running > 0)
    {
        // refresh temperature
        monitor_update();

        // poll temp 1 / second
        usleep(1000 * 1000);
    }
}

void robot()
{
    bool connected = false;
    int fd = 0;
    while (GOPTS->running > 0 && fd == 0)
    {
        fd = utils::i2c_open(2, 0x21);
        usleep(1000 * 1000);
    }
    INFO("FOUND ROBOT AT 0x21 File Descriptor @[%d]", fd);
    char drive[5] = {0xFF, 0x0D, GOPTS->min_left, GOPTS->min_front, GOPTS->min_right};
    while (GOPTS->running > 0)
    {
        //int wrote = utils::i2c_write(fd, drive, 3);
        int wrote = write(fd, drive, 3);
        // INFO("wrote bytes: [%d]", wrote);
        usleep(1000 * 1000);
    }
    if (fd > 0)
    {
        close(fd);
    }
}

/**
 * enable core dumps
 */
void auto_debug_setup()
{
    std::cout << "auto setup for debug" << std::endl;
    UNUSED int r = system("ulimit -c unlimited");
    r = system("sysctl -w kernel.core_pattern=\"/usr/local/chiton/chiton.core\"");
}

/*
 std::vector<xy_samp_t> create_planar_mapping(std::vector<VixelCalibration> vixel) {

    std::vector<xy_t> inner = gfx::circle_points(GOPTS->center_x, GOPTS->center_y, 1200, GOPTS->inner_dist);
    std::vector<xy_t> outer = gfx::circle_points(GOPTS->center_x, GOPTS->center_y, 1200, GOPTS->outer_dist);


    std::vector<xy_samp_t> points = gfx::connect_vector_points(inner, outer, 165);
    INFO("num points: [%d]", points.size());
    return points;
 }
 */

/**
 * LVGL processing thread
 */
int run_app()
{

    std::unique_ptr<ChitonFB> fb = open_fb((char *)"/dev/fb1");

    std::unique_ptr<ChitonPngImage> font_large = read_png_file((char *)"/usr/local/chiton/current/files/large_font.png", true);
    std::unique_ptr<ChitonPngImage> font_small = read_png_file((char *)"/usr/local/chiton/current/files/small_font.png", true);
    std::unique_ptr<FontChar[]> large_font_offs = init_large_fonts();
    std::unique_ptr<FontChar[]> small_font_offs = init_small_fonts();

    fb_write_font_string(fb.get(), (char *)GOPTS->eth0.c_str(), {5, 210}, 0x7C0, font_small.get(), small_font_offs.get());
    fb_write_font_string(fb.get(), (char *)GOPTS->wlan0.c_str(), {5, 225}, 0x7C0, font_small.get(), small_font_offs.get());
    memcpy(fb->raw, fb->buffer1, fb->sz);

    for (int i = 0; i <= 46; i++)
    {
        char filename[64];
        snprintf(filename, 64, "/usr/local/chiton/current/jabil/xx_%04d.png", i);

        std::unique_ptr<ChitonPngImage> img = read_png_file(filename, false);
        memcpy((u16 *)fb->raw + (320 * 10 * 2), img->image, img->width * img->height * 2);
        usleep(26333);
    }

    u16 colors[4096];
    //HSV8* hsv = new HSV8();
    for (int i = 0; i < 255; i++)
    {
        int five = i >> 3;
        int six = i >> 2;
        for (int x = 0; x < 16; x++)
        {
            u16 c = five << 11 | six << 5 | five;
            colors[(i * 16) + x] = c;
        }
    }
    //free(hsv);

    unsigned long start_time = time(NULL);
    while (GOPTS->running)
    {
        if (GOPTS->enable == false)
        {
            usleep(33333);
            continue;
        }
        if (!data::is_ready())
        {
            usleep(33333);
            continue;
        }

        int offset1 = 0;
        int offset2 = 280 * CAM_WIDTH;
        cam::wait_for_lock([&]()
                           {
                               for (int y = 280; y < 470; y++)
                               {
                                   for (int x = 160; x < 480; x++)
                                   {
                                       fb->buffer1[offset1++] = colors[raw_data1[offset2 + x]];
                                   }
                                   offset2 += CAM_WIDTH;
                               }
                               return 0;
                           });

        // clear buffer on text, line 210 - 240
        memset(&fb->buffer1[200 * 320], 0, (320 * 40 * 2));

        unsigned long end_time = time(NULL) - start_time;
        if (end_time > 86400 * 10) { start_time = time(NULL); end_time = 1; }
        char timestr[64];
        sprintf(timestr, "%ld:%ld", (end_time / 60), (end_time % 60));
        fb_write_font_string(fb.get(), (char *)GOPTS->eth0.c_str(), {5, 210}, 0x7C0, font_small.get(), small_font_offs.get());
        fb_write_font_string(fb.get(), (char *)GOPTS->wlan0.c_str(), {5, 225}, 0x7C0, font_small.get(), small_font_offs.get());
        fb_write_font_string(fb.get(), (char *)GOPTS->message.c_str(), {185, 210}, 0xC0C0, font_small.get(), small_font_offs.get());
        fb_write_font_string(fb.get(), timestr, {185, 225}, 0xC0C0, font_small.get(), small_font_offs.get());
        // TODO: only need to draw 190 rows if eth and wlan is not updated.
        memcpy(fb->raw, fb->buffer1, 240 * 320 * 2);

        usleep(110000);
    }

    //app::init();
    //app::run();
    return 0;
}

void update_ethernet()
{
    int i = 0;
    while (GOPTS->running)
    {
        usleep(33333);
        if (i++ % 300 == 1)
        {
            GOPTS->eth0 = "eth : " + utils::get_interface_address("eth0");
            GOPTS->wlan0 = "wifi: " + utils::get_interface_address("wlan0");
        }
    }
}



/**
 * program begin
 */
int main()
{

#ifdef DEBUG_MODE
    auto_debug_setup();
//utils::redirect_stdout();
#endif

    // init system defaults and load chiton.json
    sys::SystemInit(320, 240);
    utils::init_options();
    hsv_cache = init_rgb_cache();
    data::init_defaults();
    INFO("chiton ver: [%d]", CHITON_VER);
    calibration::make_calibration_data();
    // utils::print_stacktrace();

    std::string file = "calibration.bin";
    if (utils::file_exists(file)) {
        printf("reading calibration\n");
        utils::read_calibration(file);
        printf("process calibration\n");
        calibration::process_calibration();
    } else {
        float *pixel_gain = calibration::get_pixel_gain();
        float *pixel_offset = calibration::get_pixel_offset();
        for (int i=0; i<CAM_WIDTH*CAM_HEIGHT; i++) {
            pixel_gain[i] = 1.0f;
            pixel_offset[i] = 0.0f;
        }

        calibration::process_old_calibration();
    }


    INFO("gausian blur init");
    gfx::make_gausian_kernel(2);
    INFO("gausian blur complete");




    // wait until termperatue is cool enough to start...
    //std::unique_ptr<ChitonFB> fb = open_fb((char *)"/dev/fb1");
    INFO("opened");
    /*
    while (monitor_update())
    {
        WARN("temp too high, waiting to cool...");
        std::string err = "temp cooldown";
        std::unique_ptr<ChitonPngImage> font_large = read_png_file((char *)"/usr/local/chiton/current/files/large_font.png", true);
        std::unique_ptr<FontChar[]> large_font_offs = init_large_fonts();
        fb_write_font_string(fb.get(), err.c_str(), {5, 210}, 0xC0C0, font_large.get(), large_font_offs.get());
        memcpy(fb->raw, fb->buffer1, fb->sz);
        sleep(10);
    }
    INFO("while complete");
    close(fb->fd);
    INFO("setup complete");
    */


    
/*
    calibration::point *p = calibration::get_calibration();
    p[2].near_dist = 123;
    p[2].near_point = 456;
    p[2].index = 99;
    utils::write_calibration(p, "cal2.bin", CAMWIDTH * CAMHEIGHT);
    */



    
    // temp monitor thread
    std::thread monitor_thread(run_monitor);
    //std::thread robot_thread(robot);
    // the LCD thread
    std::thread lvgl_thread(run_app);
    // start the camera thread
    std::thread camera_thread(run_camera);
    std::thread ethernet_thread(update_ethernet);

    // wait for the camera to start
    utils::Wait_Forever([]()
                        { return !data::is_ready(); });

    // run the webserver
    // function noun mappings.  url format: /verb/noun/option1/option2/etc
    // TODO: move these to the web namespace
    std::map<std::string, web::IntCallbackString> fnMap;
    fnMap["camera"] = camera_fn;
    fnMap["value"] = value_fn;
    fnMap["rover"] = web::rover_fn;
    fnMap["setting"] = web::setting_fn;
    fnMap["software"] = web::software_fn;
    fnMap["calibrate"] = calibrate_fn;
    web::web_server(PORT, fnMap);

    // wait for processing threads
    INFO("web server finished");
    camera_thread.join();
    lvgl_thread.join();
    monitor_thread.join();
    // robot_thread.join();
    ethernet_thread.join();
    INFO("threads joined");
    fclose(stdout);

    return 0;
}
