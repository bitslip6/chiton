#pragma once
#include "illumination_controller.h"
#include "illumination_register_adapter.h"
#include <array>
#include <algorithm>
#include <cassert>

namespace fixture {
using namespace illumination;
// Fictional units/flags, never a production detector or hardware default.
inline Parameters parameters() { return {0, 12, 2, 1, 20000, 100000, 5, true}; }
struct Sensor {
    Tuple actual{}, boundary{{6,6,6,6}}, queued{};
    uint64_t command = 1, queued_command = 0, effect_at = 0;
    bool missing = false, supported = true, crosstalk = false;
    void queue(Tuple tuple, uint64_t generation, uint64_t frame) {
        assert(!queued_command); // Only one changed command in flight.
        queued = tuple; queued_command = generation; effect_at = frame + 2;
    }
    Observation observe(uint64_t frame, uint64_t lifecycle = 1) {
        if (queued_command && frame >= effect_at) {
            actual = queued; command = queued_command; queued_command = 0;
        }
        Observation o;
        o.frame = frame; o.time = frame; o.lifecycle = lifecycle;
        o.command = command; o.supported = supported; o.eligible = true;
        for (unsigned i = 0; i < 4; ++i) {
            bool high = actual[i] > boundary[i] || (crosstalk && i == 1 && actual[0] > 2);
            o.sectors[i] = {!missing, high ? 25u : 0u, missing ? 0u : 100u};
        }
        return o;
    }
    int apparent_depth(unsigned i) const { return actual[i] > boundary[i] ? 400 : 2000; }
};
struct FakePort : RegisterPort {
    struct Call {
        uint64_t frame = 0, command = 0;
        std::array<uint16_t,4> addresses{}, values{};
        aditof::Status status = aditof::Status::OK;
    };
    Sensor sensor;
    std::array<Call, 32> log{};
    unsigned cursor = 0;
    uint64_t calls = 0;
    aditof::Status result = aditof::Status::OK;
    bool certain = true, partial = false;
    Receipt write(uint64_t frame, uint64_t command, const std::array<uint16_t,4> &a,
                  const std::array<uint16_t,4> &v) override {
        log[cursor] = {frame, command, a, v, result};
        cursor = (cursor + 1) % log.size(); ++calls;
        Tuple t{{int(v[3]), int(v[2]), int(v[1]), int(v[0])}}; // FICTIONAL approved permutation.
        if (result == aditof::Status::OK && certain) {
            if (command != sensor.command && command != sensor.queued_command) sensor.queue(t, command, frame);
        } else if (partial) {
            sensor.actual[0] = t[0]; // Non-OK is not proof of no application.
            sensor.queued_command = 0;
        }
        return {result, certain, frame + 4}; // 2 effect + 2 settling, fictional.
    }
    // Lifecycle/external writer events are injected by test owner, NOT SDK recovery.
    void external(Tuple t) { sensor.actual = t; sensor.queued_command = 0; }
};
struct Run {
    Controller controller{parameters()};
    FakePort port;
    RegisterAdapter adapter{port, {{3,2,1,0}}, true};
    uint64_t frame = 0, lifecycle = 1;
    explicit Run(Tuple initial, Tuple boundaries = {{6,6,6,6}}) {
        port.sensor.actual = initial; port.sensor.boundary = boundaries;
        assert(controller.recover(initial, lifecycle, 1, 1));
    }
    void tick() {
        auto o = port.sensor.observe(++frame, lifecycle);
        controller.observe(o);
        adapter.opportunity(frame, controller);
        if (controller.state().pending) assert(controller.legal(controller.state().desired.tuple));
    }
    void run(unsigned n) { while (n--) tick(); }
    void constant_safe(unsigned n = 1000) {
        run(1000);
        assert(!controller.state().pending && controller.state().applied_known);
        auto t = controller.state().applied;
        auto transitions = controller.state().transitions;
        run(n);
        assert(controller.state().applied == t && controller.state().transitions == transitions);
        for (unsigned i = 0; i < 4; ++i) {
            assert(controller.state().safe_known[i]);
            assert(t[i] <= port.sensor.boundary[i]);
        }
    }
};
// Mirrors legacy arithmetic only for representable fixture distances/power.
inline int legacy(int power, int depth) {
    int mapped = int(14.0f * ((depth - 400) / 1600.0f) - 1.0f);
    if (power - 2 > mapped) return power - std::max(1, (power - mapped) / 2);
    if (power + 2 < mapped) return power + std::max(1, (mapped - power) / 2);
    return power;
}
}
