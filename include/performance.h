#pragma once
#include "json.hpp"
#include <array>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>

namespace performance {
using Clock = std::chrono::steady_clock;
inline double milliseconds(Clock::duration d) { return std::chrono::duration<double, std::milli>(d).count(); }
enum Stage { Processing, CalibrationSmoothing, PreviewCopy, Conversion, RegisterIO, TimerProbe, RecordOverhead, StageCount };
static constexpr const char *names[] = {"processing", "calibration_smoothing", "preview_copy", "conversion", "register_io", "timer_probe", "record_overhead"};
struct Frame {
    std::array<double, StageCount> ms{};
    std::array<bool, StageCount> called{};
    void set(Stage s, Clock::time_point start) { ms[s] = milliseconds(Clock::now()-start); called[s] = true; }
};
// Fixed histogram, logarithmic upper bounds: 1 us * 1.2^i. Last bin is
// overflow; quantiles landing there are null, never a false finite bound.
struct Histogram {
    std::array<uint64_t, 96> buckets{};
    uint64_t count = 0;
    double sum = 0;
    static const std::array<double, 95> &bounds() {
        static const auto values = [] { std::array<double,95> a{}; double v=.001;
            for (auto &x:a) { x=v; v*=1.2; } return a; }();
        return values;
    }
    void add(double ms) {
        if (!std::isfinite(ms) || ms < 0) return;
        size_t i=0; while(i<bounds().size() && ms>bounds()[i]) ++i;
        ++buckets[i]; ++count; sum+=ms;
    }
    nlohmann::json percentile(double q) const {
        if (!count) return nullptr;
        uint64_t rank=static_cast<uint64_t>(std::ceil(q*count)), seen=0;
        for(size_t i=0;i<buckets.size();++i) { seen+=buckets[i];
            if(seen>=rank) return i<bounds().size() ? nlohmann::json(bounds()[i]) : nlohmann::json(nullptr); }
        return nullptr;
    }
    nlohmann::json json(bool tails) const {
        nlohmann::json j={{"count",count},{"mean_ms",count ? nlohmann::json(sum/count) : nlohmann::json(nullptr)}};
        if(tails) { j["p95_ms_upper_bound"]=percentile(.95); j["p99_ms_upper_bound"]=percentile(.99); }
        return j;
    }
};
class Pipeline {
    std::mutex mutex;
    // Latest configuration epoch only: never mix modes/smoothing/calibration.
    std::array<Histogram,StageCount> stages{};
    int mode=-1; bool smooth=false, calibrated=false, sampling=false, depth=false;
    uint64_t revision=0, epoch=0;
public:
    std::atomic<long> camera_tid{0};
    void record(const Frame &f, int m, bool s, bool c, bool sample, bool d, uint64_t rev) {
        const auto started=Clock::now();
        static thread_local const long tid=syscall(SYS_gettid);
        camera_tid.store(tid, std::memory_order_relaxed);
        std::lock_guard<std::mutex> lock(mutex);
        if(m!=mode || s!=smooth || c!=calibrated || sample!=sampling || d!=depth || rev!=revision) {
            stages={}; mode=m; smooth=s; calibrated=c; sampling=sample; depth=d; revision=rev; ++epoch;
        }
        for(size_t i=0;i<stages.size();++i) if(f.called[i]) stages[i].add(f.ms[i]);
        stages[RecordOverhead].add(milliseconds(Clock::now()-started));
    }
    nlohmann::json metrics() {
        std::lock_guard<std::mutex> lock(mutex);
        nlohmann::json j={{"mode",mode},{"smoothed",smooth},{"calibrated",calibrated},{"sampling",sampling},
            {"depth",depth},{"configuration_revision",revision},{"epoch",epoch},
            {"scope","cumulative_latest_configuration_epoch"},{"bucket_relative_width",.2}};
        for(size_t i=0;i<stages.size();++i) j[names[i]]=stages[i].json(i<=Conversion);
        return j;
    }
};
inline Pipeline &pipeline() { static Pipeline p; return p; }

inline bool read_number(const std::string &path, double &value) {
    std::ifstream in(path); return bool(in>>value) && std::isfinite(value);
}
struct Task { uint64_t ticks=0; int core=-1; bool valid=false; };
inline Task task_stat(const std::string &path) {
    std::ifstream in(path); std::string line; std::getline(in,line);
    auto end=line.rfind(')'); if(end==std::string::npos) return {};
    std::istringstream fields(line.substr(end+1)); std::string token; Task t;
    try {
        for(int field=3;field<=39;++field) {
            if(!(fields>>token)) return {};
            if(field==14 || field==15) t.ticks+=std::stoull(token);
            if(field==39) t.core=std::stoi(token);
        }
        t.valid=true;
    } catch (...) { return {}; }
    return t;
}
struct Cpu { uint64_t total=0, idle=0; bool valid=false; };
inline nlohmann::json cpu_busy(const Cpu &a,const Cpu &b) {
    if(!a.valid || !b.valid || b.total<=a.total || b.idle<a.idle) return nullptr;
    double total=b.total-a.total, idle=b.idle-a.idle;
    return 100.0*std::max(0.0,total-idle)/total;
}
// Sample at most once/second when metrics are requested, never on the camera
// thread. Missing /proc or sysfs values stay null. CPU percentages are deltas;
// process/thread 100% means one full core, not aggregate machine capacity.
inline nlohmann::json system_metrics() {
    static std::mutex mutex;
    static Clock::time_point previous{};
    static nlohmann::json cached;
    static std::array<Cpu,257> old_cpus{};
    static Task old_process, old_camera;
    static long old_tid=0;
    std::lock_guard<std::mutex> lock(mutex);
    auto now=Clock::now();
    if(previous!=Clock::time_point{} && now-previous<std::chrono::seconds(1)) return cached;
    const auto started=now;
    double elapsed=previous==Clock::time_point{} ? 0 : milliseconds(now-previous)/1000;
    std::array<Cpu,257> cpus{};
    std::ifstream stat("/proc/stat"); std::string line;
    while(std::getline(stat,line)) {
        std::istringstream in(line); std::string name; in>>name;
        if(name.compare(0,3,"cpu")!=0) continue;
        size_t index=0;
        try { if(name!="cpu") index=std::stoul(name.substr(3))+1; } catch(...) { continue; }
        if(index>=cpus.size()) continue;
        uint64_t v=0; int field=0;
        while(field<8 && in>>v) { cpus[index].total+=v; if(field==3 || field==4) cpus[index].idle+=v; ++field; }
        cpus[index].valid=field>=4;
    }
    long tid=pipeline().camera_tid.load(std::memory_order_relaxed);
    auto process=task_stat("/proc/self/stat");
    auto camera=tid>0 ? task_stat("/proc/self/task/"+std::to_string(tid)+"/stat") : Task{};
    long hz=sysconf(_SC_CLK_TCK);
    auto usage=[&](const Task &a,const Task &b) -> nlohmann::json {
        if(!a.valid || !b.valid || b.ticks<a.ticks || elapsed<=0 || hz<=0) return nullptr;
        return 100.0*(b.ticks-a.ticks)/hz/elapsed;
    };
    cached={{"sample_interval_ms",elapsed*1000},{"process_cpu_percent",usage(old_process,process)},
        {"camera_cpu_percent",tid==old_tid ? usage(old_camera,camera) : nlohmann::json(nullptr)},
        {"camera_tid",tid>0 ? nlohmann::json(tid) : nlohmann::json(nullptr)},
        {"camera_last_core",camera.valid ? nlohmann::json(camera.core) : nlohmann::json(nullptr)},
        {"aggregate_cpu_busy_percent",cpu_busy(old_cpus[0],cpus[0])},
        {"per_core",nlohmann::json::array()},{"thermal_zones",nlohmann::json::array()},
        {"cooling_devices",nlohmann::json::array()},{"throttled",nullptr},
        {"throttling_note","No portable throttle flag; cooling states/frequencies are observations, not proof of throttling"}};
    for(size_t i=1;i<cpus.size();++i) if(cpus[i].valid) {
        double khz=0; bool available=read_number("/sys/devices/system/cpu/cpu"+std::to_string(i-1)+"/cpufreq/scaling_cur_freq",khz);
        cached["per_core"].push_back({{"core",i-1},{"busy_percent",cpu_busy(old_cpus[i],cpus[i])},
            {"frequency_khz",available ? nlohmann::json(khz) : nlohmann::json(nullptr)}});
    }
    for(int i=0;i<32;++i) {
        std::string base="/sys/class/thermal/thermal_zone"+std::to_string(i)+"/";
        double temp=0;
        if(read_number(base+"temp",temp)) { std::string type; std::ifstream in(base+"type"); std::getline(in,type);
            cached["thermal_zones"].push_back({{"zone",i},{"type",type},{"celsius",temp/1000}}); }
        base="/sys/class/thermal/cooling_device"+std::to_string(i)+"/";
        double state=0;
        if(read_number(base+"cur_state",state)) { std::string type; std::ifstream in(base+"type"); std::getline(in,type);
            cached["cooling_devices"].push_back({{"device",i},{"type",type},{"state",state}}); }
    }
    cached["collection_ms"]=milliseconds(Clock::now()-started);
    old_cpus=cpus; old_process=process; old_camera=camera; old_tid=tid; previous=now;
    return cached;
}
}
