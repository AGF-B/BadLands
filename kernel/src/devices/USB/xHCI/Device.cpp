#include <cstdint>

#include <shared/Response.hpp>

#include <devices/USB/xHCI/Controller.hpp>
#include <devices/USB/xHCI/Device.hpp>

namespace Devices::USB::xHCI {
    Device::Device(Controller& controller, const DeviceDescriptor& descriptor)
        : controller{controller}, descriptor{descriptor} {}

    Success Device::Initialize() {
        
    }
}
