// SPDX-License-Identifier: GPL-3.0-only
//
// Copyright (C) 2026 Alexandre Boissiere
// This file is part of the BadLands operating system.
//
// This program is free software: you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation, version 3.
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
// without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with this program.
// If not, see <https://www.gnu.org/licenses/>. 

#include <cstdint>

#include <new>

#include <utility>

#include <shared/Debug.hpp>
#include <shared/LockGuard.hpp>
#include <shared/Response.hpp>

#include <devices/USB/HID/Driver.hpp>
#include <devices/USB/MassStorage/Device.hpp>
#include <devices/USB/xHCI/Controller.hpp>
#include <devices/USB/xHCI/Device.hpp>
#include <devices/USB/xHCI/Specification.hpp>

#include <mm/Heap.hpp>
#include <mm/IOHeap.hpp>
#include <mm/Paging.hpp>
#include <mm/Utils.hpp>

#include <sched/Self.hpp>

#include <screen/Log.hpp>

namespace Devices::USB::xHCI {
    void Device::EndpointDescriptor::Release() {
        if (endpointType != EndpointType::Invalid) {
            endpointType = EndpointType::Invalid;
        }
    }

    void Device::InterfaceDescriptor::AddExtra(DeviceSpecificDescriptor* descriptor) {
        DeviceSpecificDescriptor* current = extra;

        if (current == nullptr) {
            extra = descriptor;
        }
        else {
            while (current->next != nullptr) {
                current = current->next;
            }

            current->next = descriptor;
        }
    }

