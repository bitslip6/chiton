#pragma once
#include "illumination_register_adapter.h"
#include <cstddef>
#include <algorithm>
#include <limits>

namespace illumination {
// Existing AOI runline endpoints, not display pixels or SLAM history.
struct Span { size_t begin, end; };
inline std::array<Samples, 4> depth_proxy(const uint16_t *depth, size_t pixels,
    const uint16_t *buckets, const Span *spans, size_t count) {
    std::array<Samples, 4> result{};
    if (!depth || !buckets || !spans || !count || pixels > UINT32_MAX) return result;
    size_t previous_end = 0;
    for (size_t r = 0; r < count; ++r) {
        if (spans[r].begin > spans[r].end || spans[r].end > pixels ||
            (r && spans[r].begin < previous_end)) return {}; // Malformed geometry is unknown.
        previous_end = spans[r].end;
        for (size_t i = spans[r].begin; i < spans[r].end; ++i) {
            if (buckets[i] < 128 || buckets[i] >= 896) continue;
            unsigned sector = (buckets[i] - 128) / 192;
            ++result[sector].eligible;
            // Explicit temporary operator assumption: includes zero; no inferred
            // validity mask. Not a documented sensor overexposure detector.
            result[sector].saturated += depth[i] < 250;
        }
    }
    for (auto &s : result) s.known = s.eligible != 0;
    return result;
}
inline Parameters experimental_parameters() { return {1, 12, 2, 1, 20000, 100000, 5, true}; }

// Camera-worker owned. Caller serializes lifecycle/settings/register writers.
class CameraPolicy {
    class CountedPort : public RegisterPort {
    public:
        RegisterPort &port;
        uint64_t successes = 0, failures = 0;
        aditof::Status last_status = aditof::Status::UNAVAILABLE;
        Tuple last_requested{};
        uint64_t requested_generation = 0;
        explicit CountedPort(RegisterPort &p) : port(p) {}
        Receipt write(uint64_t f, uint64_t g, const std::array<uint16_t,4> &a,
                      const std::array<uint16_t,4> &v) override {
            for (unsigned i = 0; i < 4; ++i) last_requested[i] = v[3-i];
            requested_generation = g;
            auto receipt = port.write(f, g, a, v);
            last_status = receipt.status;
            auto &counter = receipt.status == aditof::Status::OK && receipt.certain_tuple_ack ? successes : failures;
            if (counter != UINT64_MAX) ++counter;
            return receipt;
        }
    } port_;
    Controller controller_{experimental_parameters()};
    RegisterAdapter adapter_;
    Tuple establishment_;
    uint64_t next_generation_ = 1;
    bool startup_failed_ = false;
public:
    CameraPolicy(RegisterPort &port, Tuple explicit_initial)
        : port_(port), adapter_(port_, {{3,2,1,0}}, true, FailurePolicy::UnchangedByOperatorContract),
          establishment_(explicit_initial) {
        if (!controller_.legal(explicit_initial)) throw std::invalid_argument("initial illumination must be 1..12");
    }
    const Snapshot &state() const { return controller_.state(); }
    uint64_t successes() const { return port_.successes; }
    uint64_t failures() const { return port_.failures; }
    aditof::Status last_status() const { return port_.last_status; }
    const Tuple &last_requested() const { return port_.last_requested; }
    uint64_t requested_generation() const { return port_.requested_generation; }
    bool startup_failed() const { return startup_failed_; }
    void lifecycle(uint64_t generation, uint64_t frame, bool hardware_invalidated) {
        if (generation == state().lifecycle) return;
        if (state().applied_known) establishment_ = state().applied;
        const auto watermark = controller_.generation_watermark();
        next_generation_ = watermark == UINT64_MAX ? UINT64_MAX : std::max(next_generation_, watermark + 1);
        if (hardware_invalidated) {
            for (unsigned i = 0; i < 4; ++i)
                establishment_[i] = std::min(establishment_[i], state().ceiling[i]);
            controller_.invalidate(generation);
            // Re-establish the owned tuple, capped by learned ceilings so a
            // restart cannot repeat a known-unsafe level after a failed retreat.
            // Never readback, reset ceilings, force full power or normalize capture.
            startup_failed_ = false;
        } else controller_.requalify(generation, frame + 2);
    }
    void observe(Observation observation) {
        if (startup_failed_) return;
        const bool known = state().applied_known;
        if (known) establishment_ = state().applied;
        observation.command = state().command;
        controller_.observe(observation);
        if (known && !state().applied_known) startup_failed_ = true;
    }
    bool opportunity(uint64_t frame) {
        if (frame % 3 != 1 || startup_failed_) return false;
        if (state().applied_known) {
            establishment_ = state().applied;
            const bool called = adapter_.opportunity(frame, controller_);
            if (!state().applied_known) startup_failed_ = true;
            return called;
        }
        std::array<uint16_t,4> values{};
        for (unsigned i = 0; i < 4; ++i) values[i] = static_cast<uint16_t>(establishment_[3-i]);
        if (frame > UINT64_MAX - 2 || next_generation_ == UINT64_MAX) { startup_failed_ = true; return false; }
        auto receipt = port_.write(frame, next_generation_, {{0x4011,0x4012,0x4013,0x4014}}, values);
        if (receipt.status == aditof::Status::OK && receipt.certain_tuple_ack) {
            controller_.recover(establishment_, state().lifecycle, next_generation_++, receipt.eligible_from);
        } else {
            // There was no known old tuple at startup, so the unchanged-failure
            // assumption cannot conjure one. Suspend until an explicit restart.
            controller_.invalidate(state().lifecycle, Reason::WriteFailed);
            startup_failed_ = true;
        }
        return true;
    }
};
}
