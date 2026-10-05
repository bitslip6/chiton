#pragma once
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace web {
struct FrameRead {
    bool newer = false;
    uint64_t after = 0;
    int timeout_ms = 1000;
};
inline uint64_t frame_decimal(const std::string &text) {
    if (text.empty()) throw std::invalid_argument("empty frame query value");
    uint64_t value = 0;
    for (unsigned char c : text) {
        if (c < '0' || c > '9' || value > (std::numeric_limits<uint64_t>::max() - (c - '0')) / 10)
            throw std::invalid_argument("invalid frame query value");
        value = value * 10 + c - '0';
    }
    return value;
}
inline FrameRead frame_read(const std::string &query) {
    FrameRead result;
    bool timeout = false;
    for (size_t pos = 0; pos < query.size();) {
        const auto end = query.find('&', pos);
        const auto field = query.substr(pos, end == std::string::npos ? end : end - pos);
        const auto equal = field.find('=');
        const auto name = field.substr(0, equal);
        if (equal == std::string::npos) throw std::invalid_argument("invalid frame query");
        const auto value = frame_decimal(field.substr(equal + 1));
        if (name == "after" && !result.newer) { result.newer = true; result.after = value; }
        else if (name == "timeout_ms" && !timeout && value <= 1000) {
            timeout = true; result.timeout_ms = static_cast<int>(value);
        } else throw std::invalid_argument("unknown, duplicate or out-of-range frame query");
        if (end == std::string::npos) break;
        pos = end + 1;
        if (pos == query.size()) throw std::invalid_argument("empty frame query field");
    }
    if (timeout && !result.newer) throw std::invalid_argument("timeout_ms requires after");
    return result;
}
}
