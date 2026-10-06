#include "image_stream.h"
#include "performance.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>
#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>

namespace web {
uint64_t stream_clock_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
JpegHub &jpeg_hub() { static JpegHub hub; return hub; }
void JpegHub::publish(const measurement::FrameMetadata &meta, int w, int h,
                      unsigned char *owned, size_t size, double a, double p, double e) {
    std::shared_ptr<const unsigned char> bytes(owned, [](const unsigned char *p) { free(const_cast<unsigned char *>(p)); });
    const bool valid = owned && size >= 2 && size <= jpeg_stream_limit && owned[0] == 255 && owned[1] == 216;
    std::shared_ptr<const JpegSnapshot> frame;
    if (valid) frame = std::make_shared<JpegSnapshot>(JpegSnapshot{meta, w, h, bytes, size});
    std::lock_guard<std::mutex> lock(mutex);
    latest = std::move(frame);
    ++acquired;
    if (valid) ++published;
    samples.push_back({stream_clock_ms(), valid, a, p, e, valid ? size : 0});
    while (samples.size() > 120 || (samples.size() > 1 && samples.front().at + 2000 < samples.back().at)) samples.pop_front();
}
std::shared_ptr<const JpegSnapshot> JpegHub::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex); return latest;
}
nlohmann::json JpegHub::metrics(bool diagnostics) const {
    // OS reads must not hold hub ownership or block camera publication.
    const auto system = diagnostics ? performance::system_metrics() : nlohmann::json(nullptr);
    const auto stages = diagnostics ? performance::pipeline().metrics() : nlohmann::json(nullptr);
    std::lock_guard<std::mutex> lock(mutex);
    double a=0, p=0, e=0; size_t bytes=0, jpegs=0;
    for (const auto &s : samples) { a+=s.acquire; p+=s.process; e+=s.encode; bytes+=s.bytes; jpegs+=s.jpeg; }
    const double n = samples.size();
    const auto now = stream_clock_ms();
    const bool fresh = n && now - samples.back().at < 2000;
    const double seconds = n > 1 ? (samples.back().at - samples.front().at)/1000.0 : 0;
    const double fps = fresh && seconds > 0 ? (n-1)/seconds : 0;
    return {{"acquired_frames", acquired}, {"jpeg_frames", published},
        {"acquired_fps", fps}, {"jpeg_fps", n ? fps*jpegs/n : 0},
        {"acquire_ms", n ? a/n : 0}, {"process_ms", n ? p/n : 0}, {"encode_ms", n ? e/n : 0},
        {"jpeg_bytes", jpegs ? bytes/jpegs : 0}, {"last_publication_age_ms", n ? now-samples.back().at : 0},
        {"window_ms", 2000}, {"timestamp_kind", "host_receipt_not_exposure"},
        {"stages", stages}, {"system", system}};
}
namespace {
const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
std::string base64(const unsigned char *p, size_t n) {
    std::string out;
    for (size_t i=0; i<n; i+=3) {
        unsigned v=unsigned(p[i])<<16;
        if (i+1<n) v|=unsigned(p[i+1])<<8;
        if (i+2<n) v|=p[i+2];
        out+=alphabet[(v>>18)&63]; out+=alphabet[(v>>12)&63];
        out+=i+1<n ? alphabet[(v>>6)&63] : '='; out+=i+2<n ? alphabet[v&63] : '=';
    }
    return out;
}
uint32_t rol(uint32_t x, unsigned n) { return (x<<n)|(x>>(32-n)); }
std::string packet(unsigned opcode, const std::string &payload) {
    std::string out(1, char(0x80|opcode));
    const uint64_t n=payload.size();
    if (n<126) out+=char(n);
    else if (n<=65535) { out+=char(126); out+=char(n>>8); out+=char(n); }
    else { out+=char(127); for (int i=7;i>=0;--i) out+=char(n>>(8*i)); }
    return out+payload;
}
std::string lower(std::string s) { for (auto &c:s) if(c>='A' && c<='Z') c+='a'-'A'; return s; }
bool has_token(const std::string &s, const std::string &token, bool fold = true) {
    size_t pos=0;
    while(pos<s.size()) { auto end=s.find(',',pos); if(end==std::string::npos) end=s.size();
        auto part=s.substr(pos,end-pos); auto first=part.find_first_not_of(" \t");
        if(first!=std::string::npos) {
            auto value=part.substr(first,part.find_last_not_of(" \t")-first+1);
            if((fold ? lower(value) : value)==token) return true;
        }
        pos=end+1;
    }
    return false;
}
bool header_send(int fd, const std::string &s) {
    size_t at=0; auto deadline=stream_clock_ms()+100;
    while(at<s.size() && stream_clock_ms()<deadline) {
        auto n=send(fd,s.data()+at,s.size()-at,MSG_NOSIGNAL|MSG_DONTWAIT);
        if(n>0) at+=n;
        else if(n<0 && (errno==EINTR || errno==EAGAIN || errno==EWOULDBLOCK)) { pollfd p{fd,POLLOUT,0}; poll(&p,1,5); }
        else return false;
    }
    return at==s.size();
}
}
// SHA-1 is used only for RFC6455's handshake, not authentication/signatures.
std::string websocket_accept(const std::string &key) {
    if(key.size()!=24 || key.substr(22)!="==") return "";
    for(size_t i=0;i<22;++i) if(!strchr(alphabet,key[i]) || !key[i]) return "";
    if((strchr(alphabet,key[21])-alphabet)&15) return "";
    std::string input=key+"258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    const uint64_t bits=input.size()*8;
    input+=char(128); while(input.size()%64!=56) input+=char(0);
    for(int i=7;i>=0;--i) input+=char(bits>>(i*8));
    uint32_t h[5]={0x67452301,0xefcdab89,0x98badcfe,0x10325476,0xc3d2e1f0};
    for(size_t off=0;off<input.size();off+=64) {
        uint32_t w[80];
        for(int i=0;i<16;++i) { w[i]=0; for(int j=0;j<4;++j) w[i]=(w[i]<<8)|static_cast<unsigned char>(input[off+4*i+j]); }
        for(int i=16;i<80;++i) w[i]=rol(w[i-3]^w[i-8]^w[i-14]^w[i-16],1);
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4];
        for(int i=0;i<80;++i) {
            uint32_t f,k;
            if(i<20) { f=(b&c)|(~b&d); k=0x5a827999; }
            else if(i<40) { f=b^c^d; k=0x6ed9eba1; }
            else if(i<60) { f=(b&c)|(b&d)|(c&d); k=0x8f1bbcdc; }
            else { f=b^c^d; k=0xca62c1d6; }
            uint32_t t=rol(a,5)+f+e+k+w[i]; e=d; d=c; c=rol(b,30); b=a; a=t;
        }
        h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;
    }
    unsigned char result[20]; for(int i=0;i<5;++i) for(int j=0;j<4;++j) result[4*i+j]=h[i]>>(24-8*j);
    return base64(result,20);
}
struct ImageStreamServer::Impl {
    struct Client {
        int fd; std::string rx, header, pong;
        std::shared_ptr<const JpegSnapshot> frame;
        size_t offset=0; uint64_t last=0, waiting=0, deadline=0, started=0, status_at=0, credit_at=0, rx_at=0;
        unsigned controls=0;
    };
    std::function<StreamDeviceState()> state;
    std::atomic<bool> stopping{false};
    mutable std::mutex gate;
    std::vector<int> pending;
    size_t slots=0;
    uint64_t sent=0, skipped=0, slow=0, byte_count=0, acknowledgements=0;
    double send_ms=0, ack_ms=0;
    std::thread worker;
    explicit Impl(std::function<StreamDeviceState()> fn):state(std::move(fn)),worker([this]{loop();}){}
    ~Impl() { stopping=true; worker.join(); }
    void release(int fd) { shutdown(fd,SHUT_RDWR); close(fd); std::lock_guard<std::mutex> l(gate); --slots; }
    bool input(Client &c) {
        char data[256];
        auto n=recv(c.fd,data,sizeof(data),MSG_DONTWAIT);
        if(n==0) return false;
        if(n<0) return errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR;
        c.rx.append(data,n); if(c.rx.size()>512) return false;
        while(c.rx.size()>=2) {
            auto b=static_cast<unsigned char>(c.rx[0]), length=static_cast<unsigned char>(c.rx[1]);
            if((b&0xf0)!=0x80 || !(length&128) || (length&127)>125) return false;
            size_t size=length&127;
            if(c.rx.size()<6+size) break;
            const auto now=stream_clock_ms();
            if(now-c.rx_at>=1000) {c.rx_at=now;c.controls=0;}
            if(++c.controls>256) return false;
            std::string payload=c.rx.substr(6,size);
            for(size_t i=0;i<size;++i) payload[i]^=c.rx[2+i%4];
            c.rx.erase(0,6+size);
            switch(b&15) {
                case 1:
                    if(!c.waiting || payload!=std::to_string(c.waiting)) return false;
                    c.waiting=0; {std::lock_guard<std::mutex> l(gate); ++acknowledgements; ack_ms=stream_clock_ms()-c.credit_at;} break;
                case 8:
                    // Never inject control bytes into a partially sent frame.
                    if(c.header.empty() && size!=1) {
                        auto reply=packet(8,payload);
                        send(c.fd,reply.data(),reply.size(),MSG_NOSIGNAL|MSG_DONTWAIT);
                    }
                    return false;
                case 9: if(!c.pong.empty()) return false; c.pong=packet(10,payload); break;
                case 10: break;
                default: return false;
            }
        }
        return true;
    }
    void begin_frame(Client &c, std::shared_ptr<const JpegSnapshot> frame, uint64_t now) {
        const auto &m=frame->metadata;
        const std::string meta=nlohmann::json{{"version",1},{"frame_id",m.id},{"mode",m.color_mode},
            {"age_ms",now>=m.received_steady_ms ? now-m.received_steady_ms : 0},
            {"captured_at_ms",m.captured_at_ms},{"configuration_revision",m.configuration_revision},
            {"width",frame->width},{"height",frame->height},{"source_units",m.units()},
            {"units","image_intensity"},{"units_scope",m.calibrated ? "calibration_aoi" : "full_sensor"},
            {"frame_type",m.depth ? "depth" : "ir"},{"smoothed",m.smoothed},{"plugin",m.plugin},
            {"timestamp_kind","host_receipt_not_exposure"}}.dump();
        const uint64_t size=4+meta.size()+frame->size;
        c.header=std::string(1,char(0x82));
        if(size<126) c.header+=char(size);
        else if(size<=65535) { c.header+=char(126); c.header+=char(size>>8); c.header+=char(size); }
        else { c.header+=char(127); for(int i=7;i>=0;--i) c.header+=char(size>>(8*i)); }
        for(int i=3;i>=0;--i) c.header+=char(meta.size()>>(8*i));
        c.header+=meta;
        if(c.last && m.id>c.last+1) { std::lock_guard<std::mutex> l(gate); skipped+=m.id-c.last-1; }
        c.last=m.id; c.waiting=m.id; c.deadline=now+2000; c.started=now; c.credit_at=now;
        c.frame=std::move(frame); c.offset=0;
    }
    bool output(Client &c, uint64_t now) {
        if(c.header.empty()) return true;
        if(now-c.started>250) { std::lock_guard<std::mutex> l(gate); ++slow; return false; }
        // A fixed per-client iteration budget prevents a writable viewer from
        // monopolizing the reactor. No copying/concatenating JPEG per viewer.
        for(int attempt=0;attempt<8;++attempt) {
            const auto total=c.header.size()+(c.frame ? c.frame->size : 0);
            if(c.offset==total) {
                if(c.frame) { std::lock_guard<std::mutex> l(gate); ++sent; byte_count+=c.frame->size; send_ms=stream_clock_ms()-c.started; }
                c.header.clear(); c.frame.reset(); c.offset=0; return true;
            }
            const bool head=c.offset<c.header.size();
            const char *data=head ? c.header.data()+c.offset : reinterpret_cast<const char *>(c.frame->bytes.get())+c.offset-c.header.size();
            const auto size=head ? c.header.size()-c.offset : total-c.offset;
            auto n=send(c.fd,data,std::min<size_t>(size,65536),MSG_NOSIGNAL|MSG_DONTWAIT);
            if(n>0) c.offset+=n;
            else if(n<0 && (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR)) return true;
            else return false;
        }
        return true;
    }
    void loop() {
        std::vector<Client> clients; clients.reserve(4);
        while(!stopping && state().running) {
            {std::lock_guard<std::mutex> l(gate); for(int fd:pending) clients.push_back(Client{fd}); pending.clear();}
            const auto device=state(); const auto now=stream_clock_ms(); auto frame=jpeg_hub().snapshot();
            for(auto it=clients.begin();it!=clients.end();) {
                bool keep=input(*it);
                if(it->waiting && now>it->deadline) { std::lock_guard<std::mutex> l(gate); ++slow; keep=false; }
                if(keep && it->header.empty()) {
                    if(!it->pong.empty()) { it->header=std::move(it->pong); it->pong.clear(); it->started=now; }
                    else if(!it->waiting && device.enabled && frame && frame->metadata.color_mode==device.mode && frame->metadata.id!=it->last) begin_frame(*it,frame,now);
                    else if(!it->waiting && now-it->status_at>=500) {
                        auto status=nlohmann::json{{"status","waiting"},{"mode",device.mode},{"capture_enabled",device.enabled},
                            {"pipeline",jpeg_hub().metrics(false)}}.dump();
                        it->header=packet(1,status); it->started=now; it->status_at=now;
                    }
                }
                if(keep) keep=output(*it,now);
                if(!keep) { release(it->fd); it=clients.erase(it); } else ++it;
            }
            std::array<pollfd,4> fds{}; size_t count=0;
            for(const auto &c:clients) fds[count++]={c.fd,short(POLLIN | (!c.header.empty() ? POLLOUT : 0)),0};
            poll(fds.data(),count,5);
        }
        for(auto &c:clients) release(c.fd);
        std::vector<int> rest; {std::lock_guard<std::mutex> l(gate); rest.swap(pending);}
        for(int fd:rest) release(fd);
    }
};
ImageStreamServer::ImageStreamServer(std::function<StreamDeviceState()> state):impl(new Impl(std::move(state))){}
ImageStreamServer::~ImageStreamServer()=default;
void ImageStreamServer::upgrade(int fd, const framing::Result &request) {
    const auto field=[&](const std::string &key) { auto it=request.headers.find(key); return it==request.headers.end() ? std::string() : it->second; };
    int status=400;
    std::string accept=websocket_accept(field("sec-websocket-key"));
    const auto host=field("host"), origin=field("origin");
    bool valid=request.method=="GET" && request.line.find(" HTTP/1.1\r\n")!=std::string::npos &&
        lower(field("upgrade"))=="websocket" && has_token(field("connection"),"upgrade") &&
        field("sec-websocket-version")=="13" && has_token(field("sec-websocket-protocol"),"chiton.jpeg.v1",false) && !accept.empty();
    const char *disabled=getenv("CHITON_DISABLE_IMAGE_STREAM");
    if(valid && disabled && std::string(disabled)=="1") {valid=false;status=503;}
    if(valid && !origin.empty() && origin!="http://"+host && origin!="https://"+host) { valid=false; status=403; }
    if(valid) { std::lock_guard<std::mutex> l(impl->gate); if(impl->slots>=4 || impl->stopping || !impl->state().running) { valid=false;status=503; } else ++impl->slots; }
    if(!valid) {
        std::string body="{\"error\":\"stream upgrade rejected\"}";
        header_send(fd,"HTTP/1.1 "+std::to_string(status)+" Stream Rejected\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: "+std::to_string(body.size())+"\r\n\r\n"+body);
        close(fd); return;
    }
    fcntl(fd,F_SETFD,FD_CLOEXEC);
    int size=65536; setsockopt(fd,SOL_SOCKET,SO_SNDBUF,&size,sizeof(size));
    int no_delay=1; setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&no_delay,sizeof(no_delay));
    if(!header_send(fd,"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: "+accept+"\r\nSec-WebSocket-Protocol: chiton.jpeg.v1\r\n\r\n")) { impl->release(fd); return; }
    {std::lock_guard<std::mutex> l(impl->gate); if(impl->stopping || !impl->state().running) { --impl->slots;close(fd); } else impl->pending.push_back(fd);}
}
nlohmann::json ImageStreamServer::metrics() const {
    std::lock_guard<std::mutex> l(impl->gate);
    return {{"pipeline",jpeg_hub().metrics()},{"clients",impl->slots},{"client_limit",4},
        {"sent_frames",impl->sent},{"acknowledged_frames",impl->acknowledgements},
        {"skipped_sensor_frames",impl->skipped},{"slow_disconnects",impl->slow},
        {"jpeg_bytes_sent",impl->byte_count},{"last_send_ms",impl->send_ms},{"last_ack_ms",impl->ack_ms},
        {"max_jpeg_bytes",jpeg_stream_limit},{"send_deadline_ms",250},{"ack_deadline_ms",2000}};
}
}
