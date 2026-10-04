#include "http_framing.h"
#include <atomic>
#include <cassert>
#include <thread>
#include <unistd.h>

int main() {
    using namespace web::framing;
    const std::string line = "GET /range/setting/123/456 HTTP/1.1\r\nHost: localhost\r\n";
    assert(validate(line + "\r\n").status == 200);
    for (const auto &field : {"Content-Length: 0\r\nContent-Length: 0", "Content-Length: 0,0",
            "Content-Length: -1", "Transfer-Encoding: chunked", "Expect: 100-continue",
            "X : invalid", " folded: value", "Host: duplicate"})
        assert(validate(line + field + "\r\n\r\n").status == 400);
    assert(validate(line + "Content-Length: 999999999999999999999999\r\n\r\n").status == 413);
    for (int mode = 0; mode < 4; ++mode) {
        int sockets[2];
        assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
        std::atomic<bool> running{true};
        Result result;
        std::thread reader([&] { result = receive(sockets[0], [&] { return running.load(); }); });
        const auto bytes = line + "\r\n";
        if (mode == 0) {
            for (char c : bytes) {
                assert(send(sockets[1], &c, 1, MSG_NOSIGNAL) == 1);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        } else {
            assert(send(sockets[1], line.data(), line.size(), MSG_NOSIGNAL) > 0);
            if (mode == 1) shutdown(sockets[1], SHUT_WR);
            if (mode == 2) running = false;
        }
        reader.join();
        assert(result.status == (mode == 0 ? 200 : mode == 3 ? 408 : 0));
        close(sockets[0]); close(sockets[1]);
    }
}
