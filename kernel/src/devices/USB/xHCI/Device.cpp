#include <cstdint>

#include <shared/LockGuard.hpp>
#include <shared/Response.hpp>

#include <devices/USB/xHCI/Controller.hpp>
#include <devices/USB/xHCI/Device.hpp>
#include <devices/USB/xHCI/Specification.hpp>

#include <mm/Heap.hpp>
#include <mm/Utils.hpp>
#include <mm/VirtualMemory.hpp>

#include <sched/Self.hpp>

#include <screen/Log.hpp>

namespace Devices::USB::xHCI {
    void Device::EndpointDescriptor::Release() {
        if (endpointType != EndpointType::Invalid) {
            endpointType = EndpointType::Invalid;
        }
    }

    Success Device::InterfaceDescriptor::AddAlternate(const InterfaceDescriptor& alternate) {
        InterfaceDescriptor* const new_alternate = reinterpret_cast<InterfaceDescriptor*>(Heap::Allocate(sizeof(InterfaceDescriptor)));

        if (new_alternate == nullptr) {
            return Failure();
        }

        *new_alternate = alternate;

        InterfaceDescriptor* current = nextAlternate;

        if (nextAlternate == nullptr) {
            nextAlternate = new_alternate;
        }
        else {
            while (current->nextAlternate != nullptr) {
                current = current->nextAlternate;
            }

            current->nextAlternate = new_alternate;
        }

        return Success();
    }

    void Device::InterfaceDescriptor::Release() {
        if (nextAlternate != nullptr) {
            nextAlternate->Release();
            Heap::Free(nextAlternate);
            nextAlternate = nullptr;
        }

        for (size_t i = 0; i < endpointsNumber; ++i) {
            endpoints[i].Release();
        }

        Heap::Free(endpoints);
        endpoints = nullptr;

        if (extra != nullptr) {
            DeviceSpecificDescriptor* current = extra;
            DeviceSpecificDescriptor* last = current;

            while (current != nullptr) {
                last = current;
                current = current->next;
                Heap::Free(last);
            }

            extra = nullptr;
        }

        if (next != nullptr) {
            next->Release();
            Heap::Free(next);
            next = nullptr;
        }
    }

    Success Device::FunctionDescriptor::AddInterface(const InterfaceDescriptor& interface) {
        InterfaceDescriptor* const new_interface = reinterpret_cast<InterfaceDescriptor*>(Heap::Allocate(sizeof(InterfaceDescriptor)));

        if (new_interface == nullptr) {
            return Failure();
        }

        *new_interface = interface;

        InterfaceDescriptor* current = interfaces;

        if (current == nullptr) {
            interfaces = new_interface;
        }
        else {
            while (current->next != nullptr) {
                current = current->next;
            }

            current->next = new_interface;
        }

        ++interfacesNumber;

        return Success();
    }

    Device::InterfaceDescriptor* Device::FunctionDescriptor::GetInterface(uint8_t id) {
        InterfaceDescriptor* current = interfaces;

        while (current != nullptr) {
            if (current->interfaceNumber == id) {
                return current;
            }

            current = current->next;
        }

        return current;
    }

    void Device::FunctionDescriptor::Release() {
        if (interfaces != nullptr) {
            interfaces->Release();
            Heap::Free(interfaces);
            interfaces = nullptr;
        }

        if (next != nullptr) {
            next->Release();
            Heap::Free(next);
            next = nullptr;
        }
    }

    Optional<Device::FunctionDescriptor*> Device::ConfigurationDescriptor::AddFunction(const Device::FunctionDescriptor& function) {        
        FunctionDescriptor* const new_function = reinterpret_cast<FunctionDescriptor*>(Heap::Allocate(sizeof(FunctionDescriptor)));

        if (new_function == nullptr) {
            return Optional<FunctionDescriptor*>();
        }

        *new_function = function;

        if (functions == nullptr) {
            functions = new_function;
        }
        else {
            FunctionDescriptor* current = functions;

            while (current->next != nullptr) {
                current = current->next;
            }

            current->next = new_function;
        }

        return Optional(new_function);
    }

    Device::FunctionDescriptor* Device::ConfigurationDescriptor::GetFunction(uint8_t fClass, uint8_t fSubClass, uint8_t fProtocol) const {
        FunctionDescriptor* current = functions;

        while (current != nullptr) {
            if (current->functionClass == fClass &&
                current->functionSubClass == fSubClass &&
                current->functionProtocol == fProtocol) {
                return current;
            }

            current = current->next;
        }

        return nullptr;
    }

    void Device::ConfigurationDescriptor::Release() {
        if (valid) {
            for (size_t i = 0; i < interfacesNumber; ++i) {
                functions[i].Release();
            }
            Heap::Free(functions);
            functions = nullptr;

            valid = false;
        }
    }

    Optional<uint16_t> Device::GetDefaultMaxPacketSize() const {
        switch (information.port_speed) {
            case PortSpeed::LowSpeed: return Optional<uint16_t>(8); break;
            case PortSpeed::FullSpeed: return Optional<uint16_t>(64); break;
            case PortSpeed::HighSpeed: return Optional<uint16_t>(64); break;
            case PortSpeed::SuperSpeedGen1x1:
            case PortSpeed::SuperSpeedPlusGen1x2:
            case PortSpeed::SuperSpeedPlusGen2x1:
            case PortSpeed::SuperSpeedPlusGen2x2:
                return Optional<uint16_t>(512); break;
            default:
                return Optional<uint16_t>(); break;
        }
    }

