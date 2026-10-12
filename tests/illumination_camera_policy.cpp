// Real SDK wrapper through a fake DeviceInterface: no library/device access.
#include "illumination_camera_policy.h"
#include "illumination_sdk.h"
#include "illumination_selection.h"
#include "illumination_diagnostics.h"
#include <cassert>
#include <memory>
using namespace illumination;
class Device final : public aditof::DeviceInterface {
public:
    Tuple actual{{12,12,12,12}}, queued{};
    unsigned delay = 1;
    uint64_t frame = 0, effect_at = 0, calls = 0;
    bool pending = false;
    aditof::Status result = aditof::Status::OK;
    struct Call { std::array<uint16_t,4> addresses{}, values{}; aditof::Status status; };
    std::array<Call,32> log{};
    void advance(uint64_t f) { frame = f; if (pending && f >= effect_at) { actual = queued; pending = false; } }
    aditof::Status writeAfeRegisters(const uint16_t *a, const uint16_t *v, size_t n) override {
        assert(n == 4);
        Call c; std::copy_n(a,4,c.addresses.begin()); std::copy_n(v,4,c.values.begin()); c.status = result;
        log[calls++ % log.size()] = c;
        if (result == aditof::Status::OK) {
            Tuple t{{int(v[3]),int(v[2]),int(v[1]),int(v[0])}};
            for (int p : t) assert(p >= 1 && p <= 12);
            if (!pending && t != actual) { queued = t; effect_at = frame + delay; pending = true; }
        }
        return result; // Explicit fixture: non-OK leaves old tuple unchanged.
    }
    aditof::Status open() override { return aditof::Status::OK; }
    aditof::Status start() override { return aditof::Status::OK; }
    aditof::Status stop() override { return aditof::Status::OK; }
    aditof::Status getAvailableFrameTypes(std::vector<aditof::FrameDetails> &) override { return aditof::Status::UNAVAILABLE; }
    aditof::Status setFrameType(const aditof::FrameDetails &) override { return aditof::Status::UNAVAILABLE; }
    aditof::Status program(const uint8_t *, size_t) override { return aditof::Status::UNAVAILABLE; }
    aditof::Status getFrame(uint16_t *) override { return aditof::Status::UNAVAILABLE; }
    aditof::Status readEeprom(uint32_t, uint8_t *, size_t) override { return aditof::Status::UNAVAILABLE; }
    aditof::Status writeEeprom(uint32_t, const uint8_t *, size_t) override { return aditof::Status::UNAVAILABLE; }
    aditof::Status readAfeRegisters(const uint16_t *, uint16_t *, size_t) override { return aditof::Status::UNAVAILABLE; }
    aditof::Status readAfeTemp(float &) override { return aditof::Status::UNAVAILABLE; }
    aditof::Status readLaserTemp(float &) override { return aditof::Status::UNAVAILABLE; }
    aditof::Status getDetails(aditof::DeviceDetails &) const override { return aditof::Status::UNAVAILABLE; }
};
static Observation observation(uint64_t frame, uint64_t lifecycle, const Device &d, int boundary = 6) {
    Observation o; o.frame = o.time = frame; o.lifecycle = lifecycle; o.supported = o.eligible = true;
    for (unsigned i = 0; i < 4; ++i) o.sectors[i] = {true, d.actual[i] > boundary ? 25u : 0u, 100};
    return o;
}
static void latency(unsigned delay) {
    auto device = std::make_shared<Device>(); device->delay = delay;
    DevicePort port(device); CameraPolicy policy(port, {{4,4,4,4}});
    policy.lifecycle(1,1,true);
    assert(!policy.state().applied_known);
    for (uint64_t f = 1; f <= 10000; ++f) {
        device->advance(f); policy.observe(observation(f,1,*device));
        policy.opportunity(f);
        if (f == 1) {
            assert((policy.state().applied == Tuple{{4,4,4,4}}));
            assert((device->actual == Tuple{{12,12,12,12}})); // Ack != physical effect.
            assert(policy.state().eligible_from == 3);
        }
        if (f == 2) for (auto n : policy.state().clear) assert(n == 0);
        if (f == 3) for (auto n : policy.state().clear) assert(n == 1);
    }
    assert((policy.state().applied == Tuple{{6,6,6,6}}));
    assert(policy.state().failed_probes == 4);
    auto transitions = policy.state().transitions;
    auto ceilings = policy.state().ceiling;
    policy.lifecycle(2,10001,true); // Restart: knowledge invalidated, ceiling retained.
    assert(!policy.state().applied_known && policy.state().ceiling == ceilings);
    for (uint64_t f = 10001; f <= 11000; ++f) {
        device->advance(f); policy.observe(observation(f,2,*device)); policy.opportunity(f);
    }
    assert(policy.state().transitions == transitions && policy.state().ceiling == ceilings);
    assert(policy.state().failed_probes == 4);
    assert(device->calls == (11000 + 2) / 3); // Unchanged writes remain scheduled.
    auto tuple = policy.state().applied;
    policy.lifecycle(3,11001,false); // Display/config: requalify, no SDK normalization.
    assert(policy.state().applied_known && policy.state().applied == tuple);
    assert(policy.state().ceiling == ceilings);
}
static void failures() {
    for (auto status : {aditof::Status::BUSY, aditof::Status::GENERIC_ERROR, aditof::Status::UNAVAILABLE}) {
        auto device = std::make_shared<Device>(); DevicePort port(device); CameraPolicy policy(port, {{4,4,4,4}});
        policy.lifecycle(1,1,true); device->advance(1); policy.opportunity(1);
        for (uint64_t f = 2; f <= 6; ++f) {
            device->advance(f); policy.observe(observation(f,1,*device)); policy.opportunity(f);
        }
        auto old = policy.state().applied;
        auto generation = policy.state().command;
        device->result = status; device->advance(7); policy.observe(observation(7,1,*device));
        assert(policy.state().pending);
        policy.opportunity(7);
        assert(policy.state().applied_known && !policy.state().pending);
        assert(policy.state().applied == old && policy.state().command == generation);
        assert(policy.state().reason == Reason::WriteFailed);
        assert(device->actual == old);
        for (auto n : policy.state().clear) assert(n == 0);
        assert(policy.failures() == 1 && policy.state().failed_probes == 0);
        assert(!policy.opportunity(8)); // No immediate blind retry.
        device->result = aditof::Status::OK;
        for (uint64_t f = 8; f <= 11; ++f) {
            device->advance(f); policy.observe(observation(f,1,*device)); policy.opportunity(f);
            assert(!policy.state().pending && policy.state().applied == old);
        }
        device->advance(12); policy.observe(observation(12,1,*device));
        assert(policy.state().pending && policy.state().desired.generation >= 3);
        for (unsigned i = 0; i < 4; ++i) assert(policy.state().desired.tuple[i] <= old[i] + 1);
    }
    auto device = std::make_shared<Device>(); device->result = aditof::Status::BUSY;
    DevicePort port(device); CameraPolicy policy(port, {{4,4,4,4}}); policy.lifecycle(1,1,true);
    device->advance(1); policy.opportunity(1);
    assert(policy.startup_failed() && !policy.state().applied_known);
    for (uint64_t f = 2; f <= 100; ++f) { device->advance(f); policy.observe(observation(f,1,*device)); policy.opportunity(f); }
    assert(device->calls == 1); // No known old startup tuple to preserve.
    device->result = aditof::Status::OK; policy.lifecycle(2,101,true);
    device->advance(103); policy.opportunity(103);
    assert(policy.state().applied_known && !policy.startup_failed());
}
static void retreat_restart() {
    auto device = std::make_shared<Device>(); DevicePort port(device);
    CameraPolicy policy(port, {{4,4,4,4}}); policy.lifecycle(1,1,true);
    uint64_t frame = 0;
    for (frame = 1; frame < 2000; ++frame) {
        device->advance(frame); policy.observe(observation(frame,1,*device));
        if (policy.state().failed_probes && policy.state().pending) device->result = aditof::Status::BUSY;
        policy.opportunity(frame);
        if (policy.failures()) break;
    }
    assert(frame < 2000 && policy.state().applied_known);
    assert(policy.state().applied[0] == 7 && policy.state().ceiling[0] == 6);
    device->result = aditof::Status::OK;
    policy.lifecycle(2,frame+1,true);
    while (++frame % 3 != 1) device->advance(frame);
    device->advance(frame); assert(policy.opportunity(frame));
    assert(policy.state().applied[0] == 6 && policy.state().ceiling[0] == 6);
    assert(device->log[(device->calls-1)%device->log.size()].values[3] == 6);
}
static void proxy() {
    auto device = std::make_shared<Device>();
    DevicePort port(device); CameraPolicy mapping(port, {{1,2,3,12}});
    mapping.lifecycle(1,1,true); device->advance(1); assert(mapping.opportunity(1));
    assert((device->log[0].addresses == std::array<uint16_t,4>{{0x4011,0x4012,0x4013,0x4014}}));
    assert((device->log[0].values == std::array<uint16_t,4>{{12,3,2,1}}));
    assert((mapping.state().applied == Tuple{{1,2,3,12}}));
    const uint16_t depth[] = {0,249,250,4095,249,250,2000,2000,0,0};
    const uint16_t buckets[] = {128,128,128,128,320,320,512,704,127,896};
    Span spans[] = {{0,10}};
    auto s = depth_proxy(depth,10,buckets,spans,1);
    assert(s[0].known && s[0].eligible == 4 && s[0].saturated == 2);
    assert(s[1].eligible == 2 && s[1].saturated == 1);
    assert(s[2].eligible == 1 && s[2].saturated == 0);
    assert(s[3].eligible == 1);
    for (int mode : {0,1,3,4,5,6}) {
        (void)mode; assert(depth_proxy(depth,10,buckets,spans,1)[0].saturated == 2);
    }
    spans[0].end = 11;
    for (auto v : depth_proxy(depth,10,buckets,spans,1)) assert(!v.known);
    for (auto v : depth_proxy(nullptr,10,buckets,spans,1)) assert(!v.known);
    assert((parse_initial_tuple("1,2,3,12") == Tuple{{1,2,3,12}}));
    for (const char *bad : {"0,1,1,1", "13,1,1,1", "1,1,1", "1,1,1,1,1", "-1,1,1,1", "9999999,1,1,1"}) {
        bool rejected = false;
        try { parse_initial_tuple(bad); } catch (const std::invalid_argument &) { rejected = true; }
        assert(rejected);
    }
    Diagnostics d; d.experimental = d.depth = true; d.observation.supported = true;
    d.observation.sectors = s; publish_diagnostics(d);
    auto metrics = diagnostics();
    assert(metrics["selected_policy"] == "experimental_depth_proxy");
    assert(metrics["acknowledged"].is_null());
    assert(metrics["sectors"][0]["saturated"] == 2);
}
static_assert(sizeof(CameraPolicy) < 2048, "Camera policy state must remain bounded");
int main() { proxy(); latency(1); latency(2); failures(); retreat_restart(); }
