#ifndef CHITON_MEASUREMENT_H
#define CHITON_MEASUREMENT_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <deque>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace measurement {
constexpr size_t history_capacity = 60;
constexpr size_t client_capacity = 32;
constexpr uint64_t idle_timeout_ms = 300000;

// All uint16 values, including zero and 65535, are included. The SDK supplies
// no per-pixel validity mask here; do not silently guess a saturation policy.
struct Statistics {
    size_t count = 0, filtered_count = 0;
    double mean = 0, stdd = 0, rms = 0, filtered_mean = 0;
    uint16_t minimum = 0, maximum = 0;
};
inline Statistics statistics(const std::vector<uint16_t> &values) {
    Statistics result;
    result.count = values.size();
    if (values.empty()) return result;
    uint64_t sum = 0;
    result.minimum = result.maximum = values.front();
    for (uint16_t value : values) {
        sum += value;
        result.minimum = std::min(result.minimum, value);
        result.maximum = std::max(result.maximum, value);
    }
    result.mean = static_cast<double>(sum) / values.size();
    double deviations = 0, squares = 0;
    for (uint16_t value : values) {
        const double difference = static_cast<double>(value) - result.mean;
        deviations += difference * difference;
        squares += static_cast<double>(value) * value;
    }
    result.stdd = std::sqrt(deviations / values.size()); // Population deviation.
    result.rms = std::sqrt(squares / values.size());     // RMS about zero.
    uint64_t filtered_sum = 0;
    for (uint16_t value : values) {
        if (std::abs(static_cast<double>(value) - result.mean) <= result.stdd) {
            filtered_sum += value;
            ++result.filtered_count;
        }
    }
    // Roundoff at a boundary must not manufacture a zero-valued filtered mean.
    result.filtered_mean = result.filtered_count ?
        static_cast<double>(filtered_sum) / result.filtered_count : result.mean;
    return result;
}
inline double deviation(const std::vector<uint16_t> &values, double mean) {
    if (values.empty()) return 0;
    double sum = 0;
    for (uint16_t value : values) {
        const double difference = static_cast<double>(value) - mean;
        sum += difference * difference;
    }
    return std::sqrt(sum / values.size());
}

struct FrameMetadata {
    uint64_t id = 0, captured_at_ms = 0, configuration_revision = 0;
    uint64_t received_steady_ms = 0; // Host receipt age, never an exposure timestamp.
    bool depth = true, calibrated = false, smoothed = false, plugin = false;
    int color_mode = 1;
    std::string units() const {
        return calibrated ? "mm" : depth ? "depth_sensor_units" : "ir_intensity_units";
    }
};
struct HistoryResult {
    Statistics statistics;
    uint64_t first_frame = 0, last_frame = 0, first_time_ms = 0, last_time_ms = 0;
    bool reset = false;
};
class History {
    struct Sample { uint64_t frame, time_ms; uint16_t value; };
    std::deque<Sample> samples;
public:
    void clear() { samples.clear(); }
    void add(uint16_t value, uint64_t frame, uint64_t time_ms = 0) {
        if (!samples.empty() && frame <= samples.back().frame) return;
        if (samples.size() == history_capacity) samples.pop_front();
        samples.push_back({frame, time_ms, value});
    }
    HistoryResult result() const {
        HistoryResult result;
        std::vector<uint16_t> values;
        values.reserve(samples.size());
        for (const auto &sample : samples) values.push_back(sample.value);
        result.statistics = measurement::statistics(values);
        if (!samples.empty()) {
            result.first_frame = samples.front().frame; result.last_frame = samples.back().frame;
            result.first_time_ms = samples.front().time_ms; result.last_time_ms = samples.back().time_ms;
        }
        return result;
    }
};
inline void validate_client(const std::string &client) {
    if (client.empty() || client.size() > 64 || client.find_first_not_of(
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos)
        throw std::invalid_argument("invalid measurement client ID");
}
class Histories {
    struct Entry { std::string selection; uint64_t accessed_ms = 0; History history; };
    std::map<std::string, Entry> clients;
    Entry &select(const std::string &client, const std::string &selection, uint64_t now, bool &reset) {
        validate_client(client);
        for (auto it = clients.begin(); it != clients.end();) {
            if (now >= it->second.accessed_ms && now - it->second.accessed_ms >= idle_timeout_ms)
                it = clients.erase(it);
            else ++it;
        }
        if (!clients.count(client) && clients.size() == client_capacity) {
            auto oldest = std::min_element(clients.begin(), clients.end(), [](const auto &a, const auto &b) {
                return a.second.accessed_ms < b.second.accessed_ms;
            });
            clients.erase(oldest);
        }
        auto inserted = clients.emplace(client, Entry{});
        auto &entry = inserted.first->second;
        reset = inserted.second || entry.selection != selection;
        if (reset) { entry.history.clear(); entry.selection = selection; }
        entry.accessed_ms = now;
        return entry;
    }
public:
    size_t size() const { return clients.size(); }
    void clear() { clients.clear(); }
    void select_only(const std::string &client, const std::string &selection, uint64_t now) {
        bool reset; select(client, selection, now, reset);
    }
    HistoryResult sample(const std::string &client, const std::string &selection,
                         const FrameMetadata &frame, uint16_t value, uint64_t now) {
        bool reset;
        auto &entry = select(client, selection, now, reset);
        entry.history.add(value, frame.id, frame.captured_at_ms);
        auto result = entry.history.result(); result.reset = reset;
        return result;
    }
};

// Preserve the old plugin ABI/counter and ordinary pixel edits, but never expose
// its sentinel as an image sample. A throwing callback cannot partly publish edits.
template<class Callback>
void legacy_plugin(std::vector<uint16_t> &scratch, uint16_t *frame, size_t count,
                   uint16_t counter, Callback callback) {
    if (!frame || !count) throw std::invalid_argument("empty plugin frame");
    scratch.assign(frame, frame + count);
    scratch.back() = counter;
    callback(scratch.data());
    std::copy_n(scratch.data(), count - 1, frame);
}
}
#endif
