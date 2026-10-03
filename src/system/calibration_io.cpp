#include "utils.hpp"
#include <array>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <unistd.h>
#include <zlib.h>

namespace utils {
namespace {
constexpr size_t count = CAM_WIDTH * CAM_HEIGHT;
constexpr size_t payload_bytes = count * sizeof(calibration::point);
constexpr char magic[] = "CHITCAL1";
using Header = std::array<uint32_t, 7>;

void validate(const calibration::point *points) {
    if (!points) throw std::runtime_error("calibration data unavailable");
    for (size_t i = 0; i < count; ++i) {
        // Make enables fast-math; explicit IEEE bits keep this check effective.
        uint32_t angle_bits;
        static_assert(sizeof(angle_bits) == sizeof(points[i].center_angle), "32-bit float required");
        memcpy(&angle_bits, &points[i].center_angle, sizeof(angle_bits));
        if (points[i].index >= count || (angle_bits & 0x7f800000u) == 0x7f800000u)
            throw std::runtime_error("invalid calibration point " + std::to_string(i));
    }
}
uint32_t checksum(const calibration::point *points) {
    return crc32(0, reinterpret_cast<const Bytef *>(points), payload_bytes);
}
void write_bytes(int fd, const void *data, size_t size) {
    const auto *bytes = static_cast<const char *>(data);
    while (size) {
        const auto written = write(fd, bytes, size);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) throw std::runtime_error("unable to write calibration file");
        bytes += written;
        size -= written;
    }
}
}

std::string calibration_path() {
    const char *path = getenv("CHITON_CALIBRATION_FILE");
    return path && *path ? path : "calibration.bin";
}

void write_calibration(const calibration::point *data, std::string filename, int numpoints) {
    if (numpoints != static_cast<int>(count))
        throw std::invalid_argument("calibration must contain a complete sensor frame");
    validate(data);
    // Version, dimensions, record ABI size, byte order, length, CRC32.
    // The payload retains the legacy native point layout; incompatible ABIs are rejected.
    Header header{{1, CAM_WIDTH, CAM_HEIGHT, sizeof(calibration::point),
                   0x01020304u, payload_bytes, checksum(data)}};
    std::string temporary = filename + ".tmp-XXXXXX";
    int fd = mkstemp(&temporary[0]);
    if (fd < 0) throw std::runtime_error("unable to create calibration file: " + filename);
    try {
        write_bytes(fd, magic, 8);
        write_bytes(fd, header.data(), sizeof(header));
        write_bytes(fd, data, payload_bytes);
        if (fsync(fd) != 0) throw std::runtime_error("unable to flush calibration file");
        const int result = close(fd);
        fd = -1;
        if (result != 0) throw std::runtime_error("unable to close calibration file");
        if (rename(temporary.c_str(), filename.c_str()) != 0)
            throw std::runtime_error("unable to replace calibration file: " + filename);
    } catch (...) {
        if (fd >= 0) close(fd);
        unlink(temporary.c_str());
        throw;
    }
}

void read_calibration(std::string filename, calibration::point *destination) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("unable to open calibration file: " + filename);
    const auto size = file.tellg();
    const auto versioned_size = payload_bytes + 8 + sizeof(Header);
    if (size != static_cast<std::streamoff>(payload_bytes) &&
        size != static_cast<std::streamoff>(versioned_size))
        throw std::runtime_error("incorrect calibration file size");
    file.seekg(0);
    Header header{};
    const bool versioned = size == static_cast<std::streamoff>(versioned_size);
    if (versioned) {
        char signature[8];
        file.read(signature, sizeof(signature));
        file.read(reinterpret_cast<char *>(header.data()), sizeof(header));
        if (!file || memcmp(signature, magic, 8) != 0 || header[0] != 1 ||
            header[1] != CAM_WIDTH || header[2] != CAM_HEIGHT ||
            header[3] != sizeof(calibration::point) || header[4] != 0x01020304u ||
            header[5] != payload_bytes)
            throw std::runtime_error("unsupported calibration format or sensor shape");
    }
    std::vector<calibration::point> points(count);
    file.read(reinterpret_cast<char *>(points.data()), payload_bytes);
    if (!file || file.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("unable to read complete calibration file");
    if (versioned && header[6] != checksum(points.data()))
        throw std::runtime_error("calibration checksum mismatch");
    validate(points.data());
    if (!destination) destination = calibration::get_calibration();
    if (!destination) throw std::runtime_error("calibration data unavailable");
    // Do not modify the active frame until every record passed validation.
    std::copy(points.begin(), points.end(), destination);
}
}
