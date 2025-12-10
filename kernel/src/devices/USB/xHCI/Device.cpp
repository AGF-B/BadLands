#include <cstdint>

#include <shared/Response.hpp>

#include <devices/USB/xHCI/Controller.hpp>
#include <devices/USB/xHCI/Device.hpp>
#include <devices/USB/xHCI/Specification.hpp>

namespace Devices::USB::xHCI {
    Device::Device(Controller& controller, const DeviceDescriptor& descriptor)
        : controller{controller}, descriptor{descriptor} {}

    const DeviceDescriptor& Device::GetDescriptor() const {
        return descriptor;
    }

    const void* Device::GetOutputDeviceContext() const {
        return context_wrapper->GetOutputDeviceContextAddress();
    }

    Success Device::Initialize() {
        // Initialize contexts
        auto context_wrapper_result = ContextWrapper::Create(controller.HasExtendedContext());

        if (!context_wrapper_result.HasValue()) {
            return Failure();
        }

        context_wrapper = context_wrapper_result.GetValue();
        context_wrapper->Reset();

        auto* input_control_context = context_wrapper->GetInputControlContext();
        input_control_context->SetAddContext(0);
        input_control_context->SetAddContext(1);

        auto* input_slot_context = context_wrapper->GetSlotContext(true);
        input_slot_context->SetRootHubPort(descriptor.root_hub_port);
        input_slot_context->SetRouteString(descriptor.route_string);
        input_slot_context->SetContextEntries(1);

        // Initialize transfer ring for control endpoint
        auto transfer_ring_result = TransferRing::Create(1);
        if (!transfer_ring_result.HasValue()) {
            Release();
            return Failure();
        }

        control_transfer_ring = transfer_ring_result.GetValue();

        auto* control_endpoint_context = context_wrapper->GetControlEndpointContext(true);

        // compute default control endpoint max packet size
        uint16_t max_packet_size;

        switch (descriptor.port_speed) {
            case PortSpeed::LowSpeed: max_packet_size = 8; break;
            case PortSpeed::FullSpeed: max_packet_size = 64; break;
            case PortSpeed::HighSpeed: max_packet_size = 64; break;
            case PortSpeed::SuperSpeedGen1x1:
            case PortSpeed::SuperSpeedPlusGen1x2:
            case PortSpeed::SuperSpeedPlusGen2x1:
            case PortSpeed::SuperSpeedPlusGen2x2:
                max_packet_size = 512; break;
            default:
                max_packet_size = 8; break;
        }

        control_endpoint_context->SetEndpointType(EndpointType::ControlBidirectional);
        control_endpoint_context->SetMaxPacketSize(max_packet_size);
        control_endpoint_context->SetMaxBurstSize(0);
        control_endpoint_context->SetTRDequeuePointer(control_transfer_ring->GetBase());
        control_endpoint_context->SetDCS(control_transfer_ring->GetCycle());
        control_endpoint_context->SetInterval(0);
        control_endpoint_context->SetMaxPStreams(0);
        control_endpoint_context->SetMult(0);
        control_endpoint_context->SetErrorCount(3);

        controller.LoadDeviceSlot(*this);

        /// TODO: Issue Address Device command
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
}
