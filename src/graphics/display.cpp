#include "display.h"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <system_error>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#ifdef CHITON_SDL2
#include <SDL.h>
#endif

namespace display {
namespace {
class Headless : public Backend {
public:
    void present(const Buffer &) override {}
    bool visible() const override { return false; }
};

class MissingDevice : public std::system_error {
public:
    MissingDevice(int error, const std::string &device)
        : std::system_error(error, std::generic_category(), "open " + device) {}
};

class Framebuffer : public Backend {
    int fd = -1;
    uint8_t *mapping = nullptr;
    fb_var_screeninfo v{};
    fb_fix_screeninfo f{};
public:
    explicit Framebuffer(const std::string &device) {
        fd = open(device.c_str(), O_RDWR | O_CLOEXEC);
        if (fd < 0) {
            const int error = errno;
            if (error == ENOENT || error == ENOTDIR || error == ENODEV || error == ENXIO)
                throw MissingDevice(error, device);
            throw std::system_error(error, std::generic_category(), "open " + device);
        }
        try {
            if (ioctl(fd, FBIOGET_VSCREENINFO, &v) < 0 ||
                ioctl(fd, FBIOGET_FSCREENINFO, &f) < 0)
                throw std::system_error(errno, std::generic_category(), "framebuffer ioctl " + device);
            validate_framebuffer(v, f);
            void *p = mmap(nullptr, f.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (p == MAP_FAILED)
                throw std::system_error(errno, std::generic_category(), "framebuffer mmap " + device);
            mapping = static_cast<uint8_t *>(p);
        } catch (...) {
            close(fd);
            fd = -1;
            throw;
        }
    }
    ~Framebuffer() override {
        if (mapping) munmap(mapping, f.smem_len);
        if (fd >= 0) close(fd);
    }
    void present(const Buffer &pixels) override { copy_framebuffer(pixels, mapping, v, f); }
};

#ifdef CHITON_SDL2
class Desktop : public Backend {
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *texture = nullptr;
    bool initialized = false;
    void cleanup() {
        if (texture) SDL_DestroyTexture(texture);
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        if (initialized) SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
    void check(bool success, const char *operation) {
        if (!success) throw std::runtime_error(std::string(operation) + ": " + SDL_GetError());
    }
public:
    Desktop() {
        try {
            // SDL's offscreen driver can succeed without a graphical session.
            // Do not select it automatically; explicitly selected test drivers are OK.
            if (!getenv("SDL_VIDEODRIVER") && !getenv("DISPLAY") && !getenv("WAYLAND_DISPLAY"))
                throw std::runtime_error("no graphical session (DISPLAY/WAYLAND_DISPLAY unset)");
            check(SDL_InitSubSystem(SDL_INIT_VIDEO) == 0, "SDL video initialization");
            initialized = true;
            window = SDL_CreateWindow("Chiton", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                      width * 2, height * 2, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
            check(window != nullptr, "SDL window");
            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
            if (!renderer) {
                std::cerr << "Chiton: accelerated SDL renderer unavailable; using software rendering\n";
                renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
            }
            check(renderer != nullptr, "SDL renderer");
            check(SDL_RenderSetLogicalSize(renderer, width, height) == 0, "SDL logical size");
            texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, width, height);
            check(texture != nullptr, "SDL RGB565 texture");
        } catch (...) { cleanup(); throw; }
    }
    ~Desktop() override { cleanup(); }
    bool process_events() override {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT ||
                (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE) ||
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) return false;
        }
        return true;
    }
    void present(const Buffer &pixels) override {
        check(SDL_UpdateTexture(texture, nullptr, pixels.data(), width * sizeof(uint16_t)) == 0, "SDL texture update");
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        check(SDL_RenderClear(renderer) == 0, "SDL clear");
        check(SDL_RenderCopy(renderer, texture, nullptr, nullptr) == 0, "SDL copy");
        SDL_RenderPresent(renderer);
    }
};
#endif

bool field_valid(const fb_bitfield &field, unsigned bits) {
    return field.length > 0 && field.length <= 8 && !field.msb_right &&
           field.offset < bits && field.offset + field.length <= bits;
}
uint32_t channel(unsigned value, unsigned max, const fb_bitfield &field) {
    if (!field.length) return 0;
    return ((value * ((1u << field.length) - 1) + max / 2) / max) << field.offset;
}
}

Mode parse_mode(const std::string &name) {
    if (name == "auto") return Mode::Auto;
    if (name == "framebuffer") return Mode::Framebuffer;
    if (name == "desktop") return Mode::Desktop;
    if (name == "headless") return Mode::Headless;
    throw std::invalid_argument("unknown display mode '" + name + "' (use auto, framebuffer, desktop or headless)");
}

void validate_framebuffer(const fb_var_screeninfo &v, const fb_fix_screeninfo &f) {
    const uint64_t bytes = v.bits_per_pixel / 8;
    if (f.type != FB_TYPE_PACKED_PIXELS || f.visual != FB_VISUAL_TRUECOLOR ||
        (v.bits_per_pixel != 16 && v.bits_per_pixel != 24 && v.bits_per_pixel != 32) ||
        v.xres < width || v.yres < height || v.nonstd || v.grayscale ||
        !field_valid(v.red, v.bits_per_pixel) || !field_valid(v.green, v.bits_per_pixel) ||
        !field_valid(v.blue, v.bits_per_pixel) || v.transp.msb_right || v.transp.length > 8 ||
        v.transp.offset + v.transp.length > v.bits_per_pixel ||
        uint64_t(v.xoffset) + width > v.xres_virtual ||
        uint64_t(v.yoffset) + height > v.yres_virtual ||
        (uint64_t(v.xoffset) + width) * bytes > f.line_length ||
        (uint64_t(v.yoffset) + height - 1) * f.line_length +
            (uint64_t(v.xoffset) + width) * bytes > f.smem_len)
        throw std::runtime_error("unsupported framebuffer geometry/stride/pixel format (requires >=320x240 packed truecolor 16/24/32-bit)");
    uint32_t used = 0;
    for (auto field : {v.red, v.green, v.blue, v.transp}) {
        if (!field.length) continue;
        uint32_t mask = ((1u << field.length) - 1) << field.offset;
        if (used & mask) throw std::runtime_error("overlapping framebuffer color channels");
        used |= mask;
    }
}

void copy_framebuffer(const Buffer &pixels, uint8_t *mapping,
                      const fb_var_screeninfo &v, const fb_fix_screeninfo &f) {
    validate_framebuffer(v, f);
    const unsigned bytes = v.bits_per_pixel / 8;
    const bool rgb565 = v.bits_per_pixel == 16 && v.red.offset == 11 && v.red.length == 5 &&
                        v.green.offset == 5 && v.green.length == 6 && v.blue.offset == 0 && v.blue.length == 5;
    for (int y = 0; y < height; ++y) {
        uint8_t *row = mapping + (size_t(y) + v.yoffset) * f.line_length + size_t(v.xoffset) * bytes;
        const uint16_t *src = pixels.data() + y * width;
        if (rgb565) { memcpy(row, src, width * sizeof(uint16_t)); continue; }
        for (int x = 0; x < width; ++x) {
            uint16_t c = src[x];
            uint32_t out = channel(c >> 11, 31, v.red) | channel((c >> 5) & 63, 63, v.green) |
                           channel(c & 31, 31, v.blue) | channel(1, 1, v.transp);
            memcpy(row + x * bytes, &out, bytes);
        }
    }
}

std::unique_ptr<Backend> create(Mode mode, const std::string &device) {
    if (mode == Mode::Headless) {
        std::cerr << "Chiton: display backend: headless\n";
        return std::make_unique<Headless>();
    }
    if (mode == Mode::Auto || mode == Mode::Framebuffer) {
        try {
            auto backend = std::make_unique<Framebuffer>(device);
            std::cerr << "Chiton: display backend: framebuffer (" << device << ")\n";
            return backend;
        } catch (const MissingDevice &e) {
            // Only an absent device at open permits automatic fallback.
            // Permission, ioctl and mmap failures remain fatal and diagnostic.
            if (mode != Mode::Auto) throw;
            std::cerr << "Chiton: framebuffer missing: " << e.what() << "; trying desktop\n";
        }
    }
    try {
#ifdef CHITON_SDL2
        auto backend = std::make_unique<Desktop>();
        std::cerr << "Chiton: display backend: desktop (SDL2)\n";
        return backend;
#else
        throw std::runtime_error("SDL2 support was not compiled in");
#endif
    } catch (const std::exception &e) {
        if (mode == Mode::Desktop) throw;
        std::cerr << "Chiton: desktop unavailable: " << e.what() << "; switching to headless\n";
        return std::make_unique<Headless>();
    }
}
}
