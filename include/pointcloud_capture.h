#pragma once
#include "bounded_http.h"
#include "capture_sha256.h"
#include "sampled_boundary_cleanup.h"
#include "pointcloud_source_index.h"
#include "calibration.h"
#include <cmath>
#include <limits>
#include <memory>
#include <new>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

// Diagnostic-only recorder. No source pixels, windows or XYZD are staged here.
namespace pointcloud_capture {
constexpr size_t record_limit=sampled_cleanup::max_records;
// Recovered SDK packs depth+IR in a 640x960 container; DEPTH is its first
// 640x480 plane. The explicitly SDK-free stub reports plane dimensions instead.
inline bool depth_plane_supported(unsigned width,unsigned height,bounded_http::View type,bool stub=false){
    return width==640&&((type=="depth_ir"&&height==(stub?480u:960u))||(type=="depth"&&height==480));
}
inline bool file_sha256(const char *path,char out[65]) {
    int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return false;
    SHA256 sha;char bytes[8192];bool ok=true;
    for(;;){auto n=read(fd,bytes,sizeof(bytes));if(n<0&&errno==EINTR)continue;
        if(n<0){ok=false;break;}if(!n)break;sha.update(bytes,size_t(n));}
    if(close(fd))ok=false;if(ok)sha.hex(out);return ok;
}

struct Model {
    const float *gain,*offset,*x,*y,*z;
};
struct Identity {
    uint64_t frame=0,revision=0,captured_at_ms=0,started_ns=0;
    unsigned settle_frames=0; int center_x=0,center_y=0,inner=0,outer=0;
    bool calibrated=false,sdk_stub=false;unsigned noise_reduction=0,sdk_container_height=0;
    const char *camera_mode="unknown",*sdk_frame_type="unknown";
};
class Capture {
    static constexpr const char *names_[10]={"sensor-depth.u16le","processed-depth.u16le","cloud.i16le",
        "source-index.u32le","record-model.f32le","cleanup-flags.u8","runlines.json","pixel-status.u8","capture.json","SHA256SUMS"};
    int directory_=-1,fd_[10];SHA256 hashes_[10];size_t sizes_[10]{};
    std::unique_ptr<uint32_t[]> indices_;
    std::unique_ptr<uint8_t[]> pixel_status_;
    std::chrono::steady_clock::time_point deadline_;
    bool ready_=false;
    bool write_bytes(unsigned file,const void *data,size_t size){
        auto p=static_cast<const uint8_t*>(data);
        while(size){if(cancelled&&cancelled())return fail("capture interrupted");
            if(std::chrono::steady_clock::now()>deadline_)return fail("capture write deadline exceeded");
            auto n=::write(fd_[file],p,std::min<size_t>(size,65536));
            if(n<0&&errno==EINTR)continue;
            if(n<=0)return fail("capture write failed");
            hashes_[file].update(p,size_t(n));sizes_[file]+=size_t(n);p+=n;size-=size_t(n);}
        return true;
    }
    bool finish_file(unsigned file){
        if(fsync(fd_[file]))return fail("capture fsync failed");
        const int fd=fd_[file];fd_[file]=-1;
        if(close(fd))return fail("capture close failed");return true;
    }
    bool json(unsigned file,const bounded_http::Buffer &b){
        if(!b.valid)return fail("bounded metadata overflow/unsupported text");
        return write_bytes(file,b.bytes,b.size);
    }
    bool fail(const char *why){
        error=why;
        if(directory_>=0){
            int fd=openat(directory_,"INCOMPLETE",O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC|O_NOFOLLOW,0600);
            if(fd>=0){size_t length=0;while(why[length]&&length<512)++length;
                // Best-effort error label, independent of payload/hash writes.
                size_t used=0;while(used<length){auto n=::write(fd,why+used,length-used);
                    if(n<0&&errno==EINTR)continue;if(n<=0)break;used+=size_t(n);}close(fd);}
        }
        return false;
    }
public:
    Recorder trace;
    uint8_t *pixel_status(){return pixel_status_.get();}
    const char *error="capture not prepared";
    bool (*cancelled)()=nullptr;
    char binary_sha256[65]{};
    void record_failure(){fail(*error?error:"diagnostic failure; see process stderr");}
    explicit Capture(){for(auto &fd:fd_)fd=-1;}
    ~Capture(){for(int fd:fd_)if(fd>=0)close(fd);if(directory_>=0)close(directory_);}
    bool prepare(const char *path,unsigned seconds){
        const uint16_t endian=1;
        if(*reinterpret_cast<const uint8_t*>(&endian)!=1||sizeof(float)!=4||!std::numeric_limits<float>::is_iec559)
            return fail("capture requires little-endian IEEE754 float32");
        if(!seconds||seconds>300)return fail("capture timeout must be 1..300 seconds");
        deadline_=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);
        if(mkdir(path,0700))return fail("capture output must be a new directory with an existing parent");
        directory_=open(path,O_DIRECTORY|O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
        if(directory_<0)return fail("cannot open capture directory");
        int marker=openat(directory_,"INCOMPLETE",O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC|O_NOFOLLOW,0600);
        if(marker<0)return fail("cannot mark incomplete capture");
        if(close(marker))return fail("cannot close incomplete marker");
        for(unsigned j=0;j<10;++j){fd_[j]=openat(directory_,j==8?"capture.json.tmp":names_[j],O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
            if(fd_[j]<0)return fail("cannot create exclusive capture files");}
        indices_.reset(new(std::nothrow) uint32_t[record_limit]);
        pixel_status_.reset(new(std::nothrow) uint8_t[sampled_cleanup::pixels]);
        if(!indices_||!pixel_status_)return fail("cannot reserve diagnostic index/status tables");
        for(size_t j=0;j<record_limit;++j)indices_[j]=0;
        std::fill(pixel_status_.get(),pixel_status_.get()+sampled_cleanup::pixels,uint8_t(OutsideRunlines));
        trace.indices=indices_.get();trace.capacity=record_limit;trace.pixel_status=pixel_status_.get();
        // /proc/self/exe is intentionally a kernel symlink, unlike capture paths.
        char exe[4096];auto n=readlink("/proc/self/exe",exe,sizeof(exe)-1);
        if(n<=0||n>=ssize_t(sizeof(exe)-1))return fail("cannot resolve binary provenance");
        exe[n]=0;if(!file_sha256(exe,binary_sha256))return fail("cannot hash binary provenance");
        ready_=true;error="";return true;
    }
    // Structural checks only. Per-pixel failures become exclusions, never fatal
    // capture errors. diagnostic_calibrate_frame does the safe conversion once.
    bool preflight_input(const uint16_t *raw,const calibration::runline *runs,unsigned n,Model m,bool calibrated){
        if(!ready_||!raw||!m.gain||!m.offset||!m.x||!m.y||!m.z||
           !sampled_cleanup::Producer::safe(runs,n,record_limit))return fail("unsafe capture input/topology");
        (void)calibrated;
        std::fill(pixel_status_.get(),pixel_status_.get()+sampled_cleanup::pixels,uint8_t(OutsideRunlines));
        for(unsigned j=0;j<n;++j)for(unsigned i=runs[j].srcstart;i<runs[j].runlen;++i)
            pixel_status_[i]=raw[i]?Valid:ZeroSensor;
        return true;
    }
    bool preflight_projection(const uint16_t *processed,const calibration::runline *runs,unsigned n,Model m){
        if(!ready_||!processed||!m.gain||!m.offset||!m.x||!m.y||!m.z||!sampled_cleanup::Producer::safe(runs,n,record_limit))return fail("unsafe projection topology");
        for(unsigned j=0;j<n;++j)for(unsigned i=runs[j].srcstart;i<(runs[j].runlen&~3u);i+=4){
            if(pixel_status_[i]!=Valid)continue;
            if(!std::isfinite(m.gain[i])||!std::isfinite(m.offset[i])){pixel_status_[i]=CalibrationNonfinite;continue;}
            if(!processed[i]){pixel_status_[i]=ZeroProcessed;continue;}
            if(processed[i]>32767){pixel_status_[i]=ProjectionRange;continue;}
            for(float f:{m.x[i],m.y[i],m.z[i]}){float v=processed[i]*f;
                if(!std::isfinite(f)||!std::isfinite(v)){pixel_status_[i]=ProjectionNonfinite;break;}
                if(std::trunc(v)<-32768||std::trunc(v)>32767){pixel_status_[i]=ProjectionRange;break;}}}
        return true;
    }
    // Best-effort raw evidence on a genuine structural failure. Bad individual
    // pixel values instead receive mask reasons and complete the normal bundle.
    void retain_raw(const uint16_t *raw){if(ready_&&raw&&!sizes_[0])write_bytes(0,raw,sampled_cleanup::pixels*2);}
    bool save(const uint16_t *raw,const uint16_t *processed,const int16_t *cloud,size_t count,
              const calibration::runline *runs,unsigned nr,Model m,const Identity &id,
              const sampled_cleanup::Publication &publication,const uint8_t *flags){
        if(!ready_||!raw||(!cloud&&count)||!m.gain||!m.offset||!trace.valid||trace.count!=count||count>record_limit||publication.records!=count||
           publication.context.frame!=id.frame||publication.context.revision!=id.revision||
           !preflight_projection(processed,runs,nr,m))return fail("capture correspondence/preflight mismatch");
        size_t k=0,candidates=0;
        for(unsigned j=0;j<nr;++j)for(unsigned i=runs[j].srcstart;i<(runs[j].runlen&~3u);i+=4){
            ++candidates;if(pixel_status_[i]!=Valid)continue;
            if(k>=count||trace.indices[k++]!=i)return fail("recorded source order mismatch");}
        if(k!=count)return fail("recorded source count mismatch");
        if(!write_bytes(0,raw,sampled_cleanup::pixels*2)||!write_bytes(1,processed,sampled_cleanup::pixels*2)||
           !write_bytes(2,cloud,count*8)||!write_bytes(3,trace.indices,count*4))return false;
        // Only model metadata is staged, never source depths or cloud payload.
        float models[5*256];
        for(size_t start=0;start<count;start+=256){size_t end=std::min(count,start+256);
            for(size_t r=start;r<end;++r){unsigned i=trace.indices[r];float *p=models+(r-start)*5;
                p[0]=m.gain[i];p[1]=m.offset[i];p[2]=m.x[i];p[3]=m.y[i];p[4]=m.z[i];}
            if(!write_bytes(4,models,(end-start)*20))return false;}
        if((!flags&&count)||!write_bytes(5,flags,count))return fail("missing/writing same-frame flags");
        if(!write_bytes(6,"[",1))return false;
        for(unsigned j=0;j<nr;++j){bounded_http::Buffer b;if(j)b.add(",");b.add("[");b.number(runs[j].srcstart);b.add(",");b.number(runs[j].runlen);b.add("]");if(!json(6,b))return false;}
        if(!write_bytes(6,"]\n",2))return false;
        if(!write_bytes(7,pixel_status_.get(),sampled_cleanup::pixels))return false;
        uint32_t status_counts[pixel_status_count]{};
        for(unsigned i=0;i<sampled_cleanup::pixels;++i){if(pixel_status_[i]>=pixel_status_count)return fail("invalid diagnostic pixel status");++status_counts[pixel_status_[i]];}
        for(unsigned j=0;j<8;++j)if(!finish_file(j))return false;
        bounded_http::Buffer b;
        auto field=[&](const char *name,uint64_t value){b.add(",\"");b.add(name);b.add("\":");b.number(value);};
        b.add("{\"schema\":2,\"complete\":true,\"width\":640,\"height\":480,\"byte_order\":\"little\",\"model_layout\":\"gain,offset,x,y,z:float32\",\"image_origin\":\"top-left\",\"sensor_units\":\"unknown_sdk_depth_units\"");
        b.add(",\"acquisition_backend\":\"");b.add(id.sdk_stub?"file-backed-stub":"hardware-sdk");b.add("\"");
        b.add(",\"processed_units\":\"");b.add(id.calibrated?"mm":"depth_sensor_units");b.add("\",\"units_scope\":\"");b.add(id.calibrated?"calibration_aoi":"full_sensor");b.add("\"");
        field("record_count",count);field("frame_id",id.frame);field("revision",id.revision);field("captured_at_ms",id.captured_at_ms);
        field("process_id",getpid());field("process_started_ns",id.started_ns);field("settle_frames",id.settle_frames);
        field("calibrated",id.calibrated);field("smoothed",0);field("plugin",0);field("sampling",0);
        b.add(",\"camera_setup\":{\"geometry_correction\":\"off\",\"noise_reduction\":");b.number(id.noise_reduction);
        auto quote=[&](const char *s){b.add("\"");for(;*s;++s){const unsigned char c=*s;
            if(c=='\"'||c=='\\')b.add("\\");
            if(c<32||c>=127){b.valid=false;break;}b.add({s,1});}b.add("\"");};
        b.add(",\"mode\":");quote(id.camera_mode);b.add(",\"sdk_frame_type\":");quote(id.sdk_frame_type);
        field("sdk_container_height",id.sdk_container_height);b.add("}");
        b.add(",\"binary_sha256\":\"");b.add(binary_sha256);b.add("\",\"geometry\":{");
        b.add("\"center_x\":");b.number(id.center_x);field("center_y",id.center_y);field("inner_dist",id.inner);field("outer_dist",id.outer);b.add("}");
        b.add(",\"cleanup\":{\"generation\":");b.number(publication.generation);field("status",unsigned(publication.status));
        const auto &c=publication.configuration;field("enabled",c.enabled);field("algorithm",unsigned(c.algorithm));
        field("evidence_min",c.evidence_min);field("evidence_max",c.evidence_max);field("boundary_gap",c.boundary_gap);
        field("anchor_tolerance",c.anchor_tolerance);field("center_margin",c.center_margin);field("ramp_residual",c.ramp_residual);
        field("surface_support",c.surface_support);field("center_support",c.center_support);b.add("},\"filter\":{\"policy\":\"runlines_valid_values_v1\"");
        field("candidate_count",candidates);field("excluded_sample_count",candidates-count);
        b.add(",\"status_counts\":[");for(unsigned j=0;j<pixel_status_count;++j){if(j)b.add(",");b.number(status_counts[j]);}
        b.add("]},\"sizes\":{");
        for(unsigned j=0;j<8;++j){if(j)b.add(",");b.add("\"");b.add(names_[j]);b.add("\":");b.number(sizes_[j]);}b.add("}}\n");
        if(!json(8,b)||!finish_file(8))return false;
        for(unsigned j=0;j<9;++j){char hash[65];hashes_[j].hex(hash);bounded_http::Buffer line;line.add(hash);line.add("  ");line.add(names_[j]);line.add("\n");if(!json(9,line))return false;}
        if(!finish_file(9))return false;
        if((cancelled&&cancelled())||std::chrono::steady_clock::now()>deadline_)return fail("capture cancelled/expired before commit");
        if(renameat(directory_,"capture.json.tmp",directory_,"capture.json")||fsync(directory_))return fail("capture commit failed");
        if(unlinkat(directory_,"INCOMPLETE",0)||fsync(directory_)){
            // Never leave a complete-looking bundle after a failed final commit.
            renameat(directory_,"capture.json",directory_,"capture.json.tmp");
            return fail("capture completion marker failed");
        }
        ready_=false;return true;
    }
};
}
