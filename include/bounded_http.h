#pragma once
#include <string_view>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <algorithm>
#include <sys/socket.h>
#include <poll.h>

// Allocation-free framing front end. Ordinary routes may adapt the validated
// views to legacy owned objects; cleanup routes never enter those helpers.
namespace bounded_http {
using View=std::string_view;
constexpr size_t capacity=16384, line_limit=4096, field_limit=100;
inline bool token(unsigned char c) noexcept {
    return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||
        (c && std::strchr("!#$%&'*+-.^_`|~",c));
}
inline bool equal(View a,View b) noexcept {
    if(a.size()!=b.size()) return false;
    for(size_t i=0;i<a.size();++i) {
        unsigned char c=a[i]; if(c>='A'&&c<='Z') c+='a'-'A';
        if(c!=b[i]) return false;
    }
    return true;
}
struct Request {
    char bytes[capacity];
    size_t size=0, fields=0;
    struct Field { View name,value; } headers[field_limit];
    View line,method,target;
    int status=0;
};
inline int validate(Request &r) noexcept {
    View bytes(r.bytes,r.size);
    auto end=bytes.find("\r\n");
    if(end==View::npos||end>line_limit) return 400;
    View line=bytes.substr(0,end);
    auto a=line.find(' '), b=line.find(' ',a==View::npos?0:a+1);
    if(a==View::npos||b==View::npos||!a||b<=a+1||line.find(' ',b+1)!=View::npos) return 400;
    auto method=line.substr(0,a), target=line.substr(a+1,b-a-1), version=line.substr(b+1);
    for(unsigned char c:method) if(!token(c)) return 400;
    if(target[0]!='/'||target.find('#')!=View::npos||(version!="HTTP/1.1"&&version!="HTTP/1.0")) return 400;
    for(unsigned char c:target) if(c<=32||c>=127) return 400;
    bool length=false,host=false,body=false;
    r.fields=0;
    for(size_t pos=end+2;pos<bytes.size()-2;) {
        auto next=bytes.find("\r\n",pos);
        if(next==View::npos||next-pos>line_limit||r.fields==field_limit) return 400;
        auto field=bytes.substr(pos,next-pos); auto colon=field.find(':');
        if(colon==View::npos||!colon) return 400;
        auto name=field.substr(0,colon), value=field.substr(colon+1);
        for(unsigned char c:name) if(!token(c)) return 400;
        for(unsigned char c:value) if((c<32&&c!='\t')||c==127) return 400;
        auto first=value.find_first_not_of(" \t");
        value=first==View::npos ? View{} : value.substr(first,value.find_last_not_of(" \t")-first+1);
        if(equal(name,"transfer-encoding")||equal(name,"expect")) return 400;
        if(equal(name,"content-length")) {
            if(length||value.empty()) return 400;
            length=true;
            for(unsigned char c:value) { if(c<'0'||c>'9') return 400; body|=c!='0'; }
        }
        if(equal(name,"host")) {
            if(host||value.empty()||value.find_first_of(" \t,/")!=View::npos) return 400;
            host=true;
        }
        r.headers[r.fields++]={name,value}; pos=next+2;
    }
    if(version=="HTTP/1.1"&&!host) return 400;
    if(body) return 413;
    r.line=bytes.substr(0,end+2); r.method=method; r.target=target; return 200;
}
template<class Running> void receive(int fd,Request &r,Running running) noexcept {
    r.size=r.fields=0; r.status=0; r.line=r.method=r.target={};
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(1000);
    while(running()) {
        const auto left=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
        if(left<=0) { r.status=408; return; }
        pollfd p{fd,POLLIN,0}; const int ready=poll(&p,1,int(std::min<int64_t>(left,50)));
        if(ready<0&&errno==EINTR) continue;
        if(ready<0||(p.revents&POLLNVAL)) return;
        if(!ready) continue;
        auto n=recv(fd,r.bytes+r.size,std::min<size_t>(1024,capacity-r.size),MSG_DONTWAIT);
        if(!n) return;
        if(n<0) { if(errno==EINTR||errno==EAGAIN||errno==EWOULDBLOCK) continue; return; }
        r.size+=size_t(n);
        for(size_t i=0;i<r.size;++i) {
            const unsigned char c=r.bytes[i];
            if((c<32&&c!='\r'&&c!='\n'&&c!='\t')||c==127||
               (c=='\n'&&(!i||r.bytes[i-1]!='\r'))||
               (c=='\r'&&i+1<r.size&&r.bytes[i+1]!='\n')) { r.status=400; return; }
        }
        View bytes(r.bytes,r.size); auto end=bytes.find("\r\n\r\n");
        if(end!=View::npos) { r.status=end+4!=r.size ? 400 : running()?validate(r):0; return; }
        if(r.size==capacity) { r.status=431; return; }
    }
}
struct Buffer {
    char bytes[4096]; size_t size=0; bool valid=true;
    void add(View v) noexcept {
        if(!valid) return;
        if(v.size()>sizeof(bytes)-size) { valid=false; return; }
        for(char c:v) bytes[size++]=c;
    }
    void number(uint64_t n) noexcept {
        if(!valid) return;
        char digits[20]; // UINT64_MAX has 20 decimal digits; no terminator.
        size_t count=0;
        do { digits[count++]=char('0'+n%10); n/=10; } while(n);
        if(count>sizeof(bytes)-size) { valid=false; return; }
        while(count) bytes[size++]=digits[--count];
    }
};
class Writer {
    int fd_; std::chrono::steady_clock::time_point deadline_;
public:
    explicit Writer(int fd):fd_(fd),deadline_(std::chrono::steady_clock::now()+std::chrono::seconds(2)) {}
    bool send(const void *data,size_t size) noexcept {
        auto *bytes=static_cast<const char*>(data); size_t pos=0;
        while(pos<size) {
            const auto left=std::chrono::duration_cast<std::chrono::milliseconds>(deadline_-std::chrono::steady_clock::now()).count();
            if(left<=0) return false;
            auto n=::send(fd_,bytes+pos,size-pos,MSG_NOSIGNAL|MSG_DONTWAIT);
            if(n>0) { pos+=size_t(n); continue; }
            if(!n) return false;
            if(errno==EINTR) continue;
            if(errno!=EAGAIN&&errno!=EWOULDBLOCK) return false;
            pollfd p{fd_,POLLOUT,0}; auto ready=poll(&p,1,int(left));
            if(ready<0&&errno==EINTR) continue;
            if(ready<=0||(p.revents&(POLLERR|POLLHUP|POLLNVAL))) return false;
        }
        return true;
    }
    bool header(int status,View type,size_t bytes,bool post=false) noexcept {
        Buffer b; b.add("HTTP/1.1 "); b.number(status);
        b.add(status==200?" OK":status==400?" Bad Request":status==405?" Method Not Allowed":
            status==403?" Forbidden":status==409?" Conflict":status==413?" Payload Too Large":status==431?" Request Header Fields Too Large":
            status==408?" Request Timeout":status==504?" Gateway Timeout":status==404?" Not Found":" Service Unavailable");
        b.add("\r\nContent-Type: "); b.add(type); b.add("\r\nContent-Length: "); b.number(bytes);
        b.add("\r\nCache-Control: no-store\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n");
        if(status==405) b.add(post?"Allow: GET, POST\r\n":"Allow: GET\r\n");
        b.add("\r\n"); return b.valid&&send(b.bytes,b.size);
    }
    void error(int status,View message,bool post=false) noexcept {
        Buffer b; b.add("{\"error\":\""); b.add(message); b.add("\",\"status\":"); b.number(status); b.add("}");
        if(b.valid&&header(status,"application/json; charset=utf-8",b.size,post)) send(b.bytes,b.size);
    }
};
inline bool integer(View v,uint64_t &out,uint64_t maximum=UINT64_MAX) noexcept {
    if(v.empty()) return false;
    uint64_t n=0;
    for(unsigned char c:v) { if(c<'0'||c>'9'||n>maximum/10||(n==maximum/10&&unsigned(c-'0')>maximum%10)) return false; n=n*10+c-'0'; }
    out=n; return true;
}
}
