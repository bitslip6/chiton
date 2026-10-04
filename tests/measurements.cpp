// F11: production statistics/history helpers and the legacy plugin adapter.
#include "measurement.h"
#include "data.h"
#include "image.h"
#include "utils.hpp"
#include "system.h"
#include <cassert>
#include <random>

GlobalOpts *GOPTS = nullptr;

static void close(double actual, long double expected, double tolerance = 1e-6) {
    assert(std::abs(actual - expected) <= tolerance);
}
static void reference(const std::vector<uint16_t> &values) {
    const auto result = measurement::statistics(values);
    assert(result.count == values.size());
    if (values.empty()) { assert(result.mean == 0 && result.stdd == 0 && result.rms == 0); return; }
    long double sum = 0, squares = 0;
    for (uint16_t value : values) { sum += value; squares += static_cast<long double>(value) * value; }
    const long double mean = sum / values.size();
    long double deviations = 0;
    for (uint16_t value : values) deviations += (value - mean) * (value - mean);
    close(result.mean, mean);
    close(result.stdd, std::sqrt(deviations / values.size()));
    close(result.rms, std::sqrt(squares / values.size()));
    assert(utils::calc_mean(values) == static_cast<uint16_t>(mean));
    close(calc_std_deviation(values, result.mean), result.stdd, 0.003); // float compatibility return.
    assert(calc_rms(values, result.mean) == static_cast<uint16_t>(result.stdd));
    const auto bounds = std::minmax_element(values.begin(), values.end());
    assert(result.minimum == *bounds.first && result.maximum == *bounds.second);
}
int main() {
    reference({}); reference({0}); reference({65535}); reference({1, 2});
    std::vector<uint16_t> values(640 * 480, 65535);
    reference(values); // Sum exceeds signed and unsigned 32-bit capacity.
    for (size_t i = 0; i < values.size(); ++i) values[i] = i % 2 ? 65535 : 0;
    reference(values); close(measurement::statistics(values).stdd, 32767.5);
    close(calc_std_deviation({65535}, 0), 65535);
    assert(calc_rms({65535}, 0) == 65535);
    for (size_t i = 0; i < values.size(); ++i) values[i] = i % 4096;
    reference(values);
    for (size_t i = 0; i < values.size(); ++i)
        values[i] = ((i / 640 / 40 + i % 640 / 40) % 2) ? 6000 : 0;
    reference(values); close(measurement::statistics(values).stdd, 3000);
    std::mt19937 generator(811);
    for (auto &value : values) value = generator() % 65536;
    reference(values);
    assert(calc_rms({}, 0) == 0 && calc_std_deviation({}, 0) == 0);
    assert(calc_mean(std::vector<float>{}) == 0);
    assert(calc_temporal_noise(500) == 500 && calc_temporal_noise(10) == 10);
    close(measurement::statistics({1, 2}).filtered_mean, 1.5);

    // Empty startup/reset, no sorting of storage, oldest eviction even after range queries.
    data::clear_points(); assert(data::point_mean() == 0 && data::point_range() == 0 && data::point_stdd() == 0);
    data::add_point(50000); assert(data::point_mean() == 50000 && data::point_stdd() == 0);
    for (int i = 1; i < 60; ++i) { data::add_point(0); assert(data::point_range() == 50000); }
    data::add_point(0); assert(data::point_mean() == 0 && data::point_range() == 0);
    data::clear_points();
    for (int i = 0; i < 61; ++i) { data::add_point(300 + i); data::point_range(); }
    assert(data::point_mean() == 330 && data::point_range() == 59);
    data::clear_points(); data::add_point(900); assert(data::point_mean() == 900 && data::point_range() == 0);

    measurement::Histories histories;
    measurement::FrameMetadata frame; frame.id = 1; frame.captured_at_ms = 100;
    auto a = histories.sample("tab-A", "point-1:depth", frame, 1000, 1);
    assert(a.reset && a.statistics.count == 1 && a.statistics.mean == 1000);
    auto b = histories.sample("tab-B", "point-1:depth", frame, 9000, 2);
    assert(b.statistics.count == 1 && b.statistics.stdd == 0);
    frame.id = 2; frame.captured_at_ms = 200;
    a = histories.sample("tab-A", "point-1:depth", frame, 2000, 3);
    assert(!a.reset && a.statistics.count == 2 && a.statistics.mean == 1500 && a.statistics.stdd == 500);
    a = histories.sample("tab-A", "point-1:depth", frame, 9999, 4); // Same observed frame is not resampled.
    assert(a.statistics.count == 2 && a.statistics.mean == 1500);
    assert(a.first_frame == 1 && a.last_frame == 2 && a.first_time_ms == 100 && a.last_time_ms == 200);
    frame.id = 1;
    a = histories.sample("tab-A", "point-1:depth", frame, 9999, 4);
    assert(a.statistics.count == 2 && a.last_frame == 2 && a.statistics.mean == 1500);
    frame.id = 2;
    a = histories.sample("tab-A", "point-2:depth", frame, 777, 5);
    assert(a.reset && a.statistics.count == 1 && a.statistics.mean == 777 && a.statistics.stdd == 0);
    histories.select_only("tab-A", "roi:depth", 6);
    a = histories.sample("tab-A", "point-2:depth", frame, 888, 7);
    assert(a.reset && a.statistics.count == 1 && a.statistics.mean == 888);
    a = histories.sample("tab-A", "point-2:ir", frame, 1800, 8);
    assert(a.reset && a.statistics.count == 1 && a.statistics.mean == 1800);
    for (int i = 3; i < 70; ++i) {
        frame.id = i; histories.sample("tab-A", "point-2:ir", frame, i, i + 10);
    }
    a = histories.sample("tab-A", "point-2:ir", frame, 69, 80);
    assert(a.statistics.count == 60 && a.first_frame == 10 && a.last_frame == 69);
    a = histories.sample("tab-A", "point-2:ir", frame, 333, 80 + measurement::idle_timeout_ms);
    assert(a.reset && a.statistics.count == 1 && a.statistics.mean == 333);

    histories.clear();
    for (size_t i = 0; i < measurement::client_capacity; ++i)
        histories.sample("client-" + std::to_string(i), "point", frame, i, i + 1);
    assert(histories.size() == measurement::client_capacity);
    assert(!histories.sample("client-0", "point", frame, 0, 100).reset); // Touch oldest.
    histories.sample("new-client", "point", frame, 0, 101);
    assert(!histories.sample("client-0", "point", frame, 0, 102).reset);
    assert(histories.sample("client-1", "point", frame, 0, 103).reset); // LRU eviction.
    assert(histories.size() == measurement::client_capacity);
    for (const auto &invalid : {std::string(), std::string(65, 'a'), std::string("a/b"), std::string("a b")}) {
        bool rejected = false;
        try { histories.sample(invalid, "point", frame, 0, 104); } catch (const std::invalid_argument &) { rejected = true; }
        assert(rejected && histories.size() == measurement::client_capacity);
    }

    std::vector<uint16_t> pixels{111, 222, 333}, scratch;
    measurement::legacy_plugin(scratch, pixels.data(), pixels.size(), 65535, [](uint16_t *copy) {
        assert(copy[0] == 111 && copy[2] == 65535);
        copy[0] = 777; copy[2] = 999;
    });
    assert(pixels == std::vector<uint16_t>({777, 222, 333}));
    try {
        measurement::legacy_plugin(scratch, pixels.data(), pixels.size(), 0, [](uint16_t *copy) {
            assert(copy[2] == 0); copy[0] = 888; throw std::runtime_error("plugin failed");
        });
        assert(false);
    } catch (const std::runtime_error &) {}
    assert(pixels == std::vector<uint16_t>({777, 222, 333}));
}
