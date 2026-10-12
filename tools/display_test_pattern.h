#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

// Shared deterministic RGB565 fixture; no display access or font assets.
namespace display_test_pattern {
struct Spec {
    unsigned width, height;
    std::string pattern;
};
inline void decimal_counter(std::vector<uint16_t> &pixels, const Spec &o, unsigned frame) {
    static constexpr unsigned digits[10][7] = {
        {14, 17, 19, 21, 25, 17, 14}, {4, 12, 4, 4, 4, 4, 14},
        {14, 17, 1, 2, 4, 8, 31}, {30, 1, 1, 14, 1, 1, 30},
        {2, 6, 10, 18, 31, 2, 2}, {31, 16, 16, 30, 1, 1, 30},
        {14, 16, 16, 30, 17, 17, 14}, {31, 1, 2, 4, 8, 8, 8},
        {14, 17, 17, 14, 17, 17, 14}, {14, 17, 17, 15, 1, 1, 14},
    };
    if (o.height <= 24) return;
    const unsigned scale = std::min({7u, o.width / 37, (o.height - 24) / 9});
    if (!scale) return;
    const unsigned left = (o.width - 37 * scale) / 2;
    const unsigned top = 24 + (o.height - 24 - 9 * scale) / 2;
    for (unsigned y = top; y < top + 9 * scale; ++y)
        std::fill_n(pixels.begin() + y * o.width + left, 37 * scale, uint16_t(0));
    unsigned divisor = 100000;
    for (unsigned digit = 0; digit < 6; ++digit, divisor /= 10) {
        const auto &glyph = digits[(frame / divisor) % 10];
        for (unsigned y = 0; y < 7; ++y) {
            for (unsigned x = 0; x < 5; ++x) {
                if (!(glyph[y] & (1u << (4 - x)))) continue;
                const unsigned px = left + (1 + digit * 6 + x) * scale;
                const unsigned py = top + (1 + y) * scale;
                for (unsigned row = 0; row < scale; ++row)
                    std::fill_n(pixels.begin() + (py + row) * o.width + px, scale, uint16_t(0xffff));
            }
        }
    }
}
inline void render(std::vector<uint16_t> &pixels, const Spec &o, unsigned frame) {
    for (unsigned y = 0; y < o.height; ++y) {
        for (unsigned x = 0; x < o.width; ++x) {
            uint16_t c = 0;
            if (o.pattern == "alternate") c = (frame & 1) ? 0xffff : 0;
            else if (o.pattern == "bars") c = ((x + frame * 4) / 20 % 2) ? 0xf800 : 0x07e0;
            else {
                c = (frame & 1) ? 0x0010 : 0;
                if (y < std::min(24u, o.height)) c = (frame >> (x * 16 / o.width) & 1) ? 0xffff : 0;
                else if ((x + o.width - frame % o.width) % o.width < std::min(8u, o.width)) c = 0xffff;
            }
            pixels[y * o.width + x] = c;
        }
    }
    if (o.pattern == "counter") decimal_counter(pixels, o, frame);
}
}
