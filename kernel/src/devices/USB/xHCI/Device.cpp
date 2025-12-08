#include <cstdint>

#include <shared/Response.hpp>

#include <devices/USB/xHCI/Controller.hpp>
#include <devices/USB/xHCI/Device.hpp>
#include <devices/USB/xHCI/Specification.hpp>

namespace Devices::USB::xHCI {
    Device::Device(Controller& controller, const DeviceDescriptor& descriptor)
        : controller{controller}, descriptor{descriptor} {}

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

        control_endpoint_context->SetEndpointType(EndpointType::ControlBidirectional);
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
