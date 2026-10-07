#pragma once
#include "illumination_controller.h"
#include "json.hpp"
#include <mutex>
namespace illumination {
struct Diagnostics {
    bool experimental = false, legacy_dynamic = false, depth = false, startup_failed = false;
    uint64_t frame = 0, revision = 0, successes = 0, failures = 0;
    int sdk_status = -1;
    Tuple requested{};
    uint64_t requested_generation = 0;
    Observation observation{};
    Snapshot state{};
};
struct DiagnosticsCache { std::mutex mutex; Diagnostics value; };
inline DiagnosticsCache &diagnostics_cache() { static DiagnosticsCache cache; return cache; }
inline void publish_diagnostics(const Diagnostics &value) {
    auto &cache = diagnostics_cache(); std::lock_guard<std::mutex> lock(cache.mutex); cache.value = value;
}
inline void invalidate_published_knowledge(uint64_t lifecycle) {
    auto &cache = diagnostics_cache(); std::lock_guard<std::mutex> lock(cache.mutex);
    cache.value.state.applied_known = false;
    cache.value.state.pending = false;
    cache.value.state.safe_known.fill(false);
    cache.value.state.clear.fill(0);
    cache.value.state.lifecycle = lifecycle;
    cache.value.state.reason = Reason::Unknown;
    cache.value.observation.eligible = false;
}
inline nlohmann::json diagnostics() {
    auto &cache = diagnostics_cache();
    Diagnostics d;
    { std::lock_guard<std::mutex> lock(cache.mutex); d = cache.value; }
    static const char *reasons[] = {"unknown", "stale", "unsupported", "settling", "reducing",
        "holding", "probing", "capped", "saturated_at_minimum", "write_failed", "coupling_unknown"};
    nlohmann::json safe = nlohmann::json::array(), samples = nlohmann::json::array();
    for (unsigned i = 0; i < 4; ++i) {
        safe.push_back(d.state.safe_known[i] ? nlohmann::json(d.state.safe[i]) : nlohmann::json(nullptr));
        samples.push_back({{"known",d.observation.sectors[i].known},
            {"saturated",d.observation.sectors[i].saturated},{"eligible",d.observation.sectors[i].eligible}});
    }
#ifdef CHITON_EXPERIMENTAL_ILLUMINATION
    const bool available = true;
#else
    const bool available = false;
#endif
    return {{"experimental_available",available},
        {"selected_policy",d.experimental ? "experimental_depth_proxy" : d.legacy_dynamic ? "legacy_dynamic" : "fixed"},
        {"detector",d.experimental ? "raw_depth_lt_250_proxy_including_zero" : "none"},
        {"detector_supported",d.experimental && d.depth && d.observation.supported},
        {"failure_contract", d.experimental ? "operator_assumes_non_OK_unchanged" : "not_tracked_legacy"},
        {"frame_id",d.frame},{"configuration_revision",d.revision},{"lifecycle_generation",d.state.lifecycle},
        {"command_generation",d.state.command},{"observation_command",d.observation.command},
        {"observation_eligible",d.observation.supported && d.observation.eligible && d.state.applied_known &&
            d.observation.command == d.state.command && d.frame >= d.state.eligible_from && !d.state.pending},
        {"eligible_from_frame",d.state.eligible_from},{"sectors",samples},
        {"requested",d.requested_generation ? nlohmann::json(d.requested) : nlohmann::json(nullptr)},
        {"requested_command_generation",d.requested_generation},
        {"desired",d.experimental && d.state.desired.generation ? nlohmann::json(d.state.desired.tuple) : nlohmann::json(nullptr)},
        {"acknowledged",d.state.applied_known ? nlohmann::json(d.state.applied) : nlohmann::json(nullptr)},
        {"pending",d.state.pending},{"safe",safe},
        {"ceilings",d.experimental && d.state.lifecycle ? nlohmann::json(d.state.ceiling) : nlohmann::json(nullptr)},
        {"clear_counts",d.state.clear},
        {"reason",reasons[static_cast<unsigned>(d.state.reason)]},{"upward_frozen",d.state.upward_frozen},
        {"failed_probes",d.state.failed_probes},{"transitions",d.state.transitions},{"reversals",d.state.reversals},
        {"write_successes",d.experimental ? nlohmann::json(d.successes) : nlohmann::json(nullptr)},
        {"write_failures",d.experimental ? nlohmann::json(d.failures) : nlohmann::json(nullptr)},
        {"last_sdk_status",d.sdk_status >= 0 ? nlohmann::json(d.sdk_status) : nlohmann::json(nullptr)},
        {"startup_failed",d.startup_failed}};
}
}
