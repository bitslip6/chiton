#define CHITON_CLEANUP_TRANSPORT 1
#include "../src/graphics/calibration.cpp"
#include "cleanup_api.h"
#include "bounded_http_cases.h"
#include <cassert>
#include <new>
#include <sys/mman.h>
#include <unistd.h>

GlobalOpts options{}; GlobalOpts *GOPTS=&options;
static u16 slam_storage[SLAM_POINTS*101];
static unsigned components;
namespace data {
s16 *get_point_cloud(){return cleanup_delivery::pool.geometry();}
u16 *get_slam(){return slam_storage;}
void set_point_cloud_size(int n){assert(n>=0);components=n;}
}
namespace utils { void log_fn(int,const char*,const char*,int,const char*,...){} }
static std::recursive_mutex state_owner;
namespace cam { std::recursive_mutex &state_mutex(){return state_owner;} }
static thread_local bool guarded=false; static std::atomic<unsigned> allocations{0},copies{0};
static const uint16_t *source=nullptr;
#ifdef CHITON_CLEANUP_ALLOCATION_GUARD
extern "C" {
void *__real_malloc(size_t);void *__real_calloc(size_t,size_t);void *__real_realloc(void*,size_t);void __real_free(void*);
void *__real_aligned_alloc(size_t,size_t);int __real_posix_memalign(void**,size_t,size_t);
void *__real_memcpy(void*,const void*,size_t);void *__real_memmove(void*,const void*,size_t);
void *__wrap_malloc(size_t n){allocations+=guarded;return __real_malloc(n);}
void *__wrap_calloc(size_t n,size_t s){allocations+=guarded;return __real_calloc(n,s);}
void *__wrap_realloc(void*p,size_t n){allocations+=guarded;return __real_realloc(p,n);}
void __wrap_free(void*p){allocations+=guarded;__real_free(p);}
void *__wrap_aligned_alloc(size_t a,size_t n){allocations+=guarded;return __real_aligned_alloc(a,n);}
int __wrap_posix_memalign(void**p,size_t a,size_t n){allocations+=guarded;return __real_posix_memalign(p,a,n);}
static void copy_check(const void*p,size_t n){auto b=reinterpret_cast<uintptr_t>(p),s=reinterpret_cast<uintptr_t>(source);copies+=guarded&&n&&b<s+sampled_cleanup::pixels*2&&b+n>s;}
void *__wrap_memcpy(void*d,const void*s,size_t n){copy_check(s,n);return __real_memcpy(d,s,n);}
void *__wrap_memmove(void*d,const void*s,size_t n){copy_check(s,n);return __real_memmove(d,s,n);}
}
void *operator new(size_t n){if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
void *operator new(size_t n,std::align_val_t a){void*p=nullptr;if(posix_memalign(&p,size_t(a),n?n:1))throw std::bad_alloc();return p;}
void *operator new[](size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete(void*p,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,std::align_val_t)noexcept{std::free(p);}
void operator delete(void*p,size_t,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,size_t,std::align_val_t)noexcept{std::free(p);}
#endif
using namespace sampled_cleanup;
using namespace cleanup_delivery;
static char response[32768];
static bounded_http::Request request;
static void api(const char *raw,int expected){
    int pair[2];assert(socketpair(AF_UNIX,SOCK_STREAM,0,pair)==0);
    assert(::send(pair[1],raw,std::strlen(raw),MSG_NOSIGNAL)==ssize_t(std::strlen(raw)));
    bounded_http::receive(pair[0],request,[]{return true;});
    if(request.status==200)assert(handle(pair[0],request));
    else {bounded_http::Writer w(pair[0]);w.error(request.status,"request framing rejected");}
    assert(shutdown(pair[0],SHUT_WR)==0);
    size_t used=0;for(;;){auto n=recv(pair[1],response+used,sizeof(response)-used,0);assert(n>=0);if(!n)break;used+=size_t(n);assert(used<sizeof(response));}
    assert(used>12);uint64_t status=0;assert(bounded_http::integer({response+9,3},status));assert(status==unsigned(expected));
    close(pair[0]);close(pair[1]);
}
static void release_calibration(){using namespace calibration;
    free(pixel_dist);free(pixel_gain);free(pixel_offset);free(angle_sin_vert_cos);free(vert_sin);free(vert_cos);free(angle_cos_vert_sin);
    free(pixel_angle);free(pixel_vert_radians);free(pixel_angle_bucket);free(angle_bucket);free(pixel_line_bucket);free(src_offsets);free(runlines);free(calibration::calibration);
}
static void benchmark(const uint16_t *src,Config config,Context context){
    std::atomic<int> pending{-1};std::atomic<bool> done{false},quit{false};
    std::thread reader([&]{char bytes[8192];while(!quit.load()){
        int fd=pending.exchange(-1);if(fd<0){std::this_thread::sleep_for(std::chrono::microseconds(20));continue;}
        for(;;){auto n=recv(fd,bytes,sizeof(bytes),0);if(!n)break;assert(n>0||errno==EINTR);}
        close(fd);done=true;
    }});
    auto cpu=[](){timespec t{};assert(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t)==0);return uint64_t(t.tv_sec)*1000000000+uint64_t(t.tv_nsec);};
    constexpr unsigned repeats=250;uint64_t producer[3]{},transport[3]{};
    for(unsigned mode=0;mode<3;++mode){
        config.enabled=mode!=0;config.algorithm=mode==2?Algorithm::ConservativeEarlyExit:Algorithm::Exact;
        assert(calibration::configure_sampled_cleanup(config));
        for(unsigned n=0;n<30;++n){++context.frame;calibration::vector_point_cloud(src,context);}
        for(unsigned n=0;n<repeats;++n){
            ++context.frame;auto start=cpu();calibration::vector_point_cloud(src,context);producer[mode]+=cpu()-start;
            int pair[2];assert(socketpair(AF_UNIX,SOCK_STREAM,0,pair)==0);done=false;
            constexpr char raw[]="GET /api/cleanup/v1/cloud HTTP/1.1\r\nHost: test\r\n\r\n";
            assert(::send(pair[1],raw,sizeof(raw)-1,MSG_NOSIGNAL)==ssize_t(sizeof(raw)-1));pending=pair[1];
            start=cpu();bounded_http::receive(pair[0],request,[]{return true;});assert(request.status==200);assert(handle(pair[0],request));transport[mode]+=cpu()-start;
            assert(shutdown(pair[0],SHUT_WR)==0);close(pair[0]);
            while(!done.load())std::this_thread::yield();
        }
    }
    quit=true;reader.join();
    for(unsigned mode=0;mode<3;++mode)std::printf("pool benchmark mode=%s records=%u producer_cpu_ms=%.6f http_framing_send_cpu_ms=%.6f iterations=%u (host synthetic clutter, reader harness excluded)\n",mode==0?"off":mode==1?"exact":"early",components/4,producer[mode]/1e6/repeats,transport[mode]/1e6/repeats,repeats);
}
int main(int argc,char **argv){
    const bool bench=argc==2&&std::strcmp(argv[1],"--benchmark")==0;
    guarded=true;
    for(unsigned i=0;i<8;++i) bounded_http_number_cases();
    guarded=false;
    assert(allocations==0);
    options.running=1;options.enable=true;options.color_mode=4;
    options.center_x=320;options.center_y=240;options.inner_dist=137;options.outer_dist=285;
    calibration::make_calibration_data();
    if(!bench){
        for(unsigned i=0;i<pixels;++i){calibration::angle_sin_vert_cos[i]=1;calibration::vert_sin[i]=0;calibration::angle_cos_vert_sin[i]=0;}
        calibration::num_runlines=5;
        for(unsigned j=0;j<5;++j)calibration::runlines[j]={width*(238+j)+316,0,width*(238+j)+328,0,0};
    }
    auto *mapped=static_cast<uint16_t*>(mmap(nullptr,pixels*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));assert(mapped!=MAP_FAILED);
    for(unsigned i=0;i<pixels;++i)mapped[i]=(bench?i%width%32<16:i%width<320)?1000:2000;
    mapped[240*width+320]=1500;assert(mprotect(mapped,pixels*2,PROT_READ)==0);source=mapped;
    auto *alternate=static_cast<uint16_t*>(mmap(nullptr,pixels*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));assert(alternate!=MAP_FAILED);
    for(unsigned i=0;i<pixels;++i)alternate[i]=1800;
    assert(mprotect(alternate,pixels*2,PROT_READ)==0);
    Config config;config.enabled=true;config.algorithm=Algorithm::AggressiveBoundaryBands;assert(calibration::initialize_sampled_cleanup(config));
    Context context;context.frame=1;context.depth=true;context.units=Units::Millimeters;context.scope=Scope::CalibrationDomain;context.revision=12;context.captured_at_ms=123456;
    if(bench){benchmark(source,config,context);munmap(mapped,pixels*2);munmap(alternate,pixels*2);release_calibration();return 0;}
    static uint32_t source_indices[max_records+1];source_indices[max_records]=0xfedcba98;
    pointcloud_capture::Recorder source_recorder{source_indices,max_records,0,true};
    pointcloud_capture::recorder=&source_recorder;
    guarded=true; // FIRST producer/pool/clock/framing/API/serialization call.
    calibration::vector_point_cloud(source,context);
    pointcloud_capture::recorder=nullptr;
    assert(source_recorder.valid&&source_recorder.count==15&&source_indices[max_records]==0xfedcba98);
    for(unsigned k=0;k<15;++k)assert(source_indices[k]==width*(238+k/3)+316+4*(k%3));
    assert(components==60&&pool.settings().latest.mixed>0);
    const auto *a=pool.acquire();assert(a&&a->publication.context.frame==1);
    context.frame=2;calibration::vector_point_cloud(source,context);
    const auto *b=pool.acquire();assert(b&&b!=a&&b->publication.context.frame==2);assert(!pool.acquire());
    for(unsigned i=3;i<103;++i){context.frame=i;calibration::vector_point_cloud(source,context);}
    assert(a->publication.context.frame==1&&b->publication.context.frame==2);
    assert(a->geometry[3]==1000&&b->geometry[3]==1000);
    api("GET /api/cleanup/v1/cloud HTTP/1.1\r\nHost: test\r\n\r\n",503);
    pool.release(a);pool.release(b);
    api("GET /api/cleanup/v1/config HTTP/1.1\r\nHost: test\r\n\r\n",200);
    api("GET /api/cleanup/v1/cloud HTTP/1.1\r\nHost: test\r\n\r\n",200);
    assert(bounded_http::View(response,sizeof(response)).find("CHCLD001")!=bounded_http::View::npos);
    api("POST /api/cleanup/v1/config?enabled=0&algorithm=2&evidence_min=250&evidence_max=6000&boundary_gap=200&anchor_tolerance=40&center_margin=60&ramp_residual=30&surface_support=4&center_support=3&expected_generation=1 HTTP/1.1\r\nHost: test\r\n\r\n",200);
    assert(pool.settings().generation==2&&!pool.settings().config.enabled);
    api("POST /api/cleanup/v1/config?enabled=1&algorithm=2&evidence_min=250&evidence_max=6000&boundary_gap=200&anchor_tolerance=40&center_margin=60&ramp_residual=30&surface_support=4&center_support=3&expected_generation=1 HTTP/1.1\r\nHost: test\r\n\r\n",409);
    api("POST /api/cleanup/v1/config?enabled=1&algorithm=2&evidence_min=250&evidence_max=6000&boundary_gap=65536&anchor_tolerance=40&center_margin=60&ramp_residual=30&surface_support=4&center_support=3&expected_generation=2 HTTP/1.1\r\nHost: test\r\n\r\n",400);
    assert(pool.settings().generation==2);
    api("POST /api/cleanup/v1/config HTTP/1.1\r\nHost: test\r\nOrigin: http://other\r\n\r\n",403);
    assert(pool.settings().generation==2);
    ++context.frame;calibration::vector_point_cloud(source,context);assert(pool.settings().latest.status==Status::Off);
    config.algorithm=Algorithm::ConservativeEarlyExit;assert(calibration::configure_sampled_cleanup(config));
    ++context.frame;calibration::vector_point_cloud(source,context);assert(pool.settings().latest.version==2);
    // Concurrent writer + leased network reader, using distinct immutable frames.
    // Thread construction/teardown is test-harness work outside the guard.
    guarded=false;std::atomic<bool> camera_started{false};
    std::thread camera([&]{for(unsigned i=0;i<1000;++i){
        Context next=context;next.frame=200+i;
        {std::lock_guard<std::recursive_mutex> owner(state_owner);calibration::vector_point_cloud(next.frame%2?alternate:source,next);}
        camera_started=true;std::this_thread::sleep_for(std::chrono::microseconds(20));
    }});
    while(!camera_started.load())std::this_thread::yield();
    uint64_t first_frame=0,last_frame=0;
    for(unsigned i=0;i<200;++i){
        api("GET /api/cleanup/v1/cloud HTTP/1.1\r\nHost: test\r\n\r\n",200);
        auto at=bounded_http::View(response,sizeof(response)).find("CHCLD001");assert(at!=bounded_http::View::npos);
        const auto *h=reinterpret_cast<const uint8_t*>(response+at);uint64_t id=0;for(unsigned b=0;b<8;++b)id|=uint64_t(h[24+b])<<(8*b);
        assert(id>=200);if(!i)first_frame=id;last_frame=id;
        int16_t first_depth=0,middle_depth=0;std::memcpy(&first_depth,h+128+6,2);std::memcpy(&middle_depth,h+128+7*8+6,2);
        assert(first_depth==(id%2?1800:1000)&&middle_depth==(id%2?1800:1500));
        assert(h[128+15*8+7]==(id%2?1:8));
    }
    camera.join();assert(last_frame>first_frame);context.frame=1200;guarded=true;
    api("GET /api/cleanup/v1/cloud?after=999999&timeout_ms=1 HTTP/1.1\r\nHost: test\r\n\r\n",504);
    api("GET /api/cleanup/v1/cloud?after=1&after=2 HTTP/1.1\r\nHost: test\r\n\r\n",400);
    guarded=false;std::atomic<bool> held{false},release_owner{false};
    std::thread blocker([&]{std::lock_guard<std::recursive_mutex> owner(state_owner);held=true;while(!release_owner.load())std::this_thread::sleep_for(std::chrono::milliseconds(1));});
    while(!held.load())std::this_thread::yield();guarded=true;
    api("POST /api/cleanup/v1/config?enabled=1&algorithm=2&evidence_min=250&evidence_max=6000&boundary_gap=200&anchor_tolerance=40&center_margin=60&ramp_residual=30&surface_support=4&center_support=3&expected_generation=3 HTTP/1.1\r\nHost: test\r\n\r\n",503);
    assert(pool.settings().generation==3);
    guarded=false;release_owner=true;blocker.join();guarded=true;
    api("GET /api/cleanup/v1/config HTTP/1.1\r\nHost: test\r\nContent-Length: 1\r\n\r\n",413);
    api("GET /api/cleanup/v1/config HTTP/1.1\r\nHost: test\r\nHost: test\r\n\r\n",400);
    api("GET /api/cleanup/v1/config HTTP/1.1\r\nHost: test\r\nTransfer-Encoding: chunked\r\n\r\n",400);
    api("DELETE /api/cleanup/v1/config HTTP/1.1\r\nHost: test\r\n\r\n",405);
    api("GET /api/cleanup/v1/missing HTTP/1.1\r\nHost: test\r\n\r\n",404);
    options.enable=false;api("GET /api/cleanup/v1/cloud HTTP/1.1\r\nHost: test\r\n\r\n",503);options.enable=true;
    options.color_mode=1;api("GET /api/cleanup/v1/cloud HTTP/1.1\r\nHost: test\r\n\r\n",409);options.color_mode=4;
    // Full storage-range and cross-field tuning remains allocation-free. Zero
    // support is allowed; the kernel must never divide by a zero group count.
    api("POST /api/cleanup/v1/config?enabled=1&algorithm=1&evidence_min=0&evidence_max=65535&boundary_gap=0&anchor_tolerance=0&center_margin=0&ramp_residual=0&surface_support=0&center_support=0&expected_generation=3 HTTP/1.1\r\nHost: test\r\n\r\n",200);
    assert(pool.settings().generation==4&&pool.settings().config.surface_support==0);
    ++context.frame;calibration::vector_point_cloud(source,context);
    assert(components==60&&pool.settings().latest.status==Status::Ready);
    api("POST /api/cleanup/v1/config?enabled=1&algorithm=2&evidence_min=65535&evidence_max=0&boundary_gap=65535&anchor_tolerance=65535&center_margin=65535&ramp_residual=65535&surface_support=255&center_support=255&expected_generation=4 HTTP/1.1\r\nHost: test\r\n\r\n",200);
    assert(pool.settings().generation==5&&pool.settings().config.center_support==255);
    ++context.frame;calibration::vector_point_cloud(source,context);
    assert(components==60&&pool.settings().latest.status==Status::Ready);
    api("POST /api/cleanup/v1/config?enabled=1&algorithm=3&evidence_min=250&evidence_max=6000&boundary_gap=200&anchor_tolerance=65535&center_margin=60&ramp_residual=65535&surface_support=255&center_support=255&expected_generation=5 HTTP/1.1\r\nHost: test\r\n\r\n",200);
    assert(pool.settings().generation==6&&pool.settings().config.center_support==255);
    ++context.frame;calibration::vector_point_cloud(source,context);
    assert(pool.settings().latest.version==3&&pool.settings().latest.mixed>0&&pool.settings().latest.early_retained==0);
    api("GET /api/cleanup/v1/config HTTP/1.1\r\nHost: test\r\n\r\n",200);
    assert(bounded_http::View(response,sizeof(response)).find("\"algorithm\":[1,2,3]")!=bounded_http::View::npos);
    api("POST /api/cleanup/v1/config?enabled=1&algorithm=4&evidence_min=250&evidence_max=6000&boundary_gap=200&anchor_tolerance=40&center_margin=60&ramp_residual=30&surface_support=4&center_support=3&expected_generation=6 HTTP/1.1\r\nHost: test\r\n\r\n",400);
    assert(pool.settings().generation==6);
    // Disconnect releases the HTTP lease; capture continues without a borrowed mutable pointer.
    int pair[2];assert(socketpair(AF_UNIX,SOCK_STREAM,0,pair)==0);close(pair[1]);
    request.method="GET";request.target="/api/cleanup/v1/cloud";assert(handle(pair[0],request));close(pair[0]);
    a=pool.acquire();b=pool.acquire();assert(a&&b);pool.release(a);pool.release(b);
    // A real partial-write timeout releases its lease, with no camera ownership.
    // Expand safely: cleanup latches unavailable, but exact XYZD emission continues.
    calibration::num_runlines=1;calibration::runlines[0]={0,0,pixels,0,0};
    ++context.frame;calibration::vector_point_cloud(source,context);
    assert(components==pixels&&pool.settings().latest.status==Status::TopologyChanged);
    assert(socketpair(AF_UNIX,SOCK_STREAM,0,pair)==0);int small=1024;
    assert(setsockopt(pair[0],SOL_SOCKET,SO_SNDBUF,&small,sizeof(small))==0);
    const auto started=std::chrono::steady_clock::now();
    assert(handle(pair[0],request));
    const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count();
    assert(elapsed>=1500&&elapsed<10000);close(pair[0]);close(pair[1]);
    a=pool.acquire();b=pool.acquire();assert(a&&b);pool.release(a);pool.release(b);
    calibration::runlines[0].runlen=0;++context.frame;calibration::vector_point_cloud(source,context);
    assert(components==0&&pool.settings().latest.flags==0);
    api("GET /api/cleanup/v1/cloud HTTP/1.1\r\nHost: test\r\n\r\n",200);
    assert(mapped[240*width+320]==1500);
    // New diagnostic processing also stays allocation/copy-free on immutable
    // input, including bad calibration values. It never runs in the live path.
    static uint16_t diagnostic_processed[pixels];static uint8_t diagnostic_status[pixels];
    calibration::runlines[0]={0,0,pixels,0,0};
    const unsigned bad=240*width+320;
    calibration::pixel_gain[bad]=1;calibration::pixel_offset[bad]=-5000;
    calibration::pixel_gain[bad+1]=std::numeric_limits<float>::quiet_NaN();
    calibration::pixel_gain[bad+2]=1;calibration::pixel_offset[bad+2]=0;
    calibration::diagnostic_calibrate_frame(source,diagnostic_processed,diagnostic_status,true);
    assert(diagnostic_status[bad]==pointcloud_capture::CalibrationRange);
    assert(diagnostic_status[bad+1]==pointcloud_capture::CalibrationNonfinite);
    assert(diagnostic_status[bad+2]==pointcloud_capture::Valid&&diagnostic_processed[bad+2]==source[bad+2]);
    guarded=false;assert(allocations==0&&copies==0);
    munmap(mapped,pixels*2);munmap(alternate,pixels*2);release_calibration();
    std::printf("cleanup delivery: exact emission, leased-slot reuse/exhaustion, concurrent frames, config CAS/busy, framing/errors/disconnects PASS; allocations=%u source_copies=%u pool_bytes=%zu allocator_guard=%s\n",allocations.load(),copies.load(),sizeof(Pool),
#ifdef CHITON_CLEANUP_ALLOCATION_GUARD
        "enabled"
#else
        "disabled (sanitizer build)"
#endif
    );
}
