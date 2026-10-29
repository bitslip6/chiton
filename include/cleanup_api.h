#pragma once
#include "bounded_http.h"
#include "cleanup_publication.h"
#include "calibration.h"
#include "camera.h"
#include <thread>

namespace cleanup_delivery {
using bounded_http::View;
inline void config_json(bounded_http::Buffer &b,const Settings &s) noexcept {
    const auto &c=s.config;
    b.add("{\"generation\":"); b.number(s.generation); b.add(",\"config\":{\"enabled\":"); b.number(c.enabled);
    b.add(",\"algorithm\":"); b.number(unsigned(c.algorithm));
#define CLEANUP_FIELD(field) b.add(",\"" #field "\":"); b.number(c.field)
    CLEANUP_FIELD(evidence_min); CLEANUP_FIELD(evidence_max); CLEANUP_FIELD(boundary_gap);
    CLEANUP_FIELD(anchor_tolerance); CLEANUP_FIELD(center_margin); CLEANUP_FIELD(ramp_residual);
    CLEANUP_FIELD(surface_support); CLEANUP_FIELD(center_support);
#undef CLEANUP_FIELD
    b.add("},\"available\":"); b.add(s.available?"true":"false");
    b.add(",\"frame_generation\":"); b.number(s.latest.generation);
    b.add(",\"status\":"); b.number(unsigned(s.latest.status));
    b.add(",\"frame_id\":"); b.number(s.latest.context.frame);
    b.add(",\"constraints\":{\"algorithm\":[1,2,3],\"parameter_maximum\":65535,\"support_maximum\":255},\"scope\":\"calibrated_mm_domain_only\"}");
}
inline bool parse_config(View query,Config &c,uint64_t &generation) noexcept {
    constexpr View names[]={"enabled","algorithm","evidence_min","evidence_max","boundary_gap","anchor_tolerance",
        "center_margin","ramp_residual","surface_support","center_support","expected_generation"};
    uint16_t seen=0;
    while(!query.empty()) {
        auto end=query.find('&'); auto field=query.substr(0,end); auto equal=field.find('=');
        if(equal==View::npos) return false;
        const auto name=field.substr(0,equal), value=field.substr(equal+1);
        unsigned index=11; for(unsigned i=0;i<11;++i) if(name==names[i]) { index=i; break; }
        if(index==11||(seen&(1u<<index))) return false;
        seen|=uint16_t(1u<<index);
        uint64_t n=0, maximum=index==0?1:index==1?3:index==10?9007199254740991ull:index>=8?255:65535;
        if(!bounded_http::integer(value,n,maximum)) return false;
        switch(index) {
            case 0:c.enabled=n;break; case 1:c.algorithm=Algorithm(n);break;
            case 2:c.evidence_min=n;break; case 3:c.evidence_max=n;break; case 4:c.boundary_gap=n;break;
            case 5:c.anchor_tolerance=n;break; case 6:c.center_margin=n;break; case 7:c.ramp_residual=n;break;
            case 8:c.surface_support=n;break; case 9:c.center_support=n;break; case 10:generation=n;break;
        }
        if(end==View::npos) break;
        query.remove_prefix(end+1); if(query.empty()) return false;
    }
    return seen==2047 && valid(c);
}
// True means the descriptor was handled, including errors. Caller closes it.
inline bool handle(int fd,const bounded_http::Request &request) noexcept {
    auto mark=request.target.find('?'); auto path=request.target.substr(0,mark);
    if(path.substr(0,12)!="/api/cleanup") return false;
    bounded_http::Writer writer(fd);
    View query=mark==View::npos?View{}:request.target.substr(mark+1);
    if(path=="/api/cleanup/v1/config") {
        if(request.method!="GET"&&request.method!="POST") { writer.error(405,"unsupported cleanup method",true); return true; }
        if(request.method=="GET"&&!query.empty()) { writer.error(400,"unexpected configuration query"); return true; }
        if(request.method=="POST") {
            // Browser mutations must be same-origin. Non-browser API tools may
            // omit Origin; this is CSRF defense, not authentication.
            View origin,host; unsigned origins=0;
            for(size_t i=0;i<request.fields;++i) {
                if(bounded_http::equal(request.headers[i].name,"origin")) { origin=request.headers[i].value; ++origins; }
                if(bounded_http::equal(request.headers[i].name,"host")) host=request.headers[i].value;
            }
            if(origins) {
                const auto scheme=origin.find("://");
                if(origins!=1||scheme==View::npos||(origin.substr(0,scheme)!="http"&&origin.substr(0,scheme)!="https")||origin.substr(scheme+3)!=host) {
                    writer.error(403,"cross-origin cleanup update rejected"); return true;
                }
            }
            Config c; uint64_t expected=0;
            if(!parse_config(query,c,expected)) { writer.error(400,"invalid complete cleanup configuration"); return true; }
            std::unique_lock<std::recursive_mutex> owner(cam::state_mutex(),std::defer_lock);
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(100);
            while(!owner.try_lock()) {
                if(std::chrono::steady_clock::now()>=deadline||GOPTS->running<=0) { writer.error(503,"configuration ownership busy"); return true; }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            if(pool.settings().generation!=expected) { owner.unlock(); writer.error(409,"stale cleanup generation; reload before applying"); return true; }
            if(!calibration::configure_sampled_cleanup(c)) { owner.unlock(); writer.error(503,"cleanup not initialized"); return true; }
            const auto acknowledged=pool.settings(); owner.unlock();
            bounded_http::Buffer body; config_json(body,acknowledged);
            if(body.valid&&writer.header(200,"application/json; charset=utf-8",body.size)) writer.send(body.bytes,body.size);
            return true;
        }
        bounded_http::Buffer body; config_json(body,pool.settings());
        if(body.valid&&writer.header(200,"application/json; charset=utf-8",body.size)) writer.send(body.bytes,body.size);
        return true;
    }
    if(path!="/api/cleanup/v1/cloud") { writer.error(404,"unknown cleanup endpoint"); return true; }
    if(request.method!="GET") { writer.error(405,"unsupported cleanup method"); return true; }
    uint64_t after=0,timeout=0; unsigned seen=0;
    while(!query.empty()) {
        auto end=query.find('&'); auto f=query.substr(0,end); auto eq=f.find('=');
        if(eq==View::npos) { writer.error(400,"invalid cloud query"); return true; }
        auto key=f.substr(0,eq); unsigned bit=key=="after"?1:key=="timeout_ms"?2:0;
        uint64_t value=0;
        if(!bit||(seen&bit)||!bounded_http::integer(f.substr(eq+1),value,bit==1?9007199254740991ull:1000)) {
            writer.error(400,"invalid cloud query"); return true;
        }
        seen|=bit; if(bit==1) after=value; else timeout=value;
        if(end==View::npos) break;
        query.remove_prefix(end+1); if(query.empty()) { writer.error(400,"invalid cloud query"); return true; }
    }
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout);
    for(;;) {
        if(GOPTS->running<=0||!GOPTS->enable) { writer.error(503,"capture disabled"); return true; }
        if(GOPTS->color_mode!=4) { writer.error(409,"select cloud mode before reading"); return true; }
        Lease lease(after);
        if(lease.slot) {
            const auto &s=*lease.slot; const auto &p=s.publication;
            if(!p.context.depth) { writer.error(409,"IR geometry unavailable"); return true; }
            const size_t size=header_size+p.records*8+p.flags;
            if(writer.header(200,"application/octet-stream",size)&&writer.send(s.header,header_size)&&
               writer.send(s.geometry,p.records*8)) writer.send(s.flags,p.flags);
            return true; // Lease releases on every write failure/disconnect too.
        }
        const auto state=pool.settings();
        if(state.available&&state.latest.context.frame>after) { writer.error(503,"publication resources busy"); return true; }
        if(!timeout||std::chrono::steady_clock::now()>=deadline) { writer.error(timeout?504:503,"compatible publication unavailable"); return true; }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
}
