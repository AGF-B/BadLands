#include <cstddef>
#include <cstdint>

#include <new>

#include <shared/Response.hpp>

#include <devices/USB/HID/Device.hpp>
#include <devices/USB/HID/Keyboard.hpp>
#include <devices/USB/xHCI/Device.hpp>

#include <mm/Heap.hpp>

#include <screen/Log.hpp>

namespace Devices::USB::HID {
    Device::Device(const xHCI::Device& device, const HIDHierarchy& hierarchy) : xHCI::Device(device), hierarchy{hierarchy} { }

    Success Device::HIDHierarchy::AddDevice(InterfaceDevice* device) {
        auto* const raw = Heap::Allocate(sizeof(Node));

        if (raw == nullptr) {
            return Failure();
        }

        auto* const node = new (raw) Node;

        node->device = device;
        node->next = head;
        
        head = node;

        return Success();
    }

    Optional<InterfaceDevice*> Device::HIDHierarchy::GetDevice(DeviceClass deviceClass) const {
        Node* current = head;

        while (current != nullptr) {
            if (current->device != nullptr && current->device->GetDeviceClass() == deviceClass) {
                return Optional<InterfaceDevice*>(current->device);
            }

            current = current->next;
        }

        return Optional<InterfaceDevice*>();
    }

    void Device::HIDHierarchy::Release() {
        Node* current = head;

        while (current != nullptr) {
            Node* next = current->next;

            if (current->device != nullptr) {
                current->device->Release();
            }

            Heap::Free(current);
            current = next;
        }

        head = nullptr;
    }

    Optional<Device::HIDDescriptor> Device::GetHIDDescriptor(InterfaceDescriptor* interface) {
        auto extra = interface->GetExtra(HIDDescriptor::DESCRIPTOR_TYPE);

        if (!extra.HasValue()) {
            Log::printfSafe("[HID] Could not find HID descriptor in interface extra descriptors\n\r");
            return Optional<HIDDescriptor>();
        }

        auto* hid_descriptor_data = extra.GetValue();

        if (hid_descriptor_data->length < HIDDescriptor::DESCRIPTOR_SIZE) {
            Log::printfSafe("[HID] HID descriptor size is too small (%u bytes)\n\r", hid_descriptor_data->length);
            return Optional<HIDDescriptor>();
        }

        return ParseHIDDescriptor(hid_descriptor_data->data, hid_descriptor_data->length);
    }

    Optional<Device::HIDDescriptor> Device::ParseHIDDescriptor(const uint8_t* data, size_t length) {
        HIDDescriptor descriptor;

        descriptor.hidVersionMajor    = data[0];
        descriptor.hidVersionMinor    = data[1];
        descriptor.countryCode        = data[2];
        descriptor.descriptorsNumber  = data[3];

        if (length < static_cast<size_t>(6 + descriptor.descriptorsNumber * 3)) {
            Log::printfSafe("[HID] HID descriptor size is too small for %u class descriptors\n\r", descriptor.descriptorsNumber);
            return Optional<HIDDescriptor>();
        }

        for (size_t i = 0; i < descriptor.descriptorsNumber; ++i) {
            size_t offset = 4 + i * 3;

            uint8_t type = data[offset];
            uint16_t length = *reinterpret_cast<const uint16_t*>(&data[offset + 1]);

            if (type == HID_REPORT_DESCRIPTOR_TYPE) {
                descriptor.reportDescriptorLength = length;
                return Optional<HIDDescriptor>(descriptor);
            }
        }

        Log::printfSafe("[HID] Could not find report descriptor in HID descriptor\n\r");
        return Optional<HIDDescriptor>();
    }

    Device::ReportDescriptor::ReportDescriptor(const uint8_t* data, size_t length)
        : data{data}, length{length}, position{0} { }

