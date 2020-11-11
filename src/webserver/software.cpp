#include <string>
#include <vector>
#include <map>
#include <unistd.h>

#include "webserver.h"
#include "utils.hpp"

namespace web
{

    /**
     * TODO: find a better way to only set the return value than the turnary here
     */
    int software_fn(std::vector<std::string> params, int socket) {
        
        std::string data = "{404:404}";
        int resp = 404;

        // upgrade the setting
        if (params.size() > 2) {
            std::string url_method = params[0];
            std::string version = params[2];

            if (url_method.compare("upgrade") == 0) {
                INFO("software upgrade %s", version.c_str());
                auto result_vector = utils::get_command_output_lines("/usr/local/chiton/upgrade.sh " + version);
            }
        }

        std::string raw = "HTTP/1.1 " + std::to_string(resp) + " OK\nContent-Type: application/json\nAccess-Control-Allow-Origin: *\nContent-Length: " + std::to_string(data.length()) + "\n\n" + data;
        send_headers(raw, socket);

        return resp;
    }

}