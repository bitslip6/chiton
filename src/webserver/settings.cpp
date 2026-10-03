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
#include <dlfcn.h>



#include <linux/i2c-dev.h>
// #include <i2c/smbus.h>
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
#include "shapes.h"
#include "camera.h"
#include "request_validation.h"
#include "utils.hpp"


// UGLY AF
extern RGB8 *hsv_cache;
extern uint16_t *calibrated;
extern unsigned char *jpeg_rgb;
//extern std::vector<calibration_status> vixel_autocal_status;
//extern std::vector<xy_t> autocal_points;

namespace web
{

    /**
     * update a single setting in chiton, executes lambda if url matches
     * [params]  a vector of / separated url parameters
     * [method name]  the method name in params to invoke the lambda if there is a match
     * [min_params]  is the minimum required parameters to execute the lambda
     */
    int update_setting(std::vector<std::string> params, std::string method_name, uint min_params, std::function<int()> fn) {

        if (params.empty()) throw std::invalid_argument("missing setting");
        std::string url_method = params[0];
        if (url_method.compare(method_name) == 0 && params.size() == min_params) {
            // wait here until we are ing the API request frame thread.
            // this should ensure that any setting change occurs while we
            // are blocked waiting for the next frame 
            return cam::run_with_lock([&]() -> int { return fn(); });
        }
        return 404;
    }