    Optional<Device::ReportDescriptor::Item> Device::ReportDescriptor::GetNextItem() {
        if (position >= length) {
            return Optional<Item>();
        }

        const uint8_t prefix = data[position++];

        const uint8_t size = prefix & 0x03;
        const uint8_t type = (prefix >> 2) & 0x03;
        const uint8_t tag  = (prefix >> 4) & 0x0F;

        if (position + size > length) {
            return Optional<Item>();
        }

        uint32_t value = 0;

        switch (size) {
            case 0: break;
            case 1: value = data[position++]; break;
            case 2:
                value = data[position++];
                value <<= 8;
                value |= data[position++];
                break;
            case 3:
                value = data[position++];
                value <<= 8;
                value |= data[position++];
                value <<= 8;
                value |= data[position++];
                value <<= 8;
                value |= data[position++];
                break;
            default:
                return Optional<Item>();
        }

        ItemType item_type;

        switch (type) {
            case 0: item_type = ItemType::Main; break;
            case 1: item_type = ItemType::Global; break;
            case 2: item_type = ItemType::Local; break;
            default: return Optional<Item>();
        }

        return Optional<Item>({
            .type = item_type,
            .tag = tag,
            .value = value
        });
    }

    Optional<Device::ReportParser::GlobalEvent> Device::ReportParser::HandleGlobalItem(const Item& item, InterfaceDevice* device) {
        if (item.type != ItemType::Global) {
            return Optional<GlobalEvent>();
        }

        static constexpr uint8_t USAGE_PAGE         = 0x0;
        static constexpr uint8_t LOGICAL_MINIMUM    = 0x1;
        static constexpr uint8_t LOGICAL_MAXIMUM    = 0x2;
        static constexpr uint8_t REPORT_SIZE        = 0x7;
        static constexpr uint8_t REPORT_ID          = 0x8;
        static constexpr uint8_t REPORT_COUNT       = 0x9;

        switch (item.tag) {
        case USAGE_PAGE:
            for (auto page : SUPPORTED_PAGES) {
                if (item.value == page) {
                    globalState.usagePage = item.value;
                    return Optional(GlobalEvent::PAGE_CHANGED);
                }
            }

            if (device != nullptr) {
                if (device->IsUsageSupported(item.value, 0)) {
                    return Optional(GlobalEvent::PAGE_CHANGED);
                }
            }

            return Optional<GlobalEvent>();
        case LOGICAL_MINIMUM:
            globalState.logicalMinimum = item.value;
            return Optional(GlobalEvent::LOGICAL_MINIMUM_CHANGED);
        case LOGICAL_MAXIMUM:
            globalState.logicalMaximum = item.value;
            return Optional(GlobalEvent::LOGICAL_MAXIMUM_CHANGED);
        case REPORT_SIZE:
            globalState.reportSize = item.value;
            return Optional(GlobalEvent::REPORT_SIZE_CHANGED);
        case REPORT_ID:
            globalState.reportID = item.value;
            return Optional(GlobalEvent::REPORT_ID_CHANGED);
        case REPORT_COUNT:
            globalState.reportCount = item.value;
            return Optional(GlobalEvent::REPORT_COUNT_CHANGED);
        default:
            return Optional<GlobalEvent>();
        }
    }

    Success Device::ReportParser::HandleLocalItem(const Item& item, InterfaceDevice* device) {
        if (item.type != ItemType::Local) {
            return Failure();
        }

        static constexpr uint8_t USAGE              = 0x0;
        static constexpr uint8_t USAGE_MINIMUM      = 0x1;
        static constexpr uint8_t USAGE_MAXIMUM      = 0x2;

        switch (item.tag) {
        case USAGE:
            if (device == nullptr) {
                if (!IsUsageSupported(globalState.usagePage, item.value)) {
                    return Failure();
                }
            }
            else {
                if (!device->IsUsageSupported(globalState.usagePage, item.value)) {
                    return Failure();
                }
            }        
            localState.usage = item.value;
            return Success();
        case USAGE_MINIMUM:
            localState.usageMinimum = item.value;
            return Success();
        case USAGE_MAXIMUM:
            localState.usageMaximum = item.value;
            return Success();
        default:
            return Failure();
        }
    }

