#include <cstdint>

#include <shared/LockGuard.hpp>
#include <shared/Response.hpp>

#include <devices/USB/xHCI/Controller.hpp>
#include <devices/USB/xHCI/Device.hpp>
#include <devices/USB/xHCI/Specification.hpp>

#include <mm/VirtualMemory.hpp>

#include <sched/Self.hpp>

#include <screen/Log.hpp>

namespace Devices::USB::xHCI {
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

    Success Device::FetchDeviceDescriptor() {
        Utils::LockGuard lock{transfer_lock};

        uint8_t usb_descriptor[18] = { 0 };

        SetupTRB setup = SetupTRB::Create({
            .bmRequestType = 0x80,
            .bRequest = 6,
            .wValue = 0x0100,
            .wIndex = 0,
            .wLength = 18,
            .tranferLength = 8,
            .interrupterTarget = 0,
            .cycle = control_transfer_ring->GetCycle(),
            .interruptOnCompletion = false,
            .transferType = TransferType::DataInStage
        });

        control_transfer_ring->Enqueue(setup);

        DataTRB data = DataTRB::Create({
            .bufferPointer = VirtualMemory::GetPhysicalAddress(usb_descriptor),
            .transferLength = 18,
            .tdSize = 18,
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
