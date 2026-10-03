#include <cmath>
#include <map>
#include <mutex>
#include "system.h"
#include "webserver.h"
#include "request_validation.h"
#include "utils.hpp"
#include "camera.h"
#include "data.h"
#include "color/hsv.h"
#include <aditof/device_interface.h>

extern RGB8 *hsv_cache;
extern unsigned char *jpeg_rgb;
extern std::shared_ptr<aditof::Camera> camera;

namespace web {
HttpResponse setting_fn(std::vector<std::string> params) {
    if (params.size() < 2) throw std::invalid_argument("missing setting route");
    const std::map<std::string, size_t> counts = {
        {"degree",3}, {"quality",3}, {"camera",3}, {"depth",3}, {"slam",3},
        {"dynamic",3}, {"calibration_angle",4}, {"colormode",3}, {"max",3}, {"min",3},
        {"smoothing",3}, {"chitoncal",3}, {"planar",3}, {"noisereduction",3},
        {"width",2}, {"temp",2}, {"radians",2}, {"range",4}, {"generalread",3}, {"generalwrite",4},
        {"calibration",3}, {"wlan0",4}, {"sdk_reload",2}, {"gateway",3}
    };
    const auto route = counts.find(params[0]);
    if (route == counts.end()) return error_response(404, "unknown setting");
    // /depth/setting is also a read-back route used to synchronize the IR checkbox.
    if (!((params[0] == "depth" || params[0] == "range") && params.size() == 2))
        request::count(params, route->second);
    std::unique_lock<std::recursive_mutex> guard(cam::state_mutex());
    const std::string &name = params[0];
    const std::string value = params.size() > 2 ? params[2] : "";
    const std::map<std::string, std::pair<long,long>> ranges = {
        {"quality",{1,100}}, {"colormode",{0,6}}, {"max",{0,9000}},
        {"min",{0,9000}}, {"noisereduction",{0,200}}
    };
    long integer = 0;
    const auto range = ranges.find(name);
    if (range != ranges.end()) integer = request::integer(value, range->second.first, range->second.second);
    const std::map<std::string, std::pair<std::string,std::string>> choices = {
        {"camera",{"enable","disable"}}, {"depth",{"depth","ir"}},
        {"slam",{"on","off"}}, {"dynamic",{"on","off"}},
        {"smoothing",{"on","off"}}, {"chitoncal",{"on","off"}},
        {"planar",{"on","off"}}, {"calibration",{"on","off"}},
        {"gateway",{"eth0","wlan0"}}
    };
    const auto choice = choices.find(name);
    if (choice != choices.end() && !(name == "depth" && params.size() == 2) &&
        value != choice->second.first && value != choice->second.second)
        throw std::invalid_argument("unknown setting value");
    nlohmann::json body;
    if (name == "degree") {
        GOPTS->calibrate_angle = request::number(value, -36000, 36000) / 100.0;
        body = {{"value", GOPTS->calibrate_angle}};
    } else if (name == "quality") {
        GOPTS->quality = integer;
        body = {{"value", GOPTS->quality}};
    } else if (name == "camera") {
        GOPTS->enable = value == "enable";
        body = {{"value", GOPTS->enable.load()}};
    } else if (name == "depth") {
        // true always means DEPTH, both here and in the SDK frame selection.
        if (params.size() == 3) GOPTS->depth_frame = value == "depth";
        const bool depth = GOPTS->depth_frame.load();
        body = {{"value", depth}, {"frame_type", depth ? "depth" : "ir"}};
    } else if (name == "slam") {
        GOPTS->draw_slam = value == "on";
        body = {{"value", GOPTS->draw_slam}};
    } else if (name == "dynamic") {
        GOPTS->dynamic_illumination = value == "on";
        body = {{"value", GOPTS->dynamic_illumination.load()}};
        GOPTS->set_message("dynamic light " + value);
    } else if (name == "calibration_angle") {
        // Validate both fields before changing either one.
        const auto distance = request::integer(value, 500, 4000);
        const auto angle = request::number(params[3], 0, 6284);
        GOPTS->near_calibration = distance;
        GOPTS->calibrate_angle = static_cast<double>(PI2) - angle / 1000.0;
        body = {{"dist", distance}, {"angle", angle}};
        GOPTS->set_message("calibrate dist: " + value);
    } else if (name == "colormode") {
        if (jpeg_rgb) memset(jpeg_rgb, 0, CAM_WIDTH * CAM_HEIGHT * 3);
        GOPTS->color_mode = integer;
        data::set_frame_dim(integer > 4 ? 1500 : CAM_WIDTH, integer > 4 ? 165 : CAM_HEIGHT);
        body = {{"value", integer}};
    } else if (name == "range" || name == "max" || name == "min") {
        long minimum = GOPTS->min_cam_value, maximum = GOPTS->max_cam_value;
        if (name == "range" && params.size() == 4) {
            minimum = request::integer(value, 0, 9000);
            maximum = request::integer(params[3], 0, 9000);
        } else if (name == "min") minimum = integer;
        else if (name == "max") maximum = integer;
        if (minimum >= maximum) throw std::invalid_argument("min must be less than max");
        if (name != "range" || params.size() == 4) {
            GOPTS->min_cam_value = minimum;
            GOPTS->max_cam_value = maximum;
            init_depth_to_hsv(minimum, maximum, 0.0f, hsv_cache);
        }
        body = {{"min", minimum}, {"max", maximum}};
        if (name != "range") body["value"] = integer;
    } else if (name == "smoothing") {
        GOPTS->smoothing = value == "on";
        body = {{"setting", "smoothing"}, {"value", GOPTS->smoothing}};
    } else if (name == "chitoncal") {
        data::set_chiton_calibration(value == "on");
        body = {{"value", bool(data::get_chiton_calibration())}};
        GOPTS->set_message("calibration " + value);
    } else if (name == "planar") {
        data::set_frame_format(value == "on" ? FRAME_FORMAT_PLANAR : FRAME_FORMAT_POLAR);
        body = {{"value", data::get_frame_format()}, {"width", data::get_frame_width()},
                {"height", data::get_frame_height()}};
    } else if (name == "noisereduction") {
        GOPTS->noise_reduction = integer;
        body = {{"value", integer}};
        GOPTS->set_message("noise reduction\n" + value);
    } else if (name == "width") {
        const bool panorama = GOPTS->color_mode > 4;
        body = {{"value", data::get_frame_format()}, {"width", panorama ? 1500 : CAM_WIDTH},
                {"height", panorama ? 165 : CAM_HEIGHT}};
    } else if (name == "temp") {
        body = {{"cputemp", GOPTS->cpu_temperature}, {"vixeltemp", GOPTS->vixel_temperature}};
    } else if (name == "radians") {
        body = nlohmann::json::object();
        for (int id = 1; id <= 8; ++id) {
            if (!GOPTS->cal || GOPTS->cal->count(id) == 0 || !GOPTS->cal->at(id))
                return error_response(500, "missing calibration measurement " + std::to_string(id));
            body["radian" + std::to_string(id)] = GOPTS->cal->at(id)->radian;
        }
    } else if (name == "generalread" || name == "generalwrite") {
        uint16_t address = request::integer(value, 0, 65535, 16);
        uint16_t reg_value = name == "generalwrite" ? request::integer(params[3], 0, 65535, 16) : 0;
        // Use the SDK's checked register operations (also simulated by the stub).
        auto device = camera ? camera->getDevice() : nullptr;
        if (!device) return error_response(503, "camera device unavailable");
        const auto status = name == "generalwrite" ? device->writeAfeRegisters(&address, &reg_value, 1)
                                                   : device->readAfeRegisters(&address, &reg_value, 1);
        if (status != aditof::Status::OK) return error_response(500, "AFE register operation failed");
        body = {{"value", name == "generalwrite" ? 1 : reg_value}, {"address", address}};
        if (name == "generalwrite") body["reg_value"] = reg_value;
    } else if (name == "calibration") {
        if (value == "on") GOPTS->depth_frame = true;
        GOPTS->calibration_enabled = value == "on" ? CALIBRATION_SAMPLE_FRAMES : 0;
        body = {{"value", GOPTS->calibration_enabled}};
    } else if (name == "wlan0") {
#ifdef CHITON_ADITOF_STUB
        return error_response(501, "Wi-Fi provisioning is unavailable with the camera simulator");
#else
        utils::set_file_contents("/etc/radio_ssid", value);
        utils::set_file_contents("/etc/radio_pass", params[3]);
        int result = -1;
        utils::get_command_output_lines("sync && sudo /etc/radio.sh", &result);
        if (result != 0) return error_response(500, "Wi-Fi configuration failed");
        body = {{"value", "updated"}};
        GOPTS->set_message("ssid set\nreboot");
#endif
    } else if (name == "sdk_reload") {
        return error_response(409, "plugin reload requires restart");
    } else if (name == "gateway") {
#ifdef CHITON_ADITOF_STUB
        return error_response(501, "gateway configuration is unavailable with the camera simulator");
#else
        const int result = utils::set_gateway(value);
        if (result != 0) return error_response(500, "gateway configuration failed");
        body = {{"retcode", result}};
        GOPTS->set_message("gateway: " + value);
#endif
    }
    return json_response(200, body);
}
}
