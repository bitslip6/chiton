// Include the production translation unit to inject private geometry fixtures
// without adding test-only setters to the application's public API.
#include "../src/graphics/calibration.cpp"
#include <cassert>
#include <cmath>
#include <vector>
#include "image.h"

GlobalOpts *GOPTS = nullptr;

int main() {
    constexpr size_t capacity = CAM_WIDTH * CAM_HEIGHT;
    assert(calibration::find_closest_wall_radian(nullptr, 600) == -1.0f);
    std::vector<u16> samples(capacity), buckets(capacity);
    std::vector<float> factors(capacity, 1.0f);
    calibration::runline runs[7]{};
    calibration::runlines = runs;
    calibration::num_runlines = 7;
    calibration::pixel_angle_bucket = buckets.data();
    calibration::angle_cos_vert_sin = factors.data();

    auto find = [&](u16 target) { return calibration::find_closest_wall_radian(samples.data(), target); };
    runs[3].srcstart = 0;
    runs[3].runlen = capacity;
    assert(find(600) == -1.0f); // All empty buckets must be skipped.
    samples[0] = 1000;
    assert(find(1470) == 0.0f); // Lower bucket bound.
    samples[0] = 0;
    samples[capacity - 1] = 1000;
    buckets[capacity - 1] = std::numeric_limits<u16>::max();
    runs[3].srcstart = capacity - 1;
    runs[3].runlen = std::numeric_limits<uint>::max(); // Clamp exclusive frame end.
    const float ceiling_angle = (FRAME_PLANAR_WIDTH_DEFAULT - 1) * ANGLE_POINTS_INC;
    assert(std::abs(find(1470) - ceiling_angle) < 0.0001f);
    assert(find(1470) >= 0 && find(1470) < PI2);

    factors[capacity - 1] = -1.0f; // Projected distances have a zero floor.
    assert(find(1470) == -1.0f);
    factors[capacity - 1] = 100000.0f; // Saturate before integer conversion.
    assert(std::abs(find(1470) - ceiling_angle) < 0.0001f);
    runs[3].srcstart = std::numeric_limits<uint>::max(); // Empty after clipping.
    assert(find(600) == -1.0f);
    runs[3].srcstart = 20;
    runs[3].runlen = 10; // Reversed range is empty, not an unsigned wraparound.
    assert(find(600) == -1.0f);

    runs[3].srcstart = 0;
    runs[3].runlen = 2;
    samples[0] = 1000; buckets[0] = 10;
    samples[1] = 500; buckets[1] = 20;
    assert(std::abs(find(735) - 20 * ANGLE_POINTS_INC) < 0.0001f);
    calibration::num_runlines = 6;
    assert(find(735) == -1.0f);

    // F20 mode-zero regressions: metadata has an extra AOI-distance entry;
    // spike filtering must not read a negative neighbour on history frame zero.
    std::unique_ptr<u16, decltype(&free)> slam(calibration::make_slam_buffer(), &free);
    runs[3].srcstart = 0; runs[3].runlen = 2;
    samples[0] = 800; buckets[0] = 0;
    samples[1] = 900; buckets[1] = SLAM_POINTS - 1;
    for (u16 history = 0; history < 10; ++history) {
        calibration::vector_slam(samples.data(), slam.get(), history);
        assert(slam.get()[history * SLAM_POINTS] == 800);
        assert(slam.get()[history * SLAM_POINTS + SLAM_POINTS - 1] == 900);
        assert(slam.get()[99 * SLAM_POINTS + 1024] == 6000);
    }
    buckets[0] = SLAM_POINTS; buckets[1] = std::numeric_limits<u16>::max();
    runs[3].runlen = std::numeric_limits<uint>::max();
    calibration::vector_slam(samples.data(), slam.get(), 0);
    assert(slam.get()[0] == 9999);
    runs[3].srcstart = std::numeric_limits<uint>::max();
    calibration::vector_slam(samples.data(), slam.get(), 0);
    bool rejected = false;
    try { calibration::vector_slam(samples.data(), slam.get(), 99); }
    catch (const std::invalid_argument &) { rejected = true; }
    assert(rejected);

    GlobalOpts options{};
    GOPTS = &options;
    options.center_x = 639; options.center_y = 479;
    std::vector<JSAMPLE> rgb(capacity * 3);
    std::fill_n(slam.get() + 99 * SLAM_POINTS, SLAM_POINTS, 0);
    // Every empty/distant angle can hit the bottom-right pixel; its blue channel
    // is byte 3*n-1, not byte 3*n (the old next-pixel write).
    create_slam_jpeg(slam.get(), 0, rgb.data());
    assert(rgb.back() == 230);
    GOPTS = nullptr;
}
