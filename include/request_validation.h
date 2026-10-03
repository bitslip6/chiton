#pragma once
#include <string>
#include <vector>
#include <stdexcept>
#include <cerrno>
#include <cstdlib>
#include <cmath>

namespace request {
inline void count(const std::vector<std::string> &args, size_t expected) {
    if (args.size() != expected) throw std::invalid_argument("incorrect argument count");
}
inline long integer(const std::string &text, long low, long high, int base = 10) {
    if (text.empty() || text.find_first_of(" \t\r\n") != std::string::npos)
        throw std::invalid_argument("invalid integer");
    errno = 0;
    char *end = nullptr;
    long value = std::strtol(text.c_str(), &end, base);
    if (errno || end != text.c_str() + text.size() || value < low || value > high)
        throw std::invalid_argument("integer outside range");
    return value;
}
inline double number(const std::string &text, double low, double high) {
    if (text.empty() || text.find_first_of(" \t\r\n") != std::string::npos)
        throw std::invalid_argument("invalid number");
    errno = 0;
    char *end = nullptr;
    if (text.find_first_not_of("0123456789+-.eE") != std::string::npos)
        throw std::invalid_argument("invalid numeric syntax");
    double value = std::strtod(text.c_str(), &end);
    if (errno || end != text.c_str() + text.size() || !std::isfinite(value) || value < low || value > high)
        throw std::invalid_argument("number outside range");
    return value;
}
}
