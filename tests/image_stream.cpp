#include "image_stream.h"
#include <cassert>
#include <atomic>
#include <thread>
#include <cstdlib>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>

static unsigned char *jpeg() {
    auto p=static_cast<unsigned char *>(malloc(4));
    p[0]=255;p[1]=216;p[2]=255;p[3]=217;return p;
}
int main() {
    assert(web::websocket_accept("dGhlIHNhbXBsZSBub25jZQ==")=="s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
    assert(web::websocket_accept("bad").empty());
    assert(web::websocket_accept("dGhlIHNhbXBsZSBub25jZR==").empty());
    web::JpegHub hub;
    measurement::FrameMetadata m; m.id=1; m.received_steady_ms=web::stream_clock_ms();
    hub.publish(m,640,480,jpeg(),4,1,2,3);
    auto pinned=hub.snapshot(); assert(pinned && pinned->metadata.id==1);
    m.id=2;hub.publish(m,640,480,jpeg(),4,1,2,3);
    assert(pinned->metadata.id==1 && pinned->bytes.get()[0]==255);
    assert(hub.snapshot()->metadata.id==2);
    std::atomic<bool> stop{false};
    std::thread reader([&]{ while(!stop) {auto f=hub.snapshot(); if(f) assert(f->size==4 && f->bytes.get()[1]==216);} });
    for(int i=0;i<1000;++i) {m.id++;hub.publish(m,640,480,jpeg(),4,1,2,3);}
    stop=true;reader.join();
    hub.publish(m,640,480,nullptr,0,1,2,0);assert(!hub.snapshot());
    assert(hub.metrics()["acquired_frames"]==1003);
    web::jpeg_hub().publish(m,640,480,jpeg(),4,1,2,3);
    std::atomic<bool> running{true};
    {
        web::ImageStreamServer server([&]{return web::StreamDeviceState{running,true,1};});
        std::vector<int> peers;
        const auto request=web::framing::validate("GET /stream/camera HTTP/1.1\r\nHost: test\r\nUpgrade: websocket\r\nConnection: keep-alive, Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Protocol: chiton.jpeg.v1\r\nOrigin: http://test\r\n\r\n");
        assert(request.status==200);
        for(int i=0;i<5;++i) {
            int pair[2];assert(socketpair(AF_UNIX,SOCK_STREAM,0,pair)==0);
            server.upgrade(pair[0],request);
            pollfd fd{pair[1],POLLIN,0};assert(poll(&fd,1,1000)==1);
            char buffer[4096];auto n=recv(pair[1],buffer,sizeof(buffer),0);assert(n>0);
            auto response=std::string(buffer,n);
            assert(response.find(i<4 ? "101 Switching Protocols" : "503 Stream Rejected")!=std::string::npos);
            peers.push_back(pair[1]);
        }
        assert(server.metrics()["clients"]==4);
        for(int fd:peers) close(fd);
        for(int i=0;i<100 && server.metrics()["clients"]!=0;++i) std::this_thread::sleep_for(std::chrono::milliseconds(5));
        assert(server.metrics()["clients"]==0);
        running=false;
    }
}
