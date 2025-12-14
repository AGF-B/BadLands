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

    constexpr void TRB::SetCycle(bool cycle) {
        static constexpr uint32_t MASK = 0x00000001;
        data[3] = ModifyPacked(data[3], MASK, 0, cycle ? 1U : 0U);
    }

    constexpr void TRB::SetTRBType(uint8_t type) {
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

    constexpr void CommandTRB::SetSlotType(uint8_t type) {
        static constexpr uint8_t SHIFT = 16;
        static constexpr uint32_t MASK = 0x001F0000;
        data[3] = ModifyPacked(data[3], MASK, SHIFT, type);
    }

    constexpr void CommandTRB::SetSlotID(uint8_t id) {
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

    constexpr void TransferTRB::SetDataBufferPointer(const void* pointer) {
        const uint64_t raw_pointer = reinterpret_cast<uint64_t>(pointer);
        SetRawImmediateData(raw_pointer);
    }

    constexpr void TransferTRB::SetRawImmediateData(uint64_t data_value) {
        const uint32_t lo = static_cast<uint32_t>(data_value);
        const uint32_t hi = static_cast<uint32_t>(data_value >> 32);

        data[0] = lo;
        data[1] = hi;
    }

    constexpr void TransferTRB::SetTRBTransferLength(uint32_t length) {
        static constexpr uint8_t SHIFT = 0;
        static constexpr uint32_t MASK = 0x0001FFFF;
        data[2] = ModifyPacked(data[2], MASK, SHIFT, length);
    }

    constexpr void TransferTRB::SetTDSize(uint8_t size) {
        static constexpr uint8_t SHIFT = 17;
        static constexpr uint32_t MASK = 0x003E0000;
        data[2] = ModifyPacked(data[2], MASK, SHIFT, size);
    }

    constexpr void TransferTRB::SetInterrupterTarget(uint16_t target) {
        static constexpr uint8_t SHIFT = 22;
        static constexpr uint32_t MASK = 0xFFC00000;
        data[2] = ModifyPacked(data[2], MASK, SHIFT, target);
    }

    constexpr void TransferTRB::SetENT(bool ent) {
        static constexpr uint8_t SHIFT = 1;
        static constexpr uint32_t MASK = 0x00000002;
        data[3] = ModifyPacked(data[3], MASK, SHIFT, ent ? 1U : 0U);
    }

    constexpr void TransferTRB::SetISP(bool isp) {
        static constexpr uint8_t SHIFT = 2;
        static constexpr uint32_t MASK = 0x00000004;
        data[3] = ModifyPacked(data[3], MASK, SHIFT, isp ? 1U : 0U);
    }

    constexpr void TransferTRB::SetNoSnoop(bool no_snoop) {
        static constexpr uint8_t SHIFT = 3;
        static constexpr uint32_t MASK = 0x00000008;
        data[3] = ModifyPacked(data[3], MASK, SHIFT, no_snoop ? 1U : 0U);
    }

    constexpr void TransferTRB::SetChain(bool chain) {
        static constexpr uint8_t SHIFT = 4;
        static constexpr uint32_t MASK = 0x00000010;
        data[3] = ModifyPacked(data[3], MASK, SHIFT, chain ? 1U : 0U);
    }

    constexpr void TransferTRB::SetInterruptOnCompletion(bool ioc) {
        static constexpr uint8_t SHIFT = 5;
        static constexpr uint32_t MASK = 0x00000020;
        data[3] = ModifyPacked(data[3], MASK, SHIFT, ioc ? 1U : 0U);
    }

    constexpr void TransferTRB::SetImmediateData(bool immediate_data) {
        static constexpr uint8_t SHIFT = 6;
        static constexpr uint32_t MASK = 0x00000040;
        data[3] = ModifyPacked(data[3], MASK, SHIFT, immediate_data ? 1U : 0U);
    }

    constexpr void TransferTRB::SetBEI(bool bei) {
        static constexpr uint8_t SHIFT = 9;
        static constexpr uint32_t MASK = 0x00000200;
        data[3] = ModifyPacked(data[3], MASK, SHIFT, bei ? 1U : 0U);
    }

    constexpr void TransferTRB::SetDirection(bool direction) {
        static constexpr uint8_t SHIFT = 16;
        static constexpr uint32_t MASK = 0x00010000;
        data[3] = ModifyPacked(data[3], MASK, SHIFT, direction ? 1U : 0U);
    }

    constexpr TransferType::operator decltype(Invalid) () const {
        return value;
    }

    constexpr TransferType TransferType::FromType(uint8_t type) {
        switch (type) {
            case 0: return TransferType{ NoDataStage };
            case 2: return TransferType{ DataOutStage };
            case 3: return TransferType{ DataInStage };
            default: return TransferType{ Invalid };
        }
    }

    constexpr uint8_t TransferType::ToType() const {
        switch (value) {
            case NoDataStage:   return 0;
            case DataOutStage:  return 2;
            case DataInStage:   return 3;
            default:            return 0;
        }
    }
    
    constexpr bool TransferType::operator==(const decltype(Invalid)& type) const {
        return value == type;
    }

    constexpr bool TransferType::operator!=(const decltype(Invalid)& type) const {
        return value != type;
    }

    constexpr void SetupTRB::SetRequestType(uint8_t bmRequestType) {
        static constexpr uint8_t SHIFT = 0;
        static constexpr uint32_t MASK = 0x000000FF;
        data[0] = ModifyPacked(data[0], MASK, SHIFT, bmRequestType);
    }

    constexpr void SetupTRB::SetRequest(uint8_t bRequest) {
        static constexpr uint8_t SHIFT = 8;
        static constexpr uint32_t MASK = 0x0000FF00;
        data[0] = ModifyPacked(data[0], MASK, SHIFT, bRequest);
    }

    constexpr void SetupTRB::SetValue(uint16_t wValue) {
        static constexpr uint8_t SHIFT = 16;
        static constexpr uint32_t MASK = 0xFFFF0000;
        data[0] = ModifyPacked(data[0], MASK, SHIFT, wValue);
    }

    constexpr void SetupTRB::SetIndex(uint16_t wIndex) {
        static constexpr uint8_t SHIFT = 0;
        static constexpr uint32_t MASK = 0x0000FFFF;
        data[1] = ModifyPacked(data[1], MASK, SHIFT, wIndex);
    }

    constexpr void SetupTRB::SetLength(uint16_t wLength) {
        static constexpr uint8_t SHIFT = 16;
        static constexpr uint32_t MASK = 0xFFFF0000;
        data[1] = ModifyPacked(data[1], MASK, SHIFT, wLength);
    }

    constexpr void SetupTRB::SetTransferType(const TransferType& type) {
        static constexpr uint8_t SHIFT = 16;
        static constexpr uint32_t MASK = 0x00030000;
        data[3] = ModifyPacked(data[3], MASK, SHIFT, type.ToType());
    }

    SetupTRB SetupTRB::Create(const SetupDescriptor& descriptor) {
        static constexpr uint8_t SETUP_TYPE = 2;

        SetupTRB trb;

        trb.data[0] = 0;
        trb.data[1] = 0;
        trb.data[2] = 0;
        trb.data[3] = 0;

        trb.SetRequestType(descriptor.bmRequestType);
        trb.SetRequest(descriptor.bRequest);
        trb.SetValue(descriptor.wValue);
        trb.SetIndex(descriptor.wIndex);
        trb.SetLength(descriptor.wLength);
        trb.SetTRBTransferLength(descriptor.tranferLength);
        trb.SetInterrupterTarget(descriptor.interrupterTarget);
        trb.SetCycle(descriptor.cycle);
        trb.SetInterruptOnCompletion(descriptor.interruptOnCompletion);
        trb.SetImmediateData(true);
        trb.SetTRBType(SETUP_TYPE);
        trb.SetTransferType(descriptor.transferType);

        return trb;
    }
    
    DataTRB DataTRB::Create(const DataDescriptor& descriptor) {
        static constexpr uint8_t DATA_TYPE = 3;

        DataTRB trb;

        trb.data[0] = 0;
        trb.data[1] = 0;
        trb.data[2] = 0;
        trb.data[3] = 0;

        trb.SetDataBufferPointer(descriptor.bufferPointer);
        trb.SetTRBTransferLength(descriptor.transferLength);
        trb.SetTDSize(descriptor.tdSize);
        trb.SetInterrupterTarget(descriptor.interrupterTarget);
        trb.SetCycle(descriptor.cycle);
        trb.SetENT(descriptor.evaluateNextTRB);
        trb.SetISP(descriptor.interruptOnShortPacket);
        trb.SetNoSnoop(descriptor.noSnoop);
        trb.SetChain(descriptor.chain);
        trb.SetInterruptOnCompletion(descriptor.interruptOnCompletion);
        trb.SetImmediateData(descriptor.immediateData);
        trb.SetTRBType(DATA_TYPE);
        trb.SetDirection(descriptor.direction);

        return trb;
    }

    StatusTRB StatusTRB::Create(const StatusDescriptor& descriptor) {
        static constexpr uint8_t STATUS_TYPE = 4;

        StatusTRB trb;

        trb.data[0] = 0;
        trb.data[1] = 0;
        trb.data[2] = 0;
        trb.data[3] = 0;

        trb.SetInterrupterTarget(descriptor.interrupterTarget);
        trb.SetCycle(descriptor.cycle);
        trb.SetENT(descriptor.evaluateNextTRB);
        trb.SetChain(descriptor.chain);
        trb.SetInterruptOnCompletion(descriptor.interruptOnCompletion);
        trb.SetTRBType(STATUS_TYPE);
        trb.SetDirection(descriptor.direction);

        return trb;
    }
}
