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
        auto lines = fplus::split_lines(false, std::string(request));
        if (lines.empty()) throw std::invalid_argument("missing request line");
        auto words = split_words_space(false, lines[0]);
        if (words.size() != 3 || words[1].empty() || words[1][0] != '/' ||
            (words[2] != "HTTP/1.0" && words[2] != "HTTP/1.1"))
            throw std::invalid_argument("invalid request line");
        // Queries are not path arguments. Decode individual segments, not headers
        // or the complete path (encoded spaces/slashes must not change routing).
        auto url_parts = fplus::split('/', false, words[1].substr(0, words[1].find('?')));
        for (auto &part : url_parts) {
            part = url_decode(part);
            if (part.find('\0') != std::string::npos || part.find_first_of("\r\n") != std::string::npos)
                throw std::invalid_argument("invalid path character");
        }
        if (url_parts.size() == 1) {
            std::string filename = url_parts.back();

            size_t index = filename.find_last_of(".");
            if (index != std::string::npos &&
                (filename.substr(index+1) == "html" || filename.substr(index+1) == "js" ||
                 filename.substr(index+1) == "css" || filename.substr(index+1) == "png" ||
                 filename.substr(index+1) == "jpg" || filename.substr(index+1) == "ico" ||
                 filename.substr(index+1) == "gz" || filename.substr(index+1) == "bin" ||
                 filename.substr(index+1) == "json" || filename.substr(index+1) == "csv" ||
                 filename.substr(index+1) == "svg" || filename.substr(index+1) == "jpeg")) {
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

    HttpResponse json_response(int status, const nlohmann::json &body) {
        return {status, "application/json; charset=utf-8", body.dump(), ""};
    }

    HttpResponse error_response(int status, const std::string &message) {
        return json_response(status, {{"error", message}, {"status", status}});
    }

    HttpResponse binary_response(std::string content_type, const void *data, size_t size) {
        if (size && !data) throw std::runtime_error("unavailable response data");
        return {200, std::move(content_type),
                size ? std::string(static_cast<const char *>(data), size) : std::string(), ""};
    }

    static const char *reason_phrase(int status) {
        switch (status) {
            case 200: return "OK";
            case 400: return "Bad Request";
            case 404: return "Not Found";
            case 405: return "Method Not Allowed";
            case 409: return "Conflict";
            case 500: return "Internal Server Error";
            case 501: return "Not Implemented";
            case 503: return "Service Unavailable";
            case 504: return "Gateway Timeout";
            default: return "Unknown Status";
        }
    }

    bool send_response(int socket, const HttpResponse &response, bool head_only) {
        std::string header = "HTTP/1.1 " + std::to_string(response.status) + " " +
            reason_phrase(response.status) + "\r\nContent-Type: " + response.content_type +
            "\r\nContent-Length: " + std::to_string(response.content.size()) +
            "\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n";
        if (!response.content_encoding.empty())
            header += "Content-Encoding: " + response.content_encoding + "\r\n";
        if (response.status == 405) header += "Allow: GET\r\n";
        header += "\r\n";
        return send_headers(header, socket) && (head_only ||
            write_all(socket, reinterpret_cast<const unsigned char *>(response.content.data()),
                      response.content.size()));
    }

    bool send_data(int socket, std::string content_type, unsigned char *data, unsigned int sz) {
        return send_response(socket, binary_response(std::move(content_type), data, sz));
    }

    bool send_precompressed_data(int socket, std::string content_type, unsigned char *data, unsigned int sz) {
        auto response = binary_response(std::move(content_type), data, sz);
        response.content_encoding = "gzip";
        return send_response(socket, response);
    }

    bool buffered_write(int socket, unsigned char *data, unsigned int sz) {
        return write_all(socket, data, sz);
    }



    void build_content_map(std::map<std::string, std::string> *contentMap) {
        contentMap->insert(std::make_pair("ico", "image/x-icon"));
        contentMap->insert(std::make_pair("svg", "image/svg+xml"));
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
    int web_server(int port, std::map<std::string, RequestHandler> fnMap) {
        int server_fd, new_socket;
        struct sockaddr_in address;
        int addrlen = sizeof(address);

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

        std::map<std::string, std::string> contentMap;
        build_content_map(&contentMap);

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

            char buffer[30000] = {0};
            ssize_t num_read = read(new_socket, buffer, sizeof(buffer) - 1);

            if (num_read <= 3) {
                if (num_read > 0) send_response(new_socket, error_response(400, "incomplete request line"));
                int e = shutdown(new_socket, 0);
                WARN("err socket read shutdown [%d]\n", e);
                e = close(new_socket);
                WARN("err socket read close [%d]\n", e);
                continue;
            }
        

            HttpResponse response;
            bool head_only = false;
            try {
                auto parts = parse_url(buffer);
                const auto method = split_words_space(false, std::string(buffer))[0];
                head_only = method == "HEAD";
                if (parts.empty()) parts = {"app", "html"};
                auto handler = parts.size() >= 2 ? fnMap.find(parts[1]) : fnMap.end();
                if (method != "GET" && !(head_only && handler == fnMap.end())) {
                    response = error_response(405, "unsupported HTTP method");
                } else if (handler != fnMap.end()) {
                    response = handler->second(std::move(parts));
                } else {
                    response = error_response(404, "resource not found");
                    auto type = parts.size() == 2 ? contentMap.find(parts.back()) : contentMap.end();
                    if (type != contentMap.end() && parts[0].find_first_of("/\\") == std::string::npos) {
                        const auto path = resources::asset(parts[0] + "." + parts[1]);
                        int size = 0;
                        std::unique_ptr<uint8_t[]> bytes(path.empty() ? nullptr : get_data_u8(path, &size));
                        if (bytes) {
                            // Preserve the development UI's legacy reset-on-load;
                            // HEAD must never change camera state.
                            if (!head_only && parts[0] == "app" && parts[1] == "html")
                                cam::run_with_lock([] { GOPTS->color_mode = 1; return 0; });
                            response = binary_response(type->second, bytes.get(), size);
                        }
                    }
                }
            } catch (const std::invalid_argument &error) {
                response = error_response(400, error.what());
            } catch (const std::out_of_range &error) {
                response = error_response(400, error.what());
            } catch (const std::exception &error) {
                response = error_response(500, error.what());
            } catch (...) {
                response = error_response(500, "request failed");
            }
            // Never append an error response after a partial socket write.
            if (!send_response(new_socket, response, head_only))
                WARN("unable to send complete HTTP response");
            // close the socket cleanly 
            WARN_IF(shutdown(new_socket, 0) != 0, "socket shutdown error");
            WARN_IF(close(new_socket) != 0, "socket close error");
        }

        close(server_fd);
        WARN("web server exited\n");
        return 0;
    }

}