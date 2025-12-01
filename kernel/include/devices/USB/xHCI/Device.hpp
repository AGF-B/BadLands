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
                uint8_t slot_id;
                PortSpeed port_speed;
                uint8_t depth;
            };

            class Device {
            private:
                Controller& controller;
                const DeviceDescriptor descriptor;



                void OnFailure();

            public:
                Device(Controller& controller, const DeviceDescriptor& descriptor);

                Success Initialize();
            };
        }
    }
}