    Success Device::AddressDevice() {
        controller.LoadDeviceSlot(*this);

        const auto command_legacy = AddressDeviceTRB::Create(
            controller.GetCommandCycle(),
            true,
            information.slot_id,
            context_wrapper->GetInputDeviceContextAddress()
        );

        const auto command = AddressDeviceTRB::Create(
            controller.GetCommandCycle(),
            false,
            information.slot_id,
            context_wrapper->GetInputDeviceContextAddress()
        );

        auto result = controller.SendCommand(command_legacy);

        if (!result.HasValue() || result.GetValue().GetCompletionCode() != TRB::CompletionCode::Success) {
            Log::printfSafe("[USB] Legacy addressing failed for device %u, trying non-legacy method\r\n", information.slot_id);
            result = controller.SendCommand(command);

            if (!result.HasValue() || result.GetValue().GetCompletionCode() != TRB::CompletionCode::Success) {
                Release();
                return Failure();
            }
        }
        else {
            result = controller.SendCommand(command);

            if (!result.HasValue() || result.GetValue().GetCompletionCode() != TRB::CompletionCode::Success) {
                Release();
                return Failure();
            }
        }

        return Success();
    }

    Success Device::InitiateTransfer(const TRB* trb, uint32_t reason) {
        static constexpr uint64_t COMPLETION_TIMEOUT_MS = 1000;
        static constexpr auto TRANSFER_STATUS_PREDICATE = [](void* arg) {
            const Device* const device = reinterpret_cast<Device*>(arg);
            return device->transfer_complete.load();
        };

        transfer_complete.store(false);

        awaiting_transfer = trb;
        controller.RingDoorbell(*this, reason);

        const bool result = Self().SpinWaitMillsFor(COMPLETION_TIMEOUT_MS, TRANSFER_STATUS_PREDICATE, this);
        
        transfer_complete.store(false);

        return Success(result);
    }

    size_t Device::GetDescriptorSize(const void* data) {
        const RawDescriptorHeader* header = reinterpret_cast<const RawDescriptorHeader*>(data);
        return static_cast<size_t>(header->length);
    }

    size_t Device::GetDescriptorType(const void* data) {
        const RawDescriptorHeader* header = reinterpret_cast<const RawDescriptorHeader*>(data);
        return static_cast<size_t>(header->descriptorType);
    }

    Success Device::FetchDeviceDescriptor() {
        uint8_t usb_descriptor[DeviceDescriptor::DESCRIPTOR_SIZE] = { 0 };

        {
            Utils::LockGuard _{transfer_lock};

            SetupTRB setup = SetupTRB::Create({
                .bmRequestType = 0x80,
                .bRequest = 6,
                .wValue = 0x0100,
                .wIndex = 0,
                .wLength = DeviceDescriptor::DESCRIPTOR_SIZE,
                .tranferLength = 8,
                .interrupterTarget = 0,
                .cycle = control_transfer_ring->GetCycle(),
                .interruptOnCompletion = false,
                .transferType = TransferType::DataInStage
            });

            control_transfer_ring->Enqueue(setup);

            DataTRB data = DataTRB::Create({
                .bufferPointer = VirtualMemory::GetPhysicalAddress(usb_descriptor),
                .transferLength = DeviceDescriptor::DESCRIPTOR_SIZE,
                .tdSize = 0,
                .interrupterTarget = 0,
                .cycle = control_transfer_ring->GetCycle(),
                .evaluateNextTRB = false,
                .interruptOnShortPacket = false,
                .noSnoop = false,
                .chain = false,
                .interruptOnCompletion = false,
                .immediateData = false,
                .direction = true
            });

            control_transfer_ring->Enqueue(data);

            StatusTRB status = StatusTRB::Create({
                .interrupterTarget = 0,
                .cycle = control_transfer_ring->GetCycle(),
                .evaluateNextTRB = false,
                .chain = false,
                .interruptOnCompletion = true,
                .direction = false
            });

            const auto* ptr = control_transfer_ring->Enqueue(status);

            if (!InitiateTransfer(ptr, 1).IsSuccess()) {
                return Failure();
            }
        }

        if (!CheckDescriptor<DeviceDescriptor>(usb_descriptor).IsSuccess()) {
            return Failure();
        }

        const uint8_t* const protocolVersionMinor = &usb_descriptor[2];
        const uint8_t* const protocolVersionMajor = &usb_descriptor[3];
        const uint8_t* const deviceClass = &usb_descriptor[4];
        const uint8_t* const deviceSubClass = &usb_descriptor[5];
        const uint8_t* const deviceProtocol = &usb_descriptor[6];
        const uint8_t* const bMaxPacketSize0 = &usb_descriptor[7];
        const uint16_t* const vendorID = reinterpret_cast<uint16_t*>(&usb_descriptor[8]);
        const uint16_t* const productID = reinterpret_cast<uint16_t*>(&usb_descriptor[10]);
        const uint8_t* const deviceVersionMinor = &usb_descriptor[12];
        const uint8_t* const deviceVersionMajor = &usb_descriptor[13];
        const uint8_t* const iManufacturer = &usb_descriptor[14];
        const uint8_t* const iProduct = &usb_descriptor[15];
        const uint8_t* const iSerialNumber = &usb_descriptor[16];
        const uint8_t* const configurationsNumber = &usb_descriptor[17];

        uint16_t maxControlPacketSize = 0;

        switch (*protocolVersionMajor) {
            case 1:
            case 2:
                maxControlPacketSize = *bMaxPacketSize0;
                break;
            case 3:
                maxControlPacketSize = static_cast<uint16_t>(1U << *bMaxPacketSize0);
                break;
            default:
                return Failure();
        }

        descriptor = {
            .protocolVersionMajor = *protocolVersionMajor,
            .protocolVersionMinor = *protocolVersionMinor,
            .deviceClass = *deviceClass,
            .deviceSubClass = *deviceSubClass,
            .deviceProtocol = *deviceProtocol,
            .maxControlPacketSize = maxControlPacketSize,
            .vendorID = *vendorID,
            .productID = *productID,
            .deviceVersionMajor = *deviceVersionMajor,
            .deviceVersionMinor = *deviceVersionMinor,
            .manufacturerDescriptorIndex = *iManufacturer,
            .productDescriptorIndex = *iProduct,
            .serialNumberDescriptorIndex = *iSerialNumber,
            .configurationsNumber = *configurationsNumber
        };

        return Success();
    }

