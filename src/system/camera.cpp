#include <aditof/camera.h>
#include <aditof/frame.h>
#include <aditof/system.h>
#include <glog/logging.h>

#include "utils.hpp"
#include "webserver.h"


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
 * camera functions
 * params vector of url parts 
 * socket a connected socket
 */
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
        utils::Warn_IfNot(
            send_data(socket, std::string("image/jpeg"), jpeg_data1, jlenptr),
            "unable to send jpeg data", __LINE__, __FILE__);
        jlock = 0;
        return 200;
    });
    
    if (params[0].compare(std::string("raw")) == 0) {
        utils::Warn_IfNot(
            send_data(socket, std::string("application/octet-stream"), (unsigned char*) copy, 640*480*2);
            "unable to send raw data stream", __LINE__, __FILE__);
        return 200;
    }

    return 404;
}
