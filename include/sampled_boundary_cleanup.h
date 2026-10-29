#ifndef CHITON_SAMPLED_BOUNDARY_CLEANUP_H
#define CHITON_SAMPLED_BOUNDARY_CLEANUP_H

#include <cstddef>
#include <cstdint>

// No ownership of source pixels, dynamic storage, logging, sorting or lazy setup.
// All calls and borrowed views require the camera/state owner, including readers.
namespace sampled_cleanup {
constexpr unsigned width = 640, height = 480, pixels = width * height;
constexpr unsigned max_runs = 2048;
constexpr unsigned max_records = pixels * 3 / 4; // Actual producer storage / XYZD.
constexpr unsigned algorithm_version = 1;
constexpr uint8_t domain_member = 1, domain_interior = 2;

enum class Flag : uint8_t {
    Off = 0, Retained = 1, BoundaryRisk = 2, Unclassified = 4, MixedCandidate = 8
}; // Mutually exclusive, NOT sensor validity/confidence. Only MixedCandidate hides conservatively.
enum class Algorithm : uint8_t { Exact = 1, ConservativeEarlyExit = 2, AggressiveBoundaryBands = 3 };
constexpr unsigned aggressive_radius = 6; // Fixed policy, not a hidden tuning field.
enum class Units : uint8_t { Unknown, Millimeters, Sensor, Infrared };
enum class Scope : uint8_t { FullSensor, CalibrationDomain };
enum class Status : uint8_t {
    NotInitialized, Off, Ready, UnsupportedContext, InvalidConfiguration,
    TopologyChanged, UnsafeTopology
};
struct Context {
    uint64_t frame = 0, revision = 0, captured_at_ms = 0;
    Units units = Units::Unknown;
    Scope scope = Scope::FullSensor;
    bool depth = false, smoothed = false, plugin = false, sampling = false;
};
struct Config {
    bool enabled = false;
    Algorithm algorithm = Algorithm::Exact; // All non-default algorithms require explicit selection.
    // Explicit opt-in evidence interval, NOT an inferred SDK validity policy.
    // Provisional mm-only parameters: no empirical/device qualification yet.
    uint16_t evidence_min = 250, evidence_max = 6000;
    uint16_t boundary_gap = 200, anchor_tolerance = 40, center_margin = 60;
    uint16_t ramp_residual = 30;
    uint8_t surface_support = 4, center_support = 3;
};
inline bool valid(const Config &c) noexcept {
    // Tuning values span their full storage ranges, with no empirical limits
    // or cross-field restrictions. Unknown algorithms remain protocol errors.
    return c.algorithm == Algorithm::Exact || c.algorithm == Algorithm::ConservativeEarlyExit ||
        c.algorithm == Algorithm::AggressiveBoundaryBands;
}
inline int magnitude(int n) noexcept { return n < 0 ? -n : n; }
inline bool evidence(unsigned d, const Config &c) noexcept {
    return d >= c.evidence_min && d <= c.evidence_max;
}
inline bool location(const uint8_t *domain, int x, int y) noexcept {
    return x >= 0 && x < int(width) && y >= 0 && y < int(height) &&
        (domain[y * width + x] & domain_member);
}
struct Result { Flag flag; bool gated; bool early_retained = false; };

// Startup-only geometric erosion, in the existing membership byte. No depth is
// read or copied. Earlier interior marks do not change membership decisions.
inline void mark_interior(uint8_t *domain) noexcept {
    for (unsigned i = 0; i < pixels; ++i) {
        if (!(domain[i] & domain_member)) continue;
        const int x = i % width, y = i / width;
        bool full = true;
        for (int oy = -2; oy <= 2 && full; ++oy) for (int ox = -2; ox <= 2; ++ox) {
            if (!location(domain,x+ox,y+oy)) { full = false; break; }
        }
        if (full) domain[i] |= domain_interior;
    }
}
template<bool Interior> inline bool accessible(const uint8_t *domain, int x, int y) noexcept {
    if constexpr (Interior) return true; // Established at startup, never guessed per frame.
    else return location(domain,x,y);
}

// Preconditions: fixed full frame, immutable established processed-domain table,
// valid config, i < pixels. At most 59 scalar source loads: center + 8 gate +
// 24 first support + 24 second support + 2 profile. No depth window is staged.
template<bool Interior> inline Result classify_sample_impl(const uint16_t *src, unsigned i,
                              const uint8_t *domain, const Config &c) noexcept {
    const int x = i % width, y = i / width;
    const int center = src[i];
    if (!(domain[i] & domain_member) || !evidence(center, c)) return {Flag::Unclassified, false};
    int near = 0, far = 0, dx = 0, dy = 0, separation = 0;
    // dx/dy points toward FAR; ties use the first direction deterministically.
    constexpr int directions[4][2] = {{1,0}, {0,1}, {1,1}, {1,-1}};
    bool supported_pair = false;
    for (const auto &d : directions) {
        const int ax = x - 2*d[0], ay = y - 2*d[1];
        const int bx = x + 2*d[0], by = y + 2*d[1];
        if (!accessible<Interior>(domain, ax, ay) || !accessible<Interior>(domain, bx, by)) continue;
        const int offset = 2*(d[1]*int(width)+d[0]);
        const int a = src[int(i)-offset], b = src[int(i)+offset];
        if (!evidence(a, c) || !evidence(b, c)) continue;
        supported_pair = true;
        const int gap = magnitude(a - b);
        if (gap > separation) {
            separation = gap;
            near = a < b ? a : b; far = a < b ? b : a;
            dx = a < b ? d[0] : -d[0]; dy = a < b ? d[1] : -d[1];
        }
    }
    if (!supported_pair) return {Flag::Unclassified, false};
    if (separation <= c.boundary_gap) return {Flag::Retained, false};

    // Refinement stays in [anchor-tolerance, anchor+tolerance]. If even the
    // greatest possible center-to-refined-anchor distance cannot exceed margin,
    // this center CANNOT become MixedCandidate. Support has not been proved, so
    // revision 2 reports Unclassified, not BoundaryRisk/Retained. Exact mode
    // deliberately preserves every original flag. All subtraction is signed.
    const int necessary_margin = int(c.center_margin)-int(c.anchor_tolerance);
    if (c.algorithm == Algorithm::ConservativeEarlyExit &&
        (center-near <= necessary_margin || far-center <= necessary_margin))
        return {Flag::Unclassified, true, true};

    unsigned nc = 0, fc = 0;
    uint32_t ns = 0, fs = 0;
    // Each sum <= 24 * 65535 = 1,572,840, even outside the proposed range.
    for (int oy = -2; oy <= 2; ++oy) for (int ox = -2; ox <= 2; ++ox) {
        if ((!ox && !oy) || !accessible<Interior>(domain, x+ox, y+oy)) continue;
        const int v = src[int(i)+oy*int(width)+ox];
        if (!evidence(v, c)) continue;
        if (magnitude(v-near) <= c.anchor_tolerance) { ++nc; ns += v; }
        else if (magnitude(v-far) <= c.anchor_tolerance) { ++fc; fs += v; }
    }
    // Even when the operator requests zero support, a mean needs observations.
    if (!nc || !fc || nc < c.surface_support || fc < c.surface_support)
        return {Flag::Unclassified, true};
    near = ns / nc; far = fs / fc;
    if (far-near <= c.boundary_gap || far-near <= 2*c.anchor_tolerance)
        return {Flag::Unclassified, true};

    nc = fc = 0;
    unsigned near_side = 0, far_side = 0, near_strict = 0, far_strict = 0, same = 0;
    for (int oy = -2; oy <= 2; ++oy) for (int ox = -2; ox <= 2; ++ox) {
        if ((!ox && !oy) || !accessible<Interior>(domain, x+ox, y+oy)) continue;
        const int v = src[int(i)+oy*int(width)+ox];
        if (!evidence(v, c)) continue;
        const int side = ox*dx + oy*dy;
        if (magnitude(v-center) <= c.anchor_tolerance) ++same;
        if (magnitude(v-near) <= c.anchor_tolerance) {
            ++nc; near_side += side <= 0; near_strict += side < 0;
        } else if (magnitude(v-far) <= c.anchor_tolerance) {
            ++fc; far_side += side >= 0; far_strict += side > 0;
        }
    }
    // >=75% of EACH surface in its own closed half-plane, with at least
    // surface_support pixels strictly on that side (center line alone is not evidence).
    if (nc < c.surface_support || fc < c.surface_support ||
        near_strict < c.surface_support || far_strict < c.surface_support ||
        4*near_side < 3*nc || 4*far_side < 3*fc) return {Flag::Unclassified, true};

    // Directional smooth-ramp veto. Integer affine residuals at t=-1,0,+1
    // against the refined endpoints at t=-2,+2. Products <= 4*65535.
    if (!accessible<Interior>(domain, x-dx, y-dy) || !accessible<Interior>(domain, x+dx, y+dy))
        return {Flag::Unclassified, true};
    const int offset = dy*int(width)+dx;
    const int before = src[int(i)-offset];
    const int after = src[int(i)+offset];
    if (!evidence(before, c) || !evidence(after, c)) return {Flag::Unclassified, true};
    if (magnitude(4*before - (3*near+far)) <= 4*c.ramp_residual &&
        magnitude(4*center - (2*near+2*far)) <= 4*c.ramp_residual &&
        magnitude(4*after - (near+3*far)) <= 4*c.ramp_residual)
        return {Flag::Retained, true};
    if (center-near <= c.center_margin || far-center <= c.center_margin)
        return {Flag::BoundaryRisk, true};
    if (same >= c.center_support) return {Flag::Unclassified, true};
    return {Flag::MixedCandidate, true};
}

// Aggressive policy: any usable opposite pair at radii 1..6 may bracket the
// original center. No refined groups, ramp/profile or same-depth rescue. Every
// probe is checked: domain_interior proves radius TWO only. Missing/zero pixels
// never supply evidence even when evidence_min=0. No mask feedback or staging.
// At most 49 scalar loads, 24 pairs, int differences within [-65535,65535],
// coordinates x in [-6,645], y in [-6,485]; only checked indices are loaded.
inline Result classify_aggressive_checked(const uint16_t *src, unsigned i,
                                          const uint8_t *domain, const Config &c) noexcept {
    const int center = src[i], x = i % width, y = i / width;
    if (!(domain[i] & domain_member) || !center || !evidence(center,c))
        return {Flag::Unclassified, false};
    constexpr int directions[4][2] = {{1,0}, {0,1}, {1,1}, {1,-1}};
    bool usable = false, gated = false;
    for (int radius = 1; radius <= int(aggressive_radius); ++radius) for (const auto &d : directions) {
        const int ax = x-radius*d[0], ay = y-radius*d[1];
        const int bx = x+radius*d[0], by = y+radius*d[1];
        if (!location(domain,ax,ay) || !location(domain,bx,by)) continue;
        const int a = src[ay*int(width)+ax], b = src[by*int(width)+bx];
        if (!a || !b || !evidence(a,c) || !evidence(b,c)) continue;
        usable = true;
        const int near = a < b ? a : b, far = a < b ? b : a;
        if (far-near <= c.boundary_gap) continue;
        gated = true;
        if (center-near > c.center_margin && far-center > c.center_margin)
            return {Flag::MixedCandidate, true};
    }
    return {gated ? Flag::BoundaryRisk : usable ? Flag::Retained : Flag::Unclassified, gated};
}

// Fully checked entry point also serves as the exact differential-test oracle.
inline Result classify_sample_checked(const uint16_t *src, unsigned i,
                                     const uint8_t *domain, const Config &c) noexcept {
    if (c.algorithm == Algorithm::AggressiveBoundaryBands) return classify_aggressive_checked(src,i,domain,c);
    return classify_sample_impl<false>(src,i,domain,c);
}
inline Result classify_sample(const uint16_t *src, unsigned i,
                              const uint8_t *domain, const Config &c) noexcept {
    if (c.algorithm == Algorithm::AggressiveBoundaryBands) return classify_aggressive_checked(src,i,domain,c);
    return domain[i] & domain_interior ? classify_sample_impl<true>(src,i,domain,c) :
        classify_sample_checked(src,i,domain,c);
}

struct Publication {
    Context context{};
    Config configuration{};
    uint64_t generation = 0;
    unsigned version = algorithm_version, records = 0, flags = 0;
    unsigned gated = 0, mixed = 0, boundary = 0, unclassified = 0, retained = 0;
    unsigned detailed = 0, early_retained = 0;
    Status status = Status::NotInitialized;
};
struct Metrics {
    uint64_t frames = 0, total_ns = 0, maximum_ns = 0;
    // Emitter (including XYZD), NOT a claim of isolated cleanup CPU time.
    // Upper bounds in microseconds, final bin overflow; offline percentiles only.
    uint64_t bins[10]{};
    void record(uint64_t ns) noexcept {
        constexpr uint64_t upper_us[9] = {125,250,500,1000,2000,3000,5000,8000,16000};
        unsigned b = 0;
        while (b < 9 && ns > upper_us[b]*1000) ++b;
        ++bins[b]; ++frames; total_ns += ns;
        if (ns > maximum_ns) maximum_ns = ns;
    }
};

template<unsigned LocalFlags = max_records> class BasicProducer {
    struct Run { unsigned begin, end; } topology_[max_runs]{};
    uint8_t domain_[pixels]{};
    uint8_t flags_[LocalFlags ? LocalFlags : 1]{};
    uint8_t *external_flags_ = nullptr;
    unsigned run_count_ = 0, capacity_ = 0;
    bool attempted_ = false, initialized_ = false, topology_invalid_ = false;
    Config config_{};
    Status config_status_ = Status::Off;
    uint64_t generation_ = 0;
    Publication pending_{}, published_{};
    Metrics metrics_{};
public:
    // Validate ALL runs and exact selected count before touching membership.
    template<class R> static bool safe(const R *runs, unsigned n, unsigned capacity) noexcept {
        if ((!runs && n) || n > max_runs) return false;
        unsigned count = 0, previous_end = 0;
        for (unsigned j = 0; j < n; ++j) {
            const unsigned b = runs[j].srcstart, e = runs[j].runlen;
            if (b > e || e > pixels || b < previous_end) return false;
            previous_end = e;
            const unsigned stop = e & ~3u;
            count += b < stop ? (stop-b+3)/4 : 0;
            if (count > capacity) return false;
        }
        return true;
    }
    template<class R> bool initialize(const R *runs, unsigned n, unsigned components,
                                      const Config &c = {}) noexcept {
        if (attempted_) return false; // Startup-only, including failure; never retry live.
        attempted_ = true;
        capacity_ = components / 4;
        if ((!LocalFlags && !external_flags_) || !capacity_ || capacity_ > max_records || !valid(c) || !safe(runs,n,capacity_)) return false;
        run_count_ = n;
        for (unsigned j = 0; j < n; ++j) {
            topology_[j] = {runs[j].srcstart, runs[j].runlen};
            for (unsigned i = runs[j].srcstart; i < runs[j].runlen; ++i) domain_[i] = domain_member;
        }
        mark_interior(domain_);
        initialized_ = true;
        configure(c);
        return true;
    }
    bool configure(const Config &c) noexcept {
        if (!initialized_) return false;
        ++generation_;
        config_ = c;
        config_status_ = !valid(c) ? Status::InvalidConfiguration : c.enabled ? Status::Ready : Status::Off;
        return valid(c);
    }
    template<class R> bool begin(const R *runs, unsigned n, const Context &context) noexcept {
        pending_ = {};
        pending_.context = context; pending_.configuration = config_;
        pending_.generation = generation_;
        pending_.version = static_cast<unsigned>(config_.algorithm);
        pending_.status = Status::NotInitialized;
        // Also preflight legacy callers, so a bad runtime run cannot overrun XYZD.
        if (!safe(runs,n,initialized_ ? capacity_ : max_records)) {
            topology_invalid_ = initialized_;
            pending_.status = Status::UnsafeTopology;
            return false;
        }
        if (!initialized_) return true; // Legacy fixtures/tools have no cleanup publication.
        if (n != run_count_) topology_invalid_ = true;
        else for (unsigned j = 0; j < n; ++j)
            if (runs[j].srcstart != topology_[j].begin || runs[j].runlen != topology_[j].end)
                topology_invalid_ = true;
        pending_.status = topology_invalid_ ? Status::TopologyChanged : config_status_;
        if (pending_.status == Status::Ready &&
            (!context.depth || context.units != Units::Millimeters ||
             context.scope != Scope::CalibrationDomain || context.plugin || context.sampling))
            pending_.status = Status::UnsupportedContext;
        return true;
    }
    void sample(const uint16_t *src, unsigned i, unsigned record) noexcept {
        if (!initialized_) return;
        const auto r = pending_.status == Status::Ready ? classify_sample(src,i,domain_,config_) :
            Result{pending_.status == Status::Off ? Flag::Off : Flag::Unclassified, false};
        (external_flags_ ? external_flags_ : flags_)[record] = static_cast<uint8_t>(r.flag);
        pending_.gated += r.gated;
        if (r.early_retained) ++pending_.early_retained;
        pending_.mixed += r.flag == Flag::MixedCandidate;
        pending_.boundary += r.flag == Flag::BoundaryRisk;
        pending_.unclassified += r.flag == Flag::Unclassified;
        pending_.retained += r.flag == Flag::Retained;
    }
    void finish(unsigned records) noexcept {
        pending_.records = records;
        pending_.detailed = pending_.gated-pending_.early_retained;
        pending_.flags = initialized_ ? records : 0;
        published_ = pending_; // Complete identity/counts under the existing state owner.
    }
    // Borrow only under camera/state ownership; valid prefix is publication.flags.
    void bind_flags(uint8_t *flags) noexcept { external_flags_ = flags; }
    const Config &configuration() const noexcept { return config_; }
    uint64_t generation() const noexcept { return generation_; }
    const uint8_t *flags() const noexcept { return external_flags_ ? external_flags_ : flags_; }
    const Publication &publication() const noexcept { return published_; }
    const Metrics &metrics() const noexcept { return metrics_; }
    void record_duration(uint64_t ns) noexcept { if (initialized_) metrics_.record(ns); }
};
using Producer = BasicProducer<>;
} // namespace sampled_cleanup
#endif
