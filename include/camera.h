#include <aditof/camera.h>
#include <aditof/frame.h>
#include <aditof/system.h>

#include "utils.hpp"
#include "webserver.h"


namespace cam {
    std::recursive_mutex &state_mutex();
    web::HttpResponse set_capture(bool enabled);
    
    int update_setting(std::vector<std::string> params, std::string method_name, uint min_params, std::function<int()> fn);
    int run_with_lock(std::function<int()> fn);
    int wait_for_lock(std::function<int()> fn);
    // int camera_fn(std::vector<std::string> params, int socket);
}