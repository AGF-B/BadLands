#include <cstdint>

#include <devices/USB/Driver.hpp>
#include <devices/USB/xHCI/Device.hpp>
#include <devices/USB/xHCI/Specification.hpp>

namespace Devices::USB {
    xHCI::TransferRing* Driver::GetEndpointTransferRing(uint8_t endpointAddress, bool isIn) const {
        return device.GetEndpointTransferRing(endpointAddress, isIn);
    }

    void Driver::RingDoorbell(uint8_t doorbellID) const {
        device.RingDoorbell(doorbellID);
    }

    Driver::Driver(const xHCI::Device& device) : device{device} { }
}
