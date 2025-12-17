#include <shared/Response.hpp>

#include <devices/USB/HID/Device.hpp>
#include <devices/USB/xHCI/Device.hpp>

namespace Devices::USB::HID {
    Optional<Device*> Device::Create(xHCI::Device& device) {
        return Optional<Device*>();
    }
}
