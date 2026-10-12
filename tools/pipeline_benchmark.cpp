// SDK-free controlled diagnostics of the existing kernels. No configuration,
// device, SDK, register, calibration-file, or network access.
#include "image.h"
#include "calibration.h"
#include "color/hsv.h"
#include "system.h"
#include "performance.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

GlobalOpts options{};
GlobalOpts *GOPTS = &options;
RGB8 *hsv_cache = nullptr;
static std::vector<u16> slam_fixture(SLAM_POINTS * 101, 65535);
namespace data { u16 *get_slam() { return slam_fixture.data(); } }
// Allocation/geometry diagnostics are irrelevant to kernel timings.
namespace utils { void log_fn(int, const char *, const char *, int, const char *, ...) {} }
static uint64_t hash_bytes(const void *data, size_t size) {
    auto p=static_cast<const unsigned char *>(data); uint64_t h=14695981039346656037ull;
    for(size_t i=0;i<size;++i) { h^=p[i]; h*=1099511628211ull; } return h;
}
int main(int argc,char **argv) {
    try {
        const std::string kernel=argc>1 ? argv[1] : "grey16";
        const int iterations=argc>2 ? std::stoi(argv[2]) : 1000;
        const int smooth=argc>3 ? std::stoi(argv[3]) : 0;
        const bool smoothing=smooth!=0;
        if(iterations<1 || iterations>1000000 || smooth<0 || smooth>1 || argc>4) throw std::invalid_argument("invalid arguments");
        options.center_x=320; options.center_y=240; options.inner_dist=137; options.outer_dist=285;
        options.depth_frame=true; options.min_cam_value=0; options.max_cam_value=6000;
        options.smoothing=smoothing;
        calibration::make_calibration_data();
        hsv_cache=init_rgb_cache();
        constexpr size_t pixels=640*480, planar=1500*165;
        std::vector<u16> src(pixels), dst(pixels,1234);
        std::vector<unsigned char> output(std::max(pixels*3+3,planar*3)+16,0xA5);
        std::vector<int> sources(planar), destinations(planar);
        // Includes all uint16 values, irregular deterministic planar gathers.
        for(size_t i=0;i<pixels;++i) src[i]=static_cast<u16>((i*4051+17)&65535);
        for(size_t i=0;i<planar;++i) { sources[i]=(i*97+13)%pixels; destinations[i]=i; }
        // Safe deterministic calibration arithmetic, including bucket collisions.
        std::fill_n(calibration::get_pixel_gain(),pixels,.5f);
        std::fill_n(calibration::get_pixel_offset(),pixels,7.f);
        const bool cal=kernel=="calibrate" || kernel=="average";
#ifdef CHITON_CALIBRATION_BENCHMARK
        if(!cal) throw std::invalid_argument("use average or calibrate");
#else
        if(cal) throw std::invalid_argument("use image kernels in this program");
#endif
        std::function<void()> run;
        if(kernel=="average") run=[&]{ calibration::average_frame(src.data(),dst.data()); };
        else if(kernel=="calibrate") run=[&]{ calibration::calibrate_frame(src.data(),dst.data()); };
        else if(kernel=="grey16" || kernel=="grey26" || kernel=="grey24") {
            options.depth_frame=kernel!="grey24";
            run=[&]{ create_greyscale_jpeg(src.data(),output.data(),kernel!="grey16"); };
        } else if(kernel=="colour") run=[&]{ create_color_jpeg(src.data(),output.data()); };
        else if(kernel=="planar16" || kernel=="planar26") {
            options.calibration_enabled=kernel=="planar26";
            run=[&]{ create_planar_jpeg(src.data(),output.data(),sources.data(),destinations.data(),planar); };
        } else if(kernel=="planar-colour") run=[&]{ create_planar_color_jpeg(src.data(),output.data(),sources.data(),planar); };
        else throw std::invalid_argument("unknown kernel");
        // Independent scalar fixture oracle, preserving the existing colour
        // destination +1 pixel and runline exclusive endpoints (not fixing them).
        auto expected_dst=dst; auto expected_output=output; auto expected_slam=slam_fixture;
        if(kernel=="average" && !smoothing) expected_dst=src;
        auto lines=calibration::get_run_lines();
        for(unsigned j=0;j<calibration::get_num_runlines();++j) {
            for(unsigned i=lines[j].srcstart;i<lines[j].runlen;++i) {
                if(kernel=="average" && smoothing) expected_dst[i]=1234*.8f+src[i]*.2f;
                else if(kernel=="calibrate") {
                    float corrected=src[i]*.5f+7.f;
                    expected_dst[i]=smoothing ? 1234*.7f+corrected*.3f : corrected;
                    auto bucket=calibration::get_pixel_bucket()[i];
                    if(src[i]>250) expected_slam[bucket]=std::min(src[i],expected_slam[bucket]);
                } else if(kernel=="grey16" || kernel=="grey26" || kernel=="grey24") {
                    unsigned div=kernel=="grey16" ? 16 : kernel=="grey26" ? 26 : 24;
                    expected_output[i]=kernel=="grey24" ? src[i]/div : 255-src[i]/div;
                } else if(kernel=="colour") {
                    const auto rgb=hsv_cache[src[i]];
                    expected_output[(i+1)*3]=rgb.r; expected_output[(i+1)*3+1]=rgb.g; expected_output[(i+1)*3+2]=rgb.b;
                }
            }
        }
        for(size_t i=0;i<planar;++i) {
            if(kernel=="planar16" || kernel=="planar26") expected_output[i]=255-src[sources[i]]/(kernel=="planar16" ? 16 : 26);
            else if(kernel=="planar-colour") {
                const auto rgb=hsv_cache[src[sources[i]]];
                expected_output[i*3]=rgb.r; expected_output[i*3+1]=rgb.g; expected_output[i*3+2]=rgb.b;
            }
        }
        performance::Histogram timings;
        const uint64_t reference=cal ? hash_bytes(expected_dst.data(),expected_dst.size()*sizeof(u16)) :
            hash_bytes(expected_output.data(),expected_output.size());
        const uint64_t slam_reference=hash_bytes(expected_slam.data(),expected_slam.size()*sizeof(u16));
        std::vector<double> elapsed; elapsed.reserve(iterations);
        for(int i=-30;i<iterations;++i) {
            // Reset histories and guard/untouched pixels OUTSIDE the timed span.
            std::fill(dst.begin(),dst.end(),1234); std::fill(output.begin(),output.end(),0xA5);
            std::fill(slam_fixture.begin(),slam_fixture.end(),65535);
            const auto started=performance::Clock::now(); run();
            const double ms=performance::milliseconds(performance::Clock::now()-started);
            const auto hash=cal ? hash_bytes(dst.data(),dst.size()*sizeof(u16)) : hash_bytes(output.data(),output.size());
            const auto slam_hash=hash_bytes(slam_fixture.data(),slam_fixture.size()*sizeof(u16));
            if(dst!=expected_dst || output!=expected_output || slam_fixture!=expected_slam)
                throw std::runtime_error("scalar fixture mismatch");
            if(hash!=reference || slam_hash!=slam_reference) throw std::runtime_error("nondeterministic output");
            for(size_t j=output.size()-16;j<output.size();++j)
                if(output[j]!=0xA5) throw std::runtime_error("output guard overwritten");
            if(i>=0) { timings.add(ms); elapsed.push_back(ms); }
        }
        std::sort(elapsed.begin(),elapsed.end());
        nlohmann::json result={{"kernel",kernel},{"iterations",iterations},{"warmup_iterations",30},
            {"smoothing",smoothing},{"fixture","uint16_4051_plus17_geometry_137_285_v1"},
            {"output_fnv1a64",std::to_string(reference)},{"slam_fnv1a64",std::to_string(slam_reference)},
            {"timing",timings.json(true)},{"p95_ms",elapsed[static_cast<size_t>(std::ceil(.95*iterations))-1]},
            {"p99_ms",elapsed[static_cast<size_t>(std::ceil(.99*iterations))-1]},
            {"note","Host timings do not predict ARM timings; compare checksums with previous accepted implementation"}};
#ifdef CHITON_SIMD_GREYSCALE
        result["simd_greyscale_div16"]=true;
#else
        result["simd_greyscale_div16"]=false;
#endif
#ifdef CHITON_SIMD_CALIBRATION
        result["simd_calibration_smoothing"]=true;
#else
        result["simd_calibration_smoothing"]=false;
#endif
        std::cout<<result.dump()<<'\n'; free(hsv_cache);
    } catch(const std::exception &e) { std::cerr<<e.what()<<'\n'; return 1; }
}
