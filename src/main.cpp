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


#include <stdio.h>
#include <string.h>
#include <zlib.h>
#include <errno.h>
#include <stdlib.h>

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
#include <glog/logging.h>

#include "system.h"
#define PORT 80 
#define CHITON_VER 022 
#define MAX65K 65536

uint16_t *calibrated;
uint16_t *raw_data1;
bool ready = false;
unsigned char *jpeg_data1 = NULL;
unsigned long jlenptr = 0;

// todo: make these atomic (std::atomic)
int jlock = 0;
int dlock = 0;

// TODO: move to common file for testing.  remove dead code
GlobalOpts *GOPTS = NULL;
std::shared_ptr<aditof::Camera> camera = NULL;
RGB8* cache;

/**
 * pre-initializing pixel cache
 */
void init_rgb_cache(void)
{
    INFO("init_rgb_cache() invoked");
    cache = new RGB8[MAX65K];
    memset(cache, 0, MAX65K * sizeof(RGB8));
}

/**
 * camera functions
 * params vector of url parts 
 * socket a connected socket
 */
int camera_fn(std::vector<std::string> params, int socket) {
    
    if (params[0].compare(std::string("enable")) == 0) {
        printf("camera enable\n");
        utils::adi_call([&]() { printf("camera start\n"); return camera->start(); });
        GOPTS->enable = true;
        printf("enable set true\n");
        return 200;
    }

    if (params[0].compare(std::string("disable")) == 0) {
        printf("camera disable\n");
        GOPTS->enable = false;
        utils::adi_call([&]() { printf("camera stop\n"); return camera->stop(); });
        printf("disable set false\n");
        return 200;
    }

    if (params[0].compare(std::string("image")) == 0) {
        jlock = 1;
        bool okay = send_data(socket, std::string("image/jpeg"), jpeg_data1, jlenptr);
        jlock = 0;

        Warn_IfNot(okay, "unable to send jpeg data", __LINE__, __FILE__);
        return okay ? 0 : -1;
    }
    
    if (params[0].compare(std::string("raw")) == 0) {
        dlock = 1;
        bool okay = send_data(socket, std::string("application/octet-stream"), (unsigned char*) calibrated, 640*480*2);
        dlock = 0;

        Warn_IfNot(okay, "unable to send raw data", __LINE__, __FILE__);
        return okay ? 0 : -1;
    }

    return 404;
}


/**
 * convert a vector of url parts to x/y point url [get, value, 100, 200] = Point2D(100, 200)
 */
std::shared_ptr<Point2D_t> url_point_to_2d(std::vector<std::string> params) {
    auto ptr = std::shared_ptr<Point2D_t>(new Point2D_t());
    ptr->x = 320;
    ptr->y = 400;
    
    Return_If(params.size() < 2, "get/value requires 2 parameters", ptr);
    
    ptr->x = std::max(0, std::min(640, std::stoi(params[0])));
    ptr->y = std::max(0, std::min(640, std::stoi(params[1])));
    return ptr;
}



/**
 * get the value of a pixel in the current data pointer 
 * TODO: move std deviation out of GOPTS and move it here
 */
int value_fn(std::vector<std::string> params, int socket) {
    TRACE("value fn");
    
    // input validation
    Return_IfNot((params.size() >= 4), "requires at least 2 params", 500);

    // result map
    std::map<std::string, int> result;
    
    // get the value at the current position
    std::shared_ptr<Point2D_t> ptr1 = url_point_to_2d(utils::slice(params, 2, 3));

    // calculate over an area
    if (params.size() > 4) {
        std::shared_ptr<Point2D_t> ptr2 = url_point_to_2d(utils::slice(params, 4, 5));

        // TODO: could be a problem with 2 threads accessing this memory at the same time?
        std::vector<uint16_t>window = get_window(calibrated, ptr1->x, ptr1->y, ptr2->x, ptr2->y);

        // calculate the statistics
        if (window.size() > 1) {
            uint16_t mean = (int)utils::calc_mean(window);
            uint16_t stdd = (int)utils::calc_std_deviation(window, mean);
            uint16_t noise = (int)calc_temporal_noise(stdd);
            uint16_t rms = (int)calc_rms(window, mean);
            
            // assign to result map
            result["mean"] =  mean;
            result["stdd_range"] = stdd;
            result["noise"] = noise;
            result["rms"] = rms;
        }

        std::sort(window.begin(),window.end());
        result["diff"] = window.front() - window.back();
        // adjust the center point from the upper left to the center
        ptr1->x = ((ptr2->x - ptr1->x) / 2) + ptr1->x;
        ptr1->y = ((ptr2->y - ptr1->y) / 2) + ptr1->y;
    }

    GOPTS->target_x = ptr1->x;
    GOPTS->target_y = ptr1->y;
    GOPTS->value_offset = ptr1->y * 640 + ptr1->x;
    result["value"] = (int)calibrated[GOPTS->value_offset];
    result["stdd_point"] = GOPTS->std_deviation;
    result["x"] = ptr1->x;
    result["y"] = ptr1->y;


    std::string response_json = utils::map_to_json_int(result);

    std::string raw = "HTTP/1.1 200 OK\nContent-Type: application/json\nAccess-Control-Allow-Origin: *\nContent-Length: "
        + std::to_string(response_json.length()) + "\n\n" + response_json;

    send_headers(raw, socket);
    return 200;
}

