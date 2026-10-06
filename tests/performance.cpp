#include "performance.h"
#include <cassert>
#include <cstdio>
#include <limits>
int main() {
    using namespace performance;
    Histogram h;
    assert(h.json(true)["mean_ms"].is_null());
    h.add(-1); h.add(std::numeric_limits<double>::infinity()); assert(h.count==0);
    for(int i=0;i<100;++i) h.add(i<95 ? 1.0 : 10.0);
    assert(h.count==100);
    assert(h.percentile(.95).get<double>()>=1 && h.percentile(.95).get<double>()<1.2);
    assert(h.percentile(.99).get<double>()>=10 && h.percentile(.99).get<double>()<12);
    Histogram overflow; overflow.add(1e10); assert(overflow.percentile(.99).is_null());
    assert(!h.json(false).contains("p99_ms_upper_bound"));
    Frame f; f.ms[PreviewCopy]=2; f.called[PreviewCopy]=true;
    Pipeline p; p.record(f,1,false,false,false,true,1);
    assert(p.metrics()["preview_copy"]["count"]==1);
    assert(p.metrics()["register_io"]["count"]==0);
    p.record(f,3,false,false,false,true,1);
    assert(p.metrics()["epoch"]==2 && p.metrics()["preview_copy"]["count"]==1);
    p.record(f,3,true,false,false,true,1); assert(p.metrics()["epoch"]==3);
    p.record(f,3,true,false,false,true,2); assert(p.metrics()["epoch"]==4);
    assert(cpu_busy(Cpu{},Cpu{}).is_null());
    assert(cpu_busy(Cpu{100,50,true},Cpu{200,75,true})==75.0);
    assert(!task_stat("/nonexistent-chiton-perf-stat").valid);
    auto first=system_metrics(), cached=system_metrics();
    assert(first==cached);
    assert(first["throttled"].is_null());
    assert(first["per_core"].size()<=256);
    assert(first["thermal_zones"].size()<=32);
}
