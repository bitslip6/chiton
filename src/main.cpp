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

DemoConf DCONF;
std::mutex raw_data_mtx;
uint16_t *copy;
uint16_t *raw_data1;
uint16_t *raw_data2;
uint16_t *raw_data_wptr;
uint16_t *raw_data_rptr;
bool raw_toggle = false;
bool ready = false;
unsigned char *jpeg_data1 = NULL;
unsigned char *jpeg_data2 = NULL;
unsigned char *jpeg_data_wptr = NULL;
unsigned char *jpeg_data_rptr = NULL;

unsigned char *samples;

unsigned long jlen1 = 0;
unsigned long jlen2 = 0;
unsigned long jlenptr = 0;
unsigned long rlen1 = 0;
unsigned long rlen2 = 0;
unsigned long rlenptr = 0;
int jlock = 0;
int dlock = 0;

// TODO: move to common file for testing.  remove dead code
std::string GMODE;
GlobalOpts *GOPTS = NULL;
std::shared_ptr<aditof::Camera> camera = NULL;
DisplayBuffers *GDISP;
time_t __HANDY_BENCH;
struct timeval __UTIL_T1, __UTIL_T2;


/**
 * camera functions
 * params vector of url parts 
 * socket a connected socket
 */
int camera_fn(std::vector<std::string> params, int socket) {
    //std::cout << "camera fn begin" << std::endl;
    
    if (params[0].compare(std::string("enable")) == 0) {
        GOPTS->enable = true;
        return 200;
    }

    if (params[0].compare(std::string("disable")) == 0) {
        GOPTS->enable = false;
        return 200;
    }

    if (params[0].compare(std::string("image")) == 0) {
        jlock = (raw_toggle) ? 1 : 2;
        //std::cout << "camera image send" << std::endl;
        bool okay = send_data(socket, std::string("image/jpeg"), jpeg_data1, jlenptr);
        jlock = 0;
        //std::cout << "send data" << okay << std::endl;
        if (!okay) { printf("unable to send jpeg data\n"); return -1; }
        return 0;
    }
    
    if (params[0].compare(std::string("raw")) == 0) {
        dlock = 1;//(raw_toggle) ? 1 : 2;
        bool okay = false;
        raw_data_mtx.lock();
        okay = send_data(socket, std::string("application/octet-stream"), (unsigned char*) copy, 640*480*2);
        raw_data_mtx.unlock();
        dlock = 0;

        // TODO: need to iterate over all raw_boxes lines and 
        // auto raw_boxes = calculate_box(raw_data_rptr);
        if (!okay) { printf("unable to send raw data\n"); return -1; }
        return 0;
    }

    return 404;
}

/**
 * get the value of a pixel in the current data pointer 
 */
int value_fn(std::vector<std::string> params, int socket) {
    if (params.size() < 4) {
        std::cout << "invalid request, get/value requires 2 additional parameters" << std::endl;
        return 500;
    }
    int x = std::stoi(params[2]);
    int y = std::stoi(params[3]);
    if (x>640||x<0) {
        std::cout << "invalid request, get/value requires 0 >= x > 640: " << x << std::endl;
        return 500;
    }
    if (y>480||y<0) {
        std::cout << "invalid request, get/value requires 0 >= x > 480: " << y << std::endl;
        return 500;
    }

    int offset = (y*640)+x;
    GOPTS->value_offset = offset;
    GOPTS->target_x = x;
    GOPTS->target_y = y;

    std::string resp = "{\"value\":"+std::to_string(GOPTS->value)+", \"stdd\":" + std::to_string(GOPTS->std_deviation) + "}";

    std::string raw = "HTTP/1.1 200 OK\nContent-Type: application/json\nAccess-Control-Allow-Origin: *\nContent-Length: "
        + std::to_string(resp.length()) + "\n\n" + resp;

    send_headers(raw, socket);
    return 200;
}

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
 * helper function for calling adi methods
 * call adi function and return status
 */
bool adi_call(std::function<aditof::Status()> fn) {
    
    aditof::Status status = aditof::Status::OK;
    status = fn();

    if (status != aditof::Status::OK) {
        LOG(ERROR) << "adi call failed!";
        GOPTS->running = false;
        return false;
    }

    return true;
}


