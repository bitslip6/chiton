// File-backed camera simulator. No hardware is accessed by this library.
#include <cstdint>
#include <aditof/camera.h>
#include <aditof/device_interface.h>
#include <aditof/frame.h>
#include <aditof/system.h>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <utility>

namespace {
constexpr unsigned width = 640, height = 480;
constexpr size_t pixels = width * height;
aditof::FrameDetails frameDetails() { return {width, height, "depth_ir"}; }

bool loadImage(const char *env, const char *fallback, std::vector<uint16_t> &out) {
    const char *overridePath = std::getenv(env);
    const std::string path = overridePath ? overridePath : fallback;
    std::ifstream file(path, std::ios::binary);
    if (!file && !overridePath) {
        // Only create missing defaults, never overwrite a supplied image.
        std::ofstream defaults(path, std::ios::binary);
        if (!defaults) {
            std::cerr << "aditof stub: cannot create " << path << '\n';
            return false;
        }
        for (size_t i = 0; i < pixels; ++i) {
            const char sample[2] = {0x40, 0x01}; // 320, little endian
            defaults.write(sample, 2);
        }
        defaults.close();
        if (!defaults) return false;
        file.open(path, std::ios::binary);
    }
    std::vector<unsigned char> bytes(pixels * 2);
    if (!file.read(reinterpret_cast<char *>(bytes.data()), bytes.size()) ||
        file.peek() != std::char_traits<char>::eof()) {
        std::cerr << "aditof stub: " << path << " must contain exactly "
                  << pixels * 2 << " bytes\n";
        return false;
    }
    out.resize(pixels);
    for (size_t i = 0; i < pixels; ++i)
        out[i] = uint16_t(bytes[2*i]) | (uint16_t(bytes[2*i+1]) << 8);
    return true;
}
}

class FrameImpl {
public:
    aditof::FrameDetails details = frameDetails();
    // RAW is depth followed by IR; all three views share the same storage.
    std::vector<uint16_t> data = std::vector<uint16_t>(pixels * 2, 320);
};

