#include <cstdint>

#include <new>

#include <shared/Bitwise.hpp>
#include <shared/Lock.hpp>
#include <shared/LockGuard.hpp>
#include <shared/Response.hpp>
#include <shared/memory/defs.hpp>

#include <devices/USB/xHCI/Specification.hpp>
#include <devices/USB/xHCI/TRB.hpp>

#include <mm/Heap.hpp>
#include <mm/Utils.hpp>
#include <mm/VirtualMemory.hpp>

#include <sched/Self.hpp>

namespace Devices::USB::xHCI {
    PortSpeed::operator decltype(InvalidSpeed)() const {
        return value;
    }

    PortSpeed PortSpeed::FromSpeedID(uint8_t id) {
        switch (id) {
            case 1: return PortSpeed { FullSpeed };
            case 2: return PortSpeed { LowSpeed };
            case 3: return PortSpeed { HighSpeed };
            case 4: return PortSpeed { SuperSpeedGen1x1 };
            case 5: return PortSpeed { SuperSpeedPlusGen2x1 };
            case 6: return PortSpeed { SuperSpeedPlusGen1x2 };
            case 7: return PortSpeed { SuperSpeedPlusGen2x2 };
            default: return PortSpeed { InvalidSpeed };
        }
    }

    uint8_t PortSpeed::ToSpeedID() const {
        switch (*this) {
            case FullSpeed: return 1;
            case LowSpeed: return 2;
            case HighSpeed: return 3;
            case SuperSpeedGen1x1: return 4;
            case SuperSpeedPlusGen2x1: return 5;
            case SuperSpeedPlusGen1x2: return 6;
            case SuperSpeedPlusGen2x2: return 7;
            default: return 0;
        }
    }

    bool PortSpeed::operator==(const decltype(InvalidSpeed)& speed) const {
        return value == speed;
    }

    bool PortSpeed::operator!=(const decltype(InvalidSpeed)& speed) const {
        return value != speed;
    }

    SlotState::SlotState(const decltype(Invalid)& state) {
        value = state;
    }

    SlotState::operator decltype(Invalid) () const {
        return value;
    }

    SlotState SlotState::FromSlotState(uint8_t state) {
        switch (state) {
            case 0: return SlotState { DisabledEnabled };
            case 1: return SlotState { Default };
            case 2: return SlotState { Addressed };
            case 3: return SlotState { Configured };
            default: return SlotState { Invalid };
        }
    }

    bool SlotState::operator==(const decltype(Default)& state) const {
        return value == state;
    }

    bool SlotState::operator!=(const decltype(Default)& state) const {
        return value != state;
    }

    void Context::Reset() {
        data[0] = 0;
        data[1] = 0;
        data[2] = 0;
        data[3] = 0;
        data[4] = 0;
        data[5] = 0;
        data[6] = 0;
        data[7] = 0;
    }

    uint32_t SlotContext::GetRouteString() const {
        return GetPacked(data[0], ROUTE_STRING_MASK, 0);
    }

    void SlotContext::SetRouteString(uint32_t route_string) {
        data[0] = ModifyPacked(data[0], ROUTE_STRING_MASK, 0, route_string);
    }

    PortSpeed SlotContext::GetPortSpeed() const {
        return PortSpeed::FromSpeedID(GetPacked<uint32_t, uint8_t>(data[0], PORT_SPEED_MASK, PORT_SPEED_SHIFT));
    }

    void SlotContext::SetPortSpeed(const PortSpeed& speed) {
        data[0] = ModifyPacked(data[0], PORT_SPEED_MASK, PORT_SPEED_SHIFT, speed.ToSpeedID());
    }

    uint8_t SlotContext::GetContextEntries() const {
        return GetPacked<uint32_t, uint8_t>(data[0], CONTEXT_ENTRIES_MASK, CONTEXT_ENTRIES_SHIFT);
    }

    void SlotContext::SetContextEntries(uint8_t count) {
        data[0] = ModifyPacked(data[0], CONTEXT_ENTRIES_MASK, CONTEXT_ENTRIES_SHIFT, count);
    }

    uint8_t SlotContext::GetRootHubPort() const {
        return GetPacked<uint32_t, uint8_t>(data[1], ROOT_HUB_PORT_MASK, ROOT_HUB_PORT_SHIFT);
    }

    void SlotContext::SetRootHubPort(uint8_t port) {
        data[1] = ModifyPacked(data[1], ROOT_HUB_PORT_MASK, ROOT_HUB_PORT_SHIFT, port);
    }

