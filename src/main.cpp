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
#include <sys/ioctl.h>

#include "fplus/fplus.hpp"
#include <jpeglib.h>
#include "app.hpp"
#include "image.h"
#include "util.h"
#include "utils.hpp"
#include "demos.h"
#include "system.h"
#include "system.hpp"
#include "webserver.h"
#include "shapes.h"
#include "data.h"
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

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include <aditof/camera.h>
#include <aditof/frame.h>
#include <aditof/system.h>

#include "system.h"
#define PORT 80 
#define CHITON_VER 022 

// TODO: find a better place for these...
RGB8 *hsv_cache;
//uint16_t *calibrated = NULL;
uint16_t *raw_data1 = NULL;
unsigned char *jpeg_data1 = NULL;
unsigned char *jpeg_data2 = NULL;
unsigned char *jpeg_ptr = NULL;
unsigned char *jpeg_rgb = NULL;
unsigned long jlenptr1 = 0;
unsigned long jlenptr2 = 0;
unsigned long jlenptr = 0;

// is calibration enabled 
//bool is_calibration_enabled = true;
double temp_avg = 0;
int vixel_index = 0;
int autocal_samples = 0;
std::vector<calibration_status> vixel_autocal_status;
double **autocalibrated_values;
calibration_status autocal_status = calibration_status::NONE;
int n_samples = 100;
bool autocal_delay_enabled = true;
int ignored_frames = 0;
std::vector<xy_t> autocal_points;

// todo: make these atomic (std::atomic)
int jlock = 0;

// TODO: move to common file for testing.  remove dead code
GlobalOpts *GOPTS = NULL;
std::shared_ptr<aditof::Camera> camera = NULL;

void bt_sighandler(int sig, siginfo_t *info, UNUSED void* ctx) {
    printf("Got signal %d, faulty address is %p\n",sig, info->si_addr);
}

/**
 * camera functions
 * params vector of url parts 
 * socket a connected socket
 */
int camera_fn(std::vector<std::string> params, int socket) {
    
    if (params[0].compare(std::string("enable")) == 0) {
        WARN("camera enable\n");
        utils::adi_call([&]() { printf("camera start\n"); return camera->start(); });
        GOPTS->enable = true;
        return 200;
    }

    if (params[0].compare(std::string("disable")) == 0) {
        WARN("camera disable\n");
        GOPTS->enable = false;
        utils::adi_call([&]() { printf("camera stop\n"); return camera->stop(); });
        return 200;
    }

    if (params[0].compare(std::string("image")) == 0 && data::is_ready()) {
        char timer_name[128];
        snprintf(timer_name, 128, "send [%lu] jpeg bytes", jlenptr);
        bool okay = false;
        TIMER2(timer_name, 30, okay = web::send_data(socket, std::string("image/jpeg"), jpeg_ptr, jlenptr));

        WARN_IFNOT(okay, "unable to send jpeg data");
        return okay ? 0 : -1;
    }
    
    if (params[0].compare(std::string("raw")) == 0) {
        DEBUG("send raw image");
        uint16_t *calibrated = data::get_calibrated_ptr();
        bool okay = web::send_data(socket, std::string("application/octet-stream"), (unsigned char*) calibrated, data::get_frame_size_bytes());

        WARN_IFNOT(okay, "unable to send raw data");
        return okay ? 0 : -1;
    }

    return 404;
}


/**
 * convert a vector of url parts to x/y point url [get, value, 100, 200] = Point2D(100, 200)
 */
std::shared_ptr<Point2D_t> url_point_to_2d(std::vector<std::string> params) {
    auto ptr = std::shared_ptr<Point2D_t>(new Point2D_t());
    const int max_width = data::get_frame_width();
    const int max_height = data::get_frame_height();

    ptr->x = max_width/2;
    ptr->y = max_height/1.5;
    
    Return_If(params.size() < 2, "get/value requires 2 parameters", ptr);
    
    //ptr->x = (s16)utils::clamp(params[2], 0, max_width);
    //ptr->y = (s16)utils::clamp(params[1], 0, max_height);
    ptr->x = (s16)utils::clamp(params[0], 0, max_width);
    ptr->y = (s16)utils::clamp(params[1], 0, max_height);
    return ptr;
}



/**
 * get the value of a pixel in the current data pointer 
 * TODO: move std deviation out of GOPTS and move it here
 */
