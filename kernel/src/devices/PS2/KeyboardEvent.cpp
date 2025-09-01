#include <cstdint>

#include <devices/PS2/Controller.hpp>
#include <devices/PS2/Keypoints.hpp>

#include <fs/IFNode.hpp>

#include <interrupts/APIC.hpp>

#include <screen/Log.hpp>

namespace Devices::PS2 {
	EventResponse (*keyboardEventConverter)(uint8_t byte, BasicKeyPacket* buffer);
    FS::IFNode* keyboardMultiplexer;

	extern "C" void PS2KeyboardEventHandler() {
        uint32_t byte = RecvBytePS2Port1();

        APIC::SendEOI();

        if (byte > 0xFF) {
            return;
        }

        BasicKeyPacket packet;

        if (keyboardEventConverter(byte, &packet) == EventResponse::PACKET_CREATED) {
            keyboardMultiplexer->Write(0, sizeof(BasicKeyPacket), reinterpret_cast<uint8_t*>(&packet));
        }
	}
}