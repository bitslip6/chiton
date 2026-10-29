// Host-only saved-frame experiments; never opens a camera. Build with:
// c++ -O2 -std=c++17 -Iinclude tools/aggressive_cleanup_replay.cpp -o replay
#include "sampled_boundary_cleanup.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
using namespace sampled_cleanup;
template<class T> std::vector<T> load(const std::string &p) {
    std::ifstream f(p,std::ios::binary|std::ios::ate); if(!f) throw std::runtime_error(p);
    const auto n=f.tellg(); if(n<0||size_t(n)%sizeof(T)) throw std::runtime_error("bad size: "+p);
    std::vector<T> a(size_t(n)/sizeof(T)); f.seekg(0);
    if(!f.read(reinterpret_cast<char*>(a.data()),n)) throw std::runtime_error("read: "+p); return a;
}
// Deliberately simple checked prototype, independent of production dispatch.
struct Votes { unsigned pairs=0, usable=0, nearer=0, farther=0; };
Votes votes(const uint16_t *src,unsigned i,const uint8_t *domain,const Config &c,unsigned radius) {
    Votes v;const int x=i%width,y=i/width,center=src[i];
    if(!center||!evidence(center,c)||!(domain[i]&domain_member))return v;
    constexpr int dirs[4][2]={{1,0},{0,1},{1,1},{1,-1}};
    for(unsigned r=1;r<=radius;++r)for(auto &d:dirs){
        const int ax=x-int(r)*d[0],ay=y-int(r)*d[1],bx=x+int(r)*d[0],by=y+int(r)*d[1];
        const bool av=location(domain,ax,ay),bv=location(domain,bx,by);
        const int a=av?src[ay*width+ax]:0,b=bv?src[by*width+bx]:0;
        const bool au=av&&a&&evidence(a,c),bu=bv&&b&&evidence(b,c);
        if(au&&bu){const int n=std::min(a,b),f=std::max(a,b);
            v.pairs+=f-n>c.boundary_gap&&center-n>c.center_margin&&f-center>c.center_margin;}
        if(r>=radius-2)for(int z=0;z<2;++z){
            if(z?bu:au){const int diff=(z?b:a)-center;++v.usable;
                v.nearer+=-diff>c.boundary_gap&&-diff>c.center_margin;
                v.farther+=diff>c.boundary_gap&&diff>c.center_margin;}
        }
    }return v;
}
int main(int argc,char **argv)try{
    if(argc!=3)throw std::runtime_error("usage: replay CAPTURE EXISTING_NEW_OUTPUT_DIR");
    const std::string root=argv[1],out=argv[2];
    auto src=load<uint16_t>(root+"/processed-depth.u16le");auto status=load<uint8_t>(root+"/pixel-status.u8");
    auto indices=load<uint32_t>(root+"/source-index.u32le");
    if(src.size()!=pixels||status.size()!=pixels)throw std::runtime_error("frame size");
    std::vector<uint8_t> domain(pixels);for(unsigned i=0;i<pixels;++i)domain[i]=status[i]==1?0:domain_member;
    mark_interior(domain.data());
    Config c;c.enabled=true;c.boundary_gap=120;c.anchor_tolerance=20;c.center_margin=30;c.ramp_residual=15;c.surface_support=1;c.center_support=50;
    std::vector<uint8_t> baseline;for(auto i:indices){if(i>=pixels)throw std::runtime_error("index");baseline.push_back(uint8_t(classify_sample(src.data(),i,domain.data(),c).flag));}
    std::ofstream(out+"/center50.u8",std::ios::binary).write(reinterpret_cast<char*>(baseline.data()),baseline.size());
    std::cout<<"center50 mixed="<<std::count(baseline.begin(),baseline.end(),8)<<" records="<<indices.size()<<'\n';
    // Production must implement the selected prototype, not merely accept ID 3.
    Config aggressive=c;aggressive.algorithm=Algorithm::AggressiveBoundaryBands;
    for(auto i:indices){const auto v=votes(src.data(),i,domain.data(),c,6);
        const auto actual=classify_sample(src.data(),i,domain.data(),aggressive);
        assert((actual.flag==Flag::MixedCandidate)==bool(v.pairs));assert(!actual.early_retained);}
    std::cout<<"production/reference rejection parity: every saved record PASS\n";
    Config defaults;defaults.enabled=true;defaults.algorithm=Algorithm::AggressiveBoundaryBands;
    std::vector<uint8_t> default_flags;for(auto i:indices)default_flags.push_back(uint8_t(classify_sample(src.data(),i,domain.data(),defaults).flag));
    std::ofstream(out+"/aggressive-default.u8",std::ios::binary).write(reinterpret_cast<char*>(default_flags.data()),default_flags.size());
    std::cout<<"aggressive defaults gap200 margin60 mixed="<<std::count(default_flags.begin(),default_flags.end(),8)<<'\n';
    auto cpu=[](){timespec t{};if(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t))throw std::runtime_error("clock");return uint64_t(t.tv_sec)*1000000000+uint64_t(t.tv_nsec);};
    volatile unsigned checksum=0;constexpr unsigned repeats=300;
    for(unsigned mode=0;mode<3;++mode){Config timed=c;if(mode==2)timed.algorithm=Algorithm::AggressiveBoundaryBands;
        const auto start=cpu();for(unsigned n=0;n<repeats;++n)for(auto i:indices)checksum+=mode?unsigned(classify_sample(src.data(),i,domain.data(),timed).flag):0;
        const auto elapsed=cpu()-start;
        std::cout<<"arm2 kernel-only mode="<<mode<<" mean_cpu_ms="<<double(elapsed)/1e6/repeats<<" repeats="<<repeats<<" checksum="<<checksum<<'\n';}
    for(unsigned radius:{4u,6u})for(unsigned minimum:{1u,2u})for(unsigned island_percent:{0u,75u,88u}){
        unsigned bracket=0,island=0,overlap=0,total=0,added=0,lost=0;std::vector<uint8_t> flags;
        for(unsigned k=0;k<indices.size();++k){const auto v=votes(src.data(),indices[k],domain.data(),c,radius);
            const bool b=v.pairs>=minimum,s=island_percent&&v.usable>=12&&100*std::max(v.nearer,v.farther)>=island_percent*v.usable;
            bracket+=b;island+=s;overlap+=b&&s;total+=b||s;added+=(b||s)&&baseline[k]!=8;lost+=!b&&!s&&baseline[k]==8;
            flags.push_back(b||s?8:1);
        }
        const std::string name="r"+std::to_string(radius)+"-v"+std::to_string(minimum)+"-island"+std::to_string(island_percent);
        std::ofstream(out+"/"+name+".u8",std::ios::binary).write(reinterpret_cast<char*>(flags.data()),flags.size());
        std::cout<<name<<" bracket="<<bracket<<" island="<<island<<" overlap="<<overlap<<" mixed="<<total<<" added="<<added<<" lost_baseline="<<lost<<'\n';
    }
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