    Optional<Device::DeviceSpecificDescriptor*> Device::InterfaceDescriptor::GetExtra(uint8_t type) {
        DeviceSpecificDescriptor* current = extra;

        while (current != nullptr && current->descriptorType != type) {
            current = current->next;
        }

        return current == nullptr ? Optional<DeviceSpecificDescriptor*>() : Optional<DeviceSpecificDescriptor*>(current);
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
            for (size_t i = 0; i < functionsNumber; ++i) {
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
            if constexpr (Debug::DEBUG_USB_ERRORS) {
                Log::printfSafe("[USB] Legacy addressing failed for device %u, trying non-legacy method\r\n", information.slot_id);
            }

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
        Utils::memset(&transfer_result.data, 0, sizeof(transfer_result.data));

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

    Success Device::SendRequest(
        Device& device,
        uint8_t bmRequestType,
        uint8_t bRequest,
        uint16_t wValue,
        uint16_t wIndex,
        uint16_t wLength,
        uint8_t* buffer
    ) {
        Utils::LockGuard _{device.transfer_lock};

        SetupTRB setup = SetupTRB::Create({
            .bmRequestType = bmRequestType,
            .bRequest = bRequest,
            .wValue = wValue,
            .wIndex = wIndex,
            .wLength = wLength,
            .tranferLength = 8,
            .interrupterTarget = 0,
            .cycle = device.control_transfer_ring->GetCycle(),
            .interruptOnCompletion = false,
            .transferType = TransferType::DataInStage
        });

        device.control_transfer_ring->Enqueue(setup);

        if (buffer != nullptr) {
            const auto buffer_pointer_wrapper = Paging::GetPhysicalAddress(buffer);

            if (!buffer_pointer_wrapper.HasValue()) {
                return Failure();
            }

            DataTRB data = DataTRB::Create({
                .bufferPointer = buffer_pointer_wrapper.GetValue(),
                .transferLength = wLength,
                .tdSize = 0,
                .interrupterTarget = 0,
                .cycle = device.control_transfer_ring->GetCycle(),
                .evaluateNextTRB = false,
                .interruptOnShortPacket = false,
                .noSnoop = false,
                .chain = false,
                .interruptOnCompletion = false,
                .immediateData = false,
                .direction = true
            });

            device.control_transfer_ring->Enqueue(data);
        }

        StatusTRB status = StatusTRB::Create({
            .interrupterTarget = 0,
            .cycle = device.control_transfer_ring->GetCycle(),
            .evaluateNextTRB = false,
            .chain = false,
            .interruptOnCompletion = true,
            .direction = false
        });

        const auto* ptr = device.control_transfer_ring->Enqueue(status);

        if (!device.InitiateTransfer(ptr, 1).IsSuccess() || device.transfer_result.GetCompletionCode() != TRB::CompletionCode::Success) {
            return Failure();
        }

        return Success();
    }

    Optional<uint8_t*> Device::GetDescriptor(uint8_t type, uint8_t index, uint8_t languageID) {
        // alignas to prevent page boundary crossing issues
        static_assert(sizeof(RawDescriptorHeader) < 0x8);
        alignas(0x8) RawDescriptorHeader header;
        
        if (!GetDescriptor(type, index, sizeof(RawDescriptorHeader), reinterpret_cast<uint8_t*>(&header), languageID).IsSuccess()) {
            return Optional<uint8_t*>();
        }

        const size_t descriptor_size = static_cast<size_t>(header.length);

        uint8_t* descriptor_data = reinterpret_cast<uint8_t*>(IOHeap::Allocate(descriptor_size));

        if (descriptor_data == nullptr) {
            return Optional<uint8_t*>();
        }

        if (!GetDescriptor(type, index, static_cast<uint16_t>(descriptor_size), descriptor_data, languageID).IsSuccess()) {
            IOHeap::Free(descriptor_data);
            return Optional<uint8_t*>();
        }

        return Optional<uint8_t*>(descriptor_data);
    }

    Success Device::GetDescriptor(uint8_t type, uint8_t index, uint16_t length, uint8_t* buffer, uint8_t languageID) {
        static constexpr uint8_t REQUEST_TYPE_STANDARD_IN = 0x80;
        static constexpr uint8_t REQUEST_GET_DESCRIPTOR = 6;
        
        return SendRequest(
            *this,
            REQUEST_TYPE_STANDARD_IN,
            REQUEST_GET_DESCRIPTOR,
            static_cast<uint16_t>((static_cast<uint16_t>(type) << 8) | index),
            static_cast<uint16_t>(languageID),
            static_cast<uint16_t>(length),
            buffer
        );
    }

    Optional<char*> Device::GetString(uint8_t index, uint16_t languageID) {
        if (languageID == 0) {
            return Optional<char*>();
        }

        auto descriptor_wrapper = GetDescriptor(StringDescriptor::DESCRIPTOR_TYPE, index, languageID);

        if (!descriptor_wrapper.HasValue()) {
            return Optional<char*>();
        }

        uint8_t* const descriptor_data = descriptor_wrapper.GetValue();

        if (GetDescriptorSize(descriptor_data) < StringDescriptor::MIN_DESCRIPTOR_SIZE ||
            GetDescriptorType(descriptor_data) != StringDescriptor::DESCRIPTOR_TYPE) {
            IOHeap::Free(descriptor_data);
            return Optional<char*>();
        }

        const size_t descriptor_size = GetDescriptorSize(descriptor_data);

        const size_t string_length = (descriptor_size - 2) / sizeof(uint16_t);
        char* const result_string = reinterpret_cast<char*>(Heap::Allocate(string_length + 1));

        if (result_string == nullptr) {
            IOHeap::Free(descriptor_data);
            return Optional<char*>();
        }

        for (size_t i = 0; i < string_length; ++i) {
            const uint16_t* const char_ptr = reinterpret_cast<uint16_t*>(&descriptor_data[2 * (i + 1)]);
            result_string[i] = static_cast<char>(*char_ptr);
        }

        result_string[string_length] = '\0';

        IOHeap::Free(descriptor_data);
        return Optional<char*>(result_string);
    }

    Success Device::SetConfiguration(Device& device, uint8_t configuration_value) {
        static constexpr uint8_t REQUEST_TYPE = 0x00;
        static constexpr uint8_t REQUEST_SET_CONFIGURATION = 9;

        return SendRequest(
            device,
            REQUEST_TYPE,
            REQUEST_SET_CONFIGURATION,
            static_cast<uint16_t>(configuration_value),
            0,
            0,
            nullptr
        );
    }

    Success Device::ConfigureEndpoint(Device& device, const EndpointDescriptor& endpoint) {
        auto& context_wrapper = device.context_wrapper;

        if (endpoint.superSpeedConfig.valid) {
            if constexpr (Debug::DEBUG_USB_WARNINGS) {
                Log::putsSafe("[USB] SuperSpeed endpoints are not yet supported\n\r");
            }

            return Failure();
        }

        const auto& type = endpoint.endpointType;

        const auto interval = ConvertEndpointInterval(device, endpoint.endpointType, endpoint.interval);

        if (!interval.HasValue()) {
            if constexpr (Debug::DEBUG_USB_ERRORS) {
                Log::putsSafe("[USB] Invalid endpoint interval\r\n");
            }

            return Failure();
        }

        const bool is_in = type == EndpointType::IsochronousIn ||
                         type == EndpointType::BulkIn ||
                         type == EndpointType::InterruptIn;
        
        const uint8_t context_index = endpoint.endpointAddress * 2 + (is_in ? 1 : 0);

        auto* const input_control_context = context_wrapper->GetInputControlContext();
        input_control_context->Reset();
        input_control_context->SetAddContext(context_index);
        input_control_context->SetAddContext(0);

        auto* const slot_context = context_wrapper->GetSlotContext(true);
        const auto context_entries = context_wrapper->GetSlotContext(false)->GetContextEntries();

        slot_context->Reset();
        slot_context->SetRootHubPort(device.information.root_hub_port);
        slot_context->SetRouteString(device.information.route_string);
        slot_context->SetContextEntries(context_index >= context_entries ? context_index : context_entries);

        auto& transfer_ring = device.endpoint_transfer_rings[context_index - 2];

        if (transfer_ring == nullptr) {
            auto transfer_ring_result = TransferRing::Create(1);

            if (!transfer_ring_result.HasValue()) {
                if constexpr (Debug::DEBUG_USB_ERRORS) {
                    Log::printfSafe("[USB] Failed to create endpoint transfer ring for device %u\r\n", device.information.slot_id);
                }

                return Failure();
            }

            transfer_ring = transfer_ring_result.GetValue();
        }

        static constexpr uint8_t MAX_ERRORS = 3;

        const uint16_t average_trb_length = type == EndpointType::ControlBidirectional ? 8 : endpoint.maxPacketSize;

        auto* const ep_context = context_wrapper->GetInputEndpointContext(endpoint.endpointAddress - 1, is_in);
        ep_context->Reset();
        ep_context->SetMult(0);
        ep_context->SetMaxPStreams(0);
        ep_context->SetInterval(interval.GetValue());
        ep_context->SetErrorCount(MAX_ERRORS);
        ep_context->SetEndpointType(endpoint.endpointType);
        ep_context->SetMaxBurstSize(endpoint.superSpeedConfig.valid ? endpoint.superSpeedConfig.maxBurst : 0);
        ep_context->SetMaxPacketSize(endpoint.maxPacketSize);
        ep_context->SetDCS(transfer_ring->GetCycle());
        ep_context->SetTRDequeuePointer(transfer_ring->GetBase());
        ep_context->SetAverageTRBLength(average_trb_length);

        const auto command = ConfigureEndpointTRB::Create(
            device.controller.GetCommandCycle(),
            false,
            device.information.slot_id,
            context_wrapper->GetInputDeviceContextAddress()
        );

        const auto result = device.controller.SendCommand(command);

        if (!result.HasValue() || result.GetValue().GetCompletionCode() != TRB::CompletionCode::Success) {
            transfer_ring->Release();
            Heap::Free(transfer_ring);
            transfer_ring = nullptr;
            return Failure();
        }

        return Success();
    }

    TransferRing* Device::GetEndpointTransferRing(uint8_t endpointAddress, bool input) const {
        const uint8_t ep_index = endpointAddress * 2 + (input ? 1 : 0) - 2;

        if (ep_index < MAX_ENDPOINT_TRANSFER_RINGS) {
            return endpoint_transfer_rings[ep_index];
        }

        return nullptr;
    }

    Optional<uint8_t> Device::ConvertEndpointInterval(const Device& device, const EndpointType& type, uint16_t interval) {
        const auto& port_speed = device.information.port_speed;

        static const auto& log2 = [](uint16_t value) {
            uint16_t result = 0;
            while (value > 1) {
                value >>= 1;
                ++result;
            }
            return result;
        };

        switch (type) {
            case EndpointType::InterruptIn:
            case EndpointType::InterruptOut:
                if (port_speed == PortSpeed::LowSpeed || port_speed == PortSpeed::FullSpeed) {
                    const auto exp = log2((interval == 0 ? 1 : interval) * 8);
                    return Optional<uint8_t>(exp < 3 ? 3 : (exp > 10 ? 10 : exp));
                }
                else {
                    const auto exp = interval < 1 ? 1 : (interval > 16 ? 16 : interval);
                    return Optional<uint8_t>(exp - 1);
                }
            case EndpointType::IsochronousIn:
            case EndpointType::IsochronousOut:
                if (port_speed == PortSpeed::LowSpeed) {
                    return Optional<uint8_t>();
                }
                else if (port_speed == PortSpeed::FullSpeed) {
                    const auto exp = interval == 0 ? 1 : (interval > 16 ? 16 : interval);
                    return Optional<uint8_t>(exp - 1 + 3);
                }
                else {
                    const auto exp = interval == 0 ? 1 : (interval > 16 ? 16 : interval);
                    return Optional<uint8_t>(exp - 1);
                }
            case EndpointType::BulkIn:
            case EndpointType::BulkOut:
            case EndpointType::ControlBidirectional:
                if (port_speed == PortSpeed::HighSpeed && interval != 0) {
                    return Optional<uint8_t>(log2(interval));
                }
                else {
                    return Optional<uint8_t>(0);
                }
            default:
                return Optional<uint8_t>();
        }
    }

    Success Device::FetchDeviceDescriptor() {
        // prevent page boundary crossing issues
        static_assert(DeviceDescriptor::DESCRIPTOR_SIZE <= 0x20);
        alignas(0x20) uint8_t usb_descriptor[DeviceDescriptor::DESCRIPTOR_SIZE] = { 0 };

        if (!GetDescriptor(DeviceDescriptor::DESCRIPTOR_TYPE, 0, DeviceDescriptor::DESCRIPTOR_SIZE, usb_descriptor).IsSuccess()) {
            return Failure();
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
            auto config_result = ParseConfigurationDescriptor(i);

            if (!config_result.HasValue()) {
                configurations[i].valid = false;

                if constexpr (Debug::DEBUG_USB_SOFT_ERRORS) {
                    Log::printfSafe("[USB] Failed to parse configuration descriptor %u for device %u\r\n", i, information.slot_id);
                }
            }
            else {
                configurations[i] = config_result.GetValue();

                if constexpr (Debug::DEBUG_USB_INFO) {
                    Log::printfSafe("[USB] Fetched configuration descriptor %u for device %u\r\n", i, information.slot_id);
                }
            }
        }

        return Success();
    }

    Optional<Device::ConfigurationDescriptor> Device::ParseConfigurationDescriptor(uint8_t index) {
        uint16_t pre_data[2] = { 0 };

        auto pre_data_wrapper = GetDescriptor(ConfigurationDescriptor::DESCRIPTOR_TYPE, index, sizeof(pre_data), reinterpret_cast<uint8_t*>(pre_data));

        if (!pre_data_wrapper.IsSuccess()) {
            return Optional<ConfigurationDescriptor>();
        }

        const size_t descriptor_size = static_cast<size_t>(pre_data[1]);

        uint8_t* const data = reinterpret_cast<uint8_t*>(IOHeap::Allocate(descriptor_size));

        if (data == nullptr) {
            return Optional<ConfigurationDescriptor>();
        }

        if (!GetDescriptor(ConfigurationDescriptor::DESCRIPTOR_TYPE, index, descriptor_size, data).IsSuccess()) {
            IOHeap::Free(data);
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

        const uint8_t* ptr = data + GetDescriptorSize(data);
        const uint8_t* const limit = data + *totalLength;

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
                        if constexpr (Debug::DEBUG_USB_INFO) {
                            Log::printfSafe("[USB] Created explicit interface association\r\n");
                        }
                    }
                    else {
                        if constexpr (Debug::DEBUG_USB_SOFT_ERRORS) {
                            Log::printfSafe("[USB] Failed to create explicit interface association\r\n");
                        }
                    }
                }

                ptr += descriptor_size;
                continue;
            }

            if (GetDescriptorSize(ptr) == 0) {
                if constexpr (Debug::DEBUG_USB_ERRORS) {
                    Log::putsSafe("[USB] Invalid descriptor with zero length, aborting configuration parsing\r\n");
                }

                config_descriptor.Release();
                IOHeap::Free(data);
                return Optional<ConfigurationDescriptor>();
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
                        if constexpr (Debug::DEBUG_USB_SOFT_ERRORS) {
                            Log::printfSafe("[USB] Failed to create function for interface %u\r\n", interface.descriptor.interfaceNumber);
                        }

                        while (ptr < limit && GetDescriptorType(ptr) != InterfaceDescriptor::DESCRIPTOR_TYPE) {
                            const size_t desc_size = GetDescriptorSize(ptr);

                            if (desc_size == 0) {
                                if constexpr (Debug::DEBUG_USB_ERRORS) {
                                    Log::putsSafe("[USB] Invalid descriptor with zero length, aborting interface parsing\r\n");
                                }

                                config_descriptor.Release();
                                IOHeap::Free(data);
                                return Optional<ConfigurationDescriptor>();
                            }

                            ptr += desc_size;
                        }

                        continue;
                    }

                    function = result.GetValue();
                }

