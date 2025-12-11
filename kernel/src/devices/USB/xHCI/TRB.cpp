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

    void CommandTRB::SetCycle(bool cycle) {
        data[3] |= cycle ? 1 : 0;
    }

    void CommandTRB::SetTRBType(uint8_t type) {
        static constexpr uint8_t SHIFT = 10;
        static constexpr uint32_t MASK = 0x0000003F;

        const uint32_t extended_type = static_cast<uint32_t>(type) & MASK;
        data[3] |= extended_type << SHIFT;
    }

    void CommandTRB::SetSlotType(uint8_t type) {
        static constexpr uint8_t SHIFT = 16;
        static constexpr uint32_t MASK = 0x0000001F;

        const uint32_t extended_type = static_cast<uint32_t>(type) & MASK;
        data[3] |= extended_type << SHIFT;
    }

    void CommandTRB::SetSlotID(uint8_t id) {
        static constexpr uint8_t SHIFT = 24;
        static constexpr uint32_t MASK = 0xFF000000;

        const uint32_t extended_id = (static_cast<uint32_t>(id) << SHIFT) & MASK;
        data[3] |= extended_id;
    }

    NoOpTRB NoOpTRB::Create(bool cycle) {
        static constexpr uint8_t NO_OP_TYPE = 23;

        NoOpTRB trb;
        
        trb.data[0] = 0;
        trb.data[1] = 0;
        trb.data[2] = 0;
        trb.data[3] = 0;

        trb.SetCycle(cycle);
        trb.SetTRBType(NO_OP_TYPE);

        return trb;
    }

    EnableSlotTRB EnableSlotTRB::Create(bool cycle, uint8_t slot_type) {
        static constexpr uint8_t ENABLE_SLOT_TYPE = 9;

        EnableSlotTRB trb;

        trb.data[0] = 0;
        trb.data[1] = 0;
        trb.data[2] = 0;
        trb.data[3] = 0;

        trb.SetCycle(cycle);
        trb.SetTRBType(ENABLE_SLOT_TYPE);
        trb.SetSlotType(slot_type);

        return trb;
    }

    AddressDeviceTRB AddressDeviceTRB::Create(bool cycle, bool bsr, uint8_t slot_id, const void* context_pointer) {
        static constexpr uint8_t    ADDRESS_DEVICE_TYPE     = 11;
        static constexpr uint64_t   CONTEXT_POINTER_MASK    = 0xFFFFFFFFFFFFFFF0;
        static constexpr uint32_t   BSR_FLAG                = 0x00000200; 

        const uint64_t raw_pointer = reinterpret_cast<uint64_t>(context_pointer) & CONTEXT_POINTER_MASK;

        AddressDeviceTRB trb;

        trb.data[0] = static_cast<uint32_t>(raw_pointer);
        trb.data[1] = static_cast<uint32_t>(raw_pointer >> 32);
        trb.data[2] = 0;
        trb.data[3] = bsr ? BSR_FLAG : 0;

        trb.SetCycle(cycle);
        trb.SetTRBType(ADDRESS_DEVICE_TYPE);
        trb.SetSlotID(slot_id);

        return trb;
    }

    LinkTRB LinkTRB::Create(bool cycle, TRB* next) {
        static constexpr uint8_t LINK_TYPE = 6;
        static constexpr uint64_t NEXT_MASK = 0xFFFFFFFFFFFFFFF0;

        const uint64_t raw_next = reinterpret_cast<uint64_t>(next) & NEXT_MASK;

        LinkTRB trb;

        trb.data[0] = static_cast<uint32_t>(raw_next);
        trb.data[1] = static_cast<uint32_t>(raw_next >> 32);
        trb.data[2] = 0;
        trb.data[3] = 0;

        trb.SetCycle(cycle);
        trb.SetTRBType(LINK_TYPE);

        return trb;
    }
}
