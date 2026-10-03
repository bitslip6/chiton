#include <cstdlib>
#include <iomanip>
#include <sstream>
#include "system.h"
#include "webserver.h"
#include "utils.hpp"
#include "request_validation.h"

namespace web {
HttpResponse software_fn(std::vector<std::string> params) {
    if (params.size() < 2) throw std::invalid_argument("missing software action");
    const auto &action = params[0];
    if (action != "upgrade" && action != "restart" && action != "debug")
        return error_response(404, "unknown software action");
    request::count(params, action == "upgrade" ? 3 : 2);
    if (action == "restart") {
        GOPTS->set_message("system restarting");
        GOPTS->running = EXIT_UPGRADE;
        return json_response(200, {{"restart", 1}});
    }
    if (action == "upgrade") {
        const int version = request::integer(params[2], 1, 999);
#ifdef CHITON_ADITOF_STUB
        return error_response(501, "software upgrade is unavailable with the camera simulator");
#else
        std::ostringstream padded;
        padded << std::setfill('0') << std::setw(3) << version;
        GOPTS->set_message("upgrade " + padded.str());
        int result = -1;
        utils::get_command_output_lines("sudo /usr/local/chiton/upgrade.sh " + padded.str(), &result);
        if (result != 0) {
            GOPTS->set_message("upgrade FAIL\nretry");
            return error_response(500, "software upgrade failed");
        }
        GOPTS->set_message("upgrade OK\nreboot");
        return json_response(200, {{"upgrade", version}});
#endif
    }
#ifdef CHITON_ADITOF_STUB
    return error_response(501, "system diagnostics are unavailable with the camera simulator");
#else
    if (system("sudo rm -rf /usr/local/chiton/debuginfo && mkdir /usr/local/chiton/debuginfo") != 0)
        return error_response(500, "unable to create diagnostics directory");
    utils::get_rclocal_output();
    utils::get_dmesg_output();
    utils::get_console_log_output();
    utils::get_i2cdetect_output();
    utils::get_network_status_output();
    return json_response(200, {{"value", utils::compress_debuginfo()}});
#endif
}
}
