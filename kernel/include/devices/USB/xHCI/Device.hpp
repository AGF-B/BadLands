#pragma once

#include <cstdint>

#include <shared/Lock.hpp>
#include <shared/Response.hpp>
#include <shared/SimpleAtomic.hpp>

#include <devices/USB/xHCI/Specification.hpp>

namespace Devices {
    namespace USB {
        namespace xHCI {
            class Controller;

            struct DeviceInformation {
                uint32_t route_string;
                uint8_t parent_port;
                uint8_t root_hub_port;
                uint8_t slot_id;
                PortSpeed port_speed;
                uint8_t depth;
            };

            class Device {
            public:
                struct DeviceDescriptor {
                    uint8_t protocolVersionMajor        = 0;
                    uint8_t protocolVersionMinor        = 0;
                    uint8_t deviceClass                 = 0;
                    uint8_t deviceSubClass              = 0;
                    uint8_t deviceProtocol              = 0;
                    uint16_t maxControlPacketSize       = 0;
                    uint16_t vendorID                   = 0;
                    uint16_t productID                  = 0;
                    uint8_t deviceVersionMajor          = 0;
                    uint8_t deviceVersionMinor          = 0;
                    uint8_t manufacturerDescriptorIndex = 0;
                    uint8_t productDescriptorIndex      = 0;
                    uint8_t serialNumberDescriptorIndex = 0;
                    uint8_t configurationsNumber        = 0;
                };

            private:
                Controller& controller;
                const DeviceInformation information;

                ContextWrapper* context_wrapper = nullptr;
                TransferRing* control_transfer_ring = nullptr;

                Utils::Lock transfer_lock;
                Utils::SimpleAtomic<bool> transfer_complete{false};
                const TRB* awaiting_transfer = nullptr;
                TransferEventTRB transfer_result;

                DeviceDescriptor descriptor;

                Optional<uint16_t> GetDefaultMaxPacketSize() const;
                Success AddressDevice();
                Success InitiateTransfer(const TRB* trb, uint32_t reason);

                Success FetchDeviceDescriptor();
            public:
                Device(Controller& controller, const DeviceInformation& information);

                const DeviceInformation& GetInformation() const;
                const void* GetOutputDeviceContext() const;

                Success Initialize();
                void Release();

                void SignalTransferComplete(const TransferEventTRB& trb);

                const DeviceDescriptor& GetDeviceDescriptor() const;
            };
        }
    }
}