/**
 * calculate distance between x1/y1 and x2/y2
 */
int calc_distance(int x1, int y1, int x2, int y2)
{
    float dist = std::sqrt((float)(x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
    return (int)dist;
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
    send_headers(raw, socket);

    return resp;
}



/**
 * run the camera and convert data into jpeg data
 */
int run_camera() {
    google::InitGoogleLogging("chiton");
    FLAGS_alsologtostderr = 1;

    aditof::System system;
    std::vector<std::shared_ptr<aditof::Camera>> cameras;
    std::vector<std::string> frameTypes;
    std::vector<std::string> modes;
    aditof::Frame frame;
    aditof::FrameDataType type;
    
    // initialize the calibrated data buffer
    calibrated = (uint16_t *)NEWMEM(uint16_t, 640*480);

    printf("sys run camera\n");
    // system initialize
    utils::adi_call([&]() { return system.initialize(); });
    utils::adi_call([&]() { system.getCameraList(cameras); camera = cameras.front(); return aditof::Status::OK; });
    utils::adi_call([&]() { return camera->initialize(); });
    utils::adi_call([&]() {
        camera->getAvailableFrameTypes(frameTypes);
        return frameTypes.empty() ? aditof::Status::GENERIC_ERROR : aditof::Status::OK;
    });
    

    // set frame type and mode
    utils::adi_call([&]() { return camera->setFrameType(frameTypes.front()); });
    utils::adi_call([&]() { return camera->setMode(GOPTS->mode); });

    // turn off adi camera calibration and geometry correction
    camera->setControl("geometry_correction", "off");

    std::vector<float> window(30);
    float *window_ptr = window.data();
    uint frame_number = 0;
    DEBUG("camera running");
    unsigned char *samples = (unsigned char *)calloc(640*480*sizeof(uint16_t), 3);

    while(GOPTS->running) {

        // if the camera is disabled, wait 1/30 second and loop again..
        if (GOPTS->enable == false) { usleep(33333); continue; }

        // request frame data from API
        type = (GOPTS->depth_frame) ? aditof::FrameDataType::DEPTH : aditof::FrameDataType::IR;
        utils::adi_call([&]() { return camera->requestFrame(&frame); });
        while (dlock != 0) { usleep(10000); }

        // get a reference to the internal API data pointer
        utils::adi_call([&]() { return frame.getData(type, &raw_data1); });
        ready = true;
        
        // process raw data with calibration offsets and jpeg
        ChronoTimer timer;
        process_image(raw_data1, calibrated, samples);
        std::cout << "process_image elapsed time: " << timer.elapsed_time_us() << std::endl;

        // get value at sample point
        GOPTS->value = calibrated[GOPTS->value_offset];
        window_ptr[frame_number++ % 30] = GOPTS->value;
        GOPTS->std_deviation = (float)calc_std_deviation(window, static_cast<double>(calc_mean(window)));

        while (jlock == 1) { usleep(1000); }
        if (jpeg_data1 != NULL) { free(jpeg_data1); jpeg_data1 = NULL; }
        write_jpeg(samples, (long)&jpeg_data1, &jlenptr);

        //process_statistics(calibrated);
    }

    ready = true;
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
 * program begin
 */
int main() {

    // init system defaults and load config.json
    sys::SystemInit(320, 240);
    utils::init_options();
    init_rgb_cache();
    init_mappings();
    printf("chiton ver: %d\n", CHITON_VER);

    // precompute floating point hsv mappings
    RGB8 c;
    uint16_t t=1;

    depth_to_hsv(&t, &c, true);

    // LCD display thread
    std::thread lvgl_thread(run_app);
    // start the camera thread
    std::thread camera_thread(run_camera);
        
    // wait for the camera to start 
    // (ready is global variable that is set after all AFE camera bring-up)
    utils::Wait_Until([]() { return !ready; });
    


    // run the webserver
    // function noun mappings.  url format: /verb/noun/option1/option2/etc
    std::map<std::string, IntCallbackString> fnMap;
    fnMap["camera"] = camera_fn;
    fnMap["value"] = value_fn;
    fnMap["position"] = position_fn;
    fnMap["setting"] = setting_fn;
    fnMap["software"] = software_fn;
    web_server(PORT, fnMap);

    // wait for processing threads
    printf("web server exited!\n");
    camera_thread.join();
    lvgl_thread.join();
    printf("server stopped! ...\n");

    return 0;
}