int value_fn(std::vector<std::string> params, int socket) {
    TRACE("");
    
    // input validation
    Return_IfNot((params.size() >= 4), "requires at least 2 params", 500);

    // result map
    std::map<std::string, int> result;
    
    // get the value at the current position
    std::shared_ptr<Point2D_t> ptr1 = url_point_to_2d(utils::slice(params, 2, 3));

    uint16_t *calibrated = data::get_calibrated_ptr();
    // calculate over an area
    if (params.size() > 4) {
        std::shared_ptr<Point2D_t> ptr2 = url_point_to_2d(utils::slice(params, 4, 5));

        // TODO: could be a problem with 2 threads accessing this memory at the same time?
        std::vector<uint16_t>window = get_window(calibrated, data::get_frame_width(), ptr1->x, ptr1->y, ptr2->x, ptr2->y);

        // calculate the statistics
        if (window.size() > 2) {
            uint16_t mean =  (int)utils::calc_mean(window);
            uint16_t stdd =  (int)utils::calc_std_deviation(window, mean);
            uint16_t noise = (int)calc_temporal_noise(stdd);
            uint16_t rms =   (int)calc_rms(window, mean);
            // keep only items that are within 1 standard deviation
            auto window2 = fplus::keep_if([&](uint16_t point) { return std::abs(point - mean) <= stdd; }, window);
            uint16_t mean2 = (int)utils::calc_mean(window2);
            
            // assign to result map
            result["mean"] =  mean;
            result["1std_mean"] =  mean2;
            result["stdd_range"] = stdd;
            result["temporal"] = noise;
            result["rms"] = rms;
        }

        if (window.size() > 2) {
            std::sort(window.begin(), window.end());
            result["diff"] = window.back() - window.front();
        } else {
            result["diff"] = 0;
        }


        // adjust the center point from the upper left to the center
        ptr1->x = ((ptr2->x - ptr1->x) / 2) + ptr1->x;
        ptr1->y = ((ptr2->y - ptr1->y) / 2) + ptr1->y;
    }

    
    int width = data::get_frame_width();
    GOPTS->value_offset = (ptr1->y * width) + ptr1->x;
    int value = (int)calibrated[GOPTS->value_offset];


    result["value"] = value;
    result["stdd_point"] = GOPTS->std_deviation;
    result["x"] = ptr1->x;
    result["y"] = ptr1->y;
    result["radians"] = gfx::pixel_to_radians(xy_t{(s16)GOPTS->center_x, (s16)GOPTS->center_y}, xy_t{ptr1->x, ptr1->y}) * 1000;


    std::string response_json = utils::map_to_json_int(result);

    std::string raw = "HTTP/1.1 200 OK\nContent-Type: application/json\nAccess-Control-Allow-Origin: *\nContent-Length: "
        + std::to_string(response_json.length()) + "\n\n" + response_json;

    web::send_headers(raw, socket);
    return 200;
}


// // calculate the mean of a vector of numbers
// uint16_t calc_mean(std::vector<uint16_t> data) {
//     return std::accumulate(data.begin(), data.end(), 0) / data.size();
// }

// uint16_t calc_std_deviation(std::vector<uint16_t> data, uint16_t mean) {
//     // new list of numbers (window size)
//     std::vector<uint16_t> diff(data.size());
//     // calculate distance from mean for each data element
//     std::transform(data.begin(), data.end(), diff.begin(), std::bind2nd(std::minus<uint16_t>(), mean));
//     // calculate inner product
//     uint16_t sq_sum = std::inner_product(diff.begin(), diff.end(), diff.begin(), 0);
//     // square root of sum of differences
//     return std::sqrt(sq_sum / data.size());
// }


/**
 * TODO: create a setting map function and move these to settings.cpp
 */
int position_fn(std::vector<std::string> params, int socket) {
    std::string data;
    int resp = 404;
    // quadrant read data
    if (params[0].compare(std::string("quadrant")) == 0) {
        resp = 500;
        float at = 0.0f;
        if (params.size() > 3) {
            int x = stoi(params[2]);
            int y = stoi(params[3]);
            at = find_laser(x, y);
        }
        data = "{\"quad\":" + 
            std::to_string(GOPTS->color_rotation) + 
            ", \"arctan\": "+std::to_string(at)+"}";
    }

    std::string raw = "HTTP/1.1 " + std::to_string(resp) + " OK\nContent-Type: application/json\nAccess-Control-Allow-Origin: *\nContent-Length: " + std::to_string(data.length()) + "\n\n" + data;
    web::send_headers(raw, socket);

    return resp;
}



