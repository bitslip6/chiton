#include <cstdint>
#include <aditof/camera.h>
#include <aditof/device_interface.h>
#include <aditof/frame.h>
#include <aditof/system.h>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <utility>

#define CHECK(expr) do { if (!(expr)) { \
    std::cerr << "Failed at line " << __LINE__ << ": " << #expr << '\n'; \
    return 1; } } while (0)

static void image(const char *path, uint16_t value, size_t count = 640 * 480) {
    std::ofstream out(path, std::ios::binary);
    for (size_t i = 0; i < count; ++i) {
        const char bytes[2] = {static_cast<char>(value & 255), static_cast<char>(value >> 8)};
        out.write(bytes, 2);
    }
}

int main() {
    using namespace aditof;
    unsetenv("ADITOF_STUB_IR");
    unsetenv("ADITOF_STUB_DEPTH");
    System system;
    CHECK(system.initialize() == Status::OK);
    std::vector<std::shared_ptr<Camera>> cameras;
    CHECK(system.getCameraList(cameras) == Status::OK && cameras.size() == 1);
    auto camera = cameras.front();
    CHECK(camera->initialize() == Status::OK);
    CHECK(camera->start() == Status::OK);
    Frame frame;
    bool called = false;
    CHECK(camera->requestFrame(&frame, [&](Status s, Frame *f) {
        called = s == Status::OK && f == &frame;
    }) == Status::OK && called);
    FrameDetails details;
    CHECK(frame.getDetails(details) == Status::OK);
    CHECK(details.width == 640 && details.height == 480);
    uint16_t *ir = nullptr, *depth = nullptr;
    CHECK(frame.getData(FrameDataType::IR, &ir) == Status::OK);
    CHECK(frame.getData(FrameDataType::DEPTH, &depth) == Status::OK);
    for (size_t i = 0; i < 640 * 480; ++i) CHECK(ir[i] == 320 && depth[i] == 320);
    for (const char *path : {"aditof-ir.bin", "aditof-depth.bin"}) {
        std::ifstream in(path, std::ios::binary | std::ios::ate);
        CHECK(in && in.tellg() == 640 * 480 * 2);
    }
    Frame copy(frame);
    depth[0] = 1;
    uint16_t *copied = nullptr;
    CHECK(copy.getData(FrameDataType::DEPTH, &copied) == Status::OK && copied[0] == 320);
    Frame moved(std::move(copy));
    CHECK(moved.getData(FrameDataType::DEPTH, &copied) == Status::OK && copied[0] == 320);

    image("custom-ir.bin", 0x1234);
    image("custom-depth.bin", 0xabcd);
    setenv("ADITOF_STUB_IR", "custom-ir.bin", 1);
    setenv("ADITOF_STUB_DEPTH", "custom-depth.bin", 1);
    CHECK(camera->initialize() == Status::OK);
    CHECK(camera->requestFrame(&frame) == Status::OK);
    for (size_t i = 0; i < 640 * 480; ++i) CHECK(ir[i] == 0x1234 && depth[i] == 0xabcd);
    CHECK(camera->requestFrame(nullptr) == Status::INVALID_ARGUMENT);
    CHECK(frame.getData(FrameDataType::IR, nullptr) == Status::INVALID_ARGUMENT);
    uint16_t address = 4, value = 77, read = 0;
    CHECK(camera->getDevice()->writeAfeRegisters(&address, &value, 1) == Status::OK);
    CHECK(camera->getDevice()->readAfeRegisters(&address, &read, 1) == Status::OK && read == 77);

    image("short.bin", 5, 1);
    image("long.bin", 5, 640 * 480 + 1);
    for (const char *path : {"short.bin", "long.bin", "missing.bin"}) {
        setenv("ADITOF_STUB_IR", path, 1);
        CHECK(camera->initialize() == Status::GENERIC_ERROR);
        CHECK(camera->requestFrame(&frame) == Status::UNAVAILABLE);
    }
    CHECK(camera->stop() == Status::OK);
    std::cout << "aditof stub tests passed\n";
}