                const auto& raw_interface = interface.descriptor;
                auto* const function_interface = function->GetInterface(raw_interface.interfaceNumber);

                if (function_interface != nullptr) {
                    if (function_interface->AddAlternate(raw_interface).IsSuccess()) {
                        if constexpr (Debug::DEBUG_USB_INFO) {
                            Log::printfSafe(
                                "[USB] Parsed interface %u.%u\r\n",
                                raw_interface.interfaceNumber,
                                raw_interface.alternateSetting
                            );
                        }
                    }                    
                    else {
                        if constexpr (Debug::DEBUG_USB_SOFT_ERRORS) {
                            Log::printfSafe(
                                "[USB] Failed to add alternate interface %u.%u\r\n",
                                raw_interface.interfaceNumber,
                                raw_interface.alternateSetting
                            );
                        }
                    }
                }
                else {
                    if (function->AddInterface(raw_interface).IsSuccess()) {
                        if constexpr (Debug::DEBUG_USB_INFO) {
                            Log::printfSafe(
                                "[USB] Parsed interface %u.%u\r\n",
                                raw_interface.interfaceNumber,
                                raw_interface.alternateSetting
                            );
                        }

                        found_valid_interface = true;
                    }
                    else {
                        if constexpr (Debug::DEBUG_USB_SOFT_ERRORS) {
                            Log::printfSafe(
                                "[USB] Failed to add interface %u.%u\r\n",
                                raw_interface.interfaceNumber,
                                raw_interface.alternateSetting
                            );
                        }
                    }
                }
            }
            else {
                if constexpr (Debug::DEBUG_USB_SOFT_ERRORS) {
                    Log::printfSafe("[USB] Failed to parse interface descriptor in configuration %u\r\n", index);
                }
            }
        }

        IOHeap::Free(data);

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
                    // Check for invalid descriptor length to prevent infinite loops
                    const size_t desc_size = GetDescriptorSize(data);
                    
                    if (desc_size == 0) {
                        if constexpr (Debug::DEBUG_USB_ERRORS) {
                            Log::printfSafe("[USB] Invalid descriptor with zero length, aborting interface parsing\r\n");
                        }

                        interface_descriptor.Release();
                        return Optional<InterfaceWrapper>();
                    }

                    auto extra_wrapper = ParseDeviceSpecificDescriptor(data, limit);

                    if (extra_wrapper.HasValue()) {
                        auto* const extra = extra_wrapper.GetValue();

                        interface_descriptor.AddExtra(extra);

                        if constexpr (Debug::DEBUG_USB_INFO) {
                            Log::printfSafe(
                                "[USB] Parsed extra descriptor (type 0x%0.2hhx) for interface %u.%u\r\n",
                                extra->descriptorType,
                                interface_descriptor.interfaceNumber,
                                interface_descriptor.alternateSetting
                            );
                        }
                    }
                }
                
                auto endpoint_wrapper = ParseEndpointDescriptor(data, limit);

                if (endpoint_wrapper.HasValue()) {
                    interface_descriptor.endpoints[descriptor_index] = endpoint_wrapper.GetValue();

                    if constexpr (Debug::DEBUG_USB_INFO) {
                        Log::printfSafe(
                            "[USB] Parsed endpoint %u of interface %u.%u\r\n",
                            interface_descriptor.endpoints[descriptor_index].endpointAddress,
                            interface_descriptor.interfaceNumber,
                            interface_descriptor.alternateSetting
                        );
                    }
                }
                else {
                    // If we can't parse an interface endpoint, fail on this interface
                    if constexpr (Debug::DEBUG_USB_ERRORS) {
                        Log::printfSafe(
                            "[USB] Failed to parse endpoint %u of interface %u.%u\r\n",
                            descriptor_index,
                            interface_descriptor.interfaceNumber,
                            interface_descriptor.alternateSetting
                        );
                    }

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
            if constexpr (Debug::DEBUG_USB_ERRORS) {
                Log::printfSafe("%u : %u\n\r", GetDescriptorSize(data), GetDescriptorType(data));
            }

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

        DeviceSpecificDescriptor* descriptor = reinterpret_cast<DeviceSpecificDescriptor*>(
            Heap::Allocate(length - 2 + sizeof(DeviceSpecificDescriptor))
        );

        if (descriptor == nullptr) {
            data += length;
            return Optional<DeviceSpecificDescriptor*>();
        }

        descriptor->descriptorType = descriptorType;
        descriptor->length = length;
        descriptor->next = nullptr;
        Utils::memcpy(descriptor->data, data + 2, length - 2);

        data += length;

        return Optional<DeviceSpecificDescriptor*>(descriptor);
    }

    void Device::RingDoorbell(uint8_t endpointIndex) const {
        controller.RingDoorbell(*this, endpointIndex);
    }

    Success Device::AddDriver(Driver* driver) {
        DriversNode* prev = nullptr;
        DriversNode* node = drivers;

        while (node != nullptr) {
            for (size_t i = 0; i < DriversNode::MAX_DRIVERS; ++i) {
                if (node->drivers[i] == nullptr) {
                    node->drivers[i] = driver;
                    return Success();
                }
            }

            prev = node;
            node = node->next;
        }

        node = reinterpret_cast<DriversNode*>(Heap::Allocate(sizeof(DriversNode)));

        if (node == nullptr) {
            return Failure();
        }

        node = new (node) DriversNode;

        if (prev != nullptr) {
            prev->next = node;
        }
        else {
            drivers = node;
        }

        node->drivers[0] = driver;

        return Success();
    }

    Optional<Driver*> Device::FindDriverEvent(const TransferEventTRB& trb) const {        
        DriversNode* node = drivers;

        while (node != nullptr) {
            for (size_t i = 0; i < DriversNode::MAX_DRIVERS; ++i) {
                Driver* driver = node->drivers[i];

                if (driver != nullptr) {
                    const auto address_wrapper = Paging::GetPhysicalAddress(driver->GetAwaitingTRB());

                    if (address_wrapper.HasValue() && address_wrapper.GetValue() == trb.GetPointer()) {
                        return Optional<Driver*>(driver);
                    }
                }
            }

            node = node->next;
        }

        return Optional<Driver*>();
    }

    void Device::ReleaseDrivers() {
        DriversNode* node = drivers;

        while (node != nullptr) {
            for (size_t i = 0; i < DriversNode::MAX_DRIVERS; ++i) {
                if (node->drivers[i] != nullptr) {
                    node->drivers[i]->Release();
                    node->drivers[i] = nullptr;
                }
            }

            DriversNode* next = node->next;
            node->next = nullptr;
            Heap::Free(node);
            node = next;
        }

        drivers = nullptr;
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
        if (!SetBusy().IsSuccess()) {
            if constexpr (Debug::DEBUG_USB_ERRORS) {
                Log::printfSafe("[USB] Failed to set device %u as busy\r\n", information.slot_id);
            }

            return Failure();
        }

        if constexpr (Debug::DEBUG_USB_INFO) {
            Log::printfSafe("[USB] Initializing device %u\r\n", information.slot_id);
        }
        
        // Initialize contexts
        const auto context_wrapper_result = ContextWrapper::Create(controller.HasExtendedContext());

        if (!context_wrapper_result.HasValue()) {
            if constexpr (Debug::DEBUG_USB_ERRORS) {
                Log::printfSafe("[USB] Failed to create context wrapper for device %u\r\n", information.slot_id);
            }

            return Failure();
        }

        context_wrapper = context_wrapper_result.GetValue();
        context_wrapper->Reset();

        auto* const input_control_context = context_wrapper->GetInputControlContext();
        input_control_context->SetAddContext(0);
        input_control_context->SetAddContext(1);

        auto* const input_slot_context = context_wrapper->GetSlotContext(true);
        input_slot_context->SetRootHubPort(information.root_hub_port);
        input_slot_context->SetRouteString(information.route_string);
        input_slot_context->SetContextEntries(1);

        // Initialize transfer ring for control endpoint
        const auto transfer_ring_result = TransferRing::Create(1);
        if (!transfer_ring_result.HasValue()) {
            if constexpr (Debug::DEBUG_USB_ERRORS) {
                Log::printfSafe("[USB] Failed to create control transfer ring for device %u\r\n", information.slot_id);
            }

            Release();
            return Failure();
        }

        control_transfer_ring = transfer_ring_result.GetValue();

        auto* const control_endpoint_context = context_wrapper->GetControlEndpointContext(true);

        // Compute default control endpoint max packet size
        const auto max_packet_size = GetDefaultMaxPacketSize();

        if (!max_packet_size.HasValue()) {
            if constexpr (Debug::DEBUG_USB_ERRORS) {
                Log::printfSafe("[USB] Failed to get default max packet size for device %u\r\n", information.slot_id);
            }
            
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
            if constexpr (Debug::DEBUG_USB_ERRORS) {
                Log::printfSafe("[USB] Addressing failed for device %u\r\n", information.slot_id);
            }

            Release();
            return Failure();
        }

        if constexpr (Debug::DEBUG_USB_INFO) {
            Log::printfSafe("[USB] Device %u successfully addressed\r\n", information.slot_id);
        }

        // Get device descriptor
        if (!FetchDeviceDescriptor().IsSuccess()) {
            if constexpr (Debug::DEBUG_USB_ERRORS) {
                Log::printfSafe("[USB] Failed to fetch device descriptor for device %u\r\n", information.slot_id);
            }
            
            Release();
            return Failure();
        }

        if constexpr (Debug::DEBUG_USB_INFO) {
            Log::printfSafe("[USB] Fetched device descriptor for device %u\n\r", information.slot_id);
        }

        if (!FetchConfigurations().IsSuccess()) {
            if constexpr (Debug::DEBUG_USB_ERRORS) {
                Log::printfSafe("[USB] Failed to fetch configurations for device %u\r\n", information.slot_id);
            }
            
            Release();
            return Failure();
        }

        if constexpr (Debug::DEBUG_USB_INFO) {
            Log::printfSafe("[USB] Fetched configurations for device %u\n\r", information.slot_id);
            Log::printfSafe("[USB] Enumerating configuration topology...\n\r");

            if (descriptor.manufacturerDescriptorIndex != 0) {
                auto manufacturer_string_wrapper = GetString(descriptor.manufacturerDescriptorIndex, 0x0409);

                if (manufacturer_string_wrapper.HasValue()) {
                    Log::printfSafe("[USB] Manufacturer String: %s\r\n", manufacturer_string_wrapper.GetValue());
                    Heap::Free(manufacturer_string_wrapper.GetValue());
                }
                else {
                    Log::printfSafe("[USB] Failed to get manufacturer string\r\n");
                }
            }

            if (descriptor.productDescriptorIndex != 0) {
                auto product_string_wrapper = GetString(descriptor.productDescriptorIndex, 0x0409);

                if (product_string_wrapper.HasValue()) {
                    Log::printfSafe("[USB] Product String: %s\r\n", product_string_wrapper.GetValue());
                    Heap::Free(product_string_wrapper.GetValue());
                }
                else {
                    Log::printfSafe("[USB] Failed to get product string\r\n");
                }
            }

            if (descriptor.serialNumberDescriptorIndex != 0) {
                auto serial_string_wrapper = GetString(descriptor.serialNumberDescriptorIndex, 0x0409);

                if (serial_string_wrapper.HasValue()) {
                    Log::printfSafe("[USB] Serial Number String: %s\r\n", serial_string_wrapper.GetValue());
                    Heap::Free(serial_string_wrapper.GetValue());
                }
                else {  
                    Log::printfSafe("[USB] Failed to get serial number string\r\n");
                }
            }
        }

        // choose first configuration with known function that can be configured
        for (size_t i = 0; i < descriptor.configurationsNumber; ++i) {
            if (configurations[i].valid) {
                for (FunctionDescriptor* function = configurations[i].functions; function != nullptr; function = function->next) {
                    if (function->functionClass == HID::Driver::GetClassCode()) {
                        if constexpr (Debug::DEBUG_USB_INFO) {
                            Log::printfSafe("[USB] Found HID function in configuration %u\r\n", i);
                        }

                        auto dev = HID::Driver::Create(*this, configurations[i].configurationValue, function);

                        if (dev.HasValue()) {
                            if constexpr (Debug::DEBUG_USB_INFO) {
                                Log::printfSafe("[USB] HID device created for device %u\r\n", information.slot_id);
                            }

                            auto* const pdev = dev.GetValue();

                            if (!AddDriver(pdev).IsSuccess()) {
                                if constexpr (Debug::DEBUG_USB_SOFT_ERRORS) {
                                    Log::printfSafe("[USB] Failed to add HID driver for device %u\r\n", information.slot_id);
                                }

                                pdev->Release();
                            }
                        }
                        else {
                            if constexpr (Debug::DEBUG_USB_SOFT_ERRORS) {
                                Log::printfSafe("[USB] Failed to initialize HID device for device %u\r\n", information.slot_id);
                            }
                        }
                    }
                    else if (function->functionClass == MassStorage::Device::GetClassCode()) {
                        if constexpr (Debug::DEBUG_USB_INFO) {
                            Log::printfSafe("[USB] Found Mass Storage function in configuration %u\n\r", i);
                        }
                    }
                }
            }
        }

        ReleaseBusy();

        return Success();
    }

    void Device::SetUnvailable() {
        {
            Utils::LockGuard _{state_lock};

            unavailable.store(true);
        }

        // Wait for ongoing operations to complete
        while (IsBusy()) {
            Self().Yield();
        }
    }

    bool Device::IsUnavailable() const {
        return unavailable.load();
    }

    Success Device::SetBusy() {
        Utils::LockGuard _{state_lock};

        if (unavailable.load()) {
            return Failure();
        }

        ++current_accesses;

        return Success();
    }

    void Device::ReleaseBusy() {
        Utils::LockGuard _{state_lock};

        if (current_accesses > 0) {
            --current_accesses;
        }
    }

    bool Device::IsBusy() const {
        return current_accesses > 0;
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

        for (size_t i = 0; i < MAX_ENDPOINT_TRANSFER_RINGS; ++i) {
            if (endpoint_transfer_rings[i] != nullptr) {
                endpoint_transfer_rings[i]->Release();
                Heap::Free(endpoint_transfer_rings[i]);
                endpoint_transfer_rings[i] = nullptr;
            }
        }

        ReleaseDrivers();

        current_accesses.store(0);
        unavailable.store(true);
    }

    Success Device::PostInitialization() {
        auto* node = drivers;

        while (node != nullptr) {
            for (size_t i = 0; i < DriversNode::MAX_DRIVERS; ++i) {
                if (node->drivers[i] != nullptr) {
                    if (!node->drivers[i]->PostInitialization().IsSuccess()) {
                        return Failure();
                    }
                }
            }

            node = node->next;
        }

        return Success();
    }

    void Device::Destroy() {
        SetUnvailable();
        Release();
    }

    void Device::SignalTransferComplete(const TransferEventTRB& trb) {
        if (!SetBusy().IsSuccess()) {
            return;
        }

        const auto awaiting_transfer_address_wrapper = Paging::GetPhysicalAddress(awaiting_transfer);

        if (awaiting_transfer_address_wrapper.HasValue() && trb.GetPointer() == awaiting_transfer_address_wrapper.GetValue()) {
            transfer_result = trb;
            transfer_complete.store(true);
        }
        else {
            auto driver_wrapper = FindDriverEvent(trb);

            if (driver_wrapper.HasValue()) {
                driver_wrapper.GetValue()->HandleEvent();
            }
        }

        ReleaseBusy();
    }

    const Device::DeviceDescriptor& Device::GetDeviceDescriptor() const {
        return descriptor;
    }
}
