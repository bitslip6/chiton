#include <string>
#include <vector>
#include <map>
#include <unistd.h>

#include "system.h"
#include "webserver.h"
#include "utils.hpp"

//#include "fplus/fplus.hpp"

namespace web
{

    std::string make_debug_info() {
        TRACE("software chitondebug");

        //std::string s = std::string("rm -rf");
        std::string cmd1 = std::string("rm -rf ") + TMP_DIR;
        std::string cmd2 = std::string("mkdir ") + TMP_DIR;
        system(cmd1.c_str());
        system(cmd2.c_str());

        utils::get_i2cdetect_output();
        utils::save_command_output("cat /var/log/syslog", TMP_DIR + "/syslog.txt");
        utils::save_command_output("dmesg", TMP_DIR + "/dmesg.txt");
        utils::save_command_output("cat console_log.txt", TMP_DIR + "/console_log.txt");
        utils::save_command_output("ifconfig -a", TMP_DIR + "/ifconfig.txt");
        utils::save_command_output("netstat -nr", TMP_DIR + "/netstat.txt");
        utils::save_command_output("iwconfig", TMP_DIR + "/iwconfig.txt");

        std::string tar_name = utils::compress_debuginfo();

        std::string data = "{\"value\":\"" + tar_name + "\"}";
        return data;
    }
    
    /**
     * TODO: find a better way to only set the return value than the turnary here
     */
    int software_fn(std::vector<std::string> params, int socket) {
        Return_IfNot(params.size() >= 1, "params empty", 404);

        std::string url_method = params[0];
        std::string data = "{404:404}";
        int resp = 404;

        // upgrade the setting
        if (params.size() > 2) {
            int version = utils::clamp(params[2], 1, 99);

            // software upgrade
            if (url_method.compare("upgrade") == 0) {
                INFO("software upgrade %d", version);
                int ret = 0;
                auto result_vector = utils::get_command_output_lines("/usr/local/chiton/upgrade.sh " + version, &ret);
                data = "{upgrade:" + std::to_string(version) + "}";
                resp = 200;
                INFO("command result");
            }
        }

        // system restart
        if (url_method.compare("restart") == 0) {
            PANIC("system restarting");
            data = "{restart:1}";
            resp = 200;
            GOPTS->running = EXIT_UPGRADE;
        }

        // make debug info
        if (url_method.compare("debug") == 0) {
            WARN("DEBUG DATA");
            data = make_debug_info();
            resp = 200;
        }

        std::string raw = "HTTP/1.1 " + std::to_string(resp) + " OK\nContent-Type: application/json\nAccess-Control-Allow-Origin: *\nContent-Length: " + std::to_string(data.length()) + "\n\n" + data;
        send_headers(raw, socket);

        return resp;
    }

}