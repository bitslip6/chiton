#ifndef CHITON_DISPLAY_H
#define CHITON_DISPLAY_H

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <linux/fb.h>

namespace display {
constexpr int width = 320;
constexpr int height = 240;
using Buffer = std::array<uint16_t, width * height>;
enum class Mode { Auto, Framebuffer, Desktop, Headless };
Mode parse_mode(const std::string &name);

// All backend operations, including destruction, belong on the main thread.
class Backend {
public:
    virtual ~Backend() = default;
    virtual bool process_events() { return true; }
    virtual void present(const Buffer &pixels) = 0;
    virtual bool visible() const { return true; }
};
std::unique_ptr<Backend> create(Mode mode, const std::string &device = "/dev/fb1");

// Validates geometry, stride, offsets and pixel format before any mapped writes.
void validate_framebuffer(const fb_var_screeninfo &v, const fb_fix_screeninfo &f);
void copy_framebuffer(const Buffer &pixels, uint8_t *mapping,
                      const fb_var_screeninfo &v, const fb_fix_screeninfo &f);
}
#endif
