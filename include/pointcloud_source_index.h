#pragma once
#include <cstddef>
#include <cstdint>
namespace pointcloud_capture {
enum PixelStatus : uint8_t { Valid=0, OutsideRunlines=1, ZeroSensor=2,
    CalibrationNonfinite=3, CalibrationRange=4, ZeroProcessed=5,
    ProjectionNonfinite=6, ProjectionRange=7 };
constexpr unsigned pixel_status_count=8;
// Derived provenance only; no source pixels or geometry staging.
struct Recorder {
    uint32_t *indices=nullptr; size_t capacity=0,count=0; bool valid=true;
    const uint8_t *pixel_status=nullptr; // nullptr preserves the ordinary live emitter.
    void begin() noexcept {count=0;valid=true;}
    void record(size_t k,unsigned i) noexcept {
        if(!indices||k!=count||k>=capacity||i>=640u*480u){valid=false;return;}
        indices[k]=i;++count;
    }
};
inline Recorder *recorder=nullptr; // Startup-bound in one-shot diagnostics only.
}