/**
 * run the camera and convert data into jpeg data
 */
int run_camera() {
    //google::InitGoogleLogging("chiton");
    //FLAGS_alsologtostderr = 1;
    INFO("run camera");
    aditof::System system;
    std::vector<std::shared_ptr<aditof::Camera>> cameras;
    std::vector<std::string> frameTypes;
    std::vector<std::string> modes;
    aditof::Frame frame;
    aditof::FrameDataType type;
    
    //resolution_t planar_rez = {FRAME_PLANAR_WIDTH_DEFAULT, 165};

    //std::vector<xy_samp_t> mapping_points = gfx::calibrate_map(gfx::make_all_points_int(planar_rez));
    //std::vector<offset_samp_t> mapping_points = gfx::make_all_planar_points_int();
    // recompute width based on removed points (270-360)
    //planar_rez.width = mapping_points.size() / planar_rez.height;

    // make sure calibrated is large enough to store whatever we may need
    //calibrated = (uint16_t *)NEWMEM(uint16_t, MAX(CAM_WIDTH, planar_rez.width)*MAX(CAM_HEIGHT, planar_rez.height));
    //INFO("planar rez: [%d.%d]", planar_rez.width, planar_rez.height);
    GOPTS->dst_width = data::get_frame_width();
    GOPTS->dst_height = data::get_frame_height();

    //std::vector<offset_samp_t> offset = gfx::xy_to_offset(mapping_points);



    // system initialize
    TRACE("camera sys init");

/*
    printf(" cache: %d\n", hsv_cache);
    
    auto depth_to_hsv2 = std::bind(depth_to_hsv, hsv_cache, std::placeholders::_1, std::placeholders::_2);
    std::function<void(uint16_t*, RGB8*)> color_fn2 = depth_to_hsv2;
    uint16_t v = 1000;
    //auto depth_to_hsv3 = fplus::bind_1st_of_3(depth_to_hsv, &hsv_cache);
    TRACE("camera sys init 2");

    RGB8 delme;
    depth_to_hsv2(&v, &delme);
    TRACE("camera sys init 3");

    printf ("[%d.%d.%d] \n", delme.r, delme.g, delme.b);
    */

    
    utils::adi_call([&]() { return system.initialize(); });
    TRACE("camera get list");
    utils::adi_call([&]() { system.getCameraList(cameras); camera = cameras.front(); return aditof::Status::OK; });
    TRACE("camera init");
    utils::adi_call([&]() { return camera->initialize(); });
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
    INFO("adi geom correction turned off");
    camera->setControl("geometry_correction", "off");
    INFO("fin adi geom correction turned off");

    std::vector<float> window(30);
    float *window_ptr = window.data();
    uint16_t frame_number = 0;
    INFO("camera running");
    jpeg_rgb = (unsigned char *)NEWMEM(uint16_t, data::get_frame_size_bytes() * 3);
    uint16_t *calibrated = data::get_calibrated_ptr();


    //auto map_fn = std::bind(gfx::map_points, mapping_points, planar_rez, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    //auto color_fn = std::bind(depth_to_grayscale2, samples, std::placeholders::_1, std::placeholders::_2);

    //std::bind_1st

    //auto fn2 = depth_to_grayscale_curry(samples);


    jlock = 1;
    while(GOPTS->running > 0) {

        // if the camera is disabled, wait 1/30 second and loop again..
        if (GOPTS->enable == false) { usleep(33333); continue; }

        utils::Wait_Until([]() { return GOPTS->waiting != 2; });

        // request frame data from API
        GOPTS->waiting = 1;

        // get a reference to the internal API data pointer
        type = (GOPTS->depth_frame) ? aditof::FrameDataType::DEPTH : aditof::FrameDataType::IR;
        std::vector<offset_samp_t> *mapping_points = data::get_point_mapping();
        TIMER2("adi call", 30, utils::adi_call([&]() { return camera->requestFrame(&frame); }));
        utils::adi_call([&]() { return frame.getData(type, &raw_data1); });

        GOPTS->waiting = 0;

        
        //PixelFunc color_fn = (GOPTS->color_mode == 1) ? depth_to_grayscale : depth_to_hsv;
        //PixelFunc color_fn = depth_to_grayscale : depth_to_hsv;

        // process raw data with calibration offsets and jpeg
        //TIMER("process_image", process_image(raw_data1, calibrated, samples, color_fn));
        
        /*
        color_fn(&dst[soffset], &c);
        jpeg_data[doffset++] = c.r;
        jpeg_data[doffset++] = c.g;
        jpeg_data[doffset++] = c.b;
        */

        //PixelFunc2 fn3 = depth_to_grayscale3;
        //TIMER("planar", map_fn(raw_data1, dst, color_fn));
        //TIMER2("planar", 30, map_fn(raw_data1, calibrated, depth_to_grayscale2));
        //map_points_greyscale();

        bool processed = false;
        if (GOPTS->color_mode == 1) {
            TIMER2("mapping", 30, processed = gfx::map_points_greyscale(*mapping_points, raw_data1, calibrated, jpeg_rgb));
        } else {
            TIMER2("mapping", 30, processed = gfx::map_points_hsv(*mapping_points, raw_data1, calibrated, jpeg_rgb));
        }
        if (!processed) { continue; }

        /* Autocalibration process START*/

        if (ignored_frames == 200) {
            autocal_delay_enabled = false;
            ignored_frames = 0;
        }

        if (GOPTS->is_calibration_enabled == true && autocal_delay_enabled == false) {
            temp_avg += utils::mean_cal_square(vixel_index, raw_data1, autocal_points);
            if (autocal_samples == n_samples) {
                double calibr_mean = utils::mean_cal_square_nsamples(temp_avg, n_samples);
                int int_autocal_status = static_cast<int>(vixel_autocal_status[vixel_index]);
                std::cout << "Vixel " << vixel_index + 1 
                          << " | autocal_status: " << int_autocal_status
                          << " | autocal_samples: " << autocal_samples
                          << " | calibr_mean: " << calibr_mean << std::endl;
                std::cout << "GOPTS->near_calibration: " << GOPTS->near_calibration
                          << " | GOPTS->far_calibration: " << GOPTS->far_calibration << std::endl;
                          
                /* autocalibration first distance*/
                if (vixel_autocal_status[vixel_index] == calibration_status::NONE) {
                    double estimated_value = 0;
                    if (utils::is_within_range(calibr_mean, GOPTS->near_calibration, &estimated_value, 2)) {
                        vixel_autocal_status[vixel_index] = calibration_status::PARTIAL;
                        autocalibrated_values[NEAR_DISTANCE][vixel_index] = estimated_value;
                        autocal_delay_enabled = true;
                        ignored_frames = 0;
                        std::cout << "Vixel " << vixel_index + 1 << ": calibr_mean: " << calibr_mean << std::endl;
                        std::cout << "Vixel " << vixel_index + 1 << ": calibration_status::PARTIAL" << std::endl;
                    }
                }
                /* autocalibration second distance*/ 
                else if (vixel_autocal_status[vixel_index] == calibration_status::PARTIAL) {
                    double estimated_value = 0;
                    if (utils::is_within_range(calibr_mean, GOPTS->far_calibration, &estimated_value, 2)) {
                        vixel_autocal_status[vixel_index] = calibration_status::COMPLETED;
                        autocalibrated_values[FAR_DISTANCE][vixel_index] = estimated_value;
                        autocal_delay_enabled = true;
                        ignored_frames = 0;
                        std::cout << "Vixel " << vixel_index + 1 << ": calibration_status::COMPLETED" << std::endl;
                        vixel_index++;
                        //vixel_autocal_status[vixel_index] = calibration_status::NONE;
                        if(vixel_index == NUMBER_OF_VIXELS) {
                            GOPTS->is_calibration_enabled = false;
                            std::cout << "=== calibration finished ===" << std::endl;
                            utils::update_calibration_on_json(autocalibrated_values);
                            for(int i = 0; i < NUMBER_OF_CAL_DISTANCES; i++) {
                                for(int j = 0; j < NUMBER_OF_VIXELS; j++) {
                                    std::cout << "autocalibrated_values: " << autocalibrated_values[i][j] << std::endl;
                                }
                                std::cout << std::endl;
                            }
                        }
                    }
                }
                autocal_samples = 0;
                temp_avg = 0;
            } else {
                autocal_samples++;
            }
        } else {
            ignored_frames++;
            //std::cout << "ignored_frames: " << ignored_frames;
        }

        /* Autocalibration process END*/

        window_ptr[frame_number++%30] = calibrated[GOPTS->value_offset];

        // TODO: add frame number to last pixel
        //calibrated[(data::get_frame_height()*data::get_frame_width())-1] = frame_number;
        //WARN_IF(frame_number % 1000 == 1, "frame [%d]", frame_number);



        // get value at sample point
        //window_ptr[frame_number++ % 30] = calibrated[GOPTS->value_offset];
        GOPTS->std_deviation = (float)calc_std_deviation(window, static_cast<double>(calc_mean(window)));


        resolution_t planar_rez = {(uint16_t) data::get_frame_width(), (uint16_t) data::get_frame_height()};

        if (jlock == 1) {
            jpeg_ptr = jpeg_data2;
            jlenptr = jlenptr2;
            if (jpeg_data1 != NULL) { free(jpeg_data1); jpeg_data1 = NULL; }
            //TIMER("write_jpeg", write_jpeg(jpeg_rgb, (long)&jpeg_data1, &jlenptr1, planar_rez));
            write_jpeg(jpeg_rgb, (long)&jpeg_data1, &jlenptr1, planar_rez);
            jlock = 2;
        }
        else if (jlock == 2) {
            jpeg_ptr = jpeg_data1;
            jlenptr = jlenptr1;
            if (jpeg_data2 != NULL) { free(jpeg_data2); jpeg_data2 = NULL; }
            write_jpeg(jpeg_rgb, (long)&jpeg_data2, &jlenptr2, planar_rez);
            jlock = 1;
            data::set_ready(true);
        }
    }
    INFO("camera thread done");
    data::set_ready(true);

    return 1;
}


