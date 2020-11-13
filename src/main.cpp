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
#include "color/hsv.h"
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

#include "system.h"
#define PORT 80 
#define CHITON_VER 022 

// TODO: find a better place for these...
RGB8 *hsv_cache;
uint16_t *calibrated;
uint16_t *raw_data1;
bool ready = false;
unsigned char *jpeg_data1 = NULL;
unsigned char *jpeg_data2 = NULL;
unsigned char *jpeg_ptr = NULL;
unsigned long jlenptr1 = 0;
unsigned long jlenptr2 = 0;
unsigned long jlenptr = 0;

// todo: make these atomic (std::atomic)
int jlock = 0;

// TODO: move to common file for testing.  remove dead code
GlobalOpts *GOPTS = NULL;
std::shared_ptr<aditof::Camera> camera = NULL;



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

    if (params[0].compare(std::string("image")) == 0 && ready) {
        DEBUG("send jpeg image ptr %x len [%d]", jpeg_ptr, jlenptr);
        bool okay = web::send_data(socket, std::string("image/jpeg"), jpeg_ptr, jlenptr);

        WARN_IFNOT(okay, "unable to send jpeg data");
        return okay ? 0 : -1;
    }
    
    if (params[0].compare(std::string("raw")) == 0) {
        DEBUG("send raw image");
        bool okay = web::send_data(socket, std::string("application/octet-stream"), (unsigned char*) calibrated, 640*480*2);

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
    TRACE("");
    
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
        if (window.size() > 2) {
            uint16_t mean = (int)utils::calc_mean(window);
            uint16_t stdd = (int)utils::calc_std_deviation(window, mean);
            uint16_t noise = (int)calc_temporal_noise(stdd);
            uint16_t rms = (int)calc_rms(window, mean);
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

        std::sort(window.begin(),window.end());
        result["diff"] = window.back() - window.front();
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

    web::send_headers(raw, socket);
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
    
    // initialize the calibrated data buffer
    calibrated = (uint16_t *)NEWMEM(uint16_t, 640*480);

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
    unsigned char *samples = (unsigned char *)NEWMEM(uint16_t, 640*480*3);


    jlock = 1;
    while(GOPTS->running > 0) {

        // if the camera is disabled, wait 1/30 second and loop again..
        if (GOPTS->enable == false) { usleep(33333); continue; }

        // request frame data from API
        type = (GOPTS->depth_frame) ? aditof::FrameDataType::DEPTH : aditof::FrameDataType::IR;
        utils::adi_call([&]() { return camera->requestFrame(&frame); });

        // get a reference to the internal API data pointer
        utils::adi_call([&]() { return frame.getData(type, &raw_data1); });
        
        PixelFunc color_fn = (GOPTS->color_mode == 1) ? depth_to_grayscale : depth_to_hsv;

        // process raw data with calibration offsets and jpeg
        TIMER("process_image", process_image(raw_data1, calibrated, samples, color_fn));

        calibrated[(640*480)-1] = frame_number;
        WARN_IF(frame_number % 1000 == 1, "frame [%d]", frame_number);



        // get value at sample point
        GOPTS->value = calibrated[GOPTS->value_offset];
        window_ptr[frame_number++ % 30] = GOPTS->value;
        GOPTS->std_deviation = (float)calc_std_deviation(window, static_cast<double>(calc_mean(window)));

        if (jlock == 1) {
            jpeg_ptr = jpeg_data2;
            jlenptr = jlenptr2;
            if (jpeg_data1 != NULL) { free(jpeg_data1); jpeg_data1 = NULL; }
            TIMER("write_jpeg", write_jpeg(samples, (long)&jpeg_data1, &jlenptr1));
            jlock = 2;
        }
        else if (jlock == 2) {
            jpeg_ptr = jpeg_data1;
            jlenptr = jlenptr1;
            if (jpeg_data2 != NULL) { free(jpeg_data2); jpeg_data2 = NULL; }
            write_jpeg(samples, (long)&jpeg_data2, &jlenptr2);
            jlock = 1;
            ready = true;
        }
    }
    INFO("camera thread done");

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
 * program begin
 */
int main() {
    // init system defaults and load config.json
    sys::SystemInit(320, 240);
    utils::init_options();
    init_mappings();
    hsv_cache = init_rgb_cache();
    INFO("chiton ver: [024]");


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
    // (ready is global variable that is set after all AFE camera bring-up)
    utils::Wait_Until([]() { return !ready; });
    
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
    
    return 0;
}
