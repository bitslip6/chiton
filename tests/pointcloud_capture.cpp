#include "pointcloud_capture.h"
#include <cassert>
#include <cstdio>
#include <dirent.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <cstdlib>
#include <new>

using namespace pointcloud_capture;
static uint16_t *raw,*processed;
static const int16_t *cloud_source=nullptr;
static bool guarded=false;static unsigned allocations=0,source_copies=0;
#ifdef CHITON_CAPTURE_ALLOCATION_GUARD
extern "C" {
void *__real_malloc(size_t);void *__real_calloc(size_t,size_t);void *__real_realloc(void*,size_t);void __real_free(void*);
void *__real_memcpy(void*,const void*,size_t);void *__real_memmove(void*,const void*,size_t);
void *__wrap_malloc(size_t n){allocations+=guarded;return __real_malloc(n);}
void *__wrap_calloc(size_t n,size_t s){allocations+=guarded;return __real_calloc(n,s);}
void *__wrap_realloc(void*p,size_t n){allocations+=guarded;return __real_realloc(p,n);}
void __wrap_free(void*p){allocations+=guarded;__real_free(p);}
static void check_copy(const void *p,size_t n){const auto b=reinterpret_cast<uintptr_t>(p);
    for(auto src:{raw,processed}){const auto s=reinterpret_cast<uintptr_t>(src);source_copies+=guarded&&n&&b<s+sampled_cleanup::pixels*2&&b+n>s;}
    const auto s=reinterpret_cast<uintptr_t>(cloud_source);source_copies+=guarded&&n&&cloud_source&&b<s+40&&b+n>s;}
void *__wrap_memcpy(void*d,const void*s,size_t n){check_copy(s,n);return __real_memcpy(d,s,n);}
void *__wrap_memmove(void*d,const void*s,size_t n){check_copy(s,n);return __real_memmove(d,s,n);}
}
void *operator new(size_t n){if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
#endif
static float gain[sampled_cleanup::pixels],offset[sampled_cleanup::pixels],x[sampled_cleanup::pixels],y[sampled_cleanup::pixels],z[sampled_cleanup::pixels];
static int fault=0,calls=0,syncs=0;
extern "C" {
ssize_t __real_write(int,const void*,size_t);int __real_fsync(int);int __real_close(int);
ssize_t __wrap_write(int fd,const void*p,size_t n){
    if(fault==1)return __real_write(fd,p,std::min<size_t>(n,3));
    if(fault==2&&calls++==0){errno=EINTR;return -1;}
    if(fault==3){errno=ENOSPC;return -1;}
    return __real_write(fd,p,n);
}
int __wrap_fsync(int fd){if((fault==4&&++syncs==1)||(fault==6&&++syncs==12)){errno=EIO;return -1;}return __real_fsync(fd);}
int __wrap_close(int fd){int r=__real_close(fd);if(fault==5&&calls++==0){errno=EIO;return -1;}return r;}
}
static bool cancelled(){return true;}
static void remove_tree(const char *path){DIR*d=opendir(path);assert(d);while(auto*e=readdir(d)){
    if(e->d_name[0]=='.')continue;char p[4096];std::snprintf(p,sizeof(p),"%s/%s",path,e->d_name);assert(unlink(p)==0);}closedir(d);assert(rmdir(path)==0);}
int main(){
    assert(depth_plane_supported(640,960,"depth_ir"));
    assert(!depth_plane_supported(640,480,"depth_ir"));
    assert(depth_plane_supported(640,480,"depth_ir",true));
    assert(depth_plane_supported(640,480,"depth"));
    assert(!depth_plane_supported(640,480,"ir")&&!depth_plane_supported(320,960,"depth_ir"));
    char sha[65];SHA256 empty_hash;empty_hash.hex(sha);assert(std::strcmp(sha,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")==0);
    SHA256 s;s.update("abc",3);s.hex(sha);assert(std::strcmp(sha,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")==0);
    raw=static_cast<uint16_t*>(mmap(nullptr,sampled_cleanup::pixels*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
    processed=static_cast<uint16_t*>(mmap(nullptr,sampled_cleanup::pixels*2,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0));
    assert(raw!=MAP_FAILED&&processed!=MAP_FAILED);
    for(unsigned i=0;i<sampled_cleanup::pixels;++i){raw[i]=processed[i]=1000+i%511;gain[i]=x[i]=1;}
    assert(mprotect(raw,sampled_cleanup::pixels*2,PROT_READ)==0&&mprotect(processed,sampled_cleanup::pixels*2,PROT_READ)==0);
    calibration::runline runs[]={{0,0,4,0,0},{5,0,12,0,0},{639,0,644,0,0}};
    const uint32_t order[]={0,5,9,639,643};int16_t cloud[20];uint8_t flags[5]{};
    for(unsigned k=0;k<5;++k){cloud[k*4]=cloud[k*4+3]=processed[order[k]];cloud[k*4+1]=cloud[k*4+2]=0;}
    cloud_source=cloud;
    Model model{gain,offset,x,y,z};Identity id;id.frame=id.revision=id.captured_at_ms=id.started_ns=1;id.calibrated=true;
    sampled_cleanup::Publication pub;pub.records=5;pub.context.frame=pub.context.revision=1;
    char root[]="/tmp/chiton-capture-unit-XXXXXX";assert(mkdtemp(root));
    for(int mode=0;mode<=8;++mode){
        char path[4096];std::snprintf(path,sizeof(path),"%s/case%d",root,mode);
        {
            Capture cap;assert(cap.prepare(path,30));
            assert(cap.preflight_input(raw,runs,3,model,true));assert(cap.preflight_projection(processed,runs,3,model));
            for(unsigned k=0;k<5;++k)cap.trace.record(k,order[k]);
            if(mode==7)cap.cancelled=cancelled;
            if(mode==8)cap.trace.indices[0]=2;
            calls=syncs=0;fault=mode<=6?mode:0;
            guarded=true;
            const bool result=cap.save(raw,processed,cloud,5,runs,3,model,id,pub,flags);
            guarded=false;assert(allocations==0&&source_copies==0);
            fault=0;assert(result==(mode<=2));
            char manifest[4096],marker[4096];std::snprintf(manifest,sizeof(manifest),"%s/capture.json",path);std::snprintf(marker,sizeof(marker),"%s/INCOMPLETE",path);
            if(!result)assert(access(manifest,F_OK)!=0||access(marker,F_OK)==0);
        }
        // Existing directories are never overwritten.
        Capture second;assert(!second.prepare(path,30));remove_tree(path);
    }
    char path[4096];std::snprintf(path,sizeof(path),"%s/empty",root);
    {
        Capture cap;assert(cap.prepare(path,30));auto empty=pub;empty.records=0;
        assert(cap.save(raw,processed,nullptr,0,nullptr,0,model,id,empty,nullptr));
    }
    remove_tree(path);std::snprintf(path,sizeof(path),"%s/preflight",root);
    {
        Capture cap;assert(cap.prepare(path,1));
        assert(cap.preflight_input(raw,runs,3,model,true));
        assert(mprotect(processed,sampled_cleanup::pixels*2,PROT_READ|PROT_WRITE)==0);
        processed[0]=65535;assert(cap.preflight_projection(processed,runs,3,model));
        assert(cap.pixel_status()[0]==ProjectionRange);processed[0]=raw[0];
        assert(cap.preflight_input(raw,runs,3,model,true));
        offset[0]=std::numeric_limits<float>::quiet_NaN();assert(cap.preflight_projection(processed,runs,3,model));
        assert(cap.pixel_status()[0]==CalibrationNonfinite);offset[0]=0;
        assert(cap.preflight_input(raw,runs,3,model,true));
        x[0]=std::numeric_limits<float>::infinity();assert(cap.preflight_projection(processed,runs,3,model));
        assert(cap.pixel_status()[0]==ProjectionNonfinite);x[0]=1;
        auto missing=model;missing.x=nullptr;assert(!cap.preflight_projection(processed,runs,3,missing));
        cap.trace.capacity=0;cap.trace.record(0,0);assert(!cap.trace.valid);cap.trace.begin();assert(cap.trace.count==0&&cap.trace.valid);
        auto empty=pub;empty.records=0;
        sleep(2);assert(!cap.save(raw,processed,nullptr,0,nullptr,0,model,id,empty,nullptr));
        assert(std::strstr(cap.error,"deadline"));
    }
    remove_tree(path);assert(rmdir(root)==0);
    assert(munmap(raw,sampled_cleanup::pixels*2)==0&&munmap(processed,sampled_cleanup::pixels*2)==0);
    std::puts("pointcloud_capture: hashes, exact order, partial/EINTR/ENOSPC/close/fsync/final commit/cancel/correspondence failures, no overwrite and conversion preflight PASS");
}
