#include "preview.h"
#include "resources.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>

namespace display {
namespace {
// Built-in 5x7 uppercase/digit font, one byte per column. No desktop font dependency.
const uint8_t letters[][5] = {
 {0x7e,0x11,0x11,0x11,0x7e}, {0x7f,0x49,0x49,0x49,0x36},
 {0x3e,0x41,0x41,0x41,0x22}, {0x7f,0x41,0x41,0x22,0x1c},
 {0x7f,0x49,0x49,0x49,0x41}, {0x7f,0x09,0x09,0x09,0x01},
 {0x3e,0x41,0x49,0x49,0x7a}, {0x7f,0x08,0x08,0x08,0x7f},
 {0x00,0x41,0x7f,0x41,0x00}, {0x20,0x40,0x41,0x3f,0x01},
 {0x7f,0x08,0x14,0x22,0x41}, {0x7f,0x40,0x40,0x40,0x40},
 {0x7f,0x02,0x0c,0x02,0x7f}, {0x7f,0x04,0x08,0x10,0x7f},
 {0x3e,0x41,0x41,0x41,0x3e}, {0x7f,0x09,0x09,0x09,0x06},
 {0x3e,0x41,0x51,0x21,0x5e}, {0x7f,0x09,0x19,0x29,0x46},
 {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7f,0x01,0x01},
 {0x3f,0x40,0x40,0x40,0x3f}, {0x1f,0x20,0x40,0x20,0x1f},
 {0x3f,0x40,0x38,0x40,0x3f}, {0x63,0x14,0x08,0x14,0x63},
 {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}
};
const uint8_t digits[][5] = {
 {0x3e,0x51,0x49,0x45,0x3e}, {0x00,0x42,0x7f,0x40,0x00},
 {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4b,0x31},
 {0x18,0x14,0x12,0x7f,0x10}, {0x27,0x45,0x45,0x45,0x39},
 {0x3c,0x4a,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
 {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1e}
};
std::array<uint8_t, 5> glyph(unsigned char c) {
    c = std::toupper(c);
    std::array<uint8_t, 5> result{};
    if (c >= 'A' && c <= 'Z') std::copy_n(letters[c - 'A'], 5, result.begin());
    else if (c >= '0' && c <= '9') std::copy_n(digits[c - '0'], 5, result.begin());
    else if (c == ':') result = {0,0x36,0x36,0,0};
    else if (c == '.') result = {0,0x60,0x60,0,0};
    else if (c == '-') result.fill(0x08);
    else if (c == '/') result = {0x20,0x10,0x08,0x04,0x02};
    else if (c != ' ') result = {0x02,0x01,0x51,0x09,0x06};
    return result;
}
}
Preview::Preview() : offsets(init_small_fonts()) {
    auto path = resources::asset("small_font.png");
    if (!path.empty()) font = read_png_file(path.c_str(), true);
    if (!font) std::cerr << "Chiton: small_font.png unavailable; using built-in 5x7 status font\n";
    for (size_t i = 0; i < colors.size(); ++i) {
        unsigned gray = i >> 4;
        colors[i] = ((gray >> 3) << 11) | ((gray >> 2) << 5) | (gray >> 3);
    }
}
void Preview::text(const std::string &msg, int x, int y, uint16_t color) {
    if (font) {
        ChitonFB surface{};
        surface.buffer1 = buffer.data();
        surface.width = width;
        surface.height = height;
        fb_write_font_string(&surface, msg.c_str(), {static_cast<s16>(x), static_cast<s16>(y)}, color, font.get(), offsets.get());
        return;
    }
    const int initial_x = x;
    for (unsigned char c : msg) {
        if (c == '\n') { x = initial_x; y += 8; continue; }
        auto columns = glyph(c);
        for (int dx = 0; dx < 5; ++dx)
            for (int dy = 0; dy < 7; ++dy)
                if (x + dx >= 0 && x + dx < width && y + dy >= 0 && y + dy < height &&
                    (columns[dx] & (1 << dy))) buffer[(y + dy) * width + x + dx] = color;
        x += 7;
        if (x >= width) break;
    }
}
const Buffer &Preview::render(const Samples *samples, const std::string &ethernet,
                             const std::string &wifi, const std::string &message,
                             unsigned long seconds) {
    if (samples) {
        for (size_t i = 0; i < samples->size(); ++i)
            buffer[i] = colors[std::min<unsigned>((*samples)[i], colors.size() - 1)];
    }
    std::fill(buffer.begin() + 200 * width, buffer.end(), 0);
    text(ethernet, 5, 210, 0x07c0);
    text(wifi, 5, 225, 0x07c0);
    text(message, 185, 210, 0xc0c0);
    text(std::to_string(seconds / 60) + ":" + std::to_string(seconds % 60), 185, 225, 0xc0c0);
    return buffer;
}
const Buffer &Preview::startup_image(const ChitonPngImage &image) {
    const int w = std::min<int>(width, image.width);
    const int h = std::min<int>(height - 20, image.height);
    for (int y = 0; y < h; ++y)
        std::copy_n(image.image + y * image.width, w, buffer.data() + (y + 20) * width);
    return buffer;
}
}
