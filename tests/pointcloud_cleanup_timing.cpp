// SDK-free integration timing of the PRODUCTION point-cloud emitter + cleanup.
// Private topology injection follows pointcloud_geometry.cpp. No alternate emitter,
// camera, calibration file, HTTP transport, or worker thread is involved.
#include "../src/graphics/calibration.cpp"
#include "json.hpp"
#include <cassert>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

GlobalOpts options{};
GlobalOpts *GOPTS = &options;
static s16 cloud[data::point_cloud_components + 16];
static s16 reference[data::point_cloud_components];
static uint8_t reference_rejected[sampled_cleanup::max_records];
static u16 source[sampled_cleanup::pixels];
static u16 slam_storage[SLAM_POINTS * 101];
static unsigned cloud_components;
namespace data {
s16 *get_point_cloud() { return cloud; }
u16 *get_slam() { return slam_storage; }
void set_point_cloud_size(int n) { assert(n >= 0); cloud_components = n; }
}
namespace utils { void log_fn(int, const char *, const char *, int, const char *, ...) {} }

namespace {
using namespace sampled_cleanup;
constexpr unsigned layouts = 2, scenes = 6, phases = 3;
constexpr unsigned max_rounds = 5, max_samples = 1000;
struct Timing { uint64_t cpu_ns = 0, wall_ns = 0; };
// All sample/scratch storage exists before timed emission. Statistics and JSON
// allocation/sorting run only AFTER every measured run has completed.
static Timing samples[layouts][scenes][max_rounds][phases][max_samples];
static unsigned records[layouts], gated[layouts][scenes], mixed[layouts][scenes];
static unsigned detailed[layouts][scenes], early_retained[layouts][scenes];
static uint64_t flag_hashes[layouts][scenes], rejection_hashes[layouts][scenes];
static Algorithm selected_algorithm = Algorithm::Exact;
static double scratch[max_rounds * max_samples * 2];
constexpr const char *layout_names[] = {"production_aoi", "synthetic_full_domain"};
constexpr const char *scene_names[] = {"flat", "single_boundary", "boundary_clutter", "all_candidate_stripes", "dense_mixed_samples", "dense_noisy"};
static unsigned sample_count = 200, rounds = 3, warmup = 20;

static uint64_t thread_ns() {
    timespec ts{};
    const int status = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    assert(status == 0);
    return uint64_t(ts.tv_sec)*1000000000ull + uint64_t(ts.tv_nsec);
}
static uint64_t hash_source() {
    uint64_t h = 14695981039346656037ull;
    for (auto v : source) { h ^= v; h *= 1099511628211ull; }
    return h;
}
static unsigned argument(const char *s, unsigned maximum) {
    if (!s || !*s) return 0;
    for (const char *p = s; *p; ++p) if (*p < '0' || *p > '9') return 0;
    errno = 0;
    char *end = nullptr;
    const unsigned long n = std::strtoul(s, &end, 10);
    return errno || *end || n == 0 || n > maximum ? 0 : unsigned(n);
}
static void fill_scene(unsigned scene) {
    for (unsigned y = 0; y < height; ++y) for (unsigned x = 0; x < width; ++x) {
        unsigned depth = 1000;
        if (scene == 1) depth = x < 320 ? 1000 : (x == 320 && y%3 == 0 ? 1500 : 2000);
        if (scene == 2) depth = x%32 < 16 ? 1000 : 2000;
        if (scene >= 3) depth = x%8 < 4 ? 1000 : 2000;
        if (scene == 5) { uint32_t v=(y*width+x)*1664525u+1013904223u;v^=v>>13;v*=2246822519u;depth=1000+v%1001; }
        source[y*width+x] = depth;
    }
    if (scene == 4) {
        // Overwrite EXACT emitted centers only. Other pixels provide near/far
        // evidence; the five-row depth cycle prevents same-depth center support.
        // Unlike surface stripes, these centers are genuinely intermediate.
        for (unsigned j = 0; j < calibration::num_runlines; ++j) {
            const auto &r = calibration::runlines[j];
            for (unsigned i = r.srcstart; i < (r.runlen & ~3u); i += 4)
                source[i] = 1300 + ((i/width)%5)*100;
        }
    }
}
static void initialize_layout(unsigned layout, const Config &config) {
    // Test-only startup between independent layouts. Never a live recovery API.
    calibration::cleanup_producer = Producer{};
    if (layout == 1) {
        calibration::num_runlines = 1;
        calibration::runlines[0] = {0, 0, pixels, 0, 0};
        // The real mapping initializes only its AOI. Use explicit safe factors
        // for this synthetic maximum traversal; never read uninitialized factors.
        for (unsigned i = 0; i < pixels; ++i) {
            calibration::angle_sin_vert_cos[i] = .6f;
            calibration::vert_sin[i] = 0.f;
            calibration::angle_cos_vert_sin[i] = .8f;
        }
    }
    assert(calibration::initialize_sampled_cleanup(config));
    for (auto &v : cloud) v = 12345;
}
static void emit(Context &context) {
    ++context.frame;
    calibration::vector_point_cloud(source, context);
}
static void verify(const Context &context, bool enabled, unsigned expected_components,
                   uint64_t source_hash) {
    const auto &p = calibration::sampled_cleanup_publication();
    assert(p.status == (enabled ? Status::Ready : Status::Off));
    assert(p.configuration.enabled == enabled && p.configuration.algorithm == selected_algorithm);
    assert(p.version == static_cast<unsigned>(selected_algorithm));
    assert(p.detailed+p.early_retained == p.gated);
    assert(cloud_components == expected_components && p.records*4 == cloud_components);
    assert(p.flags == p.records && p.context.frame == context.frame);
    assert(p.context.revision == context.revision && p.context.units == Units::Millimeters);
    assert(std::memcmp(reference, cloud, cloud_components*sizeof(s16)) == 0);
    assert(hash_source() == source_hash);
    for (unsigned i = cloud_components; i < data::point_cloud_components+16; ++i) assert(cloud[i] == 12345);
    if (enabled) {
        assert(p.retained + p.boundary + p.mixed + p.unclassified == p.records);
        for (unsigned i = 0; i < p.flags; ++i)
            assert((calibration::sampled_cleanup_flags()[i] == static_cast<uint8_t>(Flag::MixedCandidate)) ==
                   bool(reference_rejected[i]));
    } else {
        assert(p.gated == 0 && p.mixed == 0);
        for (unsigned i = 0; i < p.flags; ++i) assert(calibration::sampled_cleanup_flags()[i] == 0);
    }
}
static void run_case(unsigned layout, unsigned scene) {
    fill_scene(scene);
    const uint64_t source_hash = hash_source();
    Context context;
    context.depth = true; context.units = Units::Millimeters;
    context.scope = Scope::CalibrationDomain; context.revision = 1;
    Config config; config.algorithm = selected_algorithm;
    assert(calibration::configure_sampled_cleanup(config));
    emit(context);
    const unsigned expected_components = cloud_components;
    assert(expected_components > 0 && expected_components <= data::point_cloud_components);
    records[layout] = expected_components/4;
    std::memcpy(reference, cloud, expected_components*sizeof(s16)); // Untimed test oracle only.
    Config exact = config; exact.algorithm = selected_algorithm == Algorithm::AggressiveBoundaryBands ? selected_algorithm : Algorithm::Exact; exact.enabled = true;
    assert(calibration::configure_sampled_cleanup(exact)); emit(context);
    for (unsigned i = 0; i < expected_components/4; ++i)
        reference_rejected[i] = calibration::sampled_cleanup_flags()[i] == static_cast<uint8_t>(Flag::MixedCandidate);
    for (unsigned round = 0; round < rounds; ++round) {
        for (unsigned phase = 0; phase < phases; ++phase) {
            config.enabled = phase == 1;
            assert(calibration::configure_sampled_cleanup(config));
            for (unsigned n = 0; n < warmup; ++n) emit(context);
            // Validate before and after each measured phase, outside its clocks.
            verify(context, config.enabled, expected_components, source_hash);
            for (unsigned n = 0; n < sample_count; ++n) {
                const auto wall_start = std::chrono::steady_clock::now();
                const uint64_t cpu_start = thread_ns();
                emit(context); // XYZD + flags + publication + existing emitter metrics.
                const uint64_t cpu_end = thread_ns();
                const auto wall_end = std::chrono::steady_clock::now();
                samples[layout][scene][round][phase][n] = {
                    cpu_end-cpu_start,
                    uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(wall_end-wall_start).count())
                };
            }
            verify(context, config.enabled, expected_components, source_hash);
            if (config.enabled) {
                const auto &p = calibration::sampled_cleanup_publication();
                gated[layout][scene] = p.gated;
                mixed[layout][scene] = p.mixed;
                detailed[layout][scene] = p.detailed;
                early_retained[layout][scene] = p.early_retained;
                uint64_t hash = 14695981039346656037ull, rejected = hash;
                for (unsigned k = 0; k < p.flags; ++k) {
                    const auto flag = calibration::sampled_cleanup_flags()[k];
                    hash ^= flag; hash *= 1099511628211ull;
                    rejected ^= flag == static_cast<uint8_t>(Flag::MixedCandidate);
                    rejected *= 1099511628211ull;
                }
                if (round) assert(flag_hashes[layout][scene] == hash && rejection_hashes[layout][scene] == rejected);
                flag_hashes[layout][scene] = hash;
                rejection_hashes[layout][scene] = rejected;
            }
        }
    }
    // Fail if "enabled" accidentally times a bypass or the stress scene misses
    // the expensive path. Other scenes intentionally have differing gate density.
    if (scene == 0) assert(gated[layout][scene] == 0);
    else assert(gated[layout][scene] > 0);
    if (scene >= 3) assert(gated[layout][scene]*100 > records[layout]*90);
    if (scene == 4) {
        assert(mixed[layout][scene]*100 > records[layout]*10);
        assert(detailed[layout][scene] >= mixed[layout][scene]);
        if (layout == 1) assert(mixed[layout][scene]*100 > records[layout]*90);
    }
    if (selected_algorithm != Algorithm::ConservativeEarlyExit) assert(early_retained[layout][scene] == 0);
}
static nlohmann::json statistics(unsigned layout, unsigned scene, int phase, bool cpu) {
    unsigned count = 0;
    double total = 0;
    for (unsigned r = 0; r < rounds; ++r) for (unsigned p = 0; p < phases; ++p) {
        if (phase == -1 ? p == 1 : p != unsigned(phase)) continue;
        for (unsigned n = 0; n < sample_count; ++n) {
            const auto &t = samples[layout][scene][r][p][n];
            const double ms = (cpu ? t.cpu_ns : t.wall_ns)/1e6;
            scratch[count++] = ms; total += ms;
        }
    }
    assert(count > 0);
    std::sort(scratch, scratch+count); // Offline reporting only, never in runtime cleanup.
    return {{"samples",count}, {"mean_ms",total/count},
        {"p95_ms",scratch[(95*count+99)/100-1]},
        {"p99_ms",scratch[(99*count+99)/100-1]}, {"maximum_ms",scratch[count-1]}};
}
static void report() {
    nlohmann::json cases = nlohmann::json::array();
    for (unsigned layout = 0; layout < layouts; ++layout) for (unsigned scene = 0; scene < scenes; ++scene) {
        const auto off_cpu = statistics(layout,scene,-1,true), on_cpu = statistics(layout,scene,1,true);
        const auto off_wall = statistics(layout,scene,-1,false), on_wall = statistics(layout,scene,1,false);
        const double off = off_cpu["mean_ms"], on = on_cpu["mean_ms"];
        const double off_w = off_wall["mean_ms"], on_w = on_wall["mean_ms"];
        cases.push_back({{"layout",layout_names[layout]}, {"scene",scene_names[scene]},
            {"records",records[layout]}, {"gated",gated[layout][scene]},
            {"gated_percent",100.0*gated[layout][scene]/records[layout]},
            {"mixed",mixed[layout][scene]}, {"mixed_percent",100.0*mixed[layout][scene]/records[layout]},
            {"flag_hash",std::to_string(flag_hashes[layout][scene])},
            {"rejection_hash",std::to_string(rejection_hashes[layout][scene])},
            {"detailed",detailed[layout][scene]}, {"early_retained",early_retained[layout][scene]},
            {"detailed_percent",100.0*detailed[layout][scene]/records[layout]},
            {"off_cpu",off_cpu}, {"on_cpu",on_cpu}, {"added_cpu_mean_ms",on-off},
            {"cpu_ratio",on/off}, {"off_wall",off_wall}, {"on_wall",on_wall},
            {"added_wall_mean_ms",on_w-off_w},
            {"off_before_cpu",statistics(layout,scene,0,true)},
            {"off_after_cpu",statistics(layout,scene,2,true)}});
    }
    nlohmann::json output = {{"benchmark","production_pointcloud_cleanup_v2"},
        {"algorithm",selected_algorithm == Algorithm::Exact ? "exact" : selected_algorithm == Algorithm::ConservativeEarlyExit ? "conservative_early_exit" : "aggressive_boundary_bands_r6"},
        {"compiler",__VERSION__}, {"rounds",rounds}, {"samples_per_phase",sample_count},
        {"warmup_per_phase",warmup}, {"order","Off/On/Off"},
        {"timed_scope","production vector_point_cloud: XYZD, cleanup flags, publication, emitter metrics"},
        {"cpu_clock","CLOCK_THREAD_CPUTIME_ID"},
        {"checks","byte-identical XYZD, unchanged source hash, canaries, counts/context/status, active candidate path"},
        {"note","Host synthetic-input measurements, not sensor CPU/cadence or real-scene effectiveness acceptance"},
        {"cases",cases}};
    std::puts(output.dump().c_str());
}
static void release_calibration() {
    using namespace calibration;
    free(pixel_dist); free(pixel_gain); free(pixel_offset); free(angle_sin_vert_cos);
    free(vert_sin); free(vert_cos); free(angle_cos_vert_sin); free(pixel_angle);
    free(pixel_vert_radians); free(pixel_angle_bucket); free(angle_bucket);
    free(pixel_line_bucket); free(src_offsets); free(runlines); free(calibration::calibration);
}
} // namespace

