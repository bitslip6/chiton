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
#include "camera.h"
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
#include <dlfcn.h>



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
unsigned char *jpeg_ptr = NULL;
unsigned char *jpeg_rgb = NULL;
unsigned long jpeg_len = 0;

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

void bt_sighandler(int sig, siginfo_t *info, UNUSED void* ctx) {
    printf("Got signal %d, faulty address is %p\n",sig, info->si_addr);
}

/**
 * get a function pointer from a dl loaded library
 */
void* getFunctionPointer(void* lib, const char* funcName) {
    // Get the function pointer to the function
    void* fptr = dlsym(lib, funcName);
    if (!fptr) {
      fprintf(stderr, "Could not get function pointer for %s\n  error is: %s\n\n", funcName, dlerror());
      return NULL;
    }
    return fptr;
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
        snprintf(timer_name, 128, "send [%lu] jpeg bytes", jpeg_len);
        bool okay = false;
        utils::Wait_Until([]() { return GOPTS->waiting != 3; });
        GOPTS->waiting = 2;
        TIMER2(timer_name, 30, okay = web::send_data(socket, std::string("image/jpeg"), jpeg_ptr, jpeg_len));
        GOPTS->waiting = 0;

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
 * get the value of a pixel in the current data pointer 
 * TODO: move std deviation out of GOPTS and move it here
 */
int value_fn(std::vector<std::string> params, int socket) {
    TRACE("");
    
    static u16 saved_x, saved_y = 0;
    
    // input validation
    Return_IfNot((params.size() >= 4), "requires at least 2 params", 500);

    // result map
    std::map<std::string, int> result;
    
    // get the value at the current position
    std::shared_ptr<Point2D_t> ptr1 = web::url_point_to_2d(utils::slice(params, 2, 3));

    uint16_t *calibrated = data::get_calibrated_ptr();
    // calculate over an area
    if (params.size() > 4) {
        std::shared_ptr<Point2D_t> ptr2 = web::url_point_to_2d(utils::slice(params, 4, 5));

        // TODO: could be a problem with 2 threads accessing this memory at the same time?
        std::vector<uint16_t>window = get_window(calibrated, data::get_frame_width(), ptr1->x, ptr1->y, ptr2->x, ptr2->y);
        
        printf("%d %d %d %d\n", ptr1->x, ptr1->y, ptr2->x, ptr2->y);

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

    
    int value = (int)calibrated[(ptr1->y * data::get_frame_width()) + ptr1->x];

    // someone selected a new point, clear the standard deviation point
    if (ptr1->x != saved_x || ptr1->y != saved_y) { saved_x = ptr1->x; saved_y = ptr1->y; data::clear_points(); }

    data::add_point(value);
    result["value"] = value;
    result["width"] = data::get_frame_width();
    result["stdd_point"] = data::point_stdd();
    result["x"] = ptr1->x;
    result["y"] = ptr1->y;
    result["radians"] = gfx::pixel_to_radians(xy_t{(s16)GOPTS->center_x, (s16)GOPTS->center_y}, xy_t{ptr1->x, ptr1->y}) * 1000;


    std::string response_json = utils::map_to_json_int(result);

    std::string raw = "HTTP/1.1 200 OK\nContent-Type: application/json\nAccess-Control-Allow-Origin: *\nContent-Length: "
        + std::to_string(response_json.length()) + "\n\n" + response_json;

    web::send_headers(raw, socket);
    return 200;
}


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
            ", \"vixel\": "+std::to_string(at)+"}";
    }

    std::string raw = "HTTP/1.1 " + std::to_string(resp) + " OK\nContent-Type: application/json\nAccess-Control-Allow-Origin: *\nContent-Length: " + std::to_string(data.length()) + "\n\n" + data;
    web::send_headers(raw, socket);

    return resp;
}

/**
 * auto calibration process
 */
void check_auto_calibration(u16 *raw) {
    if (!GOPTS->is_calibration_enabled) { return; }
    
    static int frame_ctr[NUMBER_OF_VIXELS];
    static int accumulator[NUMBER_OF_VIXELS];

    // printf("check: %d / %d\n");
    std::string dist = "";
    for (const auto &[index, cal] : *GOPTS->cal) {
        xy_t p = gfx::calculate_circle_point(238, cal->radian);
        xy_t p2 = {(s16)std::clamp(p.x + GOPTS->center_x, 1, 640), (s16)std::clamp(GOPTS->center_y + p.y, 1, 480)};
        
        //printf("%d.%d %d.%d\n", p.x, p.y, p2.x, p2.y);

        // INFO("%f = %d.%d x %d.%d", cal->radian, p.x, p.y, p2.x, p2.y);
        u16 avg_point = utils::calc_mean(get_window(raw, FRAME_POLAR_WIDTH_DEFAULT, p2.x-5, p2.y-5, p2.x+5, p2.y+5));
        dist += " . " + std::to_string(avg_point);

        u16 calibrated = avg_point * 1.43f;
        if (calibrated < GOPTS->near_calibration *.9 || calibrated > GOPTS->near_calibration * 1.1) { 
            frame_ctr[index] = 0;
            accumulator[index] = 0;
            continue;
        }
        frame_ctr[index]++;
        if (frame_ctr[index] < 200) {
            printf("waiting... [%d]\n", index);
        } else if (frame_ctr[index] >= 200 && frame_ctr[index] <= 300) {
            accumulator[index] += avg_point;
            printf("sampling... [%d]\n", index);
        } else if (frame_ctr[index] == 301) {
            printf("set read value... [%d] = [%d] (%d)\n", index, accumulator[index]/100, calibrated);
            utils::update_json_calibration(index, calibrated, (u16)(accumulator[index] / 100));
        }

        //WARN("Check %d.%d  - %d.%d\n", p.x, p.y, p2.x, p2.y);
    }
    
    // printf(" -- [%s] \n", dist.c_str());
}



