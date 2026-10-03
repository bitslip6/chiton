#include <aditof/camera.h>
#include <aditof/frame.h>
#include <aditof/system.h>

#include "utils.hpp"
#include "webserver.h"

#include <atomic>

extern unsigned char *jpeg_data1;



namespace cam
{
    // One owner for SDK access, settings, geometry and the complete frame pipeline.
    // Recursive because existing SDK helpers also enter this ownership boundary.
    std::recursive_mutex &state_mutex() {
        static std::recursive_mutex mutex;
        return mutex;
    }

    /**
     * update a single setting in chiton, executes lambda if url matches
     * [params]  a vector of / separated url parameters
     * [method name]  the method name in params to invoke the lambda if there is a match
     * [min_params]  is the minimum required parameters to execute the lambda
     */
    int update_setting(std::vector<std::string> params, std::string method_name, uint min_params, std::function<int()> fn) {

        if (params.empty()) throw std::invalid_argument("missing camera action");
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
        std::lock_guard<std::recursive_mutex> guard(state_mutex());
        return fn();
    }

    int wait_for_lock(std::function<int()> fn) {
        return run_with_lock(fn);
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