int main(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        const char *name = argv[i];
        if (std::strcmp(name,"--early-exit") == 0) {
            selected_algorithm = Algorithm::ConservativeEarlyExit; continue;
        }
        if (std::strcmp(name,"--aggressive") == 0) {
            selected_algorithm = Algorithm::AggressiveBoundaryBands; continue;
        }
        unsigned *target = nullptr, maximum = 0;
        if (std::strcmp(name,"--samples") == 0) { target = &sample_count; maximum = max_samples; }
        if (std::strcmp(name,"--rounds") == 0) { target = &rounds; maximum = max_rounds; }
        if (std::strcmp(name,"--warmup") == 0) { target = &warmup; maximum = 200; }
        if (!target || ++i == argc || !(*target = argument(argv[i],maximum))) {
            std::fputs("usage: pointcloud_cleanup_timing_test [--samples 1..1000] [--rounds 1..5] [--warmup 1..200] [--early-exit]\n",stderr);
            return 1;
        }
    }
    // Same fixture as pointcloud_geometry/pipeline diagnostics: real projection
    // factors and processing runs, independent of installed/user configuration.
    options.center_x = 320; options.center_y = 240;
    options.inner_dist = 137; options.outer_dist = 285;
    calibration::make_calibration_data();
    Config config; config.algorithm = selected_algorithm;
    for (unsigned layout = 0; layout < layouts; ++layout) {
        initialize_layout(layout,config);
        for (unsigned scene = 0; scene < scenes; ++scene) run_case(layout,scene);
    }
    report();
    release_calibration();
}
