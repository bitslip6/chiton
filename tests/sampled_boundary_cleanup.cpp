// Production emitter included for exact private run-fixture injection, as in
// pointcloud_geometry.cpp. No SDK/camera linkage or alternate emitter.
#include "../src/graphics/calibration.cpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <new>
#include <sys/mman.h>
#include <unistd.h>

using namespace sampled_cleanup;
GlobalOpts options{};
GlobalOpts *GOPTS = &options;
static s16 cloud[data::point_cloud_components + 16];
static s16 original_cloud[data::point_cloud_components];
static unsigned cloud_size;
static u16 slam_storage[SLAM_POINTS * 101];
namespace data {
s16 *get_point_cloud() { return cloud; }
u16 *get_slam() { return slam_storage; }
void set_point_cloud_size(int n) { assert(n >= 0); cloud_size = n; }
}
namespace utils { void log_fn(int, const char *, const char *, int, const char *, ...) {} }

static bool guarded = false;
static unsigned allocation_events = 0, source_copies = 0;
static const uint16_t *protected_source = nullptr;
#ifdef CHITON_CLEANUP_ALLOCATION_GUARD
extern "C" {
void *__real_malloc(size_t); void *__real_calloc(size_t,size_t);
void *__real_realloc(void*,size_t); void __real_free(void*);
void *__real_aligned_alloc(size_t,size_t); int __real_posix_memalign(void**,size_t,size_t);
void *__real_memcpy(void*,const void*,size_t); void *__real_memmove(void*,const void*,size_t);
void *__wrap_malloc(size_t n) { allocation_events += guarded; return __real_malloc(n); }
void *__wrap_calloc(size_t n,size_t s) { allocation_events += guarded; return __real_calloc(n,s); }
void *__wrap_realloc(void *p,size_t n) { allocation_events += guarded; return __real_realloc(p,n); }
void __wrap_free(void *p) { allocation_events += guarded; __real_free(p); }
void *__wrap_aligned_alloc(size_t a,size_t n) { allocation_events += guarded; return __real_aligned_alloc(a,n); }
int __wrap_posix_memalign(void **p,size_t a,size_t n) { allocation_events += guarded; return __real_posix_memalign(p,a,n); }
static void note_copy(const void *p, size_t n) {
    const uintptr_t b = reinterpret_cast<uintptr_t>(p), s = reinterpret_cast<uintptr_t>(protected_source);
    source_copies += guarded && n && b < s+pixels*2 && b+n > s;
}
void *__wrap_memcpy(void *d,const void *s,size_t n) { note_copy(s,n); return __real_memcpy(d,s,n); }
void *__wrap_memmove(void *d,const void *s,size_t n) { note_copy(s,n); return __real_memmove(d,s,n); }
}
void *operator new(size_t n) { if (void *p=std::malloc(n ? n : 1)) return p; throw std::bad_alloc(); }
void *operator new[](size_t n) { return ::operator new(n); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p,size_t) noexcept { std::free(p); }
void operator delete[](void *p,size_t) noexcept { std::free(p); }
void *operator new(size_t n,std::align_val_t a) {
    void *p = nullptr; if (posix_memalign(&p,static_cast<size_t>(a),n ? n : 1)) throw std::bad_alloc(); return p;
}
void *operator new[](size_t n,std::align_val_t a) { return ::operator new(n,a); }
void operator delete(void *p,std::align_val_t) noexcept { std::free(p); }
void operator delete[](void *p,std::align_val_t) noexcept { std::free(p); }
void operator delete(void *p,size_t,std::align_val_t) noexcept { std::free(p); }
void operator delete[](void *p,size_t,std::align_val_t) noexcept { std::free(p); }
#endif

