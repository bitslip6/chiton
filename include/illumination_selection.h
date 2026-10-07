#pragma once
#include "illumination_controller.h"
#include <stdexcept>
#include <string>
namespace illumination {
inline Tuple parse_initial_tuple(const std::string &text) {
    Tuple tuple{};
    size_t at = 0;
    for (unsigned i = 0; i < 4; ++i) {
        int value = 0; size_t start = at;
        while (at < text.size() && text[at] >= '0' && text[at] <= '9') {
            value = value * 10 + text[at++] - '0';
            if (value > 12 || at - start > 2) throw std::invalid_argument("initial illumination must contain four codes 1..12");
        }
        if (at == start || value < 1 || (i < 3 && (at == text.size() || text[at++] != ',')))
            throw std::invalid_argument("initial illumination must contain four codes 1..12");
        tuple[i] = value;
    }
    if (at != text.size()) throw std::invalid_argument("trailing initial illumination input");
    return tuple;
}
}
