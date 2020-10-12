#include <iostream>
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
#include "image.h"
#include "webserver.h"

template <typename String, typename ContainerOut = std::vector<String>>
ContainerOut split_words_space(const bool allowEmpty, const String &str) {
    return fplus::split_by(fplus::is_whitespace<String>, allowEmpty, str);
}

template <typename String>
bool is_slash(const typename String::value_type& c)
{
    return static_cast<unsigned char>(c) == '/';
}

template <typename String, typename ContainerOut = std::vector<String>>
ContainerOut split_slash(const bool allowEmpty, const String& str)
{
    return split_by(logical_not(is_slash<String>), allowEmpty, str);
}

/**
 * parse an HTTP request into a vector of url parts split on / and .
 */
std::vector<std::string> parse_url(char *request) {
    std::string s(request);
    auto lines = fplus::split_lines(false, s);
    auto words = split_words_space(false, lines[0]);
    auto url_parts = fplus::split_words(false, words[1]);
    //auto url_parts = split_by(is_slash<String>, allowEmpty, str);
    //return split_by(logical_not(is_letter_or_digit<String>), allowEmpty, str);
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
        +std::to_string(sz) + "\n\n";
    bool success = send_headers(raw, socket);
    //std::cout << "send headers: " << success << std::endl;
    if (!success) {
        return false;
    }
    

    int towrite = sz;
    int bufsz = std::min(1500, towrite);
    while (towrite > 0) {
        int wrote = write(socket, data+(sz-towrite), bufsz);
        //printf("towrite [%d] / [%d]\n", towrite, wrote);
        if (wrote < bufsz) {
            printf("wrote socket [%d] wrote [%d] bytes [%d] [%s] towrite:[%d/%u]\n", socket, wrote, errno, strerror(errno), towrite, sz);
            break;
        }
        bufsz = std::min(towrite, bufsz);
        towrite -= wrote;
    }
    if (towrite > 0) {
        printf("wrote socket [%d] wrote [%d] bytes [%d] [%s] towrite:[%d/%u]\n", socket, sz-towrite, errno, strerror(errno), towrite, sz);
    }
    return (towrite <= 0);
}


/**
 * run the web server until GOPTS->running is false
 */
int web_server(int port, std::map<std::string, IntCallbackString> fnMap) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    std::string ok = "HTTP/1.1 200 OK\nContent-Type: text/plain\nContent-Length: 12\n\nOK!";
    std::string fail = "HTTP/1.1 500 OK\nContent-Type: text/plain\nContent-Length: 12\n\nFAIL";
    std::string unknown = "HTTP/1.1 404 OK\nContent-Type: text/plain\nContent-Length: 12\n\nFAIL";

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("In socket");
        exit(EXIT_FAILURE);
    }

    int enable = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

	printf("!!! web server start...\n");
    memset(address.sin_zero, '\0', sizeof address.sin_zero);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("In bind");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 10) < 0) {
        perror("In listen");
        exit(EXIT_FAILURE);
    }

    std::map<int, std::string> respMap;
    std::map<std::string, std::string> contentMap;
    contentMap["ico"] = "image/jpeg";
    contentMap["png"] = "image/png";
    contentMap["jpg"] = "image/jpeg";
    contentMap["jpeg"] = "image/jpeg";
    contentMap["js"] = "text/javascript";
    contentMap["json"] = "application/json";
    contentMap["css"] = "text/css";
    contentMap["bin"] = "application/octet-stream";
    contentMap["csv"] = "text/csv";
    contentMap["gz"] = "application/gzip";
    contentMap["html"] = "text/html";

    
    respMap[500] = fail;
    respMap[200] = ok;
    respMap[404] = unknown;

	printf("about to begin server...\n");
    while (GOPTS->running) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("In accept");
            exit(EXIT_FAILURE);
        }
        //printf("post accept\n");

        int sz;
        int response_code = 500;
        char buffer[30000] = {0};
        size_t num_read = read(new_socket, buffer, 30000);
        //printf("post read [%d]\n", num_read);
        if (num_read <= 3) {
            int e = shutdown(new_socket, 0);
            //printf("err read shutdown [%d]\n", e);
            e = close(new_socket);
            //printf("err read close [%d]\n", e);
            continue;
        }
    
        auto url_parts = parse_url(buffer);
        //printf("parse url: [%d]\n", url_parts.size());

        // no valid url path, serve the homepage...
        if (url_parts.size() < 2) {
            utils::init_options();
            const char* default_page = "GET /app.html";
            url_parts = parse_url((char *)default_page);
            std::cout << "serve homepage: " << url_parts[0] << url_parts[1] << std::endl;
        }
        // find the function to execute
        auto search = fnMap.find(url_parts[1]);
        //std::cout << "found fnmap: " << url_parts[0] << " : " << url_parts[1] << std::endl;

        if (search != fnMap.end()) {
            //std::cout << "run fn" << std::endl;
            response_code = fnMap[url_parts[1]](url_parts, new_socket);
            //std::cout << "fn ret: " << response_code << std::endl;
        }
        // no function, try and serve content as a file
        else {
            std::cout << "serve file func: " << url_parts[1] << std::endl;
            response_code = 404;
            // find the content type from the file extension
            auto type = contentMap.find(url_parts[1]);
            if (type != contentMap.end()) {
                std::cout << "serve type" << type->second << std::endl;
                std::string filename = url_parts[0] + "." + url_parts[1];
                uint8_t *data = get_data_u8(filename, &sz);
                // we have a valid content type and read file bytes

                if (sz >= 1 && data != NULL) {
                    //WARN_IFNOT(send_data(new_socket, type->second, data, sz), "unable to serve");
                    if (send_data(new_socket, type->second, data, sz) == false) {
                        printf("error sending file\n");    
                    }
                    delete[] data;
                    response_code = 0;
                } else {
                    std::cout << "unable to serve file: " << url_parts[0] << "." << url_parts[1] << std::endl;
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
            }
        }
        //printf("SHUT DOWN\n");
        int e = shutdown(new_socket, 0);
        e = close(new_socket);
        //printf("#2 close [%d]\n", e);
    }

    printf("web server exited\n");
    return 0;
}