static uint16_t frame[pixels];
static uint8_t domain[pixels];
static Config cfg;
static Context context() {
    Context c;
    c.frame=91; c.revision=12; c.captured_at_ms=123456;
    c.depth=true; c.units=Units::Millimeters; c.scope=Scope::CalibrationDomain;
    return c;
}
static void fill(unsigned value) { for (auto &v : frame) v=value; }
static unsigned at(unsigned x=320,unsigned y=240) { return y*width+x; }
static void step(int dx, int dy, bool mixed=true) {
    for (unsigned y=0;y<height;++y) for (unsigned x=0;x<width;++x)
        frame[y*width+x] = (int(x)-320)*dx+(int(y)-240)*dy < 0 ? 1000 : 2000;
    if (mixed) frame[at()]=1500;
}
static Flag classify(unsigned i=at()) { return classify_sample(frame,i,domain,cfg).flag; }

static void kernel_tests() {
    cfg.enabled=true; assert(valid(cfg));
    for (auto &v : domain) v=1;
    fill(1000); assert(classify()==Flag::Retained);
    for (int dx=-1;dx<=1;++dx) for (int dy=-1;dy<=1;++dy) if(dx || dy) {
        step(dx,dy); assert(classify()==Flag::MixedCandidate);
        step(dx,dy,false); assert(classify()!=Flag::MixedCandidate);
    }
    step(1,0,false); assert(classify()==Flag::BoundaryRisk);
    // Coherent narrow third surface / thin pole: ambiguous, NEVER hide.
    for (unsigned y=0;y<height;++y) frame[y*width+320]=1500;
    assert(classify()==Flag::Unclassified);
    for (unsigned y=0;y<height;++y) frame[y*width+321]=1500;
    assert(classify()!=Flag::MixedCandidate);
    fill(1500); frame[at()-2]=1000; frame[at()+2]=2000;
    assert(classify()==Flag::Unclassified); // isolated extreme probes
    // Linear profile passes all support checks, then hits ramp veto.
    for (unsigned y=0;y<height;++y) for (unsigned x=0;x<width;++x)
        frame[y*width+x] = 1500 + (int(x%5)-2)*250;
    // Set the exact tested 5x5 ramp without repeating sawtooth endpoints.
    for (int y=-2;y<=2;++y) for(int x=-2;x<=2;++x) frame[at()+y*width+x]=1500+x*250;
    assert(classify()==Flag::Retained);
    // Broad curves and deterministic flat noise should never reject.
    for (unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x)
        frame[y*width+x]=1000+(x*x)/400+((x*17+y*13)%11);
    for (unsigned i=0;i<pixels;i+=4) assert(classify(i)!=Flag::MixedCandidate);
    // Sparse AOI support, holes, and every image border. Never read outside source.
    step(1,0);
    for (auto &v : domain) v=0;
    domain[at()]=1; assert(classify()==Flag::Unclassified);
    for (auto &v : domain) v=1;
    for (unsigned x=0;x<width;++x) { classify(x); classify((height-1)*width+x); }
    for (unsigned y=0;y<height;++y) { classify(y*width); classify(y*width+width-1); }
    // Opposite row-end values must not be treated as horizontal neighbors.
    fill(1000); for (unsigned y=0;y<height;++y) frame[y*width+width-1]=2000;
    assert(classify(at(0,240))==Flag::Retained);
    fill(0); assert(classify()==Flag::Unclassified);
    fill(65535); assert(classify()==Flag::Unclassified); // Pure kernel only; no unsafe XYZD narrowing.
    Config bad=cfg; bad.boundary_gap=80; assert(valid(bad));
    bad=cfg; bad.center_margin=40; assert(valid(bad));
    bad=cfg; bad.surface_support=0; assert(valid(bad));
    bad=cfg; bad.evidence_max=65535; assert(valid(bad));
    bad=cfg; bad.algorithm=static_cast<Algorithm>(99); assert(!valid(bad));
    // Inclusive early-exit boundary: margin-tolerance = 20, NOT tolerance=40.
    step(1,0,false);
    Config early=cfg; early.algorithm=Algorithm::ConservativeEarlyExit;
    for (unsigned depth : {1019,1020,1021,1979,1980,1981}) {
        frame[at()]=depth;
        const auto exact=classify_sample(frame,at(),domain,cfg);
        const auto quick=classify_sample(frame,at(),domain,early);
        assert(quick.early_retained == (depth<=1020 || depth>=1980));
        assert((quick.flag==Flag::MixedCandidate) == (exact.flag==Flag::MixedCandidate));
    }
}

