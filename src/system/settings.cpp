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


#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "system.h"
#include "webserver.h"
#include "image.h"
#include "utils.hpp"



/**
 * update a single setting in chiton, executes lambda if url matches
 * [params]  a vector of / separated url parameters
 * [method name]  the method name in params to invoke the lambda if there is a match
 * [min_params]  is the minimum required parameters to execute the lambda
 */
int update_setting(std::vector<std::string> params, std::string method_name, uint min_params, std::function<int()> fn) {

    std::string url_method = params[0];
    if (url_method.compare(method_name) == 0 && params.size() >= min_params) {
        return fn();
    }
    return 404;
}


/**
 * TODO: find a better way to only set the return value than the turnary here
 */
int setting_fn(std::vector<std::string> params, int socket) {
    
    std::string data;
    int resp = 404;

    // change color rotation
    resp = (resp != 404) ? resp : update_setting(params, "degree", 2, [&]() {
        GOPTS->color_rotation = atof(params[2].c_str());
        data = "{\"value\":" + std::to_string(GOPTS->color_rotation) + "}";
        return 200;
    });

    // change image quality
    resp = (resp != 404) ? resp : update_setting(params, "quality", 2, [&]() {
        GOPTS->quality = atoi(params[2].c_str());
        data = "{\"value\":" + std::to_string(GOPTS->quality) + "}";
        return 200;
    });

    // upgrade system
    resp = (resp != 404) ? resp : update_setting(params, "version", 2, [&]() {
        std::string cmd = "/usr/local/chiton/upgrade.sh " + params[2];
        system(cmd.c_str());
        GOPTS->running = false;
        data = "{\"value\":" + params[2] + "}";
        return 200;
    });

    // enable/disable camera
    resp = (resp != 404) ? resp : update_setting(params, "camera", 2, [&]() {
        GOPTS->enable = (params[2].compare("enable") == 0);
        data = "{\"value\":" + std::to_string(GOPTS->enable) + "}";
        return 200;
    });

    // enable/disable camera
    resp = (resp != 404) ? resp : update_setting(params, "depth", 2, [&]() {
        GOPTS->depth_frame = (params[2].compare("ir") == 0);
        data = "{\"value\":" + std::to_string(GOPTS->enable) + "}";
        return 200;
    });


    // change the image color coding
    resp = (resp != 404) ? resp : update_setting(params, "colormode", 2, [&]() {
        GOPTS->color_mode = std::stoi(params[2]);
        data = "{\"value\":" + std::to_string(GOPTS->color_mode) + "}";
        return 200;
    });

    // change the max value for image coding
    resp = (resp != 404) ? resp : update_setting(params, "max", 2, [&]() {
        GOPTS->max_cam_value = std::stoi(params[2]);
        RGB8 c;
        uint16_t t = 1;
        depth_to_hsv(&t, &c, true);
        data = "{\"value\":" + std::to_string(GOPTS->max_cam_value) + "}";
        return 200;
    });

    // change the min value for image coding
    resp = (resp != 404) ? resp : update_setting(params, "min", 2, [&]() {
        GOPTS->min_cam_value = std::stoi(params[2]);
        RGB8 c;
        uint16_t t = 1;
        depth_to_hsv(&t, &c, true);
        data = "{\"value\":" + std::to_string(GOPTS->min_cam_value) + "}";
        return 200;
    });

    // toggle the adi calibration
    resp = (resp != 404) ? resp : update_setting(params, "adical", 2, [&]() {
        GOPTS->geometry = params[2] == "on";
        camera->setControl("geometry_correction", params[2]);
        data = "{\"value\":" + std::to_string(GOPTS->geometry) + "}";
        return 200;
    });

    // toggle the chiton calibration
    resp = (resp != 404) ? resp : update_setting(params, "chitoncal", 2, [&]() {
        GOPTS->chitoncal = (params[2] == "on");
        data = "{\"value\":" + std::to_string(GOPTS->chitoncal) + "}";
        return 200;
    });

    // set noise reduction
    resp = (resp != 404) ? resp : update_setting(params, "noisereduction", 2, [&]() {
        GOPTS->noise_reduction = std::stoi(params[2]);
        data = "{\"value\":" + std::to_string(GOPTS->noise_reduction) + "}";
        return 200;
    });

    // TODO: get noise values
    resp = (resp != 404) ? resp : update_setting(params, "noise", 2, [&]() {
        GOPTS->noise_reduction = std::stoi(params[2]);
        data = "{\"value\":" + std::to_string(GOPTS->noise_reduction) + "}";
        return 200;
    });

    // send temp values
    resp = (resp != 404) ? resp : update_setting(params, "temp", 2, [&]() {
        data =  "{\"cputemp\":" + std::to_string(utils::get_cpu_temp_c()) + ",\"vixeltemp\":" + std::to_string(utils::get_vixel_temp_c()) + "}";
        return 200;
    });

    // read i2c data
    resp = (resp != 404) ? resp : update_setting(params, "generalread", 2, [&]() {
        uint16_t reg_address = std::strtoul(params[2].c_str(), nullptr, 16);
        data = "{\"value\":" + std::to_string(utils::i2c_read_afe_register(reg_address)) + "}";
        return 200;
    });

    // write i2c data
    resp = (resp != 404) ? resp : update_setting(params, "generalwrite", 2, [&]() {
        uint16_t reg_address = std::strtoul(params[2].c_str(), nullptr, 16);
        uint16_t reg_value = std::strtoul(params[3].c_str(), nullptr, 16);
        data = "{\"value\":" + std::to_string(utils::i2c_write_afe_register(reg_address, reg_value)) + "}";
        return 200;
    });

    resp = (resp != 404) ? resp : update_setting(params, "statstype", 2, [&]() {

        GOPTS->stats_x1 = std::stoi(params[3]);
        GOPTS->stats_y1 = std::stoi(params[4]);
        GOPTS->stats_x2 = std::stoi(params[5]);
        GOPTS->stats_y2 = std::stoi(params[6]);
        
        uint16_t stats_value = 0;
        
        if (params[2].compare("meanstat") == 0) {
            stats_value = GOPTS->mean_value;
        } else if (params[2].compare("stddevstat") == 0) {
            stats_value = GOPTS->std_deviation;
        } else if (params[2].compare("tempnoisestat") == 0) {
            stats_value = GOPTS->temporal_noise;
        } else if (params[2].compare("rmsstat") == 0) {
            stats_value = GOPTS->rms_value;
        }

        data = "{\"value\":" + std::to_string(stats_value) + "}";

        return 200;
    });

    std::string raw = "HTTP/1.1 " + std::to_string(resp) + " OK\nContent-Type: application/json\nAccess-Control-Allow-Origin: *\nContent-Length: " + std::to_string(data.length()) + "\n\n" + data;
    send_headers(raw, socket);

    return resp;
}