    Success Device::FetchConfigurations() {
        const size_t configurations_size = sizeof(ConfigurationDescriptor) * descriptor.configurationsNumber;

        configurations = reinterpret_cast<ConfigurationDescriptor*>(Heap::Allocate(configurations_size));

        if (configurations == nullptr) {
            return Failure();
        }

        for (size_t i = 0; i < descriptor.configurationsNumber; ++i) {
            configurations[i] = ConfigurationDescriptor();
        }


        for (size_t i = 0; i < descriptor.configurationsNumber; ++i) {
            uint8_t config_descriptor[ConfigurationDescriptor::DESCRIPTOR_SIZE] = { 0 };

            {
                Utils::LockGuard _{transfer_lock};

                SetupTRB setup = SetupTRB::Create({
                    .bmRequestType = 0x80,
                    .bRequest = 6,
                    .wValue = static_cast<uint16_t>((2 << 8) | (i)),
                    .wIndex = 0,
                    .wLength = ConfigurationDescriptor::DESCRIPTOR_SIZE,
                    .tranferLength = 8,
                    .interrupterTarget = 0,
                    .cycle = control_transfer_ring->GetCycle(),
                    .interruptOnCompletion = false,
                    .transferType = TransferType::DataInStage
                });

                control_transfer_ring->Enqueue(setup);

                DataTRB data = DataTRB::Create({
                    .bufferPointer = VirtualMemory::GetPhysicalAddress(config_descriptor),
                    .transferLength = ConfigurationDescriptor::DESCRIPTOR_SIZE,
                    .tdSize = 0,
                    .interrupterTarget = 0,
                    .cycle = control_transfer_ring->GetCycle(),
                    .evaluateNextTRB = false,
                    .interruptOnShortPacket = false,
                    .noSnoop = false,
                    .chain = false,
                    .interruptOnCompletion = false,
                    .immediateData = false,
                    .direction = true
                });

                control_transfer_ring->Enqueue(data);

                StatusTRB status = StatusTRB::Create({
                    .interrupterTarget = 0,
                    .cycle = control_transfer_ring->GetCycle(),
                    .evaluateNextTRB = false,
                    .chain = false,
                    .interruptOnCompletion = true,
                    .direction = false
                });

                const auto* ptr = control_transfer_ring->Enqueue(status);

                if (!InitiateTransfer(ptr, 1).IsSuccess()) {
                    configurations[i].valid = false;
                    Log::printfSafe("[USB] Failed to fetch configuration descriptor %u for device %u\r\n", i, information.slot_id);
                    continue;
                }
            }

            auto config_result = ParseConfigurationDescriptor(config_descriptor, i);

            if (!config_result.HasValue()) {
                configurations[i].valid = false;
                Log::printfSafe("[USB] Failed to parse configuration descriptor %u for device %u\r\n", i, information.slot_id);
            }
            else {
                configurations[i] = config_result.GetValue();
                Log::printfSafe("[USB] Fetched configuration descriptor %u for device %u\r\n", i, information.slot_id);
            }
        }

        return Success();
    }

