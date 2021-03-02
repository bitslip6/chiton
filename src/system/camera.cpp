#include <aditof/camera.h>
#include <aditof/frame.h>
#include <aditof/system.h>

#include "utils.hpp"
#include "webserver.h"

#include <atomic>

extern unsigned char *jpeg_data1;

std::atomic<int> camera_waiting(0);

namespace cam
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
            return fn();
        }
        return 404;
    }

    /**
     * execute function and provide exclusive locking around the camera
     */
    int run_with_lock(std::function<int()> fn) {
        utils::Wait_Until([]() { return camera_waiting == 0; });
        camera_waiting = 1;
        auto result = fn();
        camera_waiting = 0;
        return result;
    }
    

    /**
     * camera functions
     * params vector of url parts 
     * socket a connected socket
     */
     /*
    int camera_fn(std::vector<std::string> params, int socket) {
        
        std::string data = "{404:404}";
        int resp = 404;

        // turn the camera on
        resp = (resp != 404) ? resp : update_setting(params, "enable", 1, [&]() {
            utils::adi_call([&camera]() { return camera->start(); });
            GOPTS->enable = true;
            return 200;
        });

        
        // turn the camera off
        resp = (resp != 404) ? resp : update_setting(params, "disable", 1, [&]() {
            GOPTS->enable = false;
            utils::adi_call([&camera]() { return camera->stop(); });
            return 200;
        });


        // send the jpeg data
        resp = (resp != 404) ? resp : update_setting(params, "image", 1, [&]() {
            jlock = 1;
            WARN_IFNOT(
                send_data(socket, std::string("image/jpeg"), jpeg_data1, jlenptr),
                "unable to send jpeg data");
            jlock = 0;
            return 200;
        });
        
        if (params[0].compare(std::string("raw")) == 0) {
            WARN_IFNOT(
                send_data(socket, std::string("application/octet-stream"), (unsigned char*) copy, CAM_WIDTH*CAM_HEIGHT*2);
                "unable to send raw data stream");
            return 200;
        }

        return 404;
    }
    */

}