namespace aditof {
Frame::Frame() : m_impl(new FrameImpl) {}
Frame::~Frame() = default;
Frame::Frame(const Frame &other)
    : m_impl(other.m_impl ? new FrameImpl(*other.m_impl) : new FrameImpl) {}
Frame &Frame::operator=(const Frame &other) {
    if (this != &other) m_impl.reset(other.m_impl ? new FrameImpl(*other.m_impl) : new FrameImpl);
    return *this;
}
Frame::Frame(Frame &&) noexcept = default;
Frame &Frame::operator=(Frame &&) noexcept = default;
Status Frame::setDetails(const FrameDetails &details) {
    if (details.width != width || details.height != height || details.type != "depth_ir")
        return Status::INVALID_ARGUMENT;
    if (!m_impl) m_impl.reset(new FrameImpl);
    m_impl->details = details;
    return Status::OK;
}
Status Frame::getDetails(FrameDetails &details) const {
    if (!m_impl) return Status::UNAVAILABLE;
    details = m_impl->details;
    return Status::OK;
}
Status Frame::getData(FrameDataType type, uint16_t **ptr) {
    if (!ptr) return Status::INVALID_ARGUMENT;
    *ptr = nullptr;
    if (!m_impl) return Status::UNAVAILABLE;
    switch (type) {
    case FrameDataType::RAW:
    case FrameDataType::DEPTH: *ptr = m_impl->data.data(); break;
    case FrameDataType::IR: *ptr = m_impl->data.data() + pixels; break;
    default: return Status::INVALID_ARGUMENT;
    }
    return Status::OK;
}

class StubDevice final : public DeviceInterface {
    std::map<uint16_t, uint16_t> registers;
public:
    Status open() override { return Status::OK; }
    Status start() override { return Status::OK; }
    Status stop() override { return Status::OK; }
    Status getAvailableFrameTypes(std::vector<FrameDetails> &types) override {
        types = {frameDetails()}; return Status::OK;
    }
    Status setFrameType(const FrameDetails &d) override {
        return d.width == width && d.height == height && d.type == "depth_ir"
            ? Status::OK : Status::INVALID_ARGUMENT;
    }
    Status program(const uint8_t *, size_t) override { return Status::OK; }
    Status getFrame(uint16_t *) override { return Status::UNAVAILABLE; }
    Status readEeprom(uint32_t, uint8_t *, size_t) override { return Status::UNAVAILABLE; }
    Status writeEeprom(uint32_t, const uint8_t *, size_t) override { return Status::UNAVAILABLE; }
    // Narrow, opt-in release-test fault. Capture's ordinary registers continue
    // to work; this cannot affect a real SDK/hardware build.
    bool failRegisters(const uint16_t *addr, size_t n) const {
        const char *fault = std::getenv("ADITOF_STUB_FAIL_REGISTER");
        return fault && std::string(fault) == "1" &&
            std::find(addr, addr + n, uint16_t(0xc0ff)) != addr + n;
    }
    Status readAfeRegisters(const uint16_t *addr, uint16_t *data, size_t n) override {
        if (!addr || !data) return Status::INVALID_ARGUMENT;
        if (failRegisters(addr, n)) return Status::GENERIC_ERROR;
        for (size_t i = 0; i < n; ++i) data[i] = registers[addr[i]];
        return Status::OK;
    }
    Status writeAfeRegisters(const uint16_t *addr, const uint16_t *data, size_t n) override {
        if (!addr || !data) return Status::INVALID_ARGUMENT;
        if (failRegisters(addr, n)) return Status::GENERIC_ERROR;
        for (size_t i = 0; i < n; ++i) registers[addr[i]] = data[i];
        return Status::OK;
    }
    Status readAfeTemp(float &t) override { t = 25; return Status::OK; }
    Status readLaserTemp(float &t) override { t = 25; return Status::OK; }
    Status getDetails(DeviceDetails &d) const override {
        d.sensorType = SensorType::SENSOR_96TOF1; return Status::OK;
    }
};

class StubCamera final : public Camera {
    bool initialized = false, started = false;
    bool lifecycle_fault() const {
        const char *path = std::getenv("ADITOF_STUB_LIFECYCLE_FAILURE_FILE");
        return path && std::ifstream(path).good();
    }
    std::vector<uint16_t> depth, ir;
    std::string mode = "near";
    std::map<std::string, std::string> controls;
    std::shared_ptr<DeviceInterface> device = std::make_shared<StubDevice>();
public:
    Status initialize() override {
        initialized = loadImage("ADITOF_STUB_IR", "aditof-ir.bin", ir) &&
                      loadImage("ADITOF_STUB_DEPTH", "aditof-depth.bin", depth);
        return initialized ? Status::OK : Status::GENERIC_ERROR;
    }
    Status start() override {
        if (!initialized) return Status::UNAVAILABLE;
        if (lifecycle_fault()) return Status::GENERIC_ERROR;
        started = true; return Status::OK;
    }
    Status stop() override {
        if (lifecycle_fault()) return Status::GENERIC_ERROR;
        started = false; return Status::OK;
    }
    Status setMode(const std::string &value, const std::string &) override {
        mode = value; return Status::OK;
    }
    Status getAvailableModes(std::vector<std::string> &v) const override {
        v = {"near", "medium", "far"}; return Status::OK;
    }
    Status setFrameType(const std::string &v) override {
        return v == "depth_ir" ? Status::OK : Status::INVALID_ARGUMENT;
    }
    Status getAvailableFrameTypes(std::vector<std::string> &v) const override {
        v = {"depth_ir"}; return Status::OK;
    }
    Status requestFrame(Frame *frame, FrameUpdateCallback cb) override {
        if (!frame) return Status::INVALID_ARGUMENT;
        if (!initialized || !started) return Status::UNAVAILABLE;
        Status status = frame->setDetails(frameDetails());
        if (status != Status::OK) return status;
        uint16_t *dst = nullptr;
        frame->getData(FrameDataType::RAW, &dst);
        std::copy(depth.begin(), depth.end(), dst);
        std::copy(ir.begin(), ir.end(), dst + pixels);
        // Deterministic simulator: callbacks execute synchronously.
        if (cb) cb(Status::OK, frame);
        return Status::OK;
    }
    Status getDetails(CameraDetails &d) const override {
        d = {}; d.cameraId = "file-backed-stub"; d.mode = mode;
        d.frameType = frameDetails(); d.connection = ConnectionType::LOCAL;
        d.minDepth = 0; d.maxDepth = 65535; d.bitCount = 16;
        return Status::OK;
    }
    Status getAvailableControls(std::vector<std::string> &v) const override {
        v = {"geometry_correction", "noise_reduction_threshold"}; return Status::OK;
    }
    Status setControl(const std::string &key, const std::string &v) override {
        controls[key] = v; return Status::OK;
    }
    Status getControl(const std::string &key, std::string &v) const override {
        auto it = controls.find(key);
        if (it == controls.end()) return Status::INVALID_ARGUMENT;
        v = it->second; return Status::OK;
    }
    std::shared_ptr<DeviceInterface> getDevice() override { return device; }
    Status forceGain(float, float, int) override { return Status::OK; }
    Calibration96Tof1 *getCalibration() override { return nullptr; }
};
}

class SystemImpl {
public:
    std::shared_ptr<aditof::Camera> camera = std::make_shared<aditof::StubCamera>();
};
namespace aditof {
System::System() : m_impl(new SystemImpl) {}
System::~System() = default;
System::System(System &&) noexcept = default;
System &System::operator=(System &&) noexcept = default;
Status System::initialize() {
    if (!m_impl) m_impl.reset(new SystemImpl);
    return Status::OK;
}
Status System::getCameraList(std::vector<std::shared_ptr<Camera>> &v) const {
    if (!m_impl) return Status::UNAVAILABLE;
    v = {m_impl->camera}; return Status::OK;
}
Status System::getCameraListAtIp(std::vector<std::shared_ptr<Camera>> &v, const std::string &) const {
    v.clear(); return Status::UNAVAILABLE;
}
}
