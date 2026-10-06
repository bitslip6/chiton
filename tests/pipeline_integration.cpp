#include "image.h"
#include "calibration.h"
#include "system.h"
#include "color/hsv.h"
#include <algorithm>
#include <cassert>
#include <vector>
GlobalOpts options{};
GlobalOpts *GOPTS=&options;
RGB8 *hsv_cache=nullptr;
static std::vector<u16> slam(SLAM_POINTS*101,65535);
namespace data { u16 *get_slam() { return slam.data(); } }
namespace utils { void log_fn(int,const char *,const char *,int,const char *,...) {} }
int main() {
    constexpr size_t pixels=640*480;
    options.center_x=320; options.center_y=240; options.inner_dist=137; options.outer_dist=285;
    calibration::make_calibration_data();
    auto lines=calibration::get_run_lines();
    const auto number=calibration::get_num_runlines(); assert(number>0);
    for(unsigned j=0;j<number;++j) lines[j].srcstart=lines[j].runlen=0;
    lines[0].srcstart=1; lines[0].runlen=65537;
    std::vector<u16> src(pixels), actual(pixels,1234), expected=actual;
    for(size_t i=0;i<pixels;++i) src[i]=static_cast<u16>(i-1);
    for(bool depth:{false,true}) for(bool calibrated:{false,true}) {
        options.depth_frame=depth;
        std::vector<JSAMPLE> image(pixels+16,0xA5), reference=image;
        for(size_t i=1;i<65537;++i) {
            const unsigned divisor=calibrated ? (depth ? 26 : 24) : 16;
            reference[i]=depth ? 255-src[i]/divisor : src[i]/divisor;
        }
        create_greyscale_jpeg(src.data(),image.data(),calibrated);
        assert(image==reference);
    }
    // Irregular AOI gaps, short/odd runs, repeated buckets, threshold boundaries.
    for(unsigned j=0;j<number;++j) {
        lines[j].srcstart=17*j+1; lines[j].runlen=lines[j].srcstart+j%13;
        assert(lines[j].runlen<pixels);
    }
    auto gain=calibration::get_pixel_gain(); auto offset=calibration::get_pixel_offset();
    auto bucket=calibration::get_pixel_bucket();
    for(size_t i=0;i<pixels;++i) {
        src[i]=i%5==0 ? 250 : i%5==1 ? 251 : static_cast<u16>(i*4051+17);
        gain[i]=.125f+static_cast<float>(i%27)/100.f;
        offset[i]=(i%2 && src[i]>=32) ? -.499f : static_cast<float>(i%19)+.999f;
        bucket[i]=i%7;
    }
    for(bool smooth:{false,true}) {
        options.smoothing=smooth;
        std::fill(actual.begin(),actual.end(),1234); expected=actual;
        std::fill(slam.begin(),slam.end(),65535); auto expected_slam=slam;
        for(int frame=0;frame<12;++frame) {
            for(unsigned j=0;j<number;++j) for(unsigned i=lines[j].srcstart;i<lines[j].runlen;++i) {
                const float corrected=src[i]*gain[i]+offset[i];
                expected[i]=smooth ? expected[i]*.7f+corrected*.3f : corrected;
                if(src[i]>250) expected_slam[bucket[i]]=std::min(src[i],expected_slam[bucket[i]]);
            }
            calibration::calibrate_frame(src.data(),actual.data());
            assert(actual==expected); assert(slam==expected_slam);
        }
        std::fill(actual.begin(),actual.end(),1234); expected=actual;
        for(int frame=0;frame<12;++frame) {
            if(!smooth) expected=src;
            else for(unsigned j=0;j<number;++j) for(unsigned i=lines[j].srcstart;i<lines[j].runlen;++i)
                expected[i]=expected[i]*.8f+src[i]*.2f;
            calibration::average_frame(src.data(),actual.data());
            assert(actual==expected);
        }
    }
}