    Optional<Device::HIDHierarchy> Device::ReportParser::Parse() {
        HIDHierarchy hierarchy;

        InterfaceDevice* device = nullptr;

        auto item_wrapper = descriptor.GetNextItem();

        while (item_wrapper.HasValue()) {
            const auto item = item_wrapper.GetValue();

            if (item.type == ItemType::Global) {
                auto global_event_wrapper = HandleGlobalItem(item, device);

                if (!global_event_wrapper.HasValue()) {
                    Log::printfSafe("[HID] Unsupported global item - Tag: 0x%0.2hhx, Value: 0x%0.8x\n\r", item.tag, item.value);
                    hierarchy.Release();
                    return Optional<HIDHierarchy>();
                }
            }
            else if (item.type == ItemType::Local) {
                uint32_t previous_usage = localState.usage;

                if (!HandleLocalItem(item, device).IsSuccess()) {
                    Log::printfSafe("[HID] Unsupported local item - Tag: 0x%0.2hhx, Value: 0x%0.8x\n\r", item.tag, item.value);
                    hierarchy.Release();
                    return Optional<HIDHierarchy>();
                }

                if (localState.usage != previous_usage) {
                    if (device == nullptr) {
                        if (globalState.usagePage == GENERIC_DESKTOP_CONTROLS) {
                            if (localState.usage == GENERIC_KEYBOARD) {
                                auto fetched = hierarchy.GetDevice(DeviceClass::KEYBOARD);

                                if (!fetched.HasValue()) {
                                    auto* const raw = Heap::Allocate(sizeof(Keyboard));

                                    if (raw == nullptr) {
                                        Log::printfSafe("[HID] Could not allocate memory for Keyboard device\n\r");
                                        hierarchy.Release();
                                        return Optional<HIDHierarchy>();
                                    }

                                    auto* const dev = new (raw) Keyboard;

                                    if (!hierarchy.AddDevice(dev).IsSuccess()) {
                                        Log::printfSafe("[HID] Could not add Keyboard device to hierarchy\n\r");
                                        dev->Release();
                                        hierarchy.Release();
                                        return Optional<HIDHierarchy>();
                                    }

                                    device = dev;
                                }
                                else {
                                    device = fetched.GetValue();
                                }
                            }
                            else {
                                Log::printfSafe("[HID] Unsupported generic desktop usage: 0x%0.8x\n\r", localState.usage);
                                hierarchy.Release();
                                return Optional<HIDHierarchy>();
                            }
                        }
                        else {
                            Log::printfSafe("[HID] Unsupported usage page: 0x%0.8x\n\r", globalState.usagePage);
                            hierarchy.Release();
                            return Optional<HIDHierarchy>();
                        }
                    }
                }
            }
            else if (item.type == ItemType::Main) {
                static constexpr uint8_t INPUT                  = 0x8;
                static constexpr uint8_t OUTPUT                 = 0x9;
                static constexpr uint8_t FEATURE                = 0xB;
                static constexpr uint8_t COLLECTION             = 0xA;
                static constexpr uint8_t END_COLLECTION         = 0xC;

                if (device == nullptr) {
                    Log::printfSafe("[HID] No device available to handle main item - Tag: 0x%0.2hhx, Value: 0x%0.8x\n\r", item.tag, item.value);
                    hierarchy.Release();
                    return Optional<HIDHierarchy>();
                }

                const InterfaceDevice::HIDState state{
                    .globalState = globalState,
                    .localState = localState
                };

                const InterfaceDevice::IOConfiguration config {
                    .constant       = (item.value & 0x01) != 0,
                    .variable       = (item.value & 0x02) != 0,
                    .relative       = (item.value & 0x04) != 0,
                    .wrap           = (item.value & 0x08) != 0,
                    .nonLinear      = (item.value & 0x10) != 0,
                    .noPreferred    = (item.value & 0x20) != 0,
                    .nullState      = (item.value & 0x40) != 0,
                    ._volatile      = (item.value & 0x80) != 0,
                    .bufferedBytes  = (item.value & 0x100) != 0
                };

                switch (item.tag) {
                case INPUT:
                    if (!device->AddInput(state, config).IsSuccess()) {
                        hierarchy.Release();
                        return Optional<HIDHierarchy>();
                    }
                    break;
                case OUTPUT:
                    if (!device->AddOutput(state, config).IsSuccess()) {
                        hierarchy.Release();
                        return Optional<HIDHierarchy>();
                    }
                    break;
                case FEATURE:
                    Log::printfSafe("[HID] Device features not yet supported\n\r");
                    hierarchy.Release();
                    return Optional<HIDHierarchy>();
                case COLLECTION: {
                    InterfaceDevice::CollectionType collectionType;

                    switch (item.value) {
                    case 0x00: collectionType = InterfaceDevice::CollectionType::Physical; break;
                    case 0x01: collectionType = InterfaceDevice::CollectionType::Application; break;
                    case 0x02: collectionType = InterfaceDevice::CollectionType::Logical; break;
                    case 0x03: collectionType = InterfaceDevice::CollectionType::Report; break;
                    case 0x04: collectionType = InterfaceDevice::CollectionType::NamedArray; break;
                    case 0x05: collectionType = InterfaceDevice::CollectionType::UsageSwitch; break;
                    case 0x06: collectionType = InterfaceDevice::CollectionType::UsageModifier; break;
                    default:
                        Log::printfSafe("[HID] Unsupported collection type: 0x%0.2hhx\n\r", item.value);
                        hierarchy.Release();
                        return Optional<HIDHierarchy>();
                    }

                    if (!device->StartCollection(state, collectionType).IsSuccess()) {
                        hierarchy.Release();
                        return Optional<HIDHierarchy>();
                    }
                    break;
                }
                case END_COLLECTION:
                    if (!device->EndCollection().IsSuccess()) {
                        hierarchy.Release();
                        return Optional<HIDHierarchy>();
                    }
                    break;
                default:
                    Log::printfSafe("[HID] Unsupported main item - Tag: 0x%0.2hhx, Value: 0x%0.8x\n\r", item.tag, item.value);
                    hierarchy.Release();
                    return Optional<HIDHierarchy>();
                }
            }

            item_wrapper = descriptor.GetNextItem();
        }

        return Optional<HIDHierarchy>(hierarchy);
    }

