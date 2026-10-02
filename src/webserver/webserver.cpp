#include <iostream>
#include <cerrno>
#include <string>
#include <string.h>
#include <thread>
#include <unistd.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>


#include "fplus/fplus.hpp"
#include "util.h"
#include "utils.hpp"
#include "demos.h"
#include "data.h"
#include "image.h"
#include "webserver.h"
#include "system.h"


namespace web
{

    template <typename String, typename ContainerOut = std::vector<String>>
    ContainerOut split_words_space(const bool allowEmpty, const String &str) {
        return fplus::split_by(fplus::is_whitespace<String>, allowEmpty, str);
    }

    template <typename String>
    bool is_slash(const typename String::value_type& c) {
        return static_cast<unsigned char>(c) == '/' || static_cast<unsigned char>(c) == '?';
    }

    template <typename String, typename ContainerOut = std::vector<String>>
    ContainerOut split_slash(const bool allowEmpty, const String& str) {
        return fplus::split_by(is_slash<String>, allowEmpty, str);
    }

    // ugly AF. SO rip. needs functional refactor.
    // TODO: functional refactor
    std::string url_decode(std::string &src) {
        std::string ret;
        char ch;
        unsigned int i, ii;
        for (i=0; i<src.length(); i++) {
            if (src[i] == '%') {
                sscanf(src.substr(i+1,2).c_str(), "%x", &ii);
                ch = static_cast<char>(ii);
                ret += ch;
                i = i+2;
            } else if (src['i'] == '+') {
                ret += ' ';
            } else {
                ret += src[i];
            }
        }

        return ret;
    }

    /**
     * parse an HTTP request into a vector of url parts split on / and .
     */
    std::vector<std::string> parse_url(char *request) {
        std::string s(request);
        std::string s2 = url_decode(s);
        auto lines = fplus::split_lines(false, s2);
        auto words = split_words_space(false, lines[0]);
        
        auto url_parts = split_slash(false, words[1]);
        if (url_parts.size() >= 1) {
            std::string filename = url_parts.back();

            size_t index = filename.find_last_of(".");
            if (index != std::string::npos) {
                url_parts.pop_back();
                url_parts.emplace_back(filename.substr(0, index));
                url_parts.emplace_back(filename.substr(index+1));
            } else {
                //printf("NO last file name extension\n");
            }
            auto file2 = url_parts.back();
        }

        return url_parts;
    }

    /**
     * send raw header string to socket
     * return bool true if the header was sent correctly
     */
    bool send_headers(std::string raw, int socket) {
        int wrote = write(socket, raw.c_str(), raw.length());
        if (utils::su_cmp(wrote, raw.length()) != 0) {
            return false;
        }

        return true;
    }

    /**
     * send *data to socket with content type content_type
     * return bool true if the data is sent correctly
     */
    bool send_data(int socket, std::string content_type, unsigned char *data, unsigned int sz) {
        std::string raw = "HTTP/1.1 200 OK\nContent-Type: "+content_type+"\nContent-Length: "
            + std::to_string(sz) + "\nAccess-Control-Allow-Origin: *\n\n";
        bool success = send_headers(raw, socket);
        //std::cout << "send headers: " << success << std::endl;
        if (!success) {
            return false;
        }
        
        return buffered_write(socket, data, sz);
        
        /*

        int towrite = sz;
        int bufsz = std::min(1500, towrite);
        while (towrite > 0) {
            int wrote = write(socket, data+(sz-towrite), bufsz);
            if (wrote < bufsz) {
                WARN("wrote socket [%d] wrote [%d] bytes [%d] [%s] towrite:[%d/%u]\n", socket, wrote, errno, strerror(errno), towrite, sz);
                break;
            }
            bufsz = std::min(towrite, bufsz);
            towrite -= wrote;
        }
        if (towrite > 0) {
            WARN("wrote socket [%d] wrote [%d] bytes [%d] [%s] towrite:[%d/%u]\n", socket, sz-towrite, errno, strerror(errno), towrite, sz);
        }
        return (towrite <= 0);
        */
    }

