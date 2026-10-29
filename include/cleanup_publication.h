#pragma once
#include "sampled_boundary_cleanup.h"
#include <mutex>
#include <cstdlib>

// Startup-owned publication; NEVER copies source depth or completed geometry.
namespace cleanup_delivery {
using namespace sampled_cleanup;
constexpr unsigned header_size = 128;
struct Slot {
    int16_t geometry[max_records*4]{};
    uint8_t flags[max_records]{};
    uint8_t header[header_size]{};
    Publication publication{};
    unsigned readers = 0;
};
struct Settings { Config config{}; uint64_t generation = 0; Publication latest{}; bool available = false; };
class Pool {
    Slot slots_[3]{};
    mutable std::mutex mutex_;
    int latest_ = -1, active_ = 0;
    unsigned leases_ = 0;
    bool started_ = false, writing_ = false;
    Config config_{};
    uint64_t generation_ = 0;
    static void put(uint8_t *p, uint64_t n, unsigned bytes) noexcept {
        for (unsigned j=0;j<bytes;++j) { p[j]=uint8_t(n); n>>=8; }
    }
    static void encode(Slot &s) noexcept {
        auto *h=s.header; const auto &p=s.publication; const auto &c=p.configuration; const auto &f=p.context;
        constexpr char magic[]="CHCLD001";
        for(unsigned i=0;i<8;++i) h[i]=magic[i];
        put(h+8,header_size,4); put(h+12,p.records,4); put(h+16,p.records*8,4); put(h+20,p.flags,4);
        put(h+24,f.frame,8); put(h+32,f.revision,8); put(h+40,f.captured_at_ms,8); put(h+48,p.generation,8);
        put(h+56,p.version,4); put(h+60,unsigned(p.status),4);
        h[64]=uint8_t(f.units); h[65]=uint8_t(f.scope); h[66]=f.depth; h[67]=f.smoothed;
        h[68]=f.plugin; h[69]=f.sampling; h[70]=c.enabled;
        const uint16_t endian=1; h[71]=*reinterpret_cast<const uint8_t*>(&endian) ? 1 : 2;
        put(h+72,c.evidence_min,2); put(h+74,c.evidence_max,2); put(h+76,c.boundary_gap,2);
        put(h+78,c.anchor_tolerance,2); put(h+80,c.center_margin,2); put(h+82,c.ramp_residual,2);
        h[84]=c.surface_support; h[85]=c.center_support;
        put(h+88,p.gated,4); put(h+92,p.mixed,4); put(h+96,p.boundary,4); put(h+100,p.unclassified,4);
        put(h+104,p.retained,4); put(h+108,p.detailed,4); put(h+112,p.early_retained,4);
    }
public:
    // Non-constexpr startup construction keeps the multi-megabyte zero payload
    // in BSS, rather than shipping a mostly-zero initialized-data ELF section.
    Pool() noexcept {}
    bool initialize() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if(started_) return false;
        // Commit/touch all reserved payload pages before acquisition, not lazily
        // on the first large frame. This is startup-only derived/output storage.
        for(auto &s:slots_) {
            for(auto &v:s.geometry) v=0;
            for(auto &v:s.flags) v=0;
            for(auto &v:s.header) v=0;
        }
        started_=true; return true;
    }
    int16_t *geometry() noexcept { return slots_[active_].geometry; } // camera/state owner only
    uint8_t *initial_flags() noexcept { return slots_[0].flags; }
    uint8_t *begin_write() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!started_ || writing_) std::abort();
        int next=-1;
        for(int i=0;i<3;++i) if(!slots_[i].readers && i!=latest_) { next=i; break; }
        if(next<0 && latest_>=0 && !slots_[latest_].readers) next=latest_;
        // At most two readers, so at least one of three slots is writable.
        if(next<0) std::abort();
        if(next==latest_) latest_=-1;
        active_=next; writing_=true;
        return slots_[next].flags;
    }
    void publish(const Publication &p) noexcept {
        // Writer-private until published. Metadata only, never XYZD/flag copies.
        if(!writing_||p.records>max_records||p.flags!=p.records) std::abort();
        slots_[active_].publication=p; encode(slots_[active_]);
        std::lock_guard<std::mutex> lock(mutex_);
        latest_=active_; writing_=false;
    }
    const Slot *acquire(uint64_t after=0) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if(leases_==2 || latest_<0 || slots_[latest_].publication.context.frame<=after) return nullptr;
        ++leases_; ++slots_[latest_].readers; return &slots_[latest_];
    }
    void release(const Slot *s) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        for(auto &slot:slots_) if(&slot==s && slot.readers) { --slot.readers; --leases_; return; }
        std::abort();
    }
    void settings(const Config &c, uint64_t generation) noexcept {
        std::lock_guard<std::mutex> lock(mutex_); config_=c; generation_=generation;
    }
    Settings settings() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        Settings s; s.config=config_; s.generation=generation_; s.available=latest_>=0;
        if(s.available) s.latest=slots_[latest_].publication;
        return s;
    }
};
// Constant/static startup storage. No initialization-on-first-use accessor.
inline Pool pool;
struct Lease {
    const Slot *slot;
    explicit Lease(uint64_t after=0) noexcept: slot(pool.acquire(after)) {}
    ~Lease() { if(slot) pool.release(slot); }
    Lease(const Lease&)=delete; Lease &operator=(const Lease&)=delete;
};
}
