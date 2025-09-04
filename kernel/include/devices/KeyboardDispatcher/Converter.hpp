#pragma once

#include <cstdint>

#include <devices/KeyboardDispatcher/Keypacket.hpp>

namespace Devices {
    namespace KeyboardDispatcher {
        VirtualKeyPacket GetVirtualKeyPacket(const BasicKeyPacket& packet);
    }
}