static void unrestricted_tuning_tests() {
    for(auto &v:domain) v=domain_member;
    mark_interior(domain);
    step(1,0);
    Config zero=cfg;zero.surface_support=0;zero.center_margin=0;
    zero.boundary_gap=0;zero.anchor_tolerance=65535;
    assert(valid(zero));
    // Overlapping anchor bands assign every neighbor to near, leaving far empty.
    // A requested zero support count cannot authorize division by zero.
    assert(classify_sample(frame,at(),domain,zero).flag==Flag::Unclassified);
    uint32_t random=123;
    auto next=[&](){random=random*1664525u+1013904223u;return random>>8;};
    for(auto &v:frame) v=uint16_t(next());
    guarded=true;
    for(unsigned n=0;n<512;++n) {
        Config c=cfg;
        c.evidence_min=n%3?0:uint16_t(next());
        c.evidence_max=n%3?65535:uint16_t(next());
        c.boundary_gap=n%2?uint16_t(next()):0;
        c.anchor_tolerance=n%4?uint16_t(next()):0;
        c.center_margin=n%2?0:uint16_t(next());
        c.ramp_residual=uint16_t(next());
        constexpr uint8_t supports[]={0,1,2,24,25,255};
        c.surface_support=supports[n%6];c.center_support=supports[(n/6)%6];
        assert(valid(c));
        Config early=c;early.algorithm=Algorithm::ConservativeEarlyExit;
        for(unsigned sample=0;sample<128;++sample) {
            const unsigned i=next()%pixels;
            const auto checked=classify_sample_checked(frame,i,domain,c);
            const auto actual=classify_sample(frame,i,domain,c);
            const auto quick=classify_sample(frame,i,domain,early);
            assert(actual.flag==checked.flag&&actual.gated==checked.gated);
            assert((quick.flag==Flag::MixedCandidate)==(actual.flag==Flag::MixedCandidate));
        }
    }
    guarded=false;
}

static void interior_parity_tests() {
    uint32_t random = 73;
    unsigned fast = 0, checked = 0, mixed = 0, early_count = 0, changed_flags = 0;
    for (unsigned layout = 0; layout < 3; ++layout) {
        for (unsigned i = 0; i < pixels; ++i) {
            const unsigned x = i%width, y = i/width;
            bool member = true;
            if (layout == 1) member = x >= 3 && x < 638 && y >= 1 && y < 479 &&
                !(x >= 315 && x <= 325 && y >= 230 && y <= 250);
            if (layout == 2) member = ((x/17+y/19)%3) != 0;
            domain[i] = member ? domain_member : 0;
        }
        mark_interior(domain);
        for (unsigned i = 0; i < pixels; ++i) {
            bool expected = (domain[i] & domain_member) != 0;
            const int x = i%width, y = i/width;
            for (int oy = -2; oy <= 2 && expected; ++oy) for (int ox = -2; ox <= 2; ++ox)
                if (!location(domain,x+ox,y+oy)) { expected = false; break; }
            assert(bool(domain[i] & domain_interior) == expected);
        }
        for (unsigned scene = 0; scene < 12; ++scene) {
            for (unsigned i = 0; i < pixels; ++i) {
                random = random*1664525u+1013904223u;
                frame[i] = scene == 0 ? 1000 : scene <= 2 ? (i%width%8 < 4 ? 1000 : 2000) :
                    scene == 3 ? 1000+(i%width)*2 : 900+random%1400;
                if (scene == 2 && i%4 == 0) frame[i] = 1300+((i/width)%5)*100;
            }
            Config c = cfg;
            if (scene > 3) {
                c.anchor_tolerance = 20+scene*2;
                c.boundary_gap = 150+scene*10;
                c.center_margin = c.anchor_tolerance+15;
                c.ramp_residual = 10;
            }
            if (scene > 3) c.center_support = 2+scene%5;
            assert(valid(c));
            Config early = c; early.algorithm = Algorithm::ConservativeEarlyExit;
            guarded = true;
            for (unsigned i = 0; i < pixels; i += 7) {
                const auto reference = classify_sample_checked(frame,i,domain,c);
                const auto actual = classify_sample(frame,i,domain,c);
                assert(actual.flag == reference.flag && actual.gated == reference.gated);
                const auto quick = classify_sample(frame,i,domain,early);
                const auto quick_checked = classify_sample_checked(frame,i,domain,early);
                assert(quick.flag == quick_checked.flag && quick.gated == quick_checked.gated &&
                       quick.early_retained == quick_checked.early_retained);
                assert(quick.gated == reference.gated);
                assert((quick.flag == Flag::MixedCandidate) == (reference.flag == Flag::MixedCandidate));
                if (quick.flag != reference.flag) {
                    assert(quick.flag == Flag::Unclassified && quick.early_retained);
                    ++changed_flags;
                }
                early_count += quick.early_retained;
                fast += bool(domain[i] & domain_interior);
                checked += !(domain[i] & domain_interior);
                mixed += actual.flag == Flag::MixedCandidate;
            }
            guarded = false;
        }
    }
    assert(fast > 0 && checked > 0 && mixed > 0 && early_count > 0 && changed_flags > 0);
    assert(allocation_events == 0 && source_copies == 0);
}

