#pragma once

#include <cstdint>

#include <shared/Response.hpp>

#include <devices/USB/xHCI/Specification.hpp>

namespace Devices {
    namespace USB {
        namespace xHCI {
            class Controller;

            struct DeviceDescriptor {
                uint32_t route_string;
                uint8_t parent_port;
                uint8_t root_hub_port;
                uint8_t slot_id;
                PortSpeed port_speed;
                uint8_t depth;
            };

            class Device {
            private:
                Controller& controller;
                const DeviceDescriptor descriptor;

                ContextWrapper* context_wrapper = nullptr;
                TransferRing* control_transfer_ring = nullptr;

            public:
                Device(Controller& controller, const DeviceDescriptor& descriptor);

                const DeviceDescriptor& GetDescriptor() const;
                const void* GetOutputDeviceContext() const;

                Success Initialize();
                void Release();
            };
        }
    }
}