    Optional<Device::ConfigurationDescriptor> Device::ParseConfigurationDescriptor(const uint8_t* data, uint8_t index) {
        if (!CheckDescriptor<ConfigurationDescriptor>(data).IsSuccess()) {
            return Optional<ConfigurationDescriptor>();
        }
        
        const uint16_t* const totalLength = reinterpret_cast<const uint16_t*>(&data[2]);
        const uint8_t* const interfacesNumber = &data[4];
        const uint8_t* const configurationValue = &data[5];
        const uint8_t* const configurationDescriptorIndex = &data[6];
        const uint8_t* const attributes = &data[7];
        const uint8_t* const maxPower = &data[8];

        if (*interfacesNumber == 0) {
            return Optional<ConfigurationDescriptor>();
        }

        ConfigurationDescriptor config_descriptor = {
            .valid = true,
            .interfacesNumber = *interfacesNumber,
            .configurationValue = *configurationValue,
            .configurationDescriptorIndex = *configurationDescriptorIndex,
            .selfPowered = (*attributes & 0x40) != 0,
            .supportsRemoteWakeup = (*attributes & 0x20) != 0,
            .maxPower = *maxPower,
            .functionsNumber = 0,
            .functions = nullptr
        };

        uint8_t* buffer = reinterpret_cast<uint8_t*>(Heap::Allocate(*totalLength));

        if (buffer == nullptr) {
            return Optional<ConfigurationDescriptor>();
        }

        {
            Utils::LockGuard _{transfer_lock};

            SetupTRB setup = SetupTRB::Create({
                .bmRequestType = 0x80,
                .bRequest = 6,
                .wValue = static_cast<uint16_t>((2 << 8) | (index)),
                .wIndex = 0,
                .wLength = *totalLength,
                .tranferLength = 8,
                .interrupterTarget = 0,
                .cycle = control_transfer_ring->GetCycle(),
                .interruptOnCompletion = false,
                .transferType = TransferType::DataInStage
            });

            control_transfer_ring->Enqueue(setup);

            DataTRB data = DataTRB::Create({
                .bufferPointer = VirtualMemory::GetPhysicalAddress(buffer),
                .transferLength = *totalLength,
                .tdSize = 0,
                .interrupterTarget = 0,
                .cycle = control_transfer_ring->GetCycle(),
                .evaluateNextTRB = false,
                .interruptOnShortPacket = false,
                .noSnoop = false,
                .chain = false,
                .interruptOnCompletion = false,
                .immediateData = false,
                .direction = true
            });

            control_transfer_ring->Enqueue(data);

            StatusTRB status = StatusTRB::Create({
                .interrupterTarget = 0,
                .cycle = control_transfer_ring->GetCycle(),
                .evaluateNextTRB = false,
                .chain = false,
                .interruptOnCompletion = true,
                .direction = false
            });

            const auto* ptr = control_transfer_ring->Enqueue(status);

            if (!InitiateTransfer(ptr, 1).IsSuccess()) {
                Heap::Free(buffer);
                return Optional<ConfigurationDescriptor>();
            }
        }

        const uint8_t* ptr = buffer + GetDescriptorSize(data);
        const uint8_t* const limit = buffer + *totalLength;

        bool found_valid_interface = false;

        while (ptr < limit) {
            // check for interface associations
            static constexpr uint8_t INTERFACE_ASSOCIATION_TYPE = 11;
            static constexpr uint8_t INTERFACE_ASSOCIATION_SIZE = 8;

            if (GetDescriptorType(ptr) == INTERFACE_ASSOCIATION_TYPE) {
                const auto descriptor_size = GetDescriptorSize(ptr);
                if (descriptor_size >= INTERFACE_ASSOCIATION_SIZE && ptr + descriptor_size <= limit) {
                    FunctionDescriptor new_function = {
                        .functionClass = ptr[4],
                        .functionSubClass = ptr[5],
                        .functionProtocol = ptr[6],
                        .functionDescriptorIndex = ptr[7],
                        .interfacesNumber = 0,
                        .interfaces = nullptr,
                        .next = nullptr
                    };

                    if (config_descriptor.AddFunction(new_function).HasValue()) {
                        Log::printfSafe("[USB] Created explicit interface association\r\n");
                    }
                    else {
                        Log::printfSafe("[USB] Failed to create explicit interface association\r\n");
                    }
                }

                ptr += descriptor_size;
                continue;
            }

            auto interface_wrapper = ParseInterfaceDescriptor(ptr, limit);

            if (interface_wrapper.HasValue()) {
                const auto& interface = interface_wrapper.GetValue();

                FunctionDescriptor* function = config_descriptor.GetFunction(
                    interface.interfaceClass,
                    interface.interfaceSubClass,
                    interface.interfaceProtocol
                );

                if (function == nullptr) {
                    FunctionDescriptor new_function = {
                        .functionClass = interface.interfaceClass,
                        .functionSubClass = interface.interfaceSubClass,
                        .functionProtocol = interface.interfaceProtocol,
                        .functionDescriptorIndex = 0,
                        .interfacesNumber = 0,
                        .interfaces = nullptr,
                        .next = nullptr
                    };

                    auto result = config_descriptor.AddFunction(new_function);

                    if (!result.HasValue()) {
                        Log::printfSafe("[USB] Failed to create function for interface %u\n\r", interface.descriptor.interfaceNumber);

                        while (ptr < limit && GetDescriptorType(ptr) != InterfaceDescriptor::DESCRIPTOR_TYPE) {
                            ptr += GetDescriptorSize(ptr);
                        }

                        continue;
                    }

                    function = result.GetValue();
                }

                const auto& raw_interface = interface.descriptor;
                auto* const function_interface = function->GetInterface(raw_interface.interfaceNumber);

                if (function_interface != nullptr) {
                    if (function_interface->AddAlternate(raw_interface).IsSuccess()) {
                        Log::printfSafe(
                            "[USB] Parsed interface %u.%u\r\n",
                            raw_interface.interfaceNumber,
                            raw_interface.alternateSetting
                        );
                    }                    
                    else {
                        Log::printfSafe(
                            "[USB] Failed to add alternate interface %u.%u\r\n",
                            raw_interface.interfaceNumber,
                            raw_interface.alternateSetting
                        );
                    }
                }
                else {
                    if (function->AddInterface(raw_interface).IsSuccess()) {
                        Log::printfSafe("[USB] Parsed interface %u.%u\r\n", raw_interface.interfaceNumber, raw_interface.alternateSetting);
                        found_valid_interface = true;
                    }
                    else {
                        Log::printfSafe("[USB] Failed to add interface %u.%u\r\n", raw_interface.interfaceNumber, raw_interface.alternateSetting);
                    }
                }
            }
            else {
                Log::printfSafe("[USB] Failed to parse interface descriptor in configuration %u\r\n", index);
            }
        }

        Heap::Free(buffer);

        if (!found_valid_interface) {
            config_descriptor.Release();
            return Optional<ConfigurationDescriptor>();
        }

        return Optional<ConfigurationDescriptor>(config_descriptor);
    }