    SlotState SlotContext::GetSlotState() const {
        return SlotState::FromSlotState(
            GetPacked<uint32_t, uint8_t>(data[3], SLOT_STATE_MASK, SLOT_STATE_SHIFT)
        );
    }

    void SlotContext::ResetSlotState() {
        data[3] = ModifyPacked(data[3], SLOT_STATE_MASK, SLOT_STATE_SHIFT, 0);
    }

    EndpointState::EndpointState(const decltype(Invalid)& state) {
        value = state;
    }

    EndpointState::operator decltype(Invalid) () const {
        return value;
    }

    EndpointState EndpointState::FromEndpointState(uint8_t state) {
        switch (state) {
            case 0: return EndpointState { Disabled };
            case 1: return EndpointState { Running };
            case 2: return EndpointState { Halted };
            case 3: return EndpointState { Stopped };
            case 4: return EndpointState { Error };
            case 5:
            case 6:
            case 7: return EndpointState { Reserved };
            default: return EndpointState { Invalid };
        }
    }

    bool EndpointState::operator==(const decltype(Invalid)& state) const {
        return value == state;
    }

    bool EndpointState::operator!=(const decltype(Invalid)& state) const {
        return value != state;
    }

    EndpointType::EndpointType(const decltype(Invalid)& type) {
        value = type;
    }

    EndpointType::operator decltype(Invalid) () const {
        return value;
    }

    EndpointType EndpointType::FromEndpointType(uint8_t type) {
        switch (type) {
            case 0: return EndpointType { Invalid };
            case 1: return EndpointType { IsochronousOut };
            case 2: return EndpointType { BulkOut };
            case 3: return EndpointType { InterruptOut };
            case 4: return EndpointType { ControlBidirectional };
            case 5: return EndpointType { IsochronousIn };
            case 6: return EndpointType { BulkIn };
            case 7: return EndpointType { InterruptIn };
            default: return EndpointType { Invalid };
        }
    }

    uint8_t EndpointType::ToEndpointType() const {
        switch (*this) {
            case Invalid: return 0;
            case IsochronousOut: return 1;
            case BulkOut: return 2;
            case InterruptOut: return 3;
            case ControlBidirectional: return 4;
            case IsochronousIn: return 5;
            case BulkIn: return 6;
            case InterruptIn: return 7;
            default: return 0;
        }
    }

    bool EndpointType::operator==(const decltype(Invalid)& type) const {
        return value == type;
    }

    bool EndpointType::operator!=(const decltype(Invalid)& type) const {
        return value != type;
    }

    EndpointState EndpointContext::GetEndpointState() const {
        const uint8_t raw_state = GetPacked<uint32_t, uint8_t>(data[0], STATE_MASK, STATE_SHIFT);

        switch (raw_state) {
            case 0: return EndpointState::Disabled;
            case 1: return EndpointState::Running;
            case 2: return EndpointState::Halted;
            case 3: return EndpointState::Stopped;
            case 4: return EndpointState::Error;
            case 5:
            case 6:
            case 7: return EndpointState::Reserved;
            default: return EndpointState::Invalid;
        }
    }

    uint8_t EndpointContext::GetMult() const {
        return GetPacked<uint32_t, uint8_t>(data[0], MULT_MASK, MULT_SHIFT);
    }

    void EndpointContext::SetMult(uint8_t mult) {
        data[0] = ModifyPacked(data[0], MULT_MASK, MULT_SHIFT, mult);
    }

    uint8_t EndpointContext::GetMaxPStreams() const {
        return GetPacked<uint32_t, uint8_t>(data[0], MAX_PS_STREAMS_MASK, MAX_PS_STREAMS_SHIFT);
    }

    void EndpointContext::SetMaxPStreams(uint8_t streams) {
        data[0] = ModifyPacked(data[0], MAX_PS_STREAMS_MASK, MAX_PS_STREAMS_SHIFT, streams);
    }

    uint8_t EndpointContext::GetInterval() const {
        return GetPacked<uint32_t, uint8_t>(data[0], INTERVAL_MASK, INTERVAL_SHIFT);
    }

    void EndpointContext::SetInterval(uint8_t interval) {
        data[0] = ModifyPacked(data[0], INTERVAL_MASK, INTERVAL_SHIFT, interval);
    }

    uint8_t EndpointContext::GetErrorCount() const {
        return GetPacked<uint32_t, uint8_t>(data[1], CERR_MASK, CERR_SHIFT);
    }

    void EndpointContext::SetErrorCount(uint8_t count) {
        data[1] = ModifyPacked<uint32_t, uint8_t>(data[1], CERR_MASK, CERR_SHIFT, count);
    }

    EndpointType EndpointContext::GetEndpointType() const {
        return EndpointType::FromEndpointType(
            GetPacked<uint32_t, uint8_t>(data[1], ENDPOINT_TYPE_MASK, ENDPOINT_TYPE_SHIFT)
        );
    }

    void EndpointContext::SetEndpointType(const EndpointType& type) {
        data[1] = ModifyPacked(data[1], ENDPOINT_TYPE_MASK, ENDPOINT_TYPE_SHIFT, type.ToEndpointType());
    }

    uint8_t EndpointContext::GetMaxBurstSize() const {
        return GetPacked<uint32_t, uint8_t>(data[1], MAX_BURST_SIZE_MASK, MAX_BURST_SIZE_SHIFT);
    }

    void EndpointContext::SetMaxBurstSize(uint8_t size) {
        data[1] = ModifyPacked(data[1], MAX_BURST_SIZE_MASK, MAX_BURST_SIZE_SHIFT, size);
    }

    uint16_t EndpointContext::GetMaxPacketSize() const {
        return GetPacked<uint32_t, uint16_t>(data[1], MAX_PACKET_SIZE_MASK, MAX_PACKET_SIZE_SHIFT);
    }

    void EndpointContext::SetMaxPacketSize(uint16_t size) {
        data[1] = ModifyPacked(data[1], MAX_PACKET_SIZE_MASK, MAX_PACKET_SIZE_SHIFT, size);
    }

    bool EndpointContext::GetDCS() const {
        return GetPacked<uint32_t, bool>(data[2], DCS_MASK, DCS_SHIFT);
    }

    void EndpointContext::SetDCS(bool dcs) {
        data[2] = ModifyPacked<uint32_t, bool>(data[2], DCS_MASK, DCS_SHIFT, dcs);
    }

    const TransferTRB* EndpointContext::GetTRDequeuePointer() const {
        const uint64_t pointer_lo = static_cast<uint64_t>(data[2] & TR_DEQUEUE_POINTER_LO_MASK);
        const uint64_t pointer_hi = static_cast<uint64_t>(data[3]);
        return reinterpret_cast<TransferTRB*>((pointer_hi << 32) | pointer_lo);
    }

    void EndpointContext::SetTRDequeuePointer(const TransferTRB* pointer) {
        const uint64_t address = reinterpret_cast<uint64_t>(pointer);
        const uint32_t address_lo = static_cast<uint32_t>(address) & TR_DEQUEUE_POINTER_LO_MASK;
        const uint32_t address_hi = static_cast<uint32_t>((address >> 32));
        data[2] = ModifyPacked(data[2], TR_DEQUEUE_POINTER_LO_MASK, 0, address_lo);
        data[3] = address_hi;
    }

    uint16_t EndpointContext::GetAverageTRBLength() const {
        return GetPacked<uint32_t, uint16_t>(data[4], AVERAGE_TRB_LENGTH_MASK, AVERAGE_TRB_LENGTH_SHIFT);
    }

    void EndpointContext::SetAverageTRBLength(uint16_t length) {
        data[4] = ModifyPacked<uint32_t, uint16_t>(data[4], AVERAGE_TRB_LENGTH_MASK, AVERAGE_TRB_LENGTH_SHIFT, length);
    }

    void InputControlContext::SetDropContext(uint8_t id) {
        if (id >= 2 && id < 32) {
            data[0] |= (1 << id);
        }
    }

    void InputControlContext::SetAddContext(uint8_t id) {
        if (id < 31) {
            data[1] |= (1 << id);
        }
    }

    void InputControlContext::SetConfigurationValue(uint8_t config) {
        static constexpr uint32_t CONFIGURATION_VALUE_MASK = 0x000000FF;
        data[7] = (data[7] & ~CONFIGURATION_VALUE_MASK) | (static_cast<uint32_t>(config) & CONFIGURATION_VALUE_MASK);
    }

    void InputControlContext::SetInterfaceNumber(uint8_t id) {
        static constexpr uint32_t INTERFACE_NUMBER_MASK = 0x0000FF00;
        static constexpr uint8_t INTERFACE_NUMBER_SHIFT = 8;
        data[7] = ModifyPacked(data[7], INTERFACE_NUMBER_MASK, INTERFACE_NUMBER_SHIFT, id);
    }

    void InputControlContext::SetAlternateSetting(uint8_t v) {
        static constexpr uint32_t ALTERNATE_SETTING_MASK = 0x00FF0000;
        static constexpr uint8_t ALTERNATE_SETTING_SHIFT = 16;
        data[7] = ModifyPacked(data[7], ALTERNATE_SETTING_MASK, ALTERNATE_SETTING_SHIFT, v);
    }

    Optional<ContextWrapper*> ContextWrapper::Create(bool extended) {
        if (extended) {
            return ContextWrapperEx::Create();
        }
        
        return ContextWrapperBasic::Create();
    }

    ContextWrapperBasic::ContextWrapperBasic(OutputDeviceContext* out, InputDeviceContext* in)
        : output{out}, input{in} {}

    void* ContextWrapperBasic::GetInputDeviceContextAddress() const {
        return input;
    }

    void* ContextWrapperBasic::GetOutputDeviceContextAddress() const {
        return output;
    }

    InputControlContext* ContextWrapperBasic::GetInputControlContext() {
        return &input->input_control;
    }

    SlotContext* ContextWrapperBasic::GetSlotContext(bool is_in) {
        return is_in ? &input->slot : &output->slot;
    }

    EndpointContext* ContextWrapperBasic::GetControlEndpointContext(bool is_in) {
        return is_in ? &input->control_endpoint : &output->control_endpoint;
    }

    EndpointContext* ContextWrapperBasic::GetInputEndpointContext(uint8_t id, bool is_in) {
        if (id < 15) {
            return is_in ? &input->endpoints[id].in : &input->endpoints[id].out;
        }

        return nullptr;
    }

    EndpointContext* ContextWrapperBasic::GetOutputEndpointContext(uint8_t id, bool is_in) {
        if (id < 15) {
            return is_in ? &output->endpoints[id].in : &output->endpoints[id].out;
        }

        return nullptr;
    }

    void ContextWrapperBasic::ResetInputControl() {
        input->input_control.Reset();
    }

    void ContextWrapperBasic::ResetSlot() {
        output->slot.Reset();
        input->slot.Reset();
    }

    void ContextWrapperBasic::ResetControlEndpoint() {
        output->control_endpoint.Reset();
        input->control_endpoint.Reset();
    }

    void ContextWrapperBasic::ResetEndpoint(uint8_t id, bool is_in) {
        if (id < 15) {
            if (is_in) {
                output->endpoints[id].in.Reset();
                input->endpoints[id].in.Reset();
            } else {
                output->endpoints[id].out.Reset();
                input->endpoints[id].out.Reset();
            }
        }
    }

    void ContextWrapperBasic::Reset() {
        ResetInputControl();
        ResetSlot();
        ResetControlEndpoint();

        for (size_t i = 0; i < 15; i++) {
            ResetEndpoint(i, true);
            ResetEndpoint(i, false);
        }
    }

    Optional<ContextWrapper*> ContextWrapperBasic::Create() {
        void* wrapper = Heap::Allocate(sizeof(ContextWrapperBasic));

        if (wrapper == nullptr) {
            return Optional<ContextWrapper*>();
        }

        void* const shared_page = VirtualMemory::AllocateDMA(1);

        if (shared_page == nullptr) {
            Heap::Free(wrapper);
            return Optional<ContextWrapper*>();
        }

        void* const output = shared_page;
        void* const input = static_cast<uint8_t*>(shared_page) + sizeof(OutputDeviceContext);

        new (output) OutputDeviceContext();
        new (input) InputDeviceContext();

        return Optional<ContextWrapper*>(new (wrapper) ContextWrapperBasic(
            static_cast<OutputDeviceContext*>(output),
            static_cast<InputDeviceContext*>(input)
        ));
    }

    void ContextWrapperBasic::Release() {
        VirtualMemory::FreeDMA(output, 1);
        Heap::Free(this);
    }

    ContextWrapperEx::ContextWrapperEx(OutputDeviceContextEx* out, InputDeviceContextEx* in)
        : output{out}, input{in} {}

    void* ContextWrapperEx::GetInputDeviceContextAddress() const {
        return input;
    }

    void* ContextWrapperEx::GetOutputDeviceContextAddress() const {
        return output;
    }

    InputControlContext* ContextWrapperEx::GetInputControlContext() {
        return &input->input_control;
    }
    
    SlotContext* ContextWrapperEx::GetSlotContext(bool is_in) {
        return is_in ? &input->slot : &output->slot;
    }

    EndpointContext* ContextWrapperEx::GetControlEndpointContext(bool is_in) {
        return is_in ? &input->control_endpoint : &output->control_endpoint;
    }

    EndpointContext* ContextWrapperEx::GetInputEndpointContext(uint8_t id, bool is_in) {
        if (id < 15) {
            return is_in ? &input->endpoints[id].in : &input->endpoints[id].out;
        }

        return nullptr;
    }

    EndpointContext* ContextWrapperEx::GetOutputEndpointContext(uint8_t id, bool is_in) {
        if (id < 15) {
            return is_in ? &output->endpoints[id].in : &output->endpoints[id].out;
        }

        return nullptr;
    }

    void ContextWrapperEx::ResetInputControl() {
        input->input_control.Reset();
    }

    void ContextWrapperEx::ResetSlot() {
        output->slot.Reset();
        input->slot.Reset();
    }

    void ContextWrapperEx::ResetControlEndpoint() {
        output->control_endpoint.Reset();
        input->control_endpoint.Reset();
    }

    void ContextWrapperEx::ResetEndpoint(uint8_t id, bool is_in) {
        if (id < 15) {
            if (is_in) {
                output->endpoints[id].in.Reset();
                input->endpoints[id].in.Reset();
            } else {
                output->endpoints[id].out.Reset();
                input->endpoints[id].out.Reset();
            }
        }
    }

    void ContextWrapperEx::Reset() {
        ResetInputControl();
        ResetSlot();
        ResetControlEndpoint();

        for (size_t i = 0; i < 15; i++) {
            ResetEndpoint(i, true);
            ResetEndpoint(i, false);
        }
    }

    Optional<ContextWrapper*> ContextWrapperEx::Create() {
        void* wrapper = Heap::Allocate(sizeof(ContextWrapperEx));

        if (wrapper == nullptr) {
            return Optional<ContextWrapper*>();
        }

        void* const shared_memory = VirtualMemory::AllocateDMA(2);

        if (shared_memory == nullptr) {
            Heap::Free(wrapper);
            return Optional<ContextWrapper*>();
        }

        void* const output = shared_memory;
        void* const input = static_cast<uint8_t*>(shared_memory) + sizeof(OutputDeviceContextEx);

        new (output) OutputDeviceContextEx();
        new (input) InputDeviceContextEx();

        return Optional<ContextWrapper*>(new (wrapper) ContextWrapperEx(
            static_cast<OutputDeviceContextEx*>(output),
            static_cast<InputDeviceContextEx*>(input)
        ));
    }

    void ContextWrapperEx::Release() {
        VirtualMemory::FreeDMA(output, 2);
        Heap::Free(this);
    }

    TransferRing::TransferRing(TransferTRB* base, size_t capacity)
        : base{base}, index{0}, capacity{capacity}, cycle{true} { }

    const TRB* TransferRing::EnqueueTRB(const TRB& trb) {
        static constexpr uint64_t WAIT_GRANULARITY_MS = 20;

        while (base[index].GetCycle() == trb.GetCycle()){
            Self().SpinWaitMillis(WAIT_GRANULARITY_MS);
        }

        base[index] = reinterpret_cast<const TransferTRB&>(trb);
        return &base[index];
    }

    void TransferRing::UpdatePointer() {
        if (++index >= capacity - 1) {
            LinkTRB link = LinkTRB::Create(GetCycle(), base);
            EnqueueTRB(link);            
            index = 0;
            cycle = !cycle;
        }
    }

    Optional<TransferRing*> TransferRing::Create(size_t pages) {
        void* const object_memory = Heap::Allocate(sizeof(TransferRing));

        if (object_memory == nullptr) {
            return Optional<TransferRing*>();
        }

        void* const ring_memory = VirtualMemory::AllocateDMA(pages);

        if (ring_memory == nullptr) {
            Heap::Free(object_memory);
            return Optional<TransferRing*>();
        }

        const size_t allocated = pages * Shared::Memory::PAGE_SIZE;
        const size_t capacity = allocated / sizeof(TransferTRB);
        TransferTRB* const ring_base = static_cast<TransferTRB*>(ring_memory);

        Utils::memset(ring_base, 0, allocated);
        
        return Optional<TransferRing*>(new (object_memory) TransferRing(ring_base, capacity));
    }

    const TransferTRB* TransferRing::GetBase() const {
        return base;
    }

    void TransferRing::Release() {
        VirtualMemory::FreeDMA(base, (capacity * sizeof(TransferTRB)));
    }

    bool TransferRing::GetCycle() const {
        return cycle;
    }

    const TRB* TransferRing::Enqueue(const TransferTRB& trb) {
        Utils::LockGuard _{lock};

        const auto* ptr = EnqueueTRB(trb);
        UpdatePointer();
        return ptr;
    }
}
