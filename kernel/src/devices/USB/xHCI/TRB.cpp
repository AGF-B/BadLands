#include <cstdint>

#include <devices/USB/xHCI/TRB.hpp>

namespace Devices::USB::xHCI {
    uint8_t TRB::GetSlotType() const {
        static constexpr uint8_t SHIFT = 10;
        static constexpr uint32_t MASK = 0x0000003F;
        return static_cast<uint8_t>((data[3] >> SHIFT) & MASK);
    }

    bool TRB::GetCycle() const {
        return (data[3] & 0x00000001) != 0;
    }


    TRB::CompletionCode EventTRB::GetCompletionCode() const {
        static constexpr uint8_t SHIFT = 24;
        static constexpr uint32_t MASK = 0x000000FF;
        return static_cast<CompletionCode>((data[2] >> SHIFT) & MASK);
    }

    EventTRB::Type EventTRB::GetType() const {
        static constexpr uint8_t SHIFT = 10;
        static constexpr uint32_t MASK = 0x0000003F;
        return static_cast<Type>((data[3] >> SHIFT) & MASK);
    }

    uint64_t EventTRB::GetEventData() const {
        return (static_cast<uint64_t>(data[1]) << 32) | data[0];
    }

    TRB* EventTRB::GetPointer() const {
        return reinterpret_cast<TRB*>(GetEventData());
    }

    uint32_t EventTRB::GetEventParameter() const {
        static constexpr uint32_t MASK = 0x00FFFFFF;
        return data[2] & MASK;
    }

    uint8_t EventTRB::GetVFID() const {
        static constexpr uint8_t SHIFT = 16;
        return static_cast<uint8_t>(data[3] >> SHIFT);
    }

    uint8_t EventTRB::GetSlotID() const {
        static constexpr uint8_t SHIFT = 24;
        return static_cast<uint8_t>(data[3] >> SHIFT);
    }


    bool TransferEventTRB::GetEventDataPresent() const {
        static constexpr uint32_t MASK = 0x00000004;
        return (data[3] & MASK) != 0;
    }

    uint8_t TransferEventTRB::GetEndpointID() const {
        static constexpr uint8_t SHIFT = 16;
        static constexpr uint32_t MASK = 0x0000001F;
        return static_cast<uint8_t>((data[3] >> SHIFT) & MASK);
    }


    uint8_t PortStatusChangeEventTRB::GetPortID() const {
        static constexpr uint8_t SHIFT = 24;
        return static_cast<uint8_t>(data[0] >> SHIFT);
    }
}