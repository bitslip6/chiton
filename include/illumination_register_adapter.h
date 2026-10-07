#pragma once
#include "illumination_controller.h"
#include <aditof/status_definitions.h>
#include <array>
#include <stdexcept>

namespace illumination {
struct Receipt {
    aditof::Status status;
    bool certain_tuple_ack; // SDK OK alone is not physical settling or atomicity.
    uint64_t eligible_from;
};
class RegisterPort {
public:
    virtual ~RegisterPort() = default;
    virtual Receipt write(uint64_t frame, uint64_t command,
        const std::array<uint16_t, 4> &addresses,
        const std::array<uint16_t, 4> &values) = 0;
};
enum class FailurePolicy { Uncertain, UnchangedByOperatorContract };
// Narrow SDK boundary. Physical settling remains separate from acknowledgment.
class RegisterAdapter {
public:
    RegisterAdapter(RegisterPort &port, std::array<unsigned, 4> register_to_sector,
                    bool contract_available, FailurePolicy failure = FailurePolicy::Uncertain)
        : port_(port), mapping_(register_to_sector), available_(contract_available), failure_(failure) {
        std::array<bool, 4> seen{};
        for (auto i : mapping_) {
            if (i >= 4 || seen[i]) throw std::invalid_argument("register permutation");
            seen[i] = true;
        }
    }
    bool opportunity(uint64_t frame, Controller &controller) {
        if (!available_ || frame % 3 != 1) return false;
        const auto s = controller.state();
        if (!s.pending && !s.applied_known) return false;
        const auto t = s.pending ? s.desired.tuple : s.applied;
        if (!controller.legal(t)) return false;
        std::array<uint16_t, 4> values{};
        for (unsigned i = 0; i < 4; ++i) values[i] = static_cast<uint16_t>(t[mapping_[i]]);
        auto result = port_.write(frame, s.pending ? s.desired.generation : s.command,
                                 {{0x4011, 0x4012, 0x4013, 0x4014}}, values);
        bool ok = result.status == aditof::Status::OK && result.certain_tuple_ack;
        if (!ok && result.status != aditof::Status::OK && failure_ == FailurePolicy::UnchangedByOperatorContract)
            controller.write_failed_unchanged(s.pending ? s.desired.generation : s.command);
        else if (s.pending) controller.acknowledge(s.desired.generation, ok, result.eligible_from);
        else if (!ok) controller.invalidate(s.lifecycle, Reason::WriteFailed);
        // Identical successful refresh does NOT restart settling in this fixture
        // contract. This is not a claim about real registers.
        return true;
    }
private:
    RegisterPort &port_;
    std::array<unsigned, 4> mapping_;
    bool available_;
    FailurePolicy failure_;
};
}