#include "aggressive_boundary_cases.h"

static void fixture(std::initializer_list<std::pair<unsigned,unsigned>> runs) {
    // Test-only reset occurs BEFORE the allocation boundary, never a production recovery API.
    calibration::cleanup_producer = Producer{};
    unsigned j=0;
    for (auto r : runs) calibration::runlines[j++]={r.first,0,r.second,0,0};
    calibration::num_runlines=j;
    assert(calibration::initialize_sampled_cleanup(cfg));
}
static void emit(const uint16_t *src,const Context &c) { calibration::vector_point_cloud(src,c); }
static uint64_t hash(const uint16_t *src) {
    uint64_t h=1469598103934665603ull;
    for (unsigned i=0;i<pixels;++i) { h ^= src[i]; h *= 1099511628211ull; }
    return h;
}
static void check_emission(const uint16_t *src, const Context &c) {
    emit(src,c);
    unsigned k=0;
    const auto *flags=calibration::sampled_cleanup_flags();
    const auto &p=calibration::sampled_cleanup_publication();
    for(unsigned j=0;j<calibration::num_runlines;++j) {
        const auto &r=calibration::runlines[j];
        for(unsigned i=r.srcstart;i<(r.runlen & ~3u);i+=4,++k) {
            assert(cloud[4*k]==src[i] && cloud[4*k+1]==0 && cloud[4*k+2]==0 && cloud[4*k+3]==src[i]);
            if(p.status==Status::Ready) assert(flags[k]==static_cast<uint8_t>(classify_sample(src,i,domain,cfg).flag));
        }
    }
    assert(cloud_size==4*k && p.records==k && p.flags==k);
    assert(p.context.frame==c.frame && p.context.revision==c.revision && p.context.captured_at_ms==c.captured_at_ms);
    assert(p.version==algorithm_version);
    for(unsigned i=cloud_size;i<data::point_cloud_components+16;++i) assert(cloud[i]==12345);
}
static void producer_tests() {
    options.center_x=320; options.center_y=240; options.inner_dist=137; options.outer_dist=285;
    calibration::make_calibration_data();
    // Isolate topology from projection while still exercising the exact emitter.
    for(unsigned i=0;i<pixels;++i) {
        calibration::angle_sin_vert_cos[i]=1; calibration::vert_sin[i]=0; calibration::angle_cos_vert_sin[i]=0;
    }
    for(auto &v:cloud) v=12345;
    for(auto &v:domain) v=1;
    fixture({{0,pixels}});
    assert(!calibration::initialize_sampled_cleanup(cfg)); // Reinitialization prohibited.
    step(1,0);
    // Mapped read-only source makes any accidental write fatal, including SIMD stores.
    void *mapping=mmap(nullptr,pixels*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert(mapping!=MAP_FAILED);
    std::memcpy(mapping,frame,pixels*2);
    assert(mprotect(mapping,pixels*2,PROT_READ)==0);
    protected_source=static_cast<const uint16_t*>(mapping);
    const auto before=hash(protected_source);
    auto c=context();
    guarded=true; // FIRST live emitter/kernel/clock call, not a warmed path.
    check_emission(protected_source,c);
    assert(calibration::sampled_cleanup_publication().mixed>0);
    const unsigned full_size=cloud_size;
    std::memcpy(original_cloud,cloud,cloud_size*sizeof(s16)); // XYZD comparison only, NOT source.
    for(unsigned n=0;n<5;++n) {
        Config off=cfg; off.enabled=false;
        assert(calibration::configure_sampled_cleanup(off));
        emit(protected_source,c);
        assert(calibration::sampled_cleanup_publication().status==Status::Off);
        for(unsigned k=0;k<cloud_size/4;++k) assert(calibration::sampled_cleanup_flags()[k]==0);
        assert(std::memcmp(original_cloud,cloud,full_size*sizeof(s16))==0);
        assert(calibration::configure_sampled_cleanup(cfg));
        emit(protected_source,c);
        assert(std::memcmp(original_cloud,cloud,full_size*sizeof(s16))==0);
    }
    auto generation=calibration::sampled_cleanup_publication().generation;
    Config changed=cfg; changed.boundary_gap=300;
    assert(calibration::configure_sampled_cleanup(changed)); emit(protected_source,c);
    assert(calibration::sampled_cleanup_publication().generation>generation);
    changed.anchor_tolerance=1000;
    assert(calibration::configure_sampled_cleanup(changed)); emit(protected_source,c);
    assert(calibration::sampled_cleanup_publication().status==Status::Ready);
    assert(std::memcmp(original_cloud,cloud,full_size*sizeof(s16))==0);
    changed.algorithm=static_cast<Algorithm>(99);
    assert(!calibration::configure_sampled_cleanup(changed)); emit(protected_source,c);
    assert(calibration::sampled_cleanup_publication().status==Status::InvalidConfiguration);
    assert(calibration::configure_sampled_cleanup(cfg));
    for(unsigned kind=0;kind<6;++kind) {
        auto unsupported=c;
        if(kind==0) unsupported.depth=false;
        if(kind==1) unsupported.units=Units::Sensor;
        if(kind==2) unsupported.plugin=true;
        if(kind==3) unsupported.sampling=true;
        if(kind==4) unsupported.scope=Scope::FullSensor;
        if(kind==5) unsupported.units=Units::Infrared;
        emit(protected_source,unsupported);
        const auto &p=calibration::sampled_cleanup_publication();
        assert(p.status==Status::UnsupportedContext && p.unclassified==p.records && p.mixed==0);
    }
    emit(protected_source,c); assert(calibration::sampled_cleanup_publication().status==Status::Ready);
    Config early=cfg; early.algorithm=Algorithm::ConservativeEarlyExit;
    const auto exact_mixed=calibration::sampled_cleanup_publication().mixed;
    assert(calibration::configure_sampled_cleanup(early)); emit(protected_source,c);
    const auto &quick=calibration::sampled_cleanup_publication();
    assert(quick.version==2 && quick.configuration.algorithm==Algorithm::ConservativeEarlyExit);
    assert(quick.early_retained>0 && quick.mixed==exact_mixed);
    assert(quick.detailed+quick.early_retained==quick.gated);
    assert(std::memcmp(original_cloud,cloud,full_size*sizeof(s16))==0);
    Config aggressive=cfg;aggressive.algorithm=Algorithm::AggressiveBoundaryBands;
    assert(calibration::configure_sampled_cleanup(aggressive));emit(protected_source,c);
    const auto &strong=calibration::sampled_cleanup_publication();
    assert(strong.version==3&&strong.early_retained==0&&strong.gated==strong.detailed);
    assert(strong.mixed>0&&strong.mixed+strong.boundary<=strong.detailed);
    assert(strong.mixed+strong.boundary+strong.retained+strong.unclassified==strong.records);
    assert(std::memcmp(original_cloud,cloud,full_size*sizeof(s16))==0);
    // Off must not touch even the center; an unreadable source is safe in sample().
    Config off=aggressive;off.enabled=false;assert(calibration::configure_sampled_cleanup(off));
    assert(calibration::cleanup_producer.begin(calibration::runlines,calibration::num_runlines,c));
    calibration::cleanup_producer.sample(nullptr,0,0);calibration::cleanup_producer.finish(1);
    assert(calibration::sampled_cleanup_flags()[0]==0);
    assert(calibration::configure_sampled_cleanup(cfg));
    // Safe runtime shrink/zero stays unfiltered and permanently invalidates old topology.
    calibration::runlines[0].runlen=8; emit(protected_source,c);
    assert(cloud_size==8 && calibration::sampled_cleanup_publication().flags==2);
    assert(calibration::sampled_cleanup_publication().status==Status::TopologyChanged);
    calibration::num_runlines=0; emit(protected_source,c);
    assert(cloud_size==0 && calibration::sampled_cleanup_publication().flags==0);
    calibration::num_runlines=1; calibration::runlines[0].runlen=pixels;
    emit(protected_source,c); assert(calibration::sampled_cleanup_publication().status==Status::TopologyChanged);
    calibration::runlines[0].runlen=pixels+1;
    emit(protected_source,c); assert(cloud_size==0);
    assert(calibration::sampled_cleanup_publication().status==Status::UnsafeTopology);
    assert(hash(protected_source)==before);
    guarded=false;
    assert(allocation_events==0 && source_copies==0);
    assert(munmap(mapping,pixels*2)==0); protected_source=nullptr;
    // Near-all-candidate detailed path, also inside the allocation/copy guard.
    fixture({{0,pixels}});
    for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x)
        frame[y*width+x]=(x%8<4 ? 1000 : 2000);
    for(auto &v:cloud) v=12345;
    guarded=true;
    check_emission(frame,c);
    assert(calibration::sampled_cleanup_publication().gated > pixels/4*99/100);
    guarded=false;
    // Domain evidence must come from actual processing runs, NOT stale halo pixels.
    fixture({{at()-1,at()+3}});
    step(1,0); guarded=true; emit(frame,c);
    assert(calibration::sampled_cleanup_publication().mixed==0);
    assert(calibration::sampled_cleanup_publication().unclassified==1);
    guarded=false;
    // Irregular starts, tails, short/empty/adjacent/cross-row runs, final pixel.
    fixture({{0,1},{1,8},{8,8},{9,12},{637,647},{648,652},{pixels-5,pixels}});
    fill(1000); for(auto &v:cloud) v=12345;
    guarded=true;
    emit(frame,c);
    unsigned expected[]={1,5,9,637,641,648,pixels-5,pixels-1};
    assert(cloud_size==sizeof(expected)/sizeof(expected[0])*4);
    // Equal depths alone cannot prove correspondence: repeat with index-derived depths.
    guarded=false;
    for(unsigned i=0;i<pixels;++i) frame[i]=250+i%5000;
    guarded=true; emit(frame,c);
    for(unsigned k=0;k<sizeof(expected)/sizeof(expected[0]);++k) assert(cloud[k*4+3]==frame[expected[k]]);
    guarded=false;
    // Startup rejection: bounds, overlap, excessive runs, too-small real capacity,
    // invalid configuration, failure retry. No domain partial construction used.
    static Producer failure;
    calibration::runline invalid{5,0,4,0,0};
    assert(!failure.initialize(&invalid,1,data::point_cloud_components,cfg));
    invalid={0,0,4,0,0};
    assert(!failure.initialize(&invalid,1,data::point_cloud_components,cfg));
    assert(!Producer::safe(&invalid,max_runs+1,max_records));
    invalid={0,0,pixels+1,0,0}; assert(!Producer::safe(&invalid,1,max_records));
    invalid={0,0,8,0,0}; assert(!Producer::safe(&invalid,1,1));
    calibration::runline overlap[]={{0,0,8,0,0},{4,0,12,0,0}};
    assert(!Producer::safe(overlap,2,max_records));
    assert(calibration::sampled_cleanup_metrics().frames>=2);
    assert(allocation_events==0 && source_copies==0);
}