    /**
     * send *data to socket with content type content_type
     * return bool true if the data is sent correctly
     */
    bool send_precompressed_data(int socket, std::string content_type, unsigned char *data, unsigned int sz) {
        std::string raw = "HTTP/1.1 200 OK\nContent-Type: "+content_type+"\nContent-Length: "
            + std::to_string(sz) + "\nAccess-Control-Allow-Origin: *\nContent-Encoding: gzip\n\n";
        bool success = send_headers(raw, socket);

        if (!success) {
            return false;
        }
        
        return buffered_write(socket, data, sz);
    }

    bool buffered_write(int socket, unsigned char *data, unsigned int sz) {
        int towrite = sz;
        int bufsz = std::min(1500, towrite);
        while (towrite > 0) {
            int wrote = write(socket, data+(sz-towrite), bufsz);
            if (wrote < bufsz) {
                WARN("wrote socket [%d] wrote [%d] bytes [%d] [%s] towrite:[%d/%u]\n", socket, wrote, errno, strerror(errno), towrite, sz);
                break;
            }
            bufsz = std::min(towrite, bufsz);
            towrite -= wrote;
        }
        if (towrite > 0) {
            WARN("wrote socket [%d] wrote [%d] bytes [%d] [%s] towrite:[%d/%u]\n", socket, sz-towrite, errno, strerror(errno), towrite, sz);
        }
        return (towrite <= 0);
    }



    void build_content_map(std::map<std::string, std::string> *contentMap) {
        contentMap->insert(std::make_pair("ico", "image/jpeg"));
        contentMap->insert(std::make_pair("png", "image/png"));
        contentMap->insert(std::make_pair("jpg", "image/jpeg"));
        contentMap->insert(std::make_pair("jpeg", "image/jpeg"));
        contentMap->insert(std::make_pair("js", "text/javascript"));
        contentMap->insert(std::make_pair("json", "application/json"));
        contentMap->insert(std::make_pair("css", "text/css"));
        contentMap->insert(std::make_pair("bin", "application/octet-stream"));
        contentMap->insert(std::make_pair("csv", "text/csv"));
        contentMap->insert(std::make_pair("gz", "application/gzip"));
        contentMap->insert(std::make_pair("html", "text/html"));
    }


    /**
     * convert a vector of url parts to x/y point url [get, value, 100, 200] = Point2D(100, 200)
     */
    std::shared_ptr<Point2D_t> url_point_to_2d(std::vector<std::string> params) {
        auto ptr = std::shared_ptr<Point2D_t>(new Point2D_t());
        const int max_width = 1500;//data::get_frame_width();
        const int max_height = 480;//data::get_frame_height();

        ptr->x = max_width/2;
        ptr->y = max_height/1.5;
        
        Return_If(params.size() < 2, "get/value requires 2 parameters", ptr);
        
        ptr->x = (s16)utils::clamp(params[0], 0, max_width);
        ptr->y = (s16)utils::clamp(params[1], 0, max_height);
        return ptr;
    }



