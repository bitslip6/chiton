// Focused tests against production helpers. Build with tests/run_safety.sh.
#include "system.h"
#include "webserver.h"
#include "image.h"
#include "camera.h"
#include "request_validation.h"
#include "rover.h"
#include <cassert>
#include <chrono>
#include <fstream>
#include <memory>
#include <thread>
#include <sys/socket.h>
#include <unistd.h>

GlobalOpts *GOPTS = nullptr;

template<class F> void rejects(F fn) {
    bool rejected = false;
    try { fn(); } catch (const std::exception &) { rejected = true; }
    assert(rejected);
}

int main() {
    // UART packets contain six payload bytes followed by the checksum.
    int pipe_fds[2];
    assert(pipe(pipe_fds) == 0);
    rover::tx_uart(pipe_fds[1], rover::tx_data{10, 20, 30, 40});
    close(pipe_fds[1]);
    unsigned char packet[8]{};
    assert(read(pipe_fds[0], packet, sizeof(packet)) == 7);
    const unsigned char expected[] = {253, 10, 20, 0, 30, 40, 155};
    assert(std::equal(std::begin(expected), std::end(expected), packet));
    assert(read(pipe_fds[0], packet, sizeof(packet)) == 0);
    close(pipe_fds[0]);

    auto large_fonts = init_large_fonts();
    assert(large_fonts[10].x == 0 && large_fonts[10].y == 200);
    assert(large_fonts[36].height == 30 && large_fonts[36].yoffset == -1);
    auto small_fonts = init_small_fonts();
    assert(small_fonts['A'].x == 0 && small_fonts['A'].y == 22);
    assert(small_fonts['B'].x == 6 && small_fonts['B'].width == 6);

    for (size_t size : {0u, 1u, 1499u, 1500u, 1501u, 1024u * 1024u}) {
        int sockets[2];
        assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
        int buffer = 1024;
        setsockopt(sockets[0], SOL_SOCKET, SO_SNDBUF, &buffer, sizeof(buffer));
        std::vector<unsigned char> input(size, 0xa7), output;
        std::thread reader([&] {
            unsigned char chunk[137];
            ssize_t n;
            while ((n = read(sockets[1], chunk, sizeof(chunk))) > 0)
                output.insert(output.end(), chunk, chunk + n);
        });
        if (size == 1501)
            assert(web::send_headers(std::string(input.begin(), input.end()), sockets[0]));
        else
            assert(web::buffered_write(sockets[0], input.data(), input.size()));
        shutdown(sockets[0], SHUT_WR);
        reader.join();
        assert(input == output);
        close(sockets[0]); close(sockets[1]);
    }
    int sockets[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    close(sockets[1]);
    unsigned char byte = 1;
    assert(!web::buffered_write(sockets[0], &byte, 1));
    close(sockets[0]);
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    int buffer = 1024;
    setsockopt(sockets[0], SOL_SOCKET, SO_SNDBUF, &buffer, sizeof(buffer));
    std::vector<unsigned char> large(1024 * 1024);
    const auto start = std::chrono::steady_clock::now();
    assert(!web::buffered_write(sockets[0], large.data(), large.size()));
    assert(std::chrono::steady_clock::now() - start < std::chrono::seconds(4));
    close(sockets[0]); close(sockets[1]);

    char filename[] = "/tmp/chiton-asset-XXXXXX";
    int fd = mkstemp(filename);
    assert(fd >= 0);
    assert(write(fd, "actual asset", 12) == 12);
    close(fd);
    int size = 99;
    std::unique_ptr<uint8_t[]> bytes(get_data_u8(filename, &size));
    assert(size == 12 && std::string((char *)bytes.get(), size) == "actual asset");
    std::unique_ptr<uint16_t[]> words(get_data_u16(filename, &size));
    assert(words && size == 12);
    { std::ofstream file(filename, std::ios::binary | std::ios::trunc); file << 'x'; }
    words.reset(get_data_u16(filename, &size));
    assert(!words && size == 0);
    unlink(filename);
    size = 99;
    bytes.reset(get_data_u8(filename, &size));
    assert(!bytes && size == 0);

    GlobalOpts options;
    GOPTS = &options;
    options.color_mode = 1;
    assert(web::url_point_to_2d({"639", "479"})->x == 639);
    rejects([] { web::url_point_to_2d({"640", "0"}); });
    rejects([] { web::url_point_to_2d({"0", "480"}); });
    rejects([] { web::url_point_to_2d({"1x", "0"}); });
    options.color_mode = 5;
    assert(web::url_point_to_2d({"1499", "164"})->y == 164);
    rejects([] { web::url_point_to_2d({"1500", "0"}); });
    rejects([] { web::url_point_to_2d({"0", "165"}); });
    uint16_t samples[] = {1, 2, 3, 4};
    assert(get_window(samples, 2, 2, 4, 2, 2, 0, 0).size() == 4);
    assert(get_window(samples, 2, 2, 4, 0, 0, 0, 2).empty());
    rejects([&] { get_window(samples, 2, 2, 3, 0, 0, 2, 2); });
    rejects([&] { get_window(samples, 2, 2, 4, -1, 0, 2, 2); });
    rejects([] { utils::slice({"a"}, 0, 1); });
    rejects([] { request::number("nan", 0, 100); });
    rejects([] { request::integer("9999999999999999999999", 0, 100); });
    rejects([] { request::integer("1.2", 0, 100); });

    int count = 0;
    auto increment = [&] { for (int i = 0; i < 10000; ++i)
        cam::run_with_lock([&] { ++count; return 0; }); };
    std::thread first(increment), second(increment);
    first.join(); second.join();
    assert(count == 20000);
    rejects([] { cam::run_with_lock([]() -> int { throw std::runtime_error("test"); }); });
    assert(cam::run_with_lock([] { return 42; }) == 42);
}