static void benchmark() {
    // Host-only paired emitter WALL timings; NOT target CPU acceptance.
    // All timing storage is reserved before the guard; print/percentiles offline.
    constexpr unsigned repeats=100;
    uint64_t samples[repeats]{};
    auto c=context();
    for(unsigned shape=0;shape<2;++shape) {
        if(shape==0) fixture({{0,pixels}});
        else {
            calibration::make_calibration_data();
            calibration::cleanup_producer=Producer{};
            assert(calibration::initialize_sampled_cleanup(cfg));
        }
        for(unsigned scene=0;scene<3;++scene) {
            for(unsigned y=0;y<height;++y) for(unsigned x=0;x<width;++x)
                frame[y*width+x]=scene==0 ? 1000 : scene==1 ? (x%8<4 ? 1000 : 2000) :
                    (x%8<4 ? 1000 : 2000)+(y%3)*10;
            for(unsigned mode=0;mode<3;++mode) {
                Config config=cfg; config.enabled=mode==1;
                assert(calibration::configure_sampled_cleanup(config));
                uint64_t elapsed=0, worst=0; unsigned gated=0;
                guarded=true;
                for(unsigned n=0;n<repeats;++n) {
                    const auto start=std::chrono::steady_clock::now();
                    emit(frame,c);
                    samples[n]=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
                    elapsed+=samples[n]; if(samples[n]>worst) worst=samples[n];
                    gated=calibration::sampled_cleanup_publication().gated;
                }
                guarded=false;
                std::printf("host shape=%u scene=%u mode=%u records=%u gated=%u mean_ms=%.3f worst_ms=%.3f iterations=%u\n",
                    shape,scene,mode,cloud_size/4,gated,elapsed/1e6/repeats,worst/1e6,repeats);
                for(unsigned n=0;n<repeats;++n)
                    std::printf("sample,%u,%u,%u,%llu\n",shape,scene,mode,static_cast<unsigned long long>(samples[n]));
            }
        }
    }
    assert(allocation_events==0 && source_copies==0);
}

