#ifndef WEBSERVER_H
#define WEBSERVER_H 1
#include <string>
#include <vector>

typedef int (*IntCallbackString)(std::vector<std::string>, int);

std::vector<std::string> parse_url(char *request);
bool send_headers(std::string raw, int socket);
bool send_data(int socket, std::string content_type, unsigned char *data, unsigned int sz);
int web_server(int port, std::map<std::string, IntCallbackString> fnMap);
 
#endif