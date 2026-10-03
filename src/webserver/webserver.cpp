#include <iostream>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <memory>
#include "request_validation.h"
#include "camera.h"
#include <string>
#include <string.h>
#include <thread>
#include <unistd.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <poll.h>
#include "resources.h"


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
                if (i + 2 >= src.length() ||
                    !std::isxdigit(static_cast<unsigned char>(src[i+1])) ||
                    !std::isxdigit(static_cast<unsigned char>(src[i+2])))
                    throw std::invalid_argument("invalid URL escape");
                sscanf(src.substr(i+1,2).c_str(), "%x", &ii);
                ch = static_cast<char>(ii);
                ret += ch;
                i = i+2;
            } else if (src[i] == '+') {
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
        if (lines.empty()) return {};
        auto words = split_words_space(false, lines[0]);
        if (words.size() < 2) return {};
        auto url_parts = split_slash(false, words[1]);
        if (url_parts.size() >= 1) {
            std::string filename = url_parts.back();

            size_t index = filename.find_last_of(".");
            if (index != std::string::npos &&
                (filename.substr(index+1) == "html" || filename.substr(index+1) == "js" ||
                 filename.substr(index+1) == "css" || filename.substr(index+1) == "png" ||
                 filename.substr(index+1) == "jpg" || filename.substr(index+1) == "ico" ||
                 filename.substr(index+1) == "gz" || filename.substr(index+1) == "bin")) {
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
    // Nonblocking sends plus a monotonic deadline also bound slow clients.
    static bool write_all(int socket, const unsigned char *data, size_t size) {
        if (size && !data) return false;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        size_t offset = 0;
        while (offset < size) {
            auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now()).count();
            if (remaining <= 0) return false;
            ssize_t n = send(socket, data + offset, size - offset, MSG_NOSIGNAL | MSG_DONTWAIT);
            if (n > 0) { offset += static_cast<size_t>(n); continue; }
            if (n == 0) return false;
            if (errno == EINTR) continue;
            if (errno != EAGAIN && errno != EWOULDBLOCK) return false;
            pollfd pending{socket, POLLOUT, 0};
            int result = poll(&pending, 1, static_cast<int>(remaining));
            if (result < 0 && errno == EINTR) continue;
            if (result <= 0 || (pending.revents & (POLLERR | POLLHUP | POLLNVAL))) return false;
        }
        return true;
    }

    bool send_headers(std::string raw, int socket) {
        return write_all(socket, reinterpret_cast<const unsigned char *>(raw.data()), raw.size());
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
        return write_all(socket, data, sz);
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
    std::shared_ptr<Point2D_t> url_point_to_2d(std::vector<std::string> params, bool endpoint) {
        request::count(params, 2);
        const int width = GOPTS->color_mode >= 5 ? 1500 : CAM_WIDTH;
        const int height = GOPTS->color_mode >= 5 ? 165 : CAM_HEIGHT;
        auto ptr = std::make_shared<Point2D_t>();
        ptr->x = request::integer(params[0], 0, width - (endpoint ? 0 : 1));
        ptr->y = request::integer(params[1], 0, height - (endpoint ? 0 : 1));
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
        server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (server_fd < 0) {
            std::cerr << "Chiton: HTTP socket: " << strerror(errno) << std::endl;
            return -1;
        }

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
            std::cerr << "Chiton: unable to bind to final port " << port << ": " << strerror(bind_error) << std::endl;
            return -1;
        }
        if (listen(server_fd, 10) < 0) {
            std::cerr << "Chiton: unable to listen on port " << port << ": " << strerror(errno) << std::endl;
            close(server_fd);
            return -1;
        }
        std::cerr << "Chiton: web server listening on port " << port << std::endl;

        std::map<int, std::string> respMap;
        std::map<std::string, std::string> contentMap;
        build_content_map(&contentMap);
            
        respMap[500] = fail;
        respMap[200] = ok;
        respMap[404] = unknown;

        while (GOPTS->running > 0) {
            struct pollfd pending{server_fd, POLLIN, 0};
            int events = poll(&pending, 1, 100);
            if (events < 0 && errno == EINTR) continue;
            if (events < 0 || (pending.revents & (POLLERR | POLLHUP | POLLNVAL))) {
                std::cerr << "Chiton: HTTP poll failed: " << strerror(errno) << std::endl;
                close(server_fd);
                return -1;
            }
            if (events == 0 || GOPTS->running <= 0) continue;
            new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
            if (new_socket < 0) {
                if (errno == EAGAIN || errno == EINTR || errno == ECONNABORTED) continue;
                std::cerr << "Chiton: HTTP accept failed: " << strerror(errno) << std::endl;
                close(server_fd);
                return -1;
            }
            // Bound slow/idle clients so shutdown cannot stick in read/write.
            struct timeval timeout{0, 250000};
            setsockopt(new_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            setsockopt(new_socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

            int sz = 0;
            int response_code = 500;
            char buffer[30000] = {0};
            ssize_t num_read = read(new_socket, buffer, sizeof(buffer) - 1);

            if (num_read <= 3) {
                int e = shutdown(new_socket, 0);
                WARN("err socket read shutdown [%d]\n", e);
                e = close(new_socket);
                WARN("err socket read close [%d]\n", e);
                continue;
            }
        

            try {
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
                        cam::run_with_lock([]() { GOPTS->color_mode = 1; return 0; });
                    }
                    //std::cout << "serve name: " << filename << std::endl;
                    auto path = resources::asset(filename);
                    std::unique_ptr<uint8_t[]> data(path.empty() ? nullptr : get_data_u8(path, &sz));
                    if (path.empty()) sz = 0;
                    // we have a valid content type and read file bytes

                    if (sz >= 1 && data != NULL) {
                        //WARN_IFNOT(send_data(new_socket, type->second, data, sz), "unable to serve");
                        if (send_data(new_socket, type->second, data.get(), sz) == false) {
                            printf("error sending file\n");    
                        }
                        response_code = 0;
                    } else {
                        std::cout << "unable to serve file: " << filename << std::endl;
                    }
                }

                if (response_code > 0) {
                    auto search2 = respMap.find(response_code);
                    if (search2 != respMap.end()) {
                        if (!send_headers(respMap[response_code], new_socket)) {
                            printf("unable to write full response\n");
                        }
                    }
                    WARN_IF(search2 == respMap.end(), "unable to find response for code %d", response_code);
                }
            }

            } catch (const std::invalid_argument &) {
                send_headers("HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", new_socket);
            } catch (const std::out_of_range &) {
                send_headers("HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", new_socket);
            } catch (...) {
                send_headers("HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n", new_socket);
            }
            // close the socket cleanly 
            WARN_IF(shutdown(new_socket, 0) != 0, "socket shutdown error");
            WARN_IF(close(new_socket) != 0, "socket close error");
        }

        close(server_fd);
        WARN("web server exited\n");
        return 0;
    }

}