    Optional<Device::InterfaceWrapper> Device::ParseInterfaceDescriptor(const uint8_t*& data, const uint8_t* limit) {        
        if (!CheckDescriptor<InterfaceDescriptor>(data).IsSuccess() || data + InterfaceDescriptor::DESCRIPTOR_SIZE > limit) {
            data += GetDescriptorSize(data);
            return Optional<InterfaceWrapper>();
        }

        const uint8_t* const interfaceNumber = &data[2];
        const uint8_t* const alternateSetting = &data[3];
        const uint8_t* const endpointsNumber = &data[4];
        const uint8_t* const interfaceClass = &data[5];
        const uint8_t* const interfaceSubClass = &data[6];
        const uint8_t* const interfaceProtocol = &data[7];
        const uint8_t* const interfaceDescriptorIndex = &data[8];

        data += GetDescriptorSize(data);

        InterfaceDescriptor interface_descriptor = {
            .interfaceNumber = *interfaceNumber,
            .alternateSetting = *alternateSetting,
            .endpointsNumber = *endpointsNumber,
            .interfaceDescriptorIndex = *interfaceDescriptorIndex,
            .endpoints = nullptr,
            .nextAlternate = nullptr,
            .next = nullptr
        };

        if (endpointsNumber != 0) {
            const size_t endpoint_size = sizeof(EndpointDescriptor) * interface_descriptor.endpointsNumber;

            interface_descriptor.endpoints = reinterpret_cast<EndpointDescriptor*>(Heap::Allocate(endpoint_size));

            if (interface_descriptor.endpoints == nullptr) {
                interface_descriptor.Release();
                return Optional<InterfaceWrapper>();
            }

            for (size_t i = 0; i < interface_descriptor.endpointsNumber; ++i) {
                interface_descriptor.endpoints[i] = EndpointDescriptor();
            }

            for (size_t descriptor_index = 0; descriptor_index < interface_descriptor.endpointsNumber && data < limit; ++descriptor_index) {
                while (GetDescriptorType(data) != EndpointDescriptor::DESCRIPTOR_TYPE && data < limit) {
                    auto extra_wrapper = ParseDeviceSpecificDescriptor(data, limit);

                    if (extra_wrapper.HasValue()) {
                        DeviceSpecificDescriptor* extra = extra_wrapper.GetValue();

                        if (interface_descriptor.extra == nullptr) {
                            interface_descriptor.extra = extra;
                        }
                        else {
                            DeviceSpecificDescriptor* current = interface_descriptor.extra;

                            while (current->next != nullptr) {
                                current = current->next;
                            }

                            current->next = extra;
                        }

                        Log::printfSafe(
                            "[USB] Parsed extra descriptor (type 0x%0.2hhx) for interface %u.%u\r\n",
                            extra->descriptorType,
                            interface_descriptor.interfaceNumber,
                            interface_descriptor.alternateSetting
                        );
                    }
                }
                
                auto endpoint_wrapper = ParseEndpointDescriptor(data, limit);

                if (endpoint_wrapper.HasValue()) {
                    interface_descriptor.endpoints[descriptor_index] = endpoint_wrapper.GetValue();
                    Log::printfSafe(
                        "[USB] Parsed endpoint %u of interface %u.%u\r\n",
                        interface_descriptor.endpoints[descriptor_index].endpointAddress,
                        interface_descriptor.interfaceNumber,
                        interface_descriptor.alternateSetting
                    );
                }
                else {
                    // If we can't parse an interface endpoint, fail on this interface
                    Log::printfSafe(
                        "[USB] Failed to parse endpoint %u of interface %u.%u\r\n",
                        descriptor_index,
                        interface_descriptor.interfaceNumber,
                        interface_descriptor.alternateSetting
                    );
                    interface_descriptor.Release();
                    return Optional<InterfaceWrapper>();
                }
            }
        }

        return Optional<InterfaceWrapper>({
            .interfaceClass = *interfaceClass,
            .interfaceSubClass = *interfaceSubClass,
            .interfaceProtocol = *interfaceProtocol,
            .descriptor = interface_descriptor
        });
    }

