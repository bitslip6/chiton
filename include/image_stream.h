#pragma once
#include "measurement.h"
#include "json.hpp"
#include "http_framing.h"
#include <functional>
#include <memory>
#include <mutex>
#include <deque>
#include <string>

namespace web {
constexpr size_t jpeg_stream_limit = 2 * 1024 * 1024;
struct JpegSnapshot {
    measurement::FrameMetadata metadata;
    int width = 0, height = 0;
    std::shared_ptr<const unsigned char> bytes;
    size_t size = 0;
};
// Independent of camera ownership. Readers pin immutable encoded buffers, not
// raw camera memory. publish takes ownership of jpeg_mem_dest's malloc buffer.
class JpegHub {
    mutable std::mutex mutex;
    std::shared_ptr<const JpegSnapshot> latest;
    struct Sample { uint64_t at; bool jpeg; double acquire, process, encode; size_t bytes; };
    std::deque<Sample> samples;
    uint64_t acquired = 0, published = 0;
public:
    void publish(const measurement::FrameMetadata &, int width, int height,
                 unsigned char *owned, size_t size, double acquire_ms,
                 double process_ms, double encode_ms);
    std::shared_ptr<const JpegSnapshot> snapshot() const;
    nlohmann::json metrics(bool diagnostics = true) const;
};
JpegHub &jpeg_hub();
uint64_t stream_clock_ms();
std::string websocket_accept(const std::string &key);
struct StreamDeviceState { bool running, enabled; int mode; };
// One bounded reactor, four viewers maximum, one unacknowledged frame/viewer.
// No socket operation or browser acknowledgement holds camera/hub ownership.
class ImageStreamServer {
    struct Impl;
    std::unique_ptr<Impl> impl;
public:
    explicit ImageStreamServer(std::function<StreamDeviceState()> state);
    ~ImageStreamServer();
    void upgrade(int socket, const framing::Result &request); // Always owns socket.
    nlohmann::json metrics() const;
};
}