/**
 * LVGL processing thread
 */
int run_app() {
    //app::init();
    //app::run();
    return 0;
}

/**
 * move to a new file, something like background.cpp or tasks.cpp
 * return true if temperature is too high to start
 */
bool monitor_update(){

    GOPTS->vixel_temperature = utils::get_vixel_temp_c();
    GOPTS->cpu_temperature = utils::get_cpu_temp_c();

    // temp warning
    WARN_IF(GOPTS->cpu_temperature > 75 || GOPTS->vixel_temperature > 65, "TEMP WARNING cpu %.2fc vixel %.2fc", GOPTS->cpu_temperature, GOPTS->vixel_temperature);
    
    // temp shutdown
    if(GOPTS->cpu_temperature > 85 || GOPTS->vixel_temperature > 70) {
       PANIC("TEMP SHUTDOWN CPU: %.2fc VIXEL: %.2fc",GOPTS->cpu_temperature, GOPTS->vixel_temperature);
       GOPTS->running = EXIT_CONFIG_ERROR; // exit
       return true;
    }

   return false;
}

/**
 * temperature monitor thread
 */
void run_monitor() {
    while(GOPTS->running > 0) {
       // refresh temperature
       monitor_update();
       // dont this to faster due saturation of i2c bus due vixel temperature
       usleep(1000*1000); // 1 second
    }
}

