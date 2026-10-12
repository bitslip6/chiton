// Direct ST7789VI spidev benchmark. Operator must detach flexfb first.
#include "display_test_pattern.h"
#include <cerrno>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <linux/spi/spidev.h>
#include <stdexcept>
#include <string>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

namespace {
using Clock = std::chrono::steady_clock;
constexpr unsigned width = 320, height = 240, frame_bytes = width * height * 2;
volatile std::sig_atomic_t stopped = 0;
void stop(int) { stopped = 1; }
void require(bool ok, const std::string &why) { if (!ok) throw std::runtime_error(why); }
std::string error(const std::string &what) { return what + ": " + std::strerror(errno); }
void syscall_check(bool ok, const std::string &what) {
    if (!ok) throw std::runtime_error(error(what));
}
double number(const std::string &s, double low, double high) {
    size_t used = 0;
    double n = std::stod(s, &used);
    require(used == s.size() && std::isfinite(n) && n >= low && n <= high, "invalid number: " + s);
    return n;
}
unsigned integer(const std::string &s, unsigned low, unsigned high) {
    double n = number(s, low, high);
    require(n == std::floor(n), "expected integer: " + s);
    return static_cast<unsigned>(n);
}
struct Options {
    std::string device = "/dev/spidev2.0", pattern = "counter";
    unsigned speed = 48000000, bits = 8, mode = 3, chunk = 4096, frames = 1000000;
    int dc = -1, reset = -1;
    double seconds = 10, delay = 0;
    bool exclusive = false, init = false, self_test = false;
};
void help() {
    std::cout <<
        "Usage: spi_display_benchmark --ack-exclusive --dc-gpio N [OPTIONS]\n"
        "  --device PATH       default /dev/spidev2.0; flexfb must be detached\n"
        "  --speed-hz N        requested SPI clock (default 48000000, max 62500000)\n"
        "  --word-bits 8|16|32 pixel word size; commands ALWAYS use 8 bits\n"
        "  --mode 0|1|2|3      default 3, matching repository loader\n"
        "  --chunk-bytes N     per-ioctl payload bytes (default 4096, max 153600)\n"
        "  --seconds N         duration limit (default 10)\n"
        "  --frames N          frame limit (default 1000000)\n"
        "  --delay-ms N        delay AFTER each frame (default 0)\n"
        "  --pattern counter|alternate|bars\n"
        "  --init-panel --reset-gpio N  reset and use repository ST7789 init profile\n"
        "  --self-test         host-safe pattern/packing tests; no hardware access\n"
        "GPIO numbers are global Linux sysfs GPIO numbers, NOT connector pins.\n"
        "Program exports only unexported GPIOs and unexports its own on exit.\n"
        "No automatic module unloading, rebinding, DT edits or persistent changes.\n"
        "SPI-completed frame rate is NOT physical panel FPS.\n";
}
Options parse(int argc, char **argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string key = argv[i];
        if (key == "--ack-exclusive") { o.exclusive = true; continue; }
        if (key == "--init-panel") { o.init = true; continue; }
        if (key == "--self-test") { o.self_test = true; continue; }
        require(i + 1 < argc, "missing value for " + key);
        std::string value = argv[++i];
        if (key == "--device") o.device = value;
        else if (key == "--pattern") o.pattern = value;
        else if (key == "--speed-hz") o.speed = integer(value, 1, 62500000);
        else if (key == "--word-bits") o.bits = integer(value, 8, 32);
        else if (key == "--mode") o.mode = integer(value, 0, 3);
        else if (key == "--chunk-bytes") o.chunk = integer(value, 4, frame_bytes);
        else if (key == "--seconds") o.seconds = number(value, 0.01, 3600);
        else if (key == "--frames") o.frames = integer(value, 1, 1000000);
        else if (key == "--delay-ms") o.delay = number(value, 0, 10000);
        else if (key == "--dc-gpio") o.dc = integer(value, 0, 1000000);
        else if (key == "--reset-gpio") o.reset = integer(value, 0, 1000000);
        else throw std::runtime_error("unknown option: " + key);
    }
    require(o.bits == 8 || o.bits == 16 || o.bits == 32, "word bits must be 8, 16 or 32");
    require(o.chunk % (o.bits / 8) == 0, "chunk must be a multiple of SPI word bytes");
    require(o.pattern == "counter" || o.pattern == "alternate" || o.pattern == "bars", "unknown pattern");
    if (!o.self_test) {
        require(o.exclusive && o.dc >= 0, "requires --ack-exclusive and --dc-gpio; no device opened");
        require(o.init == (o.reset >= 0), "--init-panel and --reset-gpio must be supplied together");
        require(o.reset < 0 || o.reset != o.dc, "DC and reset GPIOs must differ");
    }
    return o;
}
void text_write(const std::string &path, const std::string &value) {
    std::ofstream out(path);
    require(bool(out), "cannot open " + path);
    out << value;
    out.close();
    require(!out.fail(), "cannot write " + path);
}
struct GPIO {
    int number = -1, fd = -1;
    bool owned = false;
    void claim(int n) {
        number = n;
        std::string base = "/sys/class/gpio/gpio" + std::to_string(n);
        struct stat s{};
        errno = 0;
        int status = stat(base.c_str(), &s);
        require(status < 0 && errno == ENOENT, "refusing already-exported/inaccessible GPIO: " + base);
        text_write("/sys/class/gpio/export", std::to_string(n));
        owned = true;
        auto deadline = Clock::now() + std::chrono::seconds(1);
        while (access((base + "/direction").c_str(), F_OK) != 0 && Clock::now() < deadline && !stopped)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        require(!stopped, "interrupted while exporting GPIO");
        text_write(base + "/direction", "high"); // Set initial output atomically.
        fd = open((base + "/value").c_str(), O_WRONLY | O_CLOEXEC);
        require(fd >= 0, error("open GPIO value"));
    }
    void set(bool high) {
        syscall_check(lseek(fd, 0, SEEK_SET) >= 0, "seek GPIO");
        const char value = high ? '1' : '0';
        syscall_check(write(fd, &value, 1) == 1, "write GPIO");
    }
    bool release() {
        if (fd >= 0) { close(fd); fd = -1; }
        if (!owned) return true;
        try {
            text_write("/sys/class/gpio/unexport", std::to_string(number));
            owned = false;
            return true;
        } catch (const std::exception &e) {
            std::cerr << "GPIO CLEANUP FAILED: " << e.what() << '\n';
            return false;
        }
    }
    ~GPIO() { release(); }
};
struct SPI {
    int fd = -1;
    uint32_t old_mode = 0, old_speed = 0;
    uint8_t old_bits = 0, old_lsb = 0;
    bool saved = false;
    void open_device(const Options &o) {
        fd = open(o.device.c_str(), O_RDWR | O_CLOEXEC);
        require(fd >= 0, error("open " + o.device));
        struct stat s{};
        require(fstat(fd, &s) == 0 && S_ISCHR(s.st_mode), "SPI device must be a character device");
        syscall_check(flock(fd, LOCK_EX | LOCK_NB) == 0, "SPI advisory lock");
        syscall_check(ioctl(fd, SPI_IOC_RD_MODE32, &old_mode) == 0 &&
                ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &old_speed) == 0 &&
                ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &old_bits) == 0 &&
                ioctl(fd, SPI_IOC_RD_LSB_FIRST, &old_lsb) == 0, "read spidev settings");
        std::ifstream limit_file("/sys/module/spidev/parameters/bufsiz");
        unsigned limit = 0;
        if (limit_file >> limit)
            require(o.chunk <= limit, "chunk exceeds spidev bufsiz=" + std::to_string(limit) + "; use a smaller chunk or reload spidev explicitly");
        saved = true;
        uint32_t mode = o.mode, speed = o.speed;
        uint8_t bits = 8, lsb = 0;
        syscall_check(ioctl(fd, SPI_IOC_WR_MODE32, &mode) == 0 &&
                ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) == 0 &&
                ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) == 0 &&
                ioctl(fd, SPI_IOC_WR_LSB_FIRST, &lsb) == 0, "configure spidev");
    }
    void transfer(const void *data, unsigned bytes, unsigned bits, unsigned speed) {
        spi_ioc_transfer t{};
        t.tx_buf = reinterpret_cast<uintptr_t>(data);
        t.len = bytes; t.speed_hz = speed; t.bits_per_word = bits;
        int ret = ioctl(fd, SPI_IOC_MESSAGE(1), &t);
        require(ret >= 0, error("SPI transfer (bits=" + std::to_string(bits) + ", bytes=" + std::to_string(bytes) + ")"));
        require(static_cast<unsigned>(ret) == bytes, "short SPI transfer");
    }
    bool restore() {
        if (!saved) return true;
        // Attempt every restore even if one fails.
        bool ok = ioctl(fd, SPI_IOC_WR_MODE32, &old_mode) == 0;
        ok = (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &old_speed) == 0) && ok;
        ok = (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &old_bits) == 0) && ok;
        ok = (ioctl(fd, SPI_IOC_WR_LSB_FIRST, &old_lsb) == 0) && ok;
        if (ok) saved = false;
        else std::cerr << "SPI SETTINGS RESTORE FAILED; reinitialize using the working loader\n";
        return ok;
    }
    ~SPI() { restore(); if (fd >= 0) close(fd); }
};
void command(SPI &spi, GPIO &dc, unsigned speed, uint8_t cmd, const std::vector<uint8_t> &args = {}) {
    dc.set(false);
    spi.transfer(&cmd, 1, 8, speed);
    dc.set(true);
    if (!args.empty()) spi.transfer(args.data(), args.size(), 8, speed);
}
void initialize(SPI &spi, GPIO &dc, GPIO &reset, unsigned speed) {
    reset.set(false); std::this_thread::sleep_for(std::chrono::milliseconds(10));
    reset.set(true); std::this_thread::sleep_for(std::chrono::milliseconds(120));
    command(spi, dc, speed, 0x11);
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    require(!stopped, "interrupted during panel initialization");
    // Exact profile from release/tft_device.sh, not a universal ST7789 module profile.
    const std::vector<std::pair<uint8_t, std::vector<uint8_t>>> profile = {
        {0x36,{0xA2}}, {0x3A,{0x55}}, {0xB2,{0x0C,0x0C,0x00,0x33,0x33}},
        {0xB7,{0x35}}, {0xBB,{0x1A}}, {0xC0,{0x3C}}, {0xC2,{0x01}},
        {0xC3,{0x0B}}, {0xC4,{0x20}}, {0xC6,{0x0F}}, {0xD0,{0xA4,0xA1}}, {0x21,{}},
        {0xE0,{0x00,0x19,0x1E,0x0A,0x09,0x15,0x3D,0x44,0x51,0x12,0x03,0x00,0x3F,0x3F}},
        {0xE1,{0x00,0x18,0x1E,0x0A,0x09,0x25,0x3F,0x43,0x52,0x33,0x03,0x00,0x3F,0x3F}}, {0x29,{}}
    };
    for (const auto &entry : profile) {
        require(!stopped, "interrupted during panel initialization");
        command(spi, dc, speed, entry.first, entry.second);
    }
}
// Linux SPI words >8 bits use native-endian memory representation. Shift each
// word MSB-first so the panel receives the SAME big-endian RGB565 bytes in all modes.
void pack(const std::vector<uint16_t> &pixels, std::vector<uint32_t> &storage, unsigned bits) {
    require(bits == 8 || bits == 16 || bits == 32, "invalid packing word size");
    require(pixels.size() % 2 == 0 && storage.size() * 4 >= pixels.size() * 2, "invalid packing buffers");
    auto *out = reinterpret_cast<uint8_t *>(storage.data());
    for (size_t i = 0; i < pixels.size(); ++i) {
        if (bits == 8) { out[i * 2] = pixels[i] >> 8; out[i * 2 + 1] = pixels[i] & 255; }
        else if (bits == 16) std::memcpy(out + i * 2, &pixels[i], 2);
        else {
            uint32_t word = (uint32_t(pixels[i]) << 16) | pixels[i + 1];
            std::memcpy(out + i * 2, &word, 4);
            ++i;
        }
    }
}
double milliseconds(Clock::duration d) { return std::chrono::duration<double, std::milli>(d).count(); }
double percentile(std::vector<double> values, double fraction) {
    std::sort(values.begin(), values.end());
    return values[static_cast<size_t>((values.size() - 1) * fraction)];
}
void self_test() {
    std::vector<uint16_t> pixels = {0xf800,0x07e0,0x001f,0xffff,0x1234,0xabcd};
    std::vector<uint32_t> packed(pixels.size() / 2 + 4, 0x55555555);
    for (unsigned bits : {8u,16u,32u}) {
        pack(pixels, packed, bits);
        const auto *bytes = reinterpret_cast<const uint8_t *>(packed.data());
        std::vector<uint8_t> wire;
        for (size_t i = 0; i < pixels.size() * 2; i += bits / 8) {
            uint32_t word = 0;
            if (bits == 8) word = bytes[i];
            else if (bits == 16) { uint16_t value; std::memcpy(&value, bytes + i, 2); word = value; }
            else std::memcpy(&word, bytes + i, 4);
            for (int shift = bits - 8; shift >= 0; shift -= 8) wire.push_back((word >> shift) & 255);
        }
        for (size_t i = 0; i < pixels.size(); ++i)
            require(wire[i * 2] == pixels[i] >> 8 && wire[i * 2 + 1] == (pixels[i] & 255), "SPI wire-order packing");
        require(packed[pixels.size() / 2] == 0x55555555, "packing bounds");
    }
    pixels.resize(width * height);
    display_test_pattern::render(pixels, {width,height,"counter"}, 123456);
    require(pixels[0] == 0 && pixels[width * 107 + 44] == 0, "shared counter fixture");
    std::cout << "direct SPI benchmark self-test passed\n";
}
}
int main(int argc, char **argv) {
    try {
        for (int i = 1; i < argc; ++i) if (std::string(argv[i]) == "--help") { help(); return 0; }
        Options o = parse(argc, argv);
        if (o.self_test) { self_test(); return 0; }
        std::signal(SIGINT, stop); std::signal(SIGTERM, stop);
        SPI spi; GPIO dc, reset;
        spi.open_device(o); // Validate spidev and buffer limit before claiming GPIOs.
        dc.claim(o.dc);
        if (o.init) { reset.claim(o.reset); initialize(spi, dc, reset, o.speed); }
        // Without reset/init, caller must preserve the already-initialized panel.
        command(spi, dc, o.speed, 0x36, {0xA2});
        command(spi, dc, o.speed, 0x3A, {0x55});
        std::vector<uint16_t> pixels(width * height);
        std::vector<uint32_t> packed(frame_bytes / 4);
        std::vector<double> render_times, pack_times, command_times, transfer_times;
        render_times.reserve(o.frames); pack_times.reserve(o.frames);
        command_times.reserve(o.frames); transfer_times.reserve(o.frames);
        auto start = Clock::now();
        auto end = start + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(o.seconds));
        double transfer_ms = 0;
        unsigned count = 0;
        while (!stopped && count < o.frames && Clock::now() < end) {
            auto a = Clock::now();
            display_test_pattern::render(pixels, {width,height,o.pattern}, count);
            auto b = Clock::now();
            pack(pixels, packed, o.bits);
            auto c = Clock::now();
            command(spi, dc, o.speed, 0x2A, {0,0,1,0x3F});
            command(spi, dc, o.speed, 0x2B, {0,0,0,0xEF});
            command(spi, dc, o.speed, 0x2C);
            auto d = Clock::now();
            const auto *data = reinterpret_cast<const uint8_t *>(packed.data());
            for (unsigned offset = 0; offset < frame_bytes; offset += o.chunk)
                spi.transfer(data + offset, std::min(o.chunk, frame_bytes - offset), o.bits, o.speed);
            auto e = Clock::now();
            render_times.push_back(milliseconds(b-a)); pack_times.push_back(milliseconds(c-b));
            command_times.push_back(milliseconds(d-c)); transfer_times.push_back(milliseconds(e-d));
            transfer_ms += transfer_times.back();
            ++count;
            auto wake = std::min(end, e + std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double,std::milli>(o.delay)));
            while (!stopped && Clock::now() < wake)
                std::this_thread::sleep_until(std::min(wake, Clock::now() + std::chrono::milliseconds(10)));
        }
        double elapsed = milliseconds(Clock::now()-start) / 1000;
        bool dc_ok = dc.release(), reset_ok = reset.release(), spi_ok = spi.restore();
        require(dc_ok && reset_ok && spi_ok, "cleanup failed; restore the working display loader");
        require(count > 0, "no complete frames transferred");
        std::cout << std::fixed << std::setprecision(3)
                  << "{\"benchmark\":\"direct_spi_display\",\"measurement\":\"spi_completed_frames_not_panel_fps\",\"panel_fps\":null"
                  << ",\"pattern\":\"" << o.pattern << "\",\"requested_speed_hz\":" << o.speed
                  << ",\"selected_wire_hz\":null,\"mode\":" << o.mode << ",\"word_bits\":" << o.bits
                  << ",\"chunk_bytes\":" << o.chunk << ",\"pixel_transfers_per_frame\":" << (frame_bytes + o.chunk - 1) / o.chunk
                  << ",\"width\":320,\"height\":240,\"init_panel\":" << (o.init ? "true" : "false")
                  << ",\"delay_ms\":" << o.delay << ",\"frames\":" << count << ",\"elapsed_s\":" << elapsed
                  << ",\"completed_frame_fps\":" << count / elapsed
                  << ",\"pixel_transfer_MB_s\":" << double(count) * frame_bytes / (transfer_ms * 1000)
                  << ",\"render_p50_ms\":" << percentile(render_times,0.5)
                  << ",\"pack_p50_ms\":" << percentile(pack_times,0.5)
                  << ",\"command_p50_ms\":" << percentile(command_times,0.5)
                  << ",\"pixel_transfer_p50_ms\":" << percentile(transfer_times,0.5)
                  << ",\"pixel_transfer_p95_ms\":" << percentile(transfer_times,0.95)
                  << ",\"controls_restored\":true,\"interrupted\":" << (stopped ? "true" : "false") << "}\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "direct SPI benchmark: " << e.what() << '\n';
        return 1;
    }
}
