#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <unistd.h>
#include <iomanip>

#include "system.h"
#include "webserver.h"
#include "utils.hpp"
#include "rover.h"

#include "fplus/fplus.hpp"

namespace web
{

    /**
     * TODO: find a better way to only set the return value than the turnary here
     */
    int rover_fn(std::vector<std::string> params, int socket) {
        Return_IfNot(params.size() >= 1, "params empty", 404);

        std::string url_method = params[0];
        std::string data = "{404:404}";
        int resp = 404;

        // upgrade the setting
        if (params.size() > 2) {

            INFO("rover direction begin [%s]", params[1].c_str());

            // begin motor drive
            if (url_method.compare("motorbegin") == 0) {
                data = "OK";
                INFO("motorbegin complete [%s]", data.c_str());
            }

            // end motor drive
            if (url_method.compare("motorend") == 0) {
                data = "OK";
                INFO("motorend complete [%s]", data.c_str());
            }
        }

        // system restart
        if (url_method.compare("restart") == 0) {
            WARN("rover restart");
            data = "{restart:1}";
            resp = 200;
        }

        std::string raw = "HTTP/1.1 " + std::to_string(resp) + " OK\nContent-Type: application/json\nAccess-Control-Allow-Origin: *\nContent-Length: " + std::to_string(data.length()) + "\n\n" + data;
        send_headers(raw, socket);
        return resp;
    }

}