/**
 * enable core dumps
 */
void auto_debug_setup() {
     std::cout << "auto setup for debug" << std::endl;
     system("ulimit -c unlimited");
     system("sysctl -w kernel.core_pattern=\"/tmp/chiton.core\"");
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
 * program begin
 */
int main() {

    #ifdef DEBUG_MODE
    auto_debug_setup();
    //utils::redirect_stdout();
    #endif


    // init system defaults and load config.json
    sys::SystemInit(320, 240);
    utils::init_options();
    init_mappings();
    hsv_cache = init_rgb_cache();
    data::init_defaults();
    INFO("chiton ver: [026]");
    // utils::print_stacktrace();


    // wait until termperatue is cool enough to start...
    while (monitor_update()) {
        WARN("temp too high, waiting 20 seconds to cool...");
        sleep(20);
    }




    // temp monitor thread
    std::thread monitor_thread(run_monitor);
    // the LCD thread
    std::thread lvgl_thread(run_app);
    // start the camera thread
    std::thread camera_thread(run_camera);
        
    // wait for the camera to start 
    utils::Wait_Forever([]() { return !data::is_ready(); });
    
    // run the webserver
    // function noun mappings.  url format: /verb/noun/option1/option2/etc
    // TODO: move these to the web namespace
    std::map<std::string, web::IntCallbackString> fnMap;
    fnMap["camera"] = camera_fn;
    fnMap["value"] = value_fn;
    fnMap["position"] = position_fn;
    fnMap["setting"] = web::setting_fn;
    fnMap["software"] = web::software_fn;
    web::web_server(PORT, fnMap);

    // wait for processing threads
    INFO("web server finished");
    camera_thread.join();
    lvgl_thread.join();
    monitor_thread.join();
    INFO("threads joined");
    fclose (stdout);
    
    return 0;
}
