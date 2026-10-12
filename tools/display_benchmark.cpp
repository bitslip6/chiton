// SDK-free Linux fbdev submission benchmark. Never equate mmap writes with panel FPS.
#include "display.h"
#include "display_test_pattern.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
volatile std::sig_atomic_t stopped = 0;
void stop(int) { stopped = 1; }
void require(bool ok, const std::string &message) {
    if (!ok) throw std::runtime_error(message);
}
double number(const std::string &s, double low, double high) {
    size_t used = 0;
    double n = std::stod(s, &used);
    require(used == s.size() && std::isfinite(n) && n >= low && n <= high,
            "invalid numeric value: " + s);
    return n;
}
unsigned integer(const std::string &s, unsigned low, unsigned high) {
    double n = number(s, low, high);
    require(n == std::floor(n), "expected integer: " + s);
    return static_cast<unsigned>(n);
}
struct Options {
    std::string device = "/dev/fb1", method = "mmap", pattern = "counter";
    std::string spi_path, fps_path;
    unsigned spi_hz = 0, driver_fps = 0, frames = 1000000, width = 320, height = 240;
    double seconds = 10, delay_ms = 0;
    bool vsync = false, self_test = false;
};
void help() {
    std::cout <<
        "Usage: display_refresh_benchmark [OPTIONS]\n"
        "  --device PATH          framebuffer (default /dev/fb1)\n"
        "  --seconds N            duration limit (default 10, max 3600)\n"
        "  --frames N             submission limit (default 1000000)\n"
        "  --delay-ms N           sleep after each submission (default 0)\n"
        "  --region WxH           top-left update region (default 320x240)\n"
        "  --method mmap|pwrite   memory mapping or fbdev write syscall\n"
        "  --pattern counter|alternate|bars\n"
        "  --vsync                require successful FBIO_WAITFORVSYNC\n"
        "  --spi-hz N --spi-path /sys/...   explicit writable runtime SPI control\n"
        "  --driver-fps N --fps-path /sys/... explicit writable driver FPS control\n"
        "  --self-test            host-safe pattern/validation tests; no device access\n"
        "Controls are read back and restored on exit/SIGINT/SIGTERM, not SIGKILL.\n"
        "No universal flexfb SPI/FPS control exists: unsupported paths fail.\n"
        "Stop Chiton first. Output is submission FPS, NOT measured panel FPS.\n";
}
Options parse(int argc, char **argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string key = argv[i];
        if (key == "--vsync") { o.vsync = true; continue; }
        if (key == "--self-test") { o.self_test = true; continue; }
        require(i + 1 < argc, "missing value for " + key);
        std::string value = argv[++i];
        if (key == "--device") o.device = value;
        else if (key == "--seconds") o.seconds = number(value, 0.01, 3600);
        else if (key == "--frames") o.frames = integer(value, 1, 1000000);
        else if (key == "--delay-ms") o.delay_ms = number(value, 0, 10000);
        else if (key == "--method") o.method = value;
        else if (key == "--pattern") o.pattern = value;
        else if (key == "--spi-path") o.spi_path = value;
        else if (key == "--fps-path") o.fps_path = value;
        else if (key == "--spi-hz") o.spi_hz = integer(value, 1, 1000000000);
        else if (key == "--driver-fps") o.driver_fps = integer(value, 1, 1000);
        else if (key == "--region") {
            auto x = value.find('x');
            require(x != std::string::npos, "region must be WxH");
            o.width = integer(value.substr(0, x), 1, 320);
            o.height = integer(value.substr(x + 1), 1, 240);
        } else throw std::runtime_error("unknown option: " + key);
    }
    require(o.method == "mmap" || o.method == "pwrite", "unknown method");
    require(o.pattern == "counter" || o.pattern == "alternate" || o.pattern == "bars", "unknown pattern");
    require(bool(o.spi_hz) == !o.spi_path.empty(), "--spi-hz and --spi-path must be supplied together");
    require(bool(o.driver_fps) == !o.fps_path.empty(), "--driver-fps and --fps-path must be supplied together");
    require(o.spi_path.empty() || o.spi_path != o.fps_path, "SPI and FPS paths must differ");
    require(!o.self_test || (o.spi_path.empty() && o.fps_path.empty()), "self-test cannot change controls");
    return o;
}
// Save the original value before writing. Do not reload modules or guess sysfs paths.
struct Control {
    std::string path, original;
    bool changed = false;
    static std::string read(const std::string &path) {
        std::ifstream in(path);
        std::string value;
        require(bool(in >> value), "cannot read control: " + path);
        return value;
    }
    static void write(const std::string &path, const std::string &value) {
        std::ofstream out(path);
        out << value << '\n';
        out.close();
        require(!out.fail(), "cannot write control: " + path);
    }
    void apply(const std::string &p, unsigned value) {
        if (p.empty()) return;
        char resolved[4096];
        require(realpath(p.c_str(), resolved) != nullptr, "cannot resolve control: " + p);
        path = resolved;
        require(path.compare(0, 5, "/sys/") == 0, "controls must resolve under /sys/");
        original = read(path);
        changed = true;
        write(path, std::to_string(value));
        require(integer(read(path), 0, 1000000000) == value, "control readback differs: " + path);
        std::cerr << "Applied " << path << "=" << value << " (original " << original << ")\n";
    }
    bool restore() {
        if (!changed) return true;
        try {
            write(path, original);
            require(read(path) == original, "restore readback differs: " + path);
            changed = false;
            return true;
        } catch (const std::exception &e) {
            std::cerr << "RESTORE FAILED: " << e.what() << "; manually restore " << path << "=" << original << '\n';
            return false;
        }
    }
    ~Control() { restore(); }
};
struct Framebuffer {
    int fd = -1;
    uint8_t *mapping = nullptr;
    fb_var_screeninfo v{};
    fb_fix_screeninfo f{};
    ~Framebuffer() {
        if (mapping) munmap(mapping, f.smem_len);
        if (fd >= 0) close(fd);
    }
    void open_device(const Options &o) {
        fd = open(o.device.c_str(), O_RDWR | O_CLOEXEC);
        require(fd >= 0, "open " + o.device + ": " + std::strerror(errno));
        require(ioctl(fd, FBIOGET_VSCREENINFO, &v) == 0 && ioctl(fd, FBIOGET_FSCREENINFO, &f) == 0,
                "framebuffer info ioctl failed");
        display::validate_framebuffer(v, f);
        require(v.bits_per_pixel == 16 && v.red.offset == 11 && v.red.length == 5 &&
                v.green.offset == 5 && v.green.length == 6 && v.blue.offset == 0 &&
                v.blue.length == 5 && v.transp.length == 0, "benchmark requires RGB565");
        std::cerr << "Framebuffer " << std::string(f.id, strnlen(f.id, sizeof(f.id)))
                  << " " << v.xres << "x" << v.yres << " bpp=" << v.bits_per_pixel
                  << " stride=" << f.line_length << " memory=" << f.smem_len
                  << " nonstd=" << v.nonstd << " grayscale=" << v.grayscale
                  << " offsets=" << v.xoffset << "," << v.yoffset << '\n';
        if (o.method == "mmap") {
            void *p = mmap(nullptr, f.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            require(p != MAP_FAILED, "framebuffer mmap failed");
            mapping = static_cast<uint8_t *>(p);
        }
    }
    void present(const Options &o, const std::vector<uint16_t> &pixels) {
        for (unsigned y = 0; y < o.height; ++y) {
            size_t offset = size_t(y + v.yoffset) * f.line_length + size_t(v.xoffset) * 2;
            const auto *src = reinterpret_cast<const uint8_t *>(pixels.data() + y * o.width);
            size_t bytes = o.width * 2;
            if (mapping) std::memcpy(mapping + offset, src, bytes);
            else {
                size_t done = 0;
                while (done < bytes) {
                    ssize_t n = pwrite(fd, src + done, bytes - done, offset + done);
                    if (n < 0 && errno == EINTR) continue;
                    require(n > 0, "framebuffer pwrite failed: " + std::string(std::strerror(errno)));
                    done += size_t(n);
                }
            }
        }
        if (o.vsync) {
            unsigned crtc = 0;
            require(ioctl(fd, FBIO_WAITFORVSYNC, &crtc) == 0, "FBIO_WAITFORVSYNC unsupported/failed");
        }
    }
};
void pattern(std::vector<uint16_t> &pixels, const Options &o, unsigned frame) {
    display_test_pattern::render(pixels, {o.width, o.height, o.pattern}, frame);
}
double ms(Clock::duration d) { return std::chrono::duration<double, std::milli>(d).count(); }
double percentile(std::vector<double> values, double fraction) {
    std::sort(values.begin(), values.end());
    return values[static_cast<size_t>((values.size() - 1) * fraction)];
}
void self_test() {
    Options o;
    std::vector<uint16_t> pixels(o.width * o.height);
    o.pattern = "alternate";
    pattern(pixels, o, 0); require(pixels.front() == 0 && pixels.back() == 0, "black pattern");
    pattern(pixels, o, 1); require(pixels.front() == 0xffff && pixels.back() == 0xffff, "white pattern");
    o.pattern = "counter";
    pattern(pixels, o, 1); require(pixels[0] == 0xffff && pixels[20] == 0, "counter bits");
    // At 320x240 the glyph scale is 7, backdrop origin (30,100).
    require(pixels[107 * 320 + 44] == 0xffff, "leading zero glyph");
    require(pixels[107 * 320 + 261] == 0xffff && pixels[107 * 320 + 254] == 0,
            "last decimal digit is one");
    pattern(pixels, o, 999999);
    require(pixels[107 * 320 + 254] == 0xffff, "six-digit maximum");
    // Check overlay bounds and binary-strip preservation across small/partial regions.
    for (unsigned width : {1u, 36u, 37u, 160u, 320u}) {
        for (unsigned height : {1u, 24u, 25u, 33u, 120u, 240u}) {
            o.width = width; o.height = height;
            pixels.assign(width * height + 16, 0x1234);
            pattern(pixels, o, 42);
            require(pixels[width * height] == 0x1234 && pixels.back() == 0x1234,
                    "decimal overlay bounds");
            require(pixels[0] == 0, "binary strip preserved");
        }
    }
    o.width = 1; o.height = 1; pixels.resize(1);
    pattern(pixels, o, 0); require(pixels[0] == 0, "small region");

    // Synthetic geometry and guarded partial writes; never open a real framebuffer.
    Framebuffer fb;
    fb.v.xres = 320; fb.v.yres = 240;
    fb.v.xoffset = 2; fb.v.yoffset = 1;
    fb.v.xres_virtual = 322; fb.v.yres_virtual = 241;
    fb.v.bits_per_pixel = 16; fb.v.nonstd = 1;
    fb.v.red = {11, 5, 0}; fb.v.green = {5, 6, 0}; fb.v.blue = {0, 5, 0};
    fb.f.type = FB_TYPE_PACKED_PIXELS; fb.f.visual = FB_VISUAL_TRUECOLOR;
    fb.f.line_length = 648; fb.f.smem_len = 648 * 241;
    display::validate_framebuffer(fb.v, fb.f);
    auto invalid = fb.v; invalid.nonstd = 2;
    bool rejected = false;
    try { display::validate_framebuffer(invalid, fb.f); }
    catch (const std::exception &) { rejected = true; }
    require(rejected, "unsupported nonstd must fail");
    void *p = mmap(nullptr, fb.f.smem_len, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    require(p != MAP_FAILED, "self-test mmap");
    fb.mapping = static_cast<uint8_t *>(p);
    std::memset(fb.mapping, 0x55, fb.f.smem_len);
    o.width = 320; o.height = 24; o.pattern = "alternate";
    pixels.resize(o.width * o.height);
    pattern(pixels, o, 1);
    fb.present(o, pixels);
    std::vector<uint8_t> expected(fb.f.smem_len, 0x55);
    for (unsigned y = 0; y < o.height; ++y)
        std::memset(expected.data() + (y + 1) * 648 + 4, 0xff, 640);
    require(std::memcmp(expected.data(), fb.mapping, expected.size()) == 0,
            "mmap partial writes/offsets/stride guards");

    Framebuffer file_fb;
    file_fb.v = fb.v; file_fb.f = fb.f;
    char name[] = "/tmp/display-benchmark-self-test-XXXXXX";
    file_fb.fd = mkstemp(name);
    require(file_fb.fd >= 0, "self-test temporary file");
    unlink(name);
    std::vector<uint8_t> actual(fb.f.smem_len, 0x55);
    require(write(file_fb.fd, actual.data(), actual.size()) == ssize_t(actual.size()),
            "self-test file initialization");
    o.method = "pwrite";
    file_fb.present(o, pixels);
    require(pread(file_fb.fd, actual.data(), actual.size(), 0) == ssize_t(actual.size()),
            "self-test file readback");
    require(actual == expected, "pwrite partial writes/offsets/stride guards");
    std::cout << "display benchmark self-test passed\n";
}
}
int main(int argc, char **argv) {
    try {
        for (int i = 1; i < argc; ++i) if (std::string(argv[i]) == "--help") { help(); return 0; }
        Options o = parse(argc, argv);
        if (o.self_test) { self_test(); return 0; }
        std::signal(SIGINT, stop); std::signal(SIGTERM, stop);
        Framebuffer fb;
        fb.open_device(o); // Validate device before modifying any controls.
        Control spi, fps;
        spi.apply(o.spi_path, o.spi_hz); fps.apply(o.fps_path, o.driver_fps);
        std::vector<uint16_t> pixels(o.width * o.height);
        std::vector<double> render_times, write_times;
        render_times.reserve(o.frames); write_times.reserve(o.frames);
        auto start = Clock::now();
        auto end = start + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(o.seconds));
        unsigned count = 0;
        while (!stopped && count < o.frames && Clock::now() < end) {
            auto a = Clock::now();
            pattern(pixels, o, count);
            auto b = Clock::now();
            fb.present(o, pixels);
            auto c = Clock::now();
            render_times.push_back(ms(b - a)); write_times.push_back(ms(c - b));
            ++count;
            // Bound signal response during long requested delays.
            auto wake = std::min(end, c + std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double, std::milli>(o.delay_ms)));
            while (!stopped && Clock::now() < wake)
                std::this_thread::sleep_until(std::min(wake, Clock::now() + std::chrono::milliseconds(10)));
        }
        double elapsed = ms(Clock::now() - start) / 1000;
        bool restored_spi = spi.restore(), restored_fps = fps.restore();
        require(restored_spi && restored_fps, "driver control restoration failed");
        require(count > 0, "no frames submitted");
        double submission_fps = count / elapsed;
        std::cout << std::fixed << std::setprecision(3)
                  << "{\"benchmark\":\"display_refresh\",\"measurement\":\"submission_only\","
                  << "\"panel_fps\":null,\"method\":\"" << o.method << "\",\"pattern\":\"" << o.pattern
                  << "\",\"width\":" << o.width << ",\"height\":" << o.height
                  << ",\"delay_ms\":" << o.delay_ms << ",\"vsync\":" << (o.vsync ? "true" : "false")
                  << ",\"spi_hz_requested\":" << o.spi_hz << ",\"driver_fps_requested\":" << o.driver_fps
                  << ",\"controls_restored\":true,\"frames\":" << count << ",\"elapsed_s\":" << elapsed
                  << ",\"submission_fps\":" << submission_fps
                  << ",\"submitted_MB_s\":" << submission_fps * pixels.size() * 2 / 1e6
                  << ",\"render_p50_ms\":" << percentile(render_times, 0.5)
                  << ",\"render_p95_ms\":" << percentile(render_times, 0.95)
                  << ",\"present_p50_ms\":" << percentile(write_times, 0.5)
                  << ",\"present_p95_ms\":" << percentile(write_times, 0.95)
                  << ",\"interrupted\":" << (stopped ? "true" : "false") << "}\n";
        std::cerr << "Submission FPS is not panel FPS. mmap/deferred I/O may coalesce or drop frames.\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "display benchmark: " << e.what() << '\n';
        return 1;
    }
}