/**
 * run the camera and convert data into jpeg data
 */
int run_camera() {

    INFO("run camera");
    aditof::System system;
    std::vector<std::shared_ptr<aditof::Camera>> cameras;
    std::vector<std::string> frameTypes;
    
    
    void (*custom_frame)(uint16_t *frame, int width, int height, std::vector<AngleDist> &dist) = NULL;
    GOPTS->api_lib = dlopen("/usr/local/chiton/current/api.so",  RTLD_LAZY | RTLD_GLOBAL);
    if (GOPTS->api_lib == NULL) {
        WARN("api.so failed to load [%s]", dlerror());
    }
    
    // TODO: move this out
    GOPTS->dst_width = data::get_frame_width();
    GOPTS->dst_height = data::get_frame_height();

    // system initialize
    TRACE("camera sys init");
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
    INFO("adi geometry correction off");
    camera->setControl("geometry_correction", "off");

    std::vector<float> window(30);
    uint16_t frame_number = 0;
    INFO("camera running");
    jpeg_rgb = (unsigned char *)NEWMEM(uint8_t, data::get_frame_size_bytes() * 3);
    uint16_t *calibrated = data::get_calibrated_ptr();

    std::vector<AngleDist> distances;

    aditof::Frame frame;
    while(GOPTS->running > 0) {

        // if the camera is disabled, wait 1/30 second and loop again..
        if (GOPTS->enable == false) { usleep(33333); continue; }

        // add new atomic for adi chip locking
        //utils::Wait_Until([]() { return GOPTS->waiting != 1; });
        //GOPTS->waiting = 3;

        cam::run_with_lock([&]() {
            // get a reference to the internal API data pointer
            aditof::FrameDataType type = (GOPTS->depth_frame) ? aditof::FrameDataType::DEPTH : aditof::FrameDataType::IR;
            TIMER2("adi call", 30, utils::adi_call([&]() { return camera->requestFrame(&frame); }));
            utils::adi_call([&]() { return frame.getData(type, &raw_data1); });
            return 0;
        });
        //GOPTS->waiting = 0;
        bool processed = false;

        // get the current frame pixel mapping and calibration
        // std::vector<offset_samp_t> *mapping_points = data::get_point_mapping();
        std::unordered_map<int, std::vector<offset_samp_t>> *mapping_points = data::get_point_mapping();
        

        if (GOPTS->color_mode == 4) {
            //TIMER2("fast map", 30, processed = gfx::map_points_fastest(*mapping_points, raw_data1, distances));
        } else if (GOPTS->color_mode == 1) {
            TIMER2("slow greyscale", 30, processed = gfx::map_points_greyscale2(*mapping_points, raw_data1, calibrated, jpeg_rgb, distances));
            //printf("ordering samples...\n");
            //printf("ordered samples: %d\n", ordered.size());
        } else if (GOPTS->color_mode == 2) {
            //TIMER2("hsv map", 30, processed = gfx::map_points_hsv(*mapping_points, raw_data1, calibrated, jpeg_rgb));
        } else if (GOPTS->color_mode == 3) {
            //TIMER2("gain map", 30, processed = gfx::map_points_gain(*mapping_points, raw_data1, jpeg_rgb));
            //calibrated = raw_data1;
        }
        if (!processed) { continue; }


        // set frame number to data
        calibrated[(data::get_frame_height()*data::get_frame_width())-1] = frame_number;
        raw_data1[(data::get_frame_height()*data::get_frame_width())-1] = frame_number;

        if (GOPTS->api_lib != NULL) {
            custom_frame = (void(*)(uint16_t *frame, int width, int height, std::vector<AngleDist> &distances))
                getFunctionPointer(GOPTS->api_lib, "process_standard_frame");
            if (custom_frame != NULL) {
                custom_frame(calibrated, data::get_frame_width(), data::get_frame_height(), distances);
            }
        }


        resolution_t planar_rez = {(uint16_t) data::get_frame_width(), (uint16_t) data::get_frame_height()};

        utils::Wait_Until([]() { return GOPTS->waiting != 2; });
        GOPTS->waiting = 3;
        if (jpeg_ptr != NULL) { free(jpeg_ptr); jpeg_ptr = NULL; }
        if (GOPTS->color_mode == 3 || GOPTS->color_mode == 1) {
            jpeg_len = write_greyscale_jpeg(jpeg_rgb, (long) &jpeg_ptr, planar_rez);
        } else if (GOPTS->color_mode == 2) {
            jpeg_len = write_rgb_jpeg(jpeg_rgb, (long) &jpeg_ptr, planar_rez);
        }
        GOPTS->waiting = 0;

        // update the calibration data if we are in calibration mode
        check_auto_calibration(raw_data1);

        data::set_ready(true);
    }

    INFO("camera thread done");
    data::set_ready(true);
    return 1;
}


/**
 * LVGL processing thread
 */
int run_app() {
    app::init();
    app::run();
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
     UNUSED int r = system("ulimit -c unlimited");
     r = system("sysctl -w kernel.core_pattern=\"/tmp/chiton.core\"");
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
    INFO("chiton ver: [029]");
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
