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


#include "fplus/fplus.hpp"
#include "system.h"
#include "webserver.h"
#include "image.h"
#include "utils.hpp"
#include "demos.h"
#include "data.h"
#include "color/hsv.h"


extern RGB8 *hsv_cache;
extern uint16_t *calibrated;

namespace web
{

    /**
     * update a single setting in chiton, executes lambda if url matches
     * [params]  a vector of / separated url parameters
     * [method name]  the method name in params to invoke the lambda if there is a match
     * [min_params]  is the minimum required parameters to execute the lambda
     */
    int update_setting(std::vector<std::string> params, std::string method_name, uint min_params, std::function<int()> fn) {

        std::string url_method = params[0];
        if (url_method.compare(method_name) == 0 && params.size() >= min_params) {
            // wait here until we are ing the API request frame thread.
            // this should ensure that any setting change occurs while we
            // are blocked waiting for the next frame 
            utils::Wait_Until([]() { return GOPTS->waiting != 0; });
            GOPTS->waiting = 2;
            auto result = fn();
            GOPTS->waiting = 0;
            return result;
        }
        return 404;
    }


    /**
     * TODO: find a better way to only set the return value than the turnary here
     */
    int setting_fn(std::vector<std::string> params, int socket) {
        
        TRACE(fplus::show(params).c_str());
        std::string data = "{404:404}";
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

        // enable/disable camera
        resp = (resp != 404) ? resp : update_setting(params, "camera", 2, [&]() {
            GOPTS->enable = (params[2].compare("enable") == 0);
            
            data = "{\"value\":" + std::to_string(GOPTS->enable) + "}";
            return 200;
        });

        // enable/disable camera
        resp = (resp != 404) ? resp : update_setting(params, "depth", 2, [&]() {
            GOPTS->depth_frame = (params[2].compare("ir") == 0);
            WARN("setting depth [%s]", params[2]);
            WARN("setting depth [%d]", GOPTS->depth_frame);
            data = "{\"value\":" + std::to_string(GOPTS->depth_frame) + "}";
            return 200;
        });


        // change the image color coding
        resp = (resp != 404) ? resp : update_setting(params, "colormode", 2, [&]() {
            GOPTS->color_mode = utils::clamp(params[2], 1, 3);
            data = "{\"value\":" + std::to_string(GOPTS->color_mode) + "}";
            return 200;
        });

        // change the max value for image coding
        resp = (resp != 404) ? resp : update_setting(params, "max", 2, [&]() {
            GOPTS->max_cam_value = utils::clamp(params[2], 0, 9000);
            init_depth_to_hsv(GOPTS->min_cam_value, GOPTS->max_cam_value, GOPTS->color_rotation, hsv_cache);
            data = "{\"value\":" + std::to_string(GOPTS->max_cam_value) + "}";
            return 200;
        });

        // change the min value for image coding
        resp = (resp != 404) ? resp : update_setting(params, "min", 2, [&]() {
            GOPTS->min_cam_value = utils::clamp(params[2], 0, 9000);
            init_depth_to_hsv(GOPTS->min_cam_value, GOPTS->max_cam_value, GOPTS->color_rotation, hsv_cache);
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
            data::set_chiton_calibration((params[2] == "on"));
            data = "{\"value\":" + std::to_string(data::get_chiton_calibration()) + "}";
            return 200;
        });

        // toggle the planar mapping
        resp = (resp != 404) ? resp : update_setting(params, "planar", 2, [&]() {
            TRACE("BEGIN");
            data::set_frame_format((params[2] == "on") ? FRAME_FORMAT_PLANAR : FRAME_FORMAT_POLAR);
            data = "{\"value\":" + std::to_string(data::get_frame_format()) + 
                ", \"width\":"+ std::to_string(data::get_frame_width())+
                ", \"height\":"+ std::to_string(data::get_frame_height())+"}";
            TRACE("FIN");
            return 200;
        });

        // set noise reduction
        resp = (resp != 404) ? resp : update_setting(params, "noisereduction", 2, [&]() {
            //GOPTS->noise_reduction = std::stoi(params[2]);
            GOPTS->noise_reduction = utils::clamp(params[2], 0, 100);
            data = "{\"value\":" + std::to_string(GOPTS->noise_reduction) + "}";
            return 200;
        });

        // TODO: get image width 
        resp = (resp != 404) ? resp : update_setting(params, "width", 2, [&]() {
            data = "{\"value\":" + std::to_string(data::get_frame_format()) + 
                ", \"width\":"+ std::to_string(data::get_frame_width())+
                ", \"height\":"+ std::to_string(data::get_frame_height())+"}";
            WARN("update width fin");
            return 200;
        });


        // send temp values
        resp = (resp != 404) ? resp : update_setting(params, "temp", 2, [&]() {
            data =  "{\"cputemp\":" + std::to_string(GOPTS->cpu_temperature) + ",\"vixeltemp\":" + std::to_string(GOPTS->vixel_temperature) + "}";
            return 200;
        });

        // read i2c data
        resp = (resp != 404) ? resp : update_setting(params, "generalread", 2, [&]() {
            uint16_t reg_address = std::strtoul(params[2].c_str(), nullptr, 16);
            data = "{\"value\":" + std::to_string(utils::i2c_read_afe_register(reg_address)) + ", \"address\":" + std::to_string(reg_address) + "}";
            return 200;
        });

        // write i2c data
        resp = (resp != 404) ? resp : update_setting(params, "generalwrite", 2, [&]() {
            uint16_t reg_address = std::strtoul(params[2].c_str(), nullptr, 16);
            uint16_t reg_value = std::strtoul(params[3].c_str(), nullptr, 16);
            data = "{\"value\":" + std::to_string(utils::i2c_write_afe_register(reg_address, reg_value)) 
				+ ", \"address\":" + std::to_string(reg_address) 
				+ ", \"reg_value\":" + std::to_string(reg_value) + "}"   
			+ "}";
            return 200;
        });

        resp = (resp != 404) ? resp : update_setting(params, "beta", 2, [&]() {

            utils::i2c_smbus_write(utils::i2c_afe_open(I2C_WRITE_PEC_MODE), 0x0011, 0x000C);
            utils::i2c_smbus_write(utils::i2c_afe_open(I2C_WRITE_PEC_MODE), 0x0012, 0x000C);
            utils::i2c_smbus_write(utils::i2c_afe_open(I2C_WRITE_PEC_MODE), 0x0013, 0x000C);
            utils::i2c_smbus_write(utils::i2c_afe_open(I2C_WRITE_PEC_MODE), 0x0014, 0x000C);

            data = "{\"value\":" + std::to_string(1) + "}";

            return 200;
        });

        resp = (resp != 404) ? resp : update_setting(params, "betaread", 2, [&]() {

            u16 value = 0;

            utils::i2c_smbus_read(utils::i2c_afe_open(I2C_READ_PEC_MODE), 0x4011, &value);
            std::cout << "read value: " << std::hex << value << std::endl;
            utils::i2c_smbus_read(utils::i2c_afe_open(I2C_READ_PEC_MODE), 0x4012, &value);
            std::cout << "read value: " << std::hex << value << std::endl;
            utils::i2c_smbus_read(utils::i2c_afe_open(I2C_READ_PEC_MODE), 0x4013, &value);
            std::cout << "read value: " << std::hex << value << std::endl;
            utils::i2c_smbus_read(utils::i2c_afe_open(I2C_READ_PEC_MODE), 0x4014, &value);
            std::cout << "read value: " << std::hex << value << std::endl;

            data = "{\"value\":" + std::to_string(1) + "}";

            return 200;
        });

        std::string raw = "HTTP/1.1 " + std::to_string(resp) + " OK\nContent-Type: application/json\nAccess-Control-Allow-Origin: *\nContent-Length: " + std::to_string(data.length()) + "\n\n" + data;
        web::send_headers(raw, socket);

        return resp;
    }

}