    Optional<Device*> Device::Create(xHCI::Device& device, const FunctionDescriptor* function) {
        if (function->interfacesNumber != 1) {
            Log::printfSafe("[HID] Does not support functions that do not have exactly one interface (has %u)\n\r", function->interfacesNumber);
            return Optional<Device*>();
        }

        auto* interface = function->interfaces;  

        auto hid_descriptor_wrapper = GetHIDDescriptor(interface);

        if (!hid_descriptor_wrapper.HasValue()) {
            return Optional<Device*>();
        }

        auto hid_descriptor = hid_descriptor_wrapper.GetValue();

        uint8_t* buffer = reinterpret_cast<uint8_t*>(Heap::Allocate(hid_descriptor.reportDescriptorLength));

        if (buffer == nullptr) {
            Log::printfSafe("[HID] Could not allocate memory for report descriptor (size: %u bytes)\n\r", hid_descriptor.reportDescriptorLength);
            return Optional<Device*>();
        }

        static constexpr uint8_t REQUEST_TYPE_INTERFACE_IN = 0x81;
        static constexpr uint8_t REQUEST_GET_DESCRIPTOR = 6;

        if (!SendRequest(
            device,
            REQUEST_TYPE_INTERFACE_IN,
            REQUEST_GET_DESCRIPTOR,
            static_cast<uint16_t>((static_cast<uint16_t>(HID_REPORT_DESCRIPTOR_TYPE) << 8) | 0),
            static_cast<uint16_t>(interface->interfaceNumber),
            hid_descriptor.reportDescriptorLength,
            buffer
        ).IsSuccess()) {
            Log::printfSafe("[HID] Failed to fetch report descriptor from device\n\r");
            Heap::Free(buffer);
            return Optional<Device*>();
        }

        ReportDescriptor report_descriptor(buffer, hid_descriptor.reportDescriptorLength);

        ReportParser parser{report_descriptor};

        auto hierarchy_wrapper = parser.Parse();

        if (!hierarchy_wrapper.HasValue()) {
            Log::printfSafe("[HID] Failed to parse report descriptor\n\r");
            Heap::Free(buffer);
            return Optional<Device*>();
        }

        Log::printfSafe("[HID] Successfully parsed report descriptor\n\r");

        Heap::Free(buffer);

        auto* const raw_device = Heap::Allocate(sizeof(Device));

        if (raw_device == nullptr) {
            Log::printfSafe("[HID] Could not allocate memory for HID device\n\r");
            hierarchy_wrapper.GetValue().Release();
            return Optional<Device*>();
        }

        auto* const hid_device = new (raw_device) Device(device, hierarchy_wrapper.GetValue());

        __asm__ volatile("jmp .");

        return Optional<Device*>(hid_device);
    }

    void Device::Release() {
        xHCI::Device::Release();
    }
}
