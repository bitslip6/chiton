// No device/SDK execution: exercise the production acquisition owner with a fake.
#include "camera.h"
#include "data.h"
#include "system.h"
#include <cassert>

GlobalOpts *GOPTS = nullptr;
std::shared_ptr<aditof::Camera> camera;
namespace data {
std::condition_variable &frame_changed() { static std::condition_variable cv; return cv; }
void invalidate_measurements() {}
}

class Camera : public aditof::Camera {
public:
    bool streaming = false;
    int starts = 0, stops = 0, frames = 0;
    aditof::Status start_result = aditof::Status::OK, stop_result = aditof::Status::OK;
    aditof::Status frame_result = aditof::Status::OK;
    aditof::Status initialize() override { return aditof::Status::OK; }
    aditof::Status start() override {
        ++starts;
        if (start_result != aditof::Status::OK) return start_result;
        if (streaming) return aditof::Status::BUSY;
        streaming = true;
        return aditof::Status::OK;
    }
    aditof::Status stop() override {
        ++stops;
        if (stop_result != aditof::Status::OK) return stop_result;
        if (!streaming) return aditof::Status::BUSY;
        streaming = false;
        return aditof::Status::OK;
    }
    aditof::Status setMode(const std::string &, const std::string &) override {
        streaming = true; return aditof::Status::OK; // Legacy SDK configuration auto-start.
    }
    aditof::Status getAvailableModes(std::vector<std::string> &) const override { return aditof::Status::OK; }
    aditof::Status setFrameType(const std::string &) override { return aditof::Status::OK; }
    aditof::Status getAvailableFrameTypes(std::vector<std::string> &) const override { return aditof::Status::OK; }
    aditof::Status requestFrame(aditof::Frame *, aditof::FrameUpdateCallback) override {
        ++frames;
        return streaming ? frame_result : aditof::Status::UNAVAILABLE;
    }
    aditof::Status getDetails(aditof::CameraDetails &) const override { return aditof::Status::OK; }
    aditof::Status getAvailableControls(std::vector<std::string> &) const override { return aditof::Status::OK; }
    aditof::Status setControl(const std::string &, const std::string &) override { return aditof::Status::OK; }
    aditof::Status getControl(const std::string &, std::string &) const override { return aditof::Status::OK; }
    std::shared_ptr<aditof::DeviceInterface> getDevice() override { return {}; }
    aditof::Status forceGain(float, float, int) override { return aditof::Status::OK; }
    Calibration96Tof1 *getCalibration() override { return nullptr; }
};

int main() {
    GlobalOpts options{};
    GOPTS = &options;
    assert(cam::initialize_capture(true, {}).status == 503);
    auto fake = std::make_shared<Camera>();
    camera = fake;
    auto verify = [&]() { return fake->requestFrame(nullptr, nullptr); };
    fake->setMode("near", "");
    assert(cam::initialize_capture(true, verify).status == 200);
    assert(fake->streaming && GOPTS->enable && fake->starts == 1 && fake->stops == 0 && fake->frames == 1);
    assert(cam::set_capture(true).status == 200 && fake->starts == 1);
    assert(cam::set_capture(false).status == 200 && !fake->streaming && !GOPTS->enable);
    assert(cam::set_capture(false).status == 200 && fake->stops == 1);
    fake->start_result = aditof::Status::BUSY;
    assert(cam::set_capture(true).status == 500 && !GOPTS->enable); // BUSY != acknowledged start.
    fake->start_result = aditof::Status::OK;
    assert(cam::set_capture(true).status == 200 && GOPTS->enable);

    fake->setMode("far", "");
    const int starts = fake->starts;
    assert(cam::initialize_capture(false, verify).status == 200);
    assert(!fake->streaming && !GOPTS->enable && fake->starts == starts);
    // Stopped-on-configuration SDK: perform an acknowledged start, no probe/stop.
    const int frame_count = fake->frames, stop_count = fake->stops;
    assert(cam::initialize_capture(true, verify).status == 200 && fake->streaming);
    assert(fake->frames == frame_count && fake->stops == stop_count);
    assert(cam::set_capture(false).status == 200);
    fake->stop_result = aditof::Status::GENERIC_ERROR;
    const int before = fake->starts;
    assert(cam::initialize_capture(false, verify).status == 500 && fake->starts == before && !GOPTS->enable);
    fake->stop_result = aditof::Status::OK;
    fake->start_result = aditof::Status::GENERIC_ERROR;
    assert(cam::initialize_capture(true, verify).status == 500 && !fake->streaming && !GOPTS->enable);
    // Neither resource BUSY without frames nor an unverified BUSY is success.
    fake->start_result = aditof::Status::BUSY;
    assert(cam::initialize_capture(true, verify).status == 500 && !GOPTS->enable);
    fake->streaming = true;
    fake->frame_result = aditof::Status::GENERIC_ERROR;
    assert(cam::initialize_capture(true, verify).status == 500 && !GOPTS->enable);
    assert(cam::initialize_capture(true, {}).status == 500 && !GOPTS->enable);
    // Experimental ownership generations change only on acknowledged lifecycle
    // operations; no additional SDK start/stop/frame calls are introduced.
    GOPTS->experimental_illumination = true;
    fake->streaming = false; fake->start_result = aditof::Status::OK;
    assert(cam::set_capture(true).status == 200);
    assert(GOPTS->illumination_generation == 1 && GOPTS->illumination_capture_generation == 1);
    const int experimental_starts = fake->starts;
    assert(cam::set_capture(true).status == 200 && fake->starts == experimental_starts);
    assert(GOPTS->illumination_generation == 1);
    fake->stop_result = aditof::Status::GENERIC_ERROR;
    assert(cam::set_capture(false).status == 500 && GOPTS->illumination_generation == 1);
    fake->stop_result = aditof::Status::OK;
    assert(cam::set_capture(false).status == 200);
    assert(GOPTS->illumination_generation == 2 && GOPTS->illumination_capture_generation == 2);
    camera.reset();
    GOPTS = nullptr;
}