static void release_calibration() {
    using namespace calibration;
    free(pixel_dist); free(pixel_gain); free(pixel_offset); free(angle_sin_vert_cos);
    free(vert_sin); free(vert_cos); free(angle_cos_vert_sin); free(pixel_angle);
    free(pixel_vert_radians); free(pixel_angle_bucket); free(angle_bucket);
    free(pixel_line_bucket); free(src_offsets); free(runlines); free(calibration::calibration);
}
int main(int argc,char **argv) {
    cfg.enabled=true;
    producer_tests(); // First kernel/emitter/clock call occurs under the guard.
    kernel_tests();
    unrestricted_tuning_tests();
    interior_parity_tests();
    aggressive_kernel_tests();
    if(argc==2 && std::strcmp(argv[1],"--benchmark")==0) benchmark();
    release_calibration();
    std::printf("producer_storage_bytes=%zu\n",sizeof(Producer));
    std::printf("sampled cleanup: kernel, production emission, lifecycle, immutable source: PASS; allocation_events=%u source_copies=%u%s\n",
        allocation_events,source_copies,
#ifdef CHITON_CLEANUP_ALLOCATION_GUARD
        " (allocator/copy wrappers enabled)"
#else
        " (sanitizer build; allocation guard disabled)"
#endif
    );
}