/**
 * run the camera and convert data into jpeg data
 */
int run_camera() {
    google::InitGoogleLogging("chiton");
    FLAGS_alsologtostderr = 1;

    aditof::Status status = aditof::Status::OK;
    aditof::System system;
    std::vector<std::shared_ptr<aditof::Camera>> cameras;
    std::vector<std::string> frameTypes;
    std::vector<std::string> modes;
    aditof::Frame frame;
    aditof::FrameDataType type;
    
    // initialize the calibrated data buffer
    copy = (uint16_t *)NEWMEM(uint16_t, 640*480);

    // system initialize
    adi_call([&]() { return system.initialize(); });
    adi_call([&]() { system.getCameraList(cameras); camera = cameras.front(); return aditof::Status::OK; });
    adi_call([&]() { return camera->initialize(); });
    adi_call([&]() {
        camera->getAvailableFrameTypes(frameTypes);
        return frameTypes.empty() ? aditof::Status::GENERIC_ERROR : aditof::Status::OK;
    });

    // set frame type and mode
    adi_call([&]() { return camera->setFrameType(frameTypes.front()); });
    adi_call([&]() { return camera->setMode(GOPTS->mode); });

    // turn off adi camera calibration and geometry correction
    camera->setControl("geometry_correction", "off");

    std::vector<uint16_t> window(30);
    uint frame_number = 0;
    printf("camera running\n");
    while(GOPTS->running) {

        // request frame data from API
        type = (GOPTS->depth_frame) ? aditof::FrameDataType::DEPTH : aditof::FrameDataType::IR;
        adi_call([&]() { return camera->requestFrame(&frame); });
        while (dlock != 0) { usleep(10000); }

        // get a reference to the internal API data pointer
        adi_call([&]() { return frame.getData(type, &raw_data1); });
        ready = true;
        
        // process raw data with calibration offsets and jpeg
        process_image(raw_data1, copy, samples);

        // get value at sample point
        GOPTS->value = copy[GOPTS->value_offset];
        window[frame_number % 30] = GOPTS->value;
        GOPTS->std_deviation = calc_std_deviation(window, calc_mean(window));
        //printf(" point: [%d/%d]\n", GOPTS->value, GOPTS->std_deviation);

        while (jlock == 1) { usleep(1000); }
        if (jpeg_data1 != NULL) { free(jpeg_data1); jpeg_data1 = NULL; }
        write_jpeg(samples, (long)&jpeg_data1, &jlenptr);

        process_statistic(copy, GOPTS->stats_x1, GOPTS->stats_y1, GOPTS->stats_x2, GOPTS->stats_y2);

        frame_number++;
    }

    ready = true;
    return 1;
}


/**
 * LVGL processing thread
 */
int run_app() {
    utils::safe_cout("Starting GUI app...");

    app::init();
    app::run();

    utils::safe_cout("Closing GUI app...");

    return 0;
}



/**
 * program begin
 */
int main() {
    RGB8 c;
    uint16_t t=1;

    printf("main 2\n");
    // init system defaults and load config.json
    sys::SystemInit(320, 240);
    utils::init_options();
    init_mappings();
    // precompute floating point hsv mappings
    depth_to_hsv(&t, &c, true);

    // jpeg image memory
    samples = (unsigned char *)calloc(640*480*sizeof(uint16_t), 3);
    jpeg_data_wptr = jpeg_data2;
    jlenptr = jlen1;

    // start processing threads
    //std::thread(process_image);
    std::thread lvgl_thread(run_app);
    // start the camera thread
    std::thread camera_thread(run_camera);
        
    // wait for the camera to start
    printf("waiting for camera thread to start ...\n");
    while (!ready) { usleep(50000); }
    printf("starting web server ...\n");


    // run the webserver
    // function noun mappings.  url format: /verb/noun/option1/option2/etc
    std::map<std::string, IntCallbackString> fnMap;
    fnMap["camera"] = camera_fn;
    fnMap["value"] = value_fn;
    fnMap["position"] = position_fn;
    fnMap["setting"] = setting_fn;
    web_server(PORT, fnMap);

    // wait for processing threads
    printf("web server exited!\n");
    camera_thread.join();
    lvgl_thread.join();
    printf("server stopped! ...\n");

    return 0;
}
