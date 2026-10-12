#pragma once
#include "illumination_register_adapter.h"
#include <aditof/device_interface.h>
#include <memory>
#include <utility>
namespace illumination {
class DevicePort final : public RegisterPort {
    std::shared_ptr<aditof::DeviceInterface> device_;
public:
    explicit DevicePort(std::shared_ptr<aditof::DeviceInterface> device) : device_(std::move(device)) {}
    Receipt write(uint64_t frame, uint64_t command,
        const std::array<uint16_t,4> &addresses, const std::array<uint16_t,4> &values) override;
};
}
