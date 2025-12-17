#pragma once

#include <cstddef>
#include <cstdint>

#include <shared/Response.hpp>

#include <devices/USB/xHCI/Device.hpp>

namespace Devices {
    namespace USB {
        namespace HID {
            class Device : public xHCI::Device {
            public:
                struct HIDDescriptor {
                    static inline constexpr size_t DESCRIPTOR_SIZE = 9;
                    static inline constexpr size_t DESCRIPTOR_TYPE = 0x21;

                    uint8_t hidVersionMajor      = 0;
                    uint8_t hidVersionMinor      = 0;
                    uint8_t countryCode          = 0;
                    uint8_t descriptorsNumber    = 0;
                    uint8_t descriptorType       = 0;
                    uint16_t descriptorLength    = 0;
                };

            private:
                Optional<HIDDescriptor> ParseHIDDescriptor(const uint8_t*& data, const uint8_t* limit);

            public:
                static inline constexpr uint8_t GetClassCode() { return 0x03; }

                static Optional<Device*> Create(xHCI::Device& device);
            };
        }
    }
}