    /**
     * TODO: find a better way to only set the return value than the turnary here
     */
    int setting_fn(std::vector<std::string> params, int socket) {
        
        if (params.size() < 2) throw std::invalid_argument("missing setting route");
        std::unique_lock<std::recursive_mutex> guard(cam::state_mutex());
        const std::map<std::string, size_t> counts = {
            {"degree",3}, {"quality",3}, {"camera",3}, {"depth",3}, {"slam",3},
            {"dynamic",3}, {"calibration_angle",4}, {"colormode",3}, {"max",3}, {"min",3},
            {"smoothing",3}, {"chitoncal",3}, {"planar",3}, {"noisereduction",3},
            {"width",2}, {"temp",2}, {"radians",2}, {"generalread",3}, {"generalwrite",4},
            {"calibration",3}, {"wlan0",4}, {"sdk_reload",2}, {"gateway",3}
        };
        auto route = counts.find(params[0]);
        if (route != counts.end()) request::count(params, route->second);
        const std::map<std::string, std::pair<long,long>> ranges = {
            {"quality",{1,100}}, {"colormode",{0,6}}, {"max",{0,9000}},
            {"min",{0,9000}}, {"noisereduction",{0,200}}
        };
        auto range = ranges.find(params[0]);
        if (range != ranges.end()) request::integer(params[2], range->second.first, range->second.second);
        if (params[0] == "degree") request::number(params[2], -36000, 36000);
        if (params[0] == "calibration_angle") {
            request::integer(params[2], 500, 4000);
            request::number(params[3], 0, 6284);
        }
        if (params[0] == "generalread" || params[0] == "generalwrite") {
            request::integer(params[2], 0, 65535, 16);
            if (params[0] == "generalwrite") request::integer(params[3], 0, 65535, 16);
        }
        const std::map<std::string, std::pair<std::string,std::string>> choices = {
            {"camera",{"enable","disable"}}, {"depth",{"depth","ir"}},
            {"slam",{"on","off"}}, {"dynamic",{"on","off"}},
            {"smoothing",{"on","off"}}, {"chitoncal",{"on","off"}},
            {"planar",{"on","off"}}, {"calibration",{"on","off"}}
        };
        auto choice = choices.find(params[0]);
        if (choice != choices.end() && params[2] != choice->second.first && params[2] != choice->second.second)
            throw std::invalid_argument("unknown setting value");
        TRACE(fplus::show(params).c_str());
        std::string data = "{404:404}";
        int resp = 404;

        // change color rotation
        resp = (resp != 404) ? resp : update_setting(params, "degree", 3, [&]() {
            GOPTS->calibrate_angle = static_cast<float>(atof(params[2].c_str()) / 100.0);
            data = "{\"value\":" + std::to_string(GOPTS->calibrate_angle) + "}";
            return 200;
        });

        // change image quality
        resp = (resp != 404) ? resp : update_setting(params, "quality", 3, [&]() {
            GOPTS->quality = atoi(params[2].c_str());
            data = "{\"value\":" + std::to_string(GOPTS->quality) + "}";
            return 200;
        });

        // enable/disable camera
        resp = (resp != 404) ? resp : update_setting(params, "camera", 3, [&]() {
            GOPTS->enable = (params[2].compare("enable") == 0);
            
            data = "{\"value\":" + std::to_string(GOPTS->enable) + "}";
            return 200;
        });

        // enable/disable camera
        resp = (resp != 404) ? resp : update_setting(params, "depth", 3, [&]() {
            GOPTS->depth_frame = (params[2].compare("ir") == 0);
            WARN("setting depth [%s]", params[2].c_str());
            data = "{\"value\":" + std::to_string(GOPTS->depth_frame) + "}";
            return 200;
        });

        // enable/disable camera
        resp = (resp != 404) ? resp : update_setting(params, "slam", 3, [&]() {
            GOPTS->draw_slam = (params[2].compare("on") == 0);
            WARN("setting depth [%s]", params[2].c_str());
            data = "{\"value\":" + params[2] + "}";
            return 200;
        });

        // enable/disable dynamic illumination
        resp = (resp != 404) ? resp : update_setting(params, "dynamic", 3, [&]() {
            GOPTS->dynamic_illumination = (params[2].compare("on") == 0);
            WARN("dynamic illumination depth [%s]", params[2].c_str());
            data = "{\"value\":" + params[2] + "}";
            GOPTS->set_message("dynamic light " + params[2]);
            return 200;
        });

        // enable calibration...
        resp = (resp != 404) ? resp : update_setting(params, "calibration_angle", 4, [&]() {
            GOPTS->near_calibration = utils::clamp(params[2], 500, 4000);
            std::string::size_type sz;
            GOPTS->calibrate_angle = (PI * 2) - stof(params[3], &sz) / 1000;
            WARN("calibrating depth [%dmm] @ angle (%f) (%s)", GOPTS->near_calibration, static_cast<double>(GOPTS->calibrate_angle), params[3].c_str());
            data = "{\"dist\":" + params[2] + ", \"angle\": " + params[3] + "}";
            GOPTS->set_message("calibrate dist: " + params[2]);
            return 200;
        });

        // change the image color coding
        resp = (resp != 404) ? resp : update_setting(params, "colormode", 3, [&]() {
            if (jpeg_rgb) memset(jpeg_rgb, 0, 640*480*3);
            GOPTS->color_mode = utils::clamp(params[2], 0, 6);

            if (GOPTS->color_mode > 4) { data::set_frame_dim(1500, 165); }
            else { data::set_frame_dim(640, 480); }

            data = "{\"value\":" + std::to_string(GOPTS->color_mode) + "}";
            int cm = (int)GOPTS->color_mode; 
            TRACE("setting color mode [%d]", cm);
            return 200;
        });

        // change the max value for image coding
        resp = (resp != 404) ? resp : update_setting(params, "max", 3, [&]() {
            GOPTS->max_cam_value = utils::clamp(params[2], 0, 9000);
            init_depth_to_hsv(GOPTS->min_cam_value, GOPTS->max_cam_value, 0.0f, hsv_cache);
            data = "{\"value\":" + std::to_string(GOPTS->max_cam_value) + "}";
            return 200;
        });

        // change the min value for image coding
        resp = (resp != 404) ? resp : update_setting(params, "min", 3, [&]() {
            GOPTS->min_cam_value = utils::clamp(params[2], 0, 9000);
            init_depth_to_hsv(GOPTS->min_cam_value, GOPTS->max_cam_value, 0.0f, hsv_cache);
            data = "{\"value\":" + std::to_string(GOPTS->min_cam_value) + "}";
            return 200;
        });

        // toggle the adi calibration
        resp = (resp != 404) ? resp : update_setting(params, "smoothing", 3, [&]() {
            GOPTS->smoothing = (params[2] == "on");
            data = "{\"setting\":\"smoothing\", \"value\":" + std::to_string(GOPTS->smoothing) + "}";
            return 200;
        });

        // toggle the chiton calibration
        resp = (resp != 404) ? resp : update_setting(params, "chitoncal", 3, [&]() {
            data::set_chiton_calibration((params[2] == "on"));
            data = "{\"value\":" + std::to_string(data::get_chiton_calibration()) + "}";
            GOPTS->set_message("calibration " + params[2]);
            return 200;
        });

        // toggle the planar mapping
        resp = (resp != 404) ? resp : update_setting(params, "planar", 3, [&]() {
            TRACE("BEGIN");
            data::set_frame_format((params[2] == "on") ? FRAME_FORMAT_PLANAR : FRAME_FORMAT_POLAR);
            data = "{\"value\":" + std::to_string(data::get_frame_format()) + 
                ", \"width\":"+ std::to_string(data::get_frame_width())+
                ", \"height\":"+ std::to_string(data::get_frame_height())+"}";

            TRACE("FIN");
            return 200;
        });

        // set noise reduction
        resp = (resp != 404) ? resp : update_setting(params, "noisereduction", 3, [&]() {
            //GOPTS->noise_reduction = std::stoi(params[2]);
            GOPTS->noise_reduction = utils::clamp(params[2], 0, 200);
            data = "{\"value\":" + std::to_string(GOPTS->noise_reduction) + "}";
            GOPTS->set_message("noise reduction\n" + params[2]);

            return 200;
        });

        // TODO: get image width 
        resp = (resp != 404) ? resp : update_setting(params, "width", 2, [&]() {
            int width = CAM_WIDTH;
            int height = CAM_HEIGHT;
            if (GOPTS->color_mode > 4) { width = 1500; height = 165; }
            data = "{\"value\":" + std::to_string(data::get_frame_format()) + 
                ", \"width\":"+ std::to_string(width)+
                ", \"height\":"+ std::to_string(height)+"}";;
            WARN("update width fin");
            return 200;
        });


        // send temp values
        resp = (resp != 404) ? resp : update_setting(params, "temp", 2, [&]() {
            data =  "{\"cputemp\":" + std::to_string(GOPTS->cpu_temperature) + ",\"vixeltemp\":" + std::to_string(GOPTS->vixel_temperature) + "}";
            return 200;
        });


        // send temp values
        resp = (resp != 404) ? resp : update_setting(params, "radians", 2, [&]() {
            std::map<std::string, float> map;

            map["radian1"] = GOPTS->cal->find(1)->second->radian;
            map["radian2"] = GOPTS->cal->find(2)->second->radian;
            map["radian3"] = GOPTS->cal->find(3)->second->radian;
            map["radian4"] = GOPTS->cal->find(4)->second->radian;
            map["radian5"] = GOPTS->cal->find(5)->second->radian;
            map["radian6"] = GOPTS->cal->find(6)->second->radian;
            map["radian7"] = GOPTS->cal->find(7)->second->radian;
            map["radian8"] = GOPTS->cal->find(8)->second->radian;
            data = utils::map_to_json_float(map);
            // data =  "{\"radian1\":" + std::to_string(GOPTS->cpu_temperature) + ",\"vixeltemp\":" + std::to_string(GOPTS->vixel_temperature) + "}";
            return 200;
        });



        // read i2c data
        resp = (resp != 404) ? resp : update_setting(params, "generalread", 3, [&]() {
            uint16_t reg_address = std::strtoul(params[2].c_str(), nullptr, 16);
            data = "{\"value\":" + std::to_string(utils::i2c_read_afe_register(reg_address)) + ", \"address\":" + std::to_string(reg_address) + "}";
            return 200;
        });

        // write i2c data
        resp = (resp != 404) ? resp : update_setting(params, "generalwrite", 4, [&]() {
            uint16_t reg_address = std::strtoul(params[2].c_str(), nullptr, 16);
            uint16_t reg_value = std::strtoul(params[3].c_str(), nullptr, 16);

            utils::i2c_write_afe_register(reg_address, reg_value);
            data = "{\"value\": 1, \"address\":" + std::to_string(reg_address) 
				+ ", \"reg_value\":" + std::to_string(reg_value) + "}"   
			+ "}";
            return 200;
        });


        // update vixel calibration status
        /*
        resp = (resp != 404) ? resp : update_setting(params, "calibration_angle", 2, [&]() {
            float calibration_angle = utils::clamp(stof params[2], 1, 8); 
            std::cout << "static_cast<int>(vixel_autocal_status[vixel_index]): " << static_cast<int>(vixel_autocal_status[vixel_index]) << std::endl;
            data = "{\"calibration\":" + std::to_string(static_cast<int>(vixel_autocal_status[vixel_index])) + "}";
            return 200;
        });
        */

        // enable auto calibration
        resp = (resp != 404) ? resp : update_setting(params, "calibration", 3, [&]() { 
            GOPTS->depth_frame = true;
            GOPTS->calibration_enabled = (params[2] == "on") ? CALIBRATION_SAMPLE_FRAMES : 0;
            data = "{\"value\":" + std::to_string(GOPTS->calibration_enabled) + "}";
            WARN("auto calibration frame counter: [%s]", params[2].c_str());
            return 200;
        });


        // enable auto calibration
        resp = (resp != 404) ? resp : update_setting(params, "wlan0", 4, [&]() {
            utils::set_file_contents("/etc/radio_ssid", params[2]);
            utils::set_file_contents("/etc/radio_pass", params[3]);
            WARN("set radio ssid:[%s] pass:[%s]", params[2].c_str(), params[3].c_str());
            
            utils::execute_system_cmd(std::string("sync"));
            utils::execute_system_cmd(std::string("sudo /etc/radio.sh"));

            data = "{\"value\": \"updated\"}";
            GOPTS->set_message("ssid set\nreboot");
            return 200;
        });


/*
        // update targets distance
        resp = (resp != 404) ? resp : update_setting(params, "targets", 3, [&]() {
            GOPTS->near_calibration = utils::clamp(params[2], 0, 6000);
            GOPTS->far_calibration = utils::clamp(params[3], 0, 6000);
            WARN("update calibration distances: %d / %d", GOPTS->near_calibration, GOPTS->far_calibration);
            data = "{\"near\":" + std::to_string(GOPTS->near_calibration) + ", \"far\": "+std::to_string(GOPTS->far_calibration)+"}";
            return 200;
        });

        // limit vixel calibration to 1 vixel
        resp = (resp != 404) ? resp : update_setting(params, "", 2, [&]() {
            GOPTS->calibrate_vixel = utils::clamp(params[2], 0, 8);
            WARN("limiting calibration to vixel: %d", GOPTS->calibrate_vixel);
            data = "{\"vixel\":" + std::to_string(GOPTS->calibrate_vixel) + "}";
            return 200;
        });
        */


        
        resp = (resp != 404) ? resp : update_setting(params, "sdk_reload", 2, [&]() {
            WARN("sdk_reload");
            // Capture retains plugin callback pointers; locking cannot refresh them.
            data = "{\"error\":\"plugin reload requires restart\"}";
            return 409;
        });

        // gateway update
        resp = (resp != 404) ? resp : update_setting(params, "gateway", 3, [&]() {
            std::string gateway = params[2];
            int retcode = utils::set_gateway(gateway);
            data = "{\"retcode\":" + std::to_string(retcode) + "}";
            WARN("gateway update");
            GOPTS->set_message("gateway: " + gateway);

            return 200;
        });

        std::string raw = "HTTP/1.1 " + std::to_string(resp) + " OK\nContent-Type: application/json\nAccess-Control-Allow-Origin: *\nContent-Length: " + std::to_string(data.length()) + "\n\n" + data;
        guard.unlock();
        web::send_headers(raw, socket);

        return resp;
    }
}

