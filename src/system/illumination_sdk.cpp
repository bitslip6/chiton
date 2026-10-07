#include "illumination_sdk.h"
#include <limits>
namespace illumination {
Receipt DevicePort::write(uint64_t frame, uint64_t,
    const std::array<uint16_t,4> &addresses, const std::array<uint16_t,4> &values) {
    if (!device_ || frame > std::numeric_limits<uint64_t>::max() - 2)
        return {aditof::Status::UNAVAILABLE, false, 0};
    const auto status = device_->writeAfeRegisters(addresses.data(), values.data(), values.size());
    // Operator contract: OK acknowledges all four; next returned frame may be
    // one frame behind. Second post-write acquisition is conservatively eligible.
    return {status, status == aditof::Status::OK, frame + 2};
}
}