    Optional<Device::EndpointDescriptor> Device::ParseEndpointDescriptor(const uint8_t*& data, const uint8_t* limit) {
        if (!CheckDescriptor<EndpointDescriptor>(data).IsSuccess() || data + EndpointDescriptor::DESCRIPTOR_SIZE > limit) {
            Log::printfSafe("%u : %u\n\r", GetDescriptorSize(data), GetDescriptorType(data));
            data += GetDescriptorSize(data);
            return Optional<EndpointDescriptor>();
        }

        const uint8_t bEndpointAddress = data[2];
        const uint8_t bmAttributes = data[3];
        const uint16_t maxPacketSize = *reinterpret_cast<const uint16_t*>(&data[4]);
        const uint8_t interval = data[6];

        data += GetDescriptorSize(data);

        static constexpr uint8_t ADDRESS_MASK       = 0x0F;
        static constexpr uint8_t DIRECTION_IN_MASK  = 0x80;

        static constexpr uint8_t TYPE_CONTROL       = 0;
        static constexpr uint8_t TYPE_ISOCHRONOUS   = 1;
        static constexpr uint8_t TYPE_BULK          = 2;
        static constexpr uint8_t TYPE_INTERRUPT     = 3;

        uint8_t endpoint_address = bEndpointAddress & ADDRESS_MASK;

        if (endpoint_address == 0) {
            return Optional<EndpointDescriptor>();
        }

        EndpointType endpoint_type = EndpointType::Invalid;

        if ((bEndpointAddress & DIRECTION_IN_MASK) == 0) {
            switch (bmAttributes & ADDRESS_MASK) {
                case TYPE_CONTROL:     endpoint_type = EndpointType::ControlBidirectional; break;
                case TYPE_ISOCHRONOUS: endpoint_type = EndpointType::IsochronousOut; break;
                case TYPE_BULK:        endpoint_type = EndpointType::BulkOut; break;
                case TYPE_INTERRUPT:   endpoint_type = EndpointType::InterruptOut; break;
            }
        }
        else {
            switch (bmAttributes & ADDRESS_MASK) {
                case TYPE_CONTROL:     endpoint_type = EndpointType::ControlBidirectional; break;
                case TYPE_ISOCHRONOUS: endpoint_type = EndpointType::IsochronousIn; break;
                case TYPE_BULK:        endpoint_type = EndpointType::BulkIn; break;
                case TYPE_INTERRUPT:   endpoint_type = EndpointType::InterruptIn; break;
            }
        }

        static constexpr uint8_t INTERRUPT_TYPE_MASK    = 0x30;
        static constexpr uint8_t INTERRUPT_TYPE_SHIFT   = 4;
        static constexpr uint8_t INTERRUPT_PERIODIC     = 0;
        static constexpr uint8_t INTERRUPT_NOTIFICATION = 1;

        static constexpr uint8_t ISOCH_SYNC_TYPE_MASK   = 0x0C;
        static constexpr uint8_t ISOCH_SYNC_TYPE_SHIFT  = 2;
        static constexpr uint8_t ISOCH_NO_SYNC          = 0;
        static constexpr uint8_t ISOCH_ASYNC            = 1;
        static constexpr uint8_t ISOCH_ADAPTIVE         = 2;
        static constexpr uint8_t ISOCH_SYNC             = 3;

        static constexpr uint8_t ISOCH_USAGE_TYPE_MASK  = 0x30;
        static constexpr uint8_t ISOCH_USAGE_TYPE_SHIFT = 4;
        static constexpr uint8_t ISOCH_DATA             = 0;
        static constexpr uint8_t ISOCH_FEEDBACK         = 1;
        static constexpr uint8_t ISOCH_IMPLICIT         = 2;

        EndpointDescriptor::ExtraConfig extra_config = {};

        if (endpoint_type == EndpointType::InterruptIn || endpoint_type == EndpointType::InterruptOut) {
            const uint8_t interrupt_type = (bmAttributes & INTERRUPT_TYPE_MASK) >> INTERRUPT_TYPE_SHIFT;

            switch (interrupt_type) {
                case INTERRUPT_PERIODIC:
                    extra_config.interruptUsage = EndpointDescriptor::InterruptUsage::Periodic;
                    break;
                case INTERRUPT_NOTIFICATION:
                    extra_config.interruptUsage = EndpointDescriptor::InterruptUsage::Notification;
                    break;
                default:
                    return Optional<EndpointDescriptor>();
            }
        }
        else if (endpoint_type == EndpointType::IsochronousIn || endpoint_type == EndpointType::IsochronousOut) {
            const uint8_t sync_type = (bmAttributes & ISOCH_SYNC_TYPE_MASK) >> ISOCH_SYNC_TYPE_SHIFT;
            const uint8_t usage_type = (bmAttributes & ISOCH_USAGE_TYPE_MASK) >> ISOCH_USAGE_TYPE_SHIFT;

            switch (sync_type) {
                case ISOCH_NO_SYNC:
                    extra_config.isochSync = EndpointDescriptor::IsochronousSynchronization::None;
                    break;
                case ISOCH_ASYNC:
                    extra_config.isochSync = EndpointDescriptor::IsochronousSynchronization::Asynchronous;
                    break;
                case ISOCH_ADAPTIVE:
                    extra_config.isochSync = EndpointDescriptor::IsochronousSynchronization::Adaptive;
                    break;
                case ISOCH_SYNC:
                    extra_config.isochSync = EndpointDescriptor::IsochronousSynchronization::Synchronous;
                    break;
                default:
                    return Optional<EndpointDescriptor>();
            }

            switch (usage_type) {
                case ISOCH_DATA:
                    extra_config.isochUsage = EndpointDescriptor::IsochronousUsage::Data;
                    break;
                case ISOCH_FEEDBACK:
                    extra_config.isochUsage = EndpointDescriptor::IsochronousUsage::Feedback;
                    break;
                case ISOCH_IMPLICIT:
                    extra_config.isochUsage = EndpointDescriptor::IsochronousUsage::ImplicitFeedback;
                    break;
                default:
                    return Optional<EndpointDescriptor>();
            }
        }

        // fetch SuperSpeed configuration
        static constexpr uint8_t SUPER_SPEED_ENDPOINT_COMPANION_DESCRIPTOR_TYPE = 0x30;
        static constexpr uint8_t SUPER_SPEED_ENDPOINT_COMPANION_DESCRIPTOR_SIZE = 6;
        static constexpr uint8_t SUPER_SPEED_PLUS_ISOCH_ENDPOINT_COMPANION_DESCRIPTOR_TYPE = 0x31;
        static constexpr uint8_t SUPER_SPEED_PLUS_ISOCH_ENDPOINT_COMPANION_DESCRIPTOR_SIZE = 8;

        EndpointDescriptor::SuperSpeedConfig superSpeedConfig = {};

        if (GetDescriptorType(data) == SUPER_SPEED_ENDPOINT_COMPANION_DESCRIPTOR_TYPE) {
            if (GetDescriptorSize(data) < SUPER_SPEED_ENDPOINT_COMPANION_DESCRIPTOR_SIZE) {
                data += GetDescriptorSize(data);
                return Optional<EndpointDescriptor>();
            }
            
            const uint8_t maxBurst = data[2];
            const uint8_t attributes = data[3];
            const uint16_t bytesPerInterval = *reinterpret_cast<const uint16_t*>(&data[4]);

            data += GetDescriptorSize(data);

            superSpeedConfig.valid = true;
            superSpeedConfig.maxBurst = maxBurst + 1;

            static constexpr uint8_t MAX_STREAMS_MASK = 0x1F;
            static constexpr uint8_t MULT_MASK = 0x03;
            static constexpr uint8_t SSP_ISO_MASK = 0x80;

            switch (endpoint_type) {
                case EndpointType::BulkIn:
                case EndpointType::BulkOut:
                    superSpeedConfig.maxStreams = (attributes & MAX_STREAMS_MASK);
                    if (superSpeedConfig.maxStreams > 0) {
                        superSpeedConfig.maxStreams = 1U << superSpeedConfig.maxStreams;
                    }
                    break;
                case EndpointType::IsochronousIn:
                case EndpointType::IsochronousOut:
                    superSpeedConfig.bytesPerInterval = bytesPerInterval;

                    if ((attributes & SSP_ISO_MASK) != 0) {
                        if (GetDescriptorType(data) != SUPER_SPEED_PLUS_ISOCH_ENDPOINT_COMPANION_DESCRIPTOR_TYPE) {
                            return Optional<EndpointDescriptor>();
                        }

                        if (GetDescriptorSize(data) < SUPER_SPEED_PLUS_ISOCH_ENDPOINT_COMPANION_DESCRIPTOR_SIZE) {
                            data += GetDescriptorSize(data);
                            return Optional<EndpointDescriptor>();
                        }

                        superSpeedConfig.bytesPerInterval = *reinterpret_cast<const uint32_t*>(&data[4]);
                        superSpeedConfig.maxPacketsPerInterval = (superSpeedConfig.bytesPerInterval + maxPacketSize - 1) / maxPacketSize;
                    }
                    else {
                        superSpeedConfig.maxPacketsPerInterval = ((attributes & MULT_MASK) + 1) * (superSpeedConfig.maxBurst + 1);
                    }

        
                    break;
                default:
                    break;
            }
        }
        else {
            superSpeedConfig.valid = false;
        }
        
        EndpointDescriptor endpoint_descriptor = {
            .endpointAddress = endpoint_address,
            .endpointType = endpoint_type,
            .extraConfig = extra_config,
            .maxPacketSize = maxPacketSize,
            .interval = interval,
            .superSpeedConfig = superSpeedConfig
        };
        
        return Optional<EndpointDescriptor>(endpoint_descriptor);
    }

