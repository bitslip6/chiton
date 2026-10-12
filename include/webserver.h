#ifndef WEBSERVER_H
#define WEBSERVER_H 1
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "demos.h"
#include "json.hpp"

#ifndef UNUSED
#define UNUSED __attribute__((unused))
#endif

namespace web {
    // Handlers construct an owned response; only the dispatcher writes to sockets.
    // Binary bodies may contain NUL bytes. There is exactly one response per request.
    struct HttpResponse {
        int status = 200;
        std::string content_type = "application/json; charset=utf-8";
        std::string content;
        std::string content_encoding;
        std::map<std::string, std::string> headers; // Snapshot metadata, not image pixels.
    };
    using RequestHandler = std::function<HttpResponse(std::vector<std::string>, const std::string &)>;
    HttpResponse json_response(int status, const nlohmann::json &body);
    HttpResponse error_response(int status, const std::string &message);
    HttpResponse binary_response(std::string content_type, const void *data, size_t size);
    bool send_response(int socket, const HttpResponse &response, bool head_only = false);

    std::shared_ptr<Point2D_t> url_point_to_2d(std::vector<std::string> params, bool endpoint = false);
    std::vector<std::string> parse_url(char *request);
    bool buffered_write(int socket, unsigned char *data, unsigned int sz);
    bool send_headers(std::string raw, int socket);
    bool send_data(int socket, std::string content_type, unsigned char *data, unsigned int sz);
    bool send_precompressed_data(int socket, std::string content_type, unsigned char *data, unsigned int sz);
    int web_server(int port, std::map<std::string, RequestHandler> handlers);
    HttpResponse setting_fn(std::vector<std::string> params);
    HttpResponse software_fn(std::vector<std::string> params);
    HttpResponse rover_fn(std::vector<std::string> params);
    std::string url_decode(std::string &src);
}
#endif
