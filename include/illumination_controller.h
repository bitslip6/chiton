#pragma once
#include <array>
#include <cstdint>

// Pure policy. Production integration is separately selected and default-off.
namespace illumination {
using Tuple = std::array<int, 4>;
struct Parameters {
    int minimum, maximum, decrease_step, probe_step;
    unsigned low_ppm, high_ppm, clear_frames;
    bool independent_sectors; // Must be explicitly qualified, never inferred.
};
struct Samples {
    bool known = false;
    uint32_t saturated = 0, eligible = 0;
};
struct Observation {
    uint64_t frame = 0, time = 0, lifecycle = 0, command = 0;
    bool supported = false, eligible = false;
    std::array<Samples, 4> sectors{};
};
enum class Reason {
    Unknown, Stale, Unsupported, Settling, Reducing, Holding, Probing,
    Capped, SaturatedAtMinimum, WriteFailed, CouplingUnknown
};
struct Command { Tuple tuple{}; uint64_t generation = 0; };
struct Snapshot {
    bool applied_known = false, pending = false, upward_frozen = false;
    Tuple applied{}, safe{}, ceiling{};
    std::array<bool, 4> safe_known{};
    std::array<unsigned, 4> clear{};
    Command desired{};
    uint64_t lifecycle = 0, command = 0, eligible_from = 0;
    uint64_t failed_probes = 0, transitions = 0, reversals = 0;
    Reason reason = Reason::Unknown;
};
class Controller {
public:
    explicit Controller(Parameters parameters);
    const Snapshot &state() const { return s_; }
    bool legal(const Tuple &) const;
    uint64_t generation_watermark() const { return next_command_; }
    // Knowledge invalidation preserves learned ceilings; recovery is external,
    // never a retry/readback/start-stop policy. Clear safe certifications on it.
    void invalidate(uint64_t lifecycle, Reason reason = Reason::Unknown);
    bool recover(const Tuple &, uint64_t lifecycle, uint64_t command,
                 uint64_t eligible_from);
    void reset_learning(); // Explicit approved operator/independent-scene reset ONLY.
    void observe(const Observation &);
    // Certain acknowledgment only; eligible_from is supplied by the qualified
    // adapter latency contract. Failure may mean partial application.
    bool acknowledge(uint64_t generation, bool certain_success, uint64_t eligible_from);
    // Explicit operator contract: non-OK did not change hardware. This is NOT
    // suitable for uncertain/partial writes; those still use acknowledge(false).
    bool write_failed_unchanged(uint64_t generation);
    void requalify(uint64_t lifecycle, uint64_t eligible_from);
private:
    Parameters p_;
    Snapshot s_{};
    uint64_t last_frame_ = 0, last_time_ = 0, next_command_ = 0, last_lifecycle_ = 0;
    bool seen_ = false;
    int probe_ = -1;
    unsigned cursor_ = 0;
    Tuple before_probe_{};
    std::array<int, 4> direction_{};
    void propose(const Tuple &, Reason);
};
}