    Optional<Device::DeviceSpecificDescriptor*> Device::ParseDeviceSpecificDescriptor(const uint8_t*& data, const uint8_t* limit) {
        if (data + GetDescriptorSize(data) > limit) {
            data += GetDescriptorSize(data);
            return Optional<DeviceSpecificDescriptor*>();
        }

        const uint8_t length = data[0];
        const uint8_t descriptorType = data[1];

        DeviceSpecificDescriptor* descriptor = reinterpret_cast<DeviceSpecificDescriptor*>(Heap::Allocate(length));

        if (descriptor == nullptr) {
            data += length;
            return Optional<DeviceSpecificDescriptor*>();
        }

        descriptor->descriptorType = descriptorType;
        descriptor->length = length;
        descriptor->next = nullptr;
        Utils::memcmp(descriptor->data, data + 2, length - 2);

        data += length;

        return Optional<DeviceSpecificDescriptor*>(descriptor);
    }

    Device::Device(Controller& controller, const DeviceInformation& information)
        : controller{controller}, information{information} {}

    const DeviceInformation& Device::GetInformation() const {
        return information;
    }

    const void* Device::GetOutputDeviceContext() const {
        return context_wrapper->GetOutputDeviceContextAddress();
    }

    Success Device::Initialize() {
        Log::printfSafe("[USB] Initializing device %u\r\n", information.slot_id);
        
        // Initialize contexts
        auto context_wrapper_result = ContextWrapper::Create(controller.HasExtendedContext());

        if (!context_wrapper_result.HasValue()) {
            Log::printfSafe("[USB] Failed to create context wrapper for device %u\r\n", information.slot_id);
            return Failure();
        }

        context_wrapper = context_wrapper_result.GetValue();
        context_wrapper->Reset();

        auto* input_control_context = context_wrapper->GetInputControlContext();
        input_control_context->SetAddContext(0);
        input_control_context->SetAddContext(1);

        auto* input_slot_context = context_wrapper->GetSlotContext(true);
        input_slot_context->SetRootHubPort(information.root_hub_port);
        input_slot_context->SetRouteString(information.route_string);
        input_slot_context->SetContextEntries(1);

        // Initialize transfer ring for control endpoint
        auto transfer_ring_result = TransferRing::Create(1);
        if (!transfer_ring_result.HasValue()) {
            Log::printfSafe("[USB] Failed to create control transfer ring for device %u\r\n", information.slot_id);
            Release();
            return Failure();
        }

        control_transfer_ring = transfer_ring_result.GetValue();

        auto* control_endpoint_context = context_wrapper->GetControlEndpointContext(true);

        // Compute default control endpoint max packet size
        auto max_packet_size = GetDefaultMaxPacketSize();
        if (!max_packet_size.HasValue()) {
            Log::printfSafe("[USB] Failed to determine default max packet size for device %u\r\n", information.slot_id);
            Release();
            return Failure();
        }

        control_endpoint_context->SetEndpointType(EndpointType::ControlBidirectional);
        control_endpoint_context->SetMaxPacketSize(max_packet_size.GetValue());
        control_endpoint_context->SetMaxBurstSize(0);
        control_endpoint_context->SetTRDequeuePointer(control_transfer_ring->GetBase());
        control_endpoint_context->SetDCS(control_transfer_ring->GetCycle());
        control_endpoint_context->SetInterval(0);
        control_endpoint_context->SetMaxPStreams(0);
        control_endpoint_context->SetMult(0);
        control_endpoint_context->SetErrorCount(3);

        if (!AddressDevice().IsSuccess()) {
            Log::printfSafe("[USB] Addressing failed for device %u\r\n", information.slot_id);
            Release();
            return Failure();
        }

        Log::printfSafe("[USB] Device %u successfully addressed\r\n", information.slot_id);

        // Get device descriptor
        if (!FetchDeviceDescriptor().IsSuccess()) {
            Log::printfSafe("[USB] Failed to fetch device descriptor for device %u\n\r", information.slot_id);
            Release();
            return Failure();
        }

        Log::printfSafe("[USB] Fetched device descriptor for device %u\n\r", information.slot_id);

        if (!FetchConfigurations().IsSuccess()) {
            Log::printfSafe("[USB] Failed to fetch configurations for device %u\n\r", information.slot_id);
            Release();
            return Failure();
        }

        Log::printfSafe("[USB] Fetched configurations for device %u\n\r", information.slot_id);

        Log::printfSafe("[USB] Enumerating configuration topology...\n\r");

        // enumerate configurations topology
        for (size_t i = 0; i < descriptor.configurationsNumber; ++i) {
            if (!configurations[i].valid) {
                continue;
            }

            Log::printfSafe("[USB] Configuration %u:\r\n", i);

            FunctionDescriptor* function = configurations[i].functions;
            while (function != nullptr) {
                Log::printfSafe(
                    "[USB]   Function - Class: 0x%0.2hhx, SubClass: 0x%0.2hhx, Protocol: 0x%0.2hhx\r\n",
                    function->functionClass,
                    function->functionSubClass,
                    function->functionProtocol
                );

                InterfaceDescriptor* interface = function->interfaces;
                while (interface != nullptr) {
                    Log::printfSafe(
                        "[USB]     Interface %u.%u - %u endpoints\r\n",
                        interface->interfaceNumber,
                        interface->alternateSetting,
                        interface->endpointsNumber
                    );

                    for (size_t j = 0; j < interface->endpointsNumber; ++j) {
                        const auto& endpoint = interface->endpoints[j];

                        Log::printfSafe(
                            "[USB]       Endpoint 0x%0.2hhx - Type: %u, MaxPacketSize: %u\r\n",
                            endpoint.endpointAddress,
                            endpoint.endpointType.ToEndpointType(),
                            endpoint.maxPacketSize
                        );

                        if (endpoint.endpointType == EndpointType::InterruptIn
                            || endpoint.endpointType == EndpointType::InterruptOut
                        ) {
                            Log::printfSafe(
                                "[USB]         Interrupt Usage: %u\r\n",
                                endpoint.extraConfig.interruptUsage
                            );
                        }
                        else if (endpoint.endpointType == EndpointType::IsochronousIn || endpoint.endpointType == EndpointType::IsochronousOut) {
                            Log::printfSafe(
                                "[USB]         Isoch Sync: %u, Usage: %u\r\n",
                                endpoint.extraConfig.isochSync,
                                endpoint.extraConfig.isochUsage
                            );
                        }

                        if (endpoint.superSpeedConfig.valid) {
                            Log::printfSafe(
                                "[USB]         SuperSpeed - MaxBurst: %u, MaxStreams: %u, BytesPerInterval: %u\r\n",
                                endpoint.superSpeedConfig.maxBurst,
                                endpoint.superSpeedConfig.maxStreams,
                                endpoint.superSpeedConfig.bytesPerInterval
                            );
                        }
                    }

                    interface = interface->next;
                }

                function = function->next;
            }
        }

        return Success();
    }

    void Device::Release() {
        if (context_wrapper != nullptr) {
            context_wrapper->Release();
            context_wrapper = nullptr;
        }

        if (control_transfer_ring != nullptr) {
            control_transfer_ring->Release();
            control_transfer_ring = nullptr;
        }

        if (configurations != nullptr) {
            for (size_t i = 0; i < descriptor.configurationsNumber; ++i) {
                configurations[i].Release();
            }
            Heap::Free(configurations);
            configurations = nullptr;
        }
    }

    void Device::SignalTransferComplete(const TransferEventTRB& trb) {
        if (trb.GetPointer() == VirtualMemory::GetPhysicalAddress(awaiting_transfer)) {
            transfer_result = trb;
            transfer_complete.store(true);
        }
    }

    const Device::DeviceDescriptor& Device::GetDeviceDescriptor() const {
        return descriptor;
    }
}
