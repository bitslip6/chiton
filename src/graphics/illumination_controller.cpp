#include "illumination_controller.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace illumination {
namespace {
void count(uint64_t &n) { if (n != std::numeric_limits<uint64_t>::max()) ++n; }
}
Controller::Controller(Parameters p) : p_(p) {
    if (p.minimum < 0 || p.maximum > 65535 || p.minimum > p.maximum ||
        p.decrease_step < 1 || p.decrease_step > 65535 ||
        p.probe_step < 1 || p.probe_step > 65535 || !p.clear_frames ||
        p.low_ppm >= p.high_ppm || p.high_ppm > 1000000)
        throw std::invalid_argument("unvalidated illumination parameters");
    s_.ceiling.fill(p.maximum);
}
bool Controller::legal(const Tuple &t) const {
    return std::all_of(t.begin(), t.end(), [&](int n) { return n >= p_.minimum && n <= p_.maximum; });
}
void Controller::invalidate(uint64_t lifecycle, Reason reason) {
    if (lifecycle < s_.lifecycle) { s_.reason = Reason::Stale; return; }
    s_.lifecycle = lifecycle;
    s_.applied_known = false;
    s_.pending = false;
    s_.safe_known.fill(false);
    s_.clear.fill(0);
    probe_ = -1;
    s_.reason = reason;
    // Frame IDs/time must remain monotonic within a lifecycle; restarting IDs
    // is allowed only when a new lifecycle generation is explicitly supplied.
    if (lifecycle != last_lifecycle_) { seen_ = false; last_lifecycle_ = lifecycle; }
}
bool Controller::recover(const Tuple &t, uint64_t lifecycle, uint64_t command, uint64_t from) {
    if (!legal(t) || lifecycle < s_.lifecycle || command <= next_command_) return false;
    invalidate(lifecycle);
    next_command_ = command;
    s_.applied = t;
    s_.applied_known = true;
    s_.command = command;
    s_.eligible_from = from;
    s_.desired = {t, command};
    s_.reason = Reason::Settling;
    return true;
}
void Controller::reset_learning() {
    s_.ceiling.fill(p_.maximum);
    s_.upward_frozen = false;
    s_.safe_known.fill(false);
    s_.clear.fill(0);
    // Do not forget an in-flight probe or pretend hardware knowledge recovered.
}
void Controller::propose(const Tuple &t, Reason reason) {
    if (!legal(t) || t == s_.applied || s_.pending) return;
    if (next_command_ == std::numeric_limits<uint64_t>::max()) {
        invalidate(s_.lifecycle, Reason::Unknown); return;
    }
    s_.desired = {t, ++next_command_};
    s_.pending = true;
    s_.clear.fill(0);
    s_.reason = reason;
}
bool Controller::acknowledge(uint64_t generation, bool ok, uint64_t from) {
    if (!s_.pending || generation != s_.desired.generation) return false;
    if (!ok) { invalidate(s_.lifecycle, Reason::WriteFailed); return true; }
    for (unsigned i = 0; i < 4; ++i) {
        int d = (s_.desired.tuple[i] > s_.applied[i]) - (s_.desired.tuple[i] < s_.applied[i]);
        if (d) {
            if (direction_[i] && direction_[i] != d) count(s_.reversals);
            direction_[i] = d;
        }
    }
    count(s_.transitions);
    s_.applied = s_.desired.tuple;
    s_.applied_known = true;
    s_.command = generation;
    s_.pending = false;
    s_.eligible_from = from;
    s_.reason = Reason::Settling;
    return true;
}
bool Controller::write_failed_unchanged(uint64_t generation) {
    if (!s_.applied_known || generation != (s_.pending ? s_.desired.generation : s_.command)) return false;
    if (s_.pending) {
        bool increase = false;
        for (unsigned i = 0; i < 4; ++i) increase |= s_.desired.tuple[i] > s_.applied[i];
        if (increase) probe_ = -1; // Unapplied upward proposal is not a failed optical probe.
        s_.pending = false;
        s_.desired = {s_.applied, s_.command};
    }
    s_.clear.fill(0);
    s_.reason = Reason::WriteFailed;
    return true;
}
void Controller::requalify(uint64_t lifecycle, uint64_t from) {
    if (lifecycle < s_.lifecycle) return;
    // Configuration changes without a hardware writer preserve the tuple but
    // cancel attribution and clear qualifications. Never clear learned ceilings.
    s_.lifecycle = lifecycle; last_lifecycle_ = lifecycle; seen_ = false;
    s_.pending = false; probe_ = -1;
    s_.clear.fill(0); s_.safe_known.fill(false);
    s_.desired = {s_.applied, s_.command};
    s_.eligible_from = from;
    s_.reason = s_.applied_known ? Reason::Settling : Reason::Unknown;
}
void Controller::observe(const Observation &o) {
    if (o.lifecycle != s_.lifecycle || (seen_ && (o.frame <= last_frame_ || o.time < last_time_))) {
        s_.reason = Reason::Stale; return;
    }
    seen_ = true; last_frame_ = o.frame; last_time_ = o.time;
    if (!s_.applied_known) { s_.clear.fill(0); s_.reason = Reason::Unknown; return; }
    if (s_.pending || !o.eligible || o.command != s_.command || o.frame < s_.eligible_from) {
        s_.clear.fill(0); s_.reason = Reason::Settling; return;
    }
    if (!o.supported) { s_.clear.fill(0); s_.reason = Reason::Unsupported; return; }
    Tuple target = s_.applied;
    bool all_clear = true, any_high = false, unknown = false, unresolved = false;
    std::array<bool, 4> high{};
    for (unsigned i = 0; i < 4; ++i) {
        const auto &v = o.sectors[i];
        bool valid = v.known && v.eligible && v.saturated <= v.eligible;
        high[i] = valid && uint64_t(v.saturated) * 1000000 >= uint64_t(v.eligible) * p_.high_ppm;
        bool low = valid && uint64_t(v.saturated) * 1000000 <= uint64_t(v.eligible) * p_.low_ppm;
        all_clear &= low;
        unknown |= !valid;
        if (high[i]) {
            any_high = true;
            s_.clear[i] = 0;
            s_.ceiling[i] = std::min(s_.ceiling[i], std::max(p_.minimum, s_.applied[i] - 1));
            if (s_.safe_known[i] && s_.safe[i] >= s_.applied[i]) s_.safe_known[i] = false;
            target[i] = std::max(p_.minimum, s_.applied[i] - p_.decrease_step);
            unresolved |= s_.applied[i] == p_.minimum;
        } else if (low) {
            if (s_.clear[i] < p_.clear_frames) ++s_.clear[i];
            if (probe_ < 0 && s_.clear[i] == p_.clear_frames) {
                s_.safe[i] = s_.applied[i]; s_.safe_known[i] = true;
            }
        } else s_.clear[i] = 0;
    }
    if (any_high) {
        if (probe_ >= 0) {
            // Any new saturation during a single-sector probe is ambiguous if
            // another sector is affected. Freeze ALL upward adaptation.
            for (unsigned i = 0; i < 4; ++i)
                if (int(i) != probe_ && high[i]) s_.upward_frozen = true;
            count(s_.failed_probes);
            s_.ceiling[probe_] = std::min(s_.ceiling[probe_], s_.applied[probe_] - 1);
            target[probe_] = before_probe_[probe_];
            probe_ = -1;
        }
        s_.clear.fill(0);
        s_.reason = unresolved ? Reason::SaturatedAtMinimum : Reason::Reducing;
        propose(target, s_.reason);
        return;
    }
    if (probe_ >= 0) {
        if (all_clear && std::all_of(s_.clear.begin(), s_.clear.end(), [&](unsigned n) { return n == p_.clear_frames; })) {
            s_.safe = s_.applied; s_.safe_known.fill(true);
            probe_ = -1; s_.clear.fill(0);
        }
        s_.reason = unknown ? Reason::Unknown : Reason::Holding;
        return; // Success never triggers an immediate next probe.
    }
    if (unknown) { s_.reason = Reason::Unknown; return; }
    if (!p_.independent_sectors || s_.upward_frozen) { s_.reason = Reason::CouplingUnknown; return; }
    if (!all_clear) { s_.reason = Reason::Holding; return; }
    // Require all sectors qualified for attribution before increasing one.
    if (!std::all_of(s_.clear.begin(), s_.clear.end(), [&](unsigned n) { return n == p_.clear_frames; })) {
        s_.reason = Reason::Holding; return;
    }
    for (unsigned n = 0; n < 4; ++n) {
        unsigned i = (cursor_ + n) % 4;
        int next = s_.applied[i] + p_.probe_step;
        if (next <= s_.ceiling[i] && next <= p_.maximum) {
            before_probe_ = s_.applied;
            probe_ = int(i); target[i] = next; cursor_ = (i + 1) % 4;
            propose(target, Reason::Probing); return;
        }
    }
    s_.reason = Reason::Capped;
}
}