    /**
     * run the web server until GOPTS->running is false
     */
    int web_server(int port, std::map<std::string, IntCallbackString> fnMap) {
        int server_fd, new_socket;
        struct sockaddr_in address;
        int addrlen = sizeof(address);

        std::string ok = "HTTP/1.1 200 OK\nContent-Type: text/plain\nContent-Length: 12\r\n\r\nOK!";
        std::string fail = "HTTP/1.1 500 OK\nContent-Type: text/plain\nContent-Length: 12\r\n\r\nFAIL";
        std::string unknown = "HTTP/1.1 404 OK\nContent-Type: text/plain\nContent-Length: 12\r\n\r\nFAIL";

        // Creating socket file descriptor
        PANIC_IF((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0, "unable to create socket", -1);

        int enable = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        INFO("start web server");
        memset(address.sin_zero, '\0', sizeof address.sin_zero);

        int bind_result = bind(server_fd, (struct sockaddr *)&address, sizeof(address));
        if (bind_result < 0 && port == 80) {
            const int bind_error = errno;
            std::cerr << "Chiton: cannot bind to port 80 (" << strerror(bind_error)
                      << "); falling back to port 8080" << std::endl;
            port = 8080;
            address.sin_port = htons(port);
            bind_result = bind(server_fd, (struct sockaddr *)&address, sizeof(address));
        }
        if (bind_result < 0) {
            const int bind_error = errno;
            close(server_fd);
            PANIC("Unable to bind to port %d: %s", port, strerror(bind_error));
            exit(EXIT_FAILURE);
        }
        PANIC_IF(listen(server_fd, 10) < 0, "unable to listen", -1);
        std::cerr << "Chiton: web server listening on port " << port << std::endl;

        std::map<int, std::string> respMap;
        std::map<std::string, std::string> contentMap;
        build_content_map(&contentMap);
            
        respMap[500] = fail;
        respMap[200] = ok;
        respMap[404] = unknown;

        while (GOPTS->running > 0) {
            PANIC_IF((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0, "unable to accept", -1);

            int sz;
            int response_code = 500;
            char buffer[30000] = {0};
            size_t num_read = read(new_socket, buffer, 30000);

            if (num_read <= 3) {
                int e = shutdown(new_socket, 0);
                WARN("err socket read shutdown [%d]\n", e);
                e = close(new_socket);
                WARN("err socket read close [%d]\n", e);
                continue;
            }
        

            auto url_parts = parse_url(buffer);

            // no valid url path, serve the homepage...
            if (url_parts.size() < 2) {
                //utils::init_options();
                const char* default_page = "GET /app.html";
                url_parts = parse_url((char *)default_page);
                INFO("no page, serving default homepage");
            }
            // find the function to execute
            auto search = fnMap.find(url_parts[1]);

            if (search != fnMap.end()) {
                //char tmp[64];
                //snprintf(tmp, MIN(64, strstr(buffer, "HTTP")), "%s", buffer);
                //snprintf(tmp, MIN(64, strstr(buffer, "HTTP")), "%s", buffer);
                std::string tmp(buffer);
                std::size_t idx = tmp.find("HTTP");
                TIMER2(tmp.substr(4, idx).c_str(), 100, fnMap[url_parts[1]](url_parts, new_socket));
            }
            // no function, try and serve content as a file
            else {
                //std::cout << "serve file func: " << url_parts.back() << std::endl;
                response_code = 404;
                // find the content type from the file extension
                auto type = contentMap.find(url_parts.back());
                if (type != contentMap.end()) {
                    //std::cout << "serve type" << type->second << std::endl;
                    std::string filename = url_parts.at(url_parts.size()-2) + "." + url_parts.back();
                    // make sure we flip back to sync with new client webpage...
                    if (filename == "app.html") {
                        GOPTS->color_mode = 1;
                    }
                    //std::cout << "serve name: " << filename << std::endl;
                    uint8_t *data = get_data_u8("files/"+filename, &sz);
                    // we have a valid content type and read file bytes

                    if (sz >= 1 && data != NULL) {
                        //WARN_IFNOT(send_data(new_socket, type->second, data, sz), "unable to serve");
                        if (send_data(new_socket, type->second, data, sz) == false) {
                            printf("error sending file\n");    
                        }
                        delete[] data;
                        response_code = 0;
                    } else {
                        std::cout << "unable to serve file: " << filename << std::endl;
                    }
                }

                if (response_code > 0) {
                    auto search2 = respMap.find(response_code);
                    if (search2 != respMap.end()) {
                        unsigned int wrote = write(new_socket, respMap[response_code].c_str(), respMap[response_code].length());
                        if (wrote != respMap[response_code].length()) {
                            printf("unable to write full response\n");
                        }
                    }
                    WARN_IF(search2 == respMap.end(), "unable to find response for code %d", response_code);
                }
            }

            // close the socket cleanly 
            WARN_IF(shutdown(new_socket, 0) != 0, "socket shutdown error");
            WARN_IF(close(new_socket) != 0, "socket close error");
        }

        WARN("web server exited\n");
        return 0;
    }

}