#include <cstdint>

#include <new>

#include <shared/Response.hpp>

#include <devices/USB/xHCI/Specification.hpp>

#include <mm/Heap.hpp>
#include <mm/VirtualMemory.hpp>

namespace Devices::USB::xHCI {
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
        if (*this == FullSpeed) return 1;
        else if (*this == LowSpeed) return 2;
        else if (*this == HighSpeed) return 3;
        else if (*this == SuperSpeedGen1x1) return 4;
        else if (*this == SuperSpeedPlusGen2x1) return 5;
        else if (*this == SuperSpeedPlusGen1x2) return 6;
        else if (*this == SuperSpeedPlusGen2x2) return 7;
        else return 0;
    }

    bool PortSpeed::operator==(const decltype(InvalidSpeed)& speed) const {
        return value == speed;
    }

    bool PortSpeed::operator!=(const decltype(InvalidSpeed)& speed) const {
        return value != speed;
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
        return data[0] & ROUTE_STRING_MASK;
    }

    void SlotContext::SetRouteString(uint32_t route_string) {
        data[0] = (data[0] & ~ROUTE_STRING_MASK) | (route_string & ROUTE_STRING_MASK); 
    }

    PortSpeed SlotContext::GetPortSpeed() const {
        return PortSpeed::FromSpeedID((data[0] & PORT_SPEED_MASK) >> PORT_SPEED_SHIFT);
    }

    void SlotContext::SetPortSpeed(const PortSpeed& speed) {
        data[0] = (data[0] & ~PORT_SPEED_MASK) | ((speed.ToSpeedID() << PORT_SPEED_SHIFT) & PORT_SPEED_MASK);
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

    bool EndpointType::operator==(const decltype(Invalid)& type) const {
        return value == type;
    }

    bool EndpointType::operator!=(const decltype(Invalid)& type) const {
        return value != type;
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
        const uint32_t extended_interface_number = static_cast<uint32_t>(id) << INTERFACE_NUMBER_SHIFT;
        data[7] = (data[7] & ~INTERFACE_NUMBER_MASK) | (extended_interface_number & INTERFACE_NUMBER_MASK);
    }

    void InputControlContext::SetAlternateSetting(uint8_t v) {
        static constexpr uint32_t ALTERNATE_SETTING_MASK = 0x00FF0000;
        static constexpr uint8_t ALTERNATE_SETTING_SHIFT = 16;
        const uint32_t extended_alternate_setting = static_cast<uint32_t>(v) << ALTERNATE_SETTING_SHIFT;
        data[7] = (data[7] & ~ALTERNATE_SETTING_MASK) & (extended_alternate_setting & ALTERNATE_SETTING_MASK);
    }

    Optional<ContextWrapper*> ContextWrapper::Create(bool extended) {
        if (extended) {
            return ContextWrapperEx::Create();
        }
        
        return ContextWrapperBasic::Create();
    }

    ContextWrapperBasic::ContextWrapperBasic(OutputDeviceContext* out, InputDeviceContext* in)
        : output{out}, input{in} {}

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
}
