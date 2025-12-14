#include <cstdint>

#include <shared/Bitwise.hpp>

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

    void TRB::SetCycle(bool cycle) {
        static constexpr uint32_t MASK = 0x00000001;
        data[3] = ModifyPacked(data[3], MASK, 0, cycle ? 1U : 0U);
    }

    void TRB::SetTRBType(uint8_t type) {
        static constexpr uint8_t SHIFT = 10;
        static constexpr uint32_t MASK = 0x0000FC00;
        data[3] = ModifyPacked(data[3], MASK, SHIFT, type);
    }


    TRB::CompletionCode EventTRB::GetCompletionCode() const {
        static constexpr uint8_t SHIFT = 24;
        static constexpr uint32_t MASK = 0xFF000000;
        return static_cast<CompletionCode>(GetPacked(data[2], MASK, SHIFT));
    }

    EventTRB::Type EventTRB::GetType() const {
        static constexpr uint8_t SHIFT = 10;
        static constexpr uint32_t MASK = 0x0000FC00;
        return static_cast<Type>(GetPacked(data[3], MASK, SHIFT));
    }

    uint64_t EventTRB::GetEventData() const {
        return (static_cast<uint64_t>(data[1]) << 32) | data[0];
    }

    TRB* EventTRB::GetPointer() const {
        return reinterpret_cast<TRB*>(GetEventData());
    }

    uint32_t EventTRB::GetEventParameter() const {
        static constexpr uint32_t MASK = 0x00FFFFFF;
        return GetPacked(data[2], MASK, 0);
    }

    uint8_t EventTRB::GetVFID() const {
        static constexpr uint8_t SHIFT = 16;
        static constexpr uint32_t MASK = 0x00FF0000;
        return GetPacked(data[3], MASK, SHIFT);
    }

    uint8_t EventTRB::GetSlotID() const {
        static constexpr uint8_t SHIFT = 24;
        static constexpr uint32_t MASK = 0xFF000000;
        return static_cast<uint8_t>(GetPacked(data[3], MASK, SHIFT));
    }


    bool TransferEventTRB::GetEventDataPresent() const {
        static constexpr uint32_t MASK = 0x00000004;
        static constexpr uint8_t SHIFT = 2;
        return GetPacked(data[3], MASK, SHIFT) != 0;
    }

    uint8_t TransferEventTRB::GetEndpointID() const {
        static constexpr uint8_t SHIFT = 16;
        static constexpr uint32_t MASK = 0x001F0000;
        return static_cast<uint8_t>(GetPacked(data[3], MASK, SHIFT));
    }


    uint8_t PortStatusChangeEventTRB::GetPortID() const {
        static constexpr uint8_t SHIFT = 24;
        static constexpr uint32_t MASK = 0xFF000000;
        return static_cast<uint8_t>(GetPacked(data[0], MASK, SHIFT));
    }

    void CommandTRB::SetSlotType(uint8_t type) {
        static constexpr uint8_t SHIFT = 16;
        static constexpr uint32_t MASK = 0x001F0000;
        data[3] = ModifyPacked(data[3], MASK, SHIFT, type);
    }

    void CommandTRB::SetSlotID(uint8_t id) {
        static constexpr uint8_t SHIFT = 24;
        static constexpr uint32_t MASK = 0xFF000000;
        data[3] = ModifyPacked(data[3], MASK, SHIFT, id);
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
        static constexpr uint32_t TOGGLE_CYCLE_FLAG = 0x00000002;

        const uint64_t raw_next = reinterpret_cast<uint64_t>(next) & NEXT_MASK;

        LinkTRB trb;

        trb.data[0] = static_cast<uint32_t>(raw_next);
        trb.data[1] = static_cast<uint32_t>(raw_next >> 32);
        trb.data[2] = 0;
        trb.data[3] = TOGGLE_CYCLE_FLAG;

        trb.SetCycle(cycle);
        trb.SetTRBType(LINK_TYPE);

        return trb;
    }

    void TransferTRB::SetDataBufferPointer(const void* pointer) {
        const uint64_t raw_pointer = reinterpret_cast<uint64_t>(pointer);
        SetRawImmediateData(raw_pointer);
    }

    void TransferTRB::SetRawImmediateData(uint64_t data_value) {
        const uint32_t lo = static_cast<uint32_t>(data_value);
        const uint32_t hi = static_cast<uint32_t>(data_value >> 32);

        data[0] = lo;
        data[1] = hi;
    }

    void TransferTRB::SetTRBTransferLength(uint16_t length) {
        static constexpr uint8_t SHIFT = 0;
        static constexpr uint32_t MASK = 0x0000FFFF;
        data[2] = ModifyPacked(data[2], MASK, SHIFT, length);
    }
}
