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

#include "system.h"
#include "webserver.h"
#include "image.h"
#include "utils.hpp"


/**
 * update a single setting in chiton 
 */
std::string update_setting(std::vector<std::string> params, std::string method_name, uint min_params, std::function<std::string()> fn) {

    std::string url_method = params[0];
    if (url_method.compare(method_name) == 0 && params.size() >= min_params) {
        return fn();
    }

    return "";
}


/**
 * TODO: create a setting map function and move these to settings.cpp
 */
int setting_fn(std::vector<std::string> params, int socket)
{
    std::string data;
    int resp = 404;

    // change color rotation
    data = update_setting(params, "degree", 2, [&params]() {
        GOPTS->color_rotation = atof(params[2].c_str());
        return "{\"value\":" + std::to_string(GOPTS->color_rotation) + "}";
    });

    // change image quality
    data = update_setting(params, "quality", 2, [&params]() {
        GOPTS->quality = atoi(params[2].c_str());
        return "{\"value\":" + std::to_string(GOPTS->quality) + "}";
    });

    // upgrade system
    data = update_setting(params, "version", 2, [&]() {
        std::string cmd = "/usr/local/chiton/upgrade.sh " + params[2];
        system(cmd.c_str());
        GOPTS->running = false;
        return "{\"value\":" + params[2] + "}";
    });

    // enable/disable camera
    data = update_setting(params, "camera", 2, [&]() {
        GOPTS->enable = (params[2].compare("enable") == 0);
        return "{\"value\":" + std::to_string(GOPTS->enable) + "}";
    });

    // enable/disable camera
    data = update_setting(params, "depth", 2, [&]() {
        GOPTS->depth_frame = (params[2].compare("ir") == 0);
        return "{\"value\":" + std::to_string(GOPTS->enable) + "}";
    });


    // change the image color coding
    data = update_setting(params, "colormode", 2, [&]() {
        GOPTS->color_mode = std::stoi(params[2]);
        return "{\"value\":" + std::to_string(GOPTS->color_mode) + "}";
    });

    // change the max value for image coding
    data = update_setting(params, "max", 2, [&]() {
        GOPTS->max_cam_value = std::stoi(params[2]);
        RGB8 c;
        uint16_t t = 1;
        depth_to_hsv(&t, &c, true);
        return "{\"value\":" + std::to_string(GOPTS->max_cam_value) + "}";
    });

    // change the min value for image coding
    data = update_setting(params, "min", 2, [&]() {
        GOPTS->min_cam_value = std::stoi(params[2]);
        RGB8 c;
        uint16_t t = 1;
        depth_to_hsv(&t, &c, true);
        return "{\"value\":" + std::to_string(GOPTS->min_cam_value) + "}";
    });

    // toggle the adi calibration
    data = update_setting(params, "adical", 2, [&]() {
        GOPTS->geometry = params[2] == "on";
        camera->setControl("geometry_correction", params[2]);
        return "{\"value\":" + std::to_string(GOPTS->geometry) + "}";
    });

    // toggle the chiton calibration
    data = update_setting(params, "chitoncal", 2, [&]() {
        GOPTS->chitoncal = (params[2] == "on");
        return "{\"value\":" + std::to_string(GOPTS->chitoncal) + "}";
    });

    // set noise reduction
    data = update_setting(params, "noisereduction", 2, [&]() {
        GOPTS->noise_reduction = std::stoi(params[2]);
        return "{\"value\":" + std::to_string(GOPTS->noise_reduction) + "}";
    });

    // TODO: get noise values
    data = update_setting(params, "noise", 2, [&]() {
        GOPTS->noise_reduction = std::stoi(params[2]);
        return "{\"value\":" + std::to_string(GOPTS->noise_reduction) + "}";
    });

    // send temp values
    data = update_setting(params, "temp", 2, [&]() {
        return "{\"cputemp\":" + std::to_string(utils::get_cpu_temp_c()) + ",\"vixeltemp\":" + std::to_string(utils::get_vixel_temp_c()) + "}";
    });

    std::string raw = "HTTP/1.1 " + std::to_string(resp) + " OK\nContent-Type: application/json\nAccess-Control-Allow-Origin: *\nContent-Length: " + std::to_string(data.length()) + "\n\n" + data;
    send_headers(raw, socket);

    return resp;
}
