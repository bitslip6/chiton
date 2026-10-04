// Focused F04/F05/F06/F12 regressions against production helpers.
#include "system.h"
#include "utils.hpp"
#include "webserver.h"
#include "shapes.h"
#include "neon_compat.h"
#include <array>
#include <cassert>
#include <cmath>
#include <fstream>
#include <limits>
#include <sys/socket.h>
#include <unistd.h>

GlobalOpts *GOPTS = nullptr;
namespace calibration { point *get_calibration() { return nullptr; } }
// Legacy internal helper has no public declaration; exercise its real body.
namespace gfx { void matrix_multiply_4x4_neon(float *, float *, float *); }

template<class F> void rejects(F fn) {
    bool rejected = false;
    try { fn(); } catch (const std::exception &) { rejected = true; }
    assert(rejected);
}
std::string contents(const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
void replace(const std::string &path, const std::string &bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(bytes.data(), bytes.size());
    assert(file.good());
}
std::string transmitted(const web::HttpResponse &response, bool head = false) {
    int sockets[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    assert(web::send_response(sockets[0], response, head));
    shutdown(sockets[0], SHUT_WR);
    std::string bytes;
    char buffer[1024];
    ssize_t n;
    while ((n = read(sockets[1], buffer, sizeof(buffer))) > 0) bytes.append(buffer, n);
    close(sockets[0]); close(sockets[1]);
    return bytes;
}
int main() {
    // Ubuntu's SIMDe 0.7.2 must compile and execute the same matrix helper.
    // Binary fractions keep the references exact on fused/nonfused backends.
    std::array<float, 16> identity{}, zero{}, a{}, b{}, result{}, expected{};
    for (int i = 0; i < 4; ++i) identity[i * 5] = 1.0f;
    for (int i = 0; i < 16; ++i) {
        a[i] = (i - 7) * 0.25f;
        b[i] = (13 - i * 3) * 0.125f;
    }
    gfx::matrix_multiply_4x4_neon(a.data(), identity.data(), result.data());
    assert(result == a);
    gfx::matrix_multiply_4x4_neon(identity.data(), b.data(), result.data());
    assert(result == b);
    gfx::matrix_multiply_4x4_neon(a.data(), zero.data(), result.data());
    assert(result == zero);
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            for (int k = 0; k < 4; ++k)
                expected[column * 4 + row] = std::fma(a[k * 4 + row], b[column * 4 + k],
                                                     expected[column * 4 + row]);
    gfx::matrix_multiply_4x4_neon(a.data(), b.data(), result.data());
    assert(result == expected);
#ifdef CHITON_NEON_FMA_LANE_FALLBACK
    // Fused cancellation differs from a rounded multiply-then-add (zero).
    const float x = 1.0f + std::ldexp(1.0f, -23), y = 1.0f - std::ldexp(1.0f, -23);
    float cancellation[4];
    const auto fused = vfmaq_laneq_f32(vdupq_n_f32(-1.0f), vdupq_n_f32(x), vdupq_n_f32(y), 3);
    vst1q_f32(cancellation, fused);
    for (float value : cancellation) assert(value == std::fma(x, y, -1.0f) && value != 0.0f);
#endif

    char directory[] = "/tmp/chiton-web-correctness-XXXXXX";
    assert(mkdtemp(directory));
    const std::string path = std::string(directory) + "/calibration.bin";
    std::vector<calibration::point> points(CAM_WIDTH * CAM_HEIGHT), output(points.size());
    for (size_t i = 0; i < points.size(); ++i)
        points[i] = {static_cast<unsigned>(i), 800, 1600, 0.25f, 1000, 2000};
    rejects([&] { utils::read_calibration(path, output.data()); });
    rejects([&] { utils::write_calibration(points.data(), path, points.size() - 1); });
    utils::write_calibration(points.data(), path, points.size());
    const auto saved = contents(path);
    assert(saved.substr(0, 8) == "CHITCAL1");
    utils::read_calibration(path, output.data());
    assert(memcmp(points.data(), output.data(), points.size() * sizeof(points[0])) == 0);
    const auto original = output;
    auto preserves = [&] {
        rejects([&] { utils::read_calibration(path, output.data()); });
        assert(memcmp(original.data(), output.data(), original.size() * sizeof(original[0])) == 0);
    };
    replace(path, saved.substr(0, saved.size() - 1)); preserves();
    replace(path, saved + "x"); preserves();
    auto corrupt = saved; corrupt.back() ^= 1; replace(path, corrupt); preserves();
    corrupt = saved; corrupt[8] = 99; replace(path, corrupt); preserves();
    corrupt = saved; corrupt[12] ^= 1; replace(path, corrupt); preserves();
    // Legacy full-frame files are accepted, but bad records never become live.
    replace(path, std::string(reinterpret_cast<char *>(points.data()), points.size() * sizeof(points[0])));
    utils::read_calibration(path, output.data());
    points[0].center_angle = std::numeric_limits<float>::quiet_NaN();
    replace(path, std::string(reinterpret_cast<char *>(points.data()), points.size() * sizeof(points[0])));
    preserves();
    points[0].center_angle = 0.25f;
    points[0].index = points.size();
    rejects([&] { utils::write_calibration(points.data(), path, points.size()); });
    points[0].index = 0;
    replace(path, saved);
    // Create/rename failures preserve the old file and clean up temporary siblings.
    rejects([&] { utils::write_calibration(points.data(), path + "/missing", points.size()); });
    rejects([&] { utils::write_calibration(points.data(), directory, points.size()); });
    assert(contents(path) == saved);
    setenv("CHITON_CALIBRATION_FILE", path.c_str(), 1);
    assert(utils::calibration_path() == path);
    unsetenv("CHITON_CALIBRATION_FILE");
    assert(utils::calibration_path() == "calibration.bin");

    for (int radius : {0, 1, 2, 4}) {
        std::unique_ptr<float, decltype(&free)> kernel(gfx::make_gausian_kernel(radius), &free);
        const int side = radius * 2 + 1;
        double sum = 0;
        for (int i = 0; i < side * side; ++i) { assert(kernel.get()[i] >= 0); sum += kernel.get()[i]; }
        assert(std::abs(sum - 1) < 1e-5);
        output = points;
        gfx::apply_kernel(radius, kernel.get(), output.data());
        for (size_t i = 0; i < output.size(); ++i) {
            assert(output[i].near_point == 800 && output[i].far_point == 1600);
            assert(output[i].index == points[i].index && output[i].near_dist == 1000);
        }
    }
    rejects([] { gfx::make_gausian_kernel(-1); });
    rejects([] { gfx::make_gausian_kernel(33); });
    rejects([&] { gfx::apply_kernel(1, nullptr, points.data()); });
    // Asymmetric weights expose the old row-stride bug; near/far validity differs.
    float kernel[] = {0,0,0, 0,0,0, 0,0,1};
    const int center = 20 * CAM_WIDTH + 20;
    points[center + CAM_WIDTH + 1].near_point = 1200;
    points[center + CAM_WIDTH + 1].far_point = 0;
    output = points;
    gfx::apply_kernel(1, kernel, output.data());
    assert(output[center].near_point == 1200 && output[center].far_point == 1600);
    assert(output[0].near_point == points[0].near_point);
    kernel[0] = std::numeric_limits<float>::infinity();
    rejects([&] { gfx::apply_kernel(1, kernel, points.data()); });

    const auto error = web::error_response(404, "not found: \"quoted\"");
    assert(nlohmann::json::parse(error.content)["status"] == 404);
    const auto wire = transmitted(error);
    assert(wire.find("HTTP/1.1 404 Not Found\r\n") == 0);
    assert(wire.find("Connection: close\r\n") != std::string::npos);
    assert(wire.substr(wire.find("\r\n\r\n") + 4) == error.content);
    for (size_t i = 0; i < wire.find("\r\n\r\n"); ++i)
        if (wire[i] == '\n') assert(i && wire[i - 1] == '\r');
    const char binary[] = {'a', '\0', 'b'};
    auto response = web::binary_response("application/octet-stream", binary, sizeof(binary));
    response.headers["X-Chiton-Frame-Id"] = "65536";
    response.headers["X-Chiton-Units"] = "depth_sensor_units";
    assert(response.content.size() == 3);
    assert(transmitted(response).find("Content-Length: 3\r\n") != std::string::npos);
    const auto head = transmitted(response, true);
    assert(head.substr(head.find("\r\n\r\n") + 4).empty());
    assert(head.find("X-Chiton-Frame-Id: 65536\r\n") != std::string::npos);
    assert(head.find("X-Chiton-Units: depth_sensor_units\r\n") != std::string::npos);
    int sockets[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    response.headers["X-Chiton-Units"] = "bad\r\nInjected: true";
    assert(!web::send_response(sockets[0], response));
    close(sockets[0]); close(sockets[1]);
    rejects([] { web::binary_response("image/jpeg", nullptr, 1); });
    char query[] = "GET /app.html?cache=123 HTTP/1.1\r\n\r\n";
    assert(web::parse_url(query) == std::vector<std::string>({"app", "html"}));
    char escaped[] = "GET /wlan0/setting/a%20b/p%2Fq.bin HTTP/1.1\r\n\r\n";
    assert(web::parse_url(escaped) == std::vector<std::string>({"wlan0", "setting", "a b", "p/q.bin"}));

    std::ifstream config_file("release/chiton.json");
    json config; config_file >> config;
    utils::validate_configuration(config);
    auto bad = config; bad["measurements"][0]["vixel"] = 8;
    rejects([&] { utils::validate_configuration(bad); });
    bad = config; bad["measurements"].erase(bad["measurements"].begin());
    rejects([&] { utils::validate_configuration(bad); });
    bad = config; bad["measurements"][0]["point1"] = "wrong type";
    rejects([&] { utils::validate_configuration(bad); });
    bad = config; bad["measurements"][0]["radian"] = std::numeric_limits<double>::infinity();
    rejects([&] { utils::validate_configuration(bad); });
    bad = config; bad["center_x"] = -1;
    rejects([&] { utils::validate_configuration(bad); });
    bad = config; bad["inner_dist"] = bad["outer_dist"];
    rejects([&] { utils::validate_configuration(bad); });
    unlink(path.c_str());
    assert(rmdir(directory) == 0); // Also asserts failed saves leaked no temp files.
}
