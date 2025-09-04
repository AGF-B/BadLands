#include <cstdint>

#include <devices/KeyboardDispatcher/Converter.hpp>
#include <devices/KeyboardDispatcher/Keypacket.hpp>

namespace {
    extern "C" const uint8_t KeyboardDispatcher_KeycodeMap[];
    const uint8_t (&KeycodeMap)[] = KeyboardDispatcher_KeycodeMap;
}

namespace Devices::KeyboardDispatcher {
    VirtualKeyPacket GetVirtualKeyPacket(const BasicKeyPacket& packet) {
        return {
            .keypoint = packet.keypoint,
            .keycode = KeycodeMap[packet.keypoint],
            .flags = packet.flags
        };
    }
}
