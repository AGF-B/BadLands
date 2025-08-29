#include <cstdint>

#include <devices/PS2/Controller.hpp>
#include <devices/PS2/Keypoints.hpp>

#include <interrupts/APIC.hpp>

#include <screen/Log.hpp>

namespace Devices::PS2 {
	EventResponse (*keyboardEventConverter)(uint8_t byte, BasicKeyPacket* buffer);

	extern "C" void PS2KeyboardEventHandler() {
        uint32_t byte = RecvBytePS2Port1();

        APIC::SendEOI();

        if (byte > 0xFF) {
            return;
        }

        BasicKeyPacket packet;

        if (keyboardEventConverter(byte, &packet) == EventResponse::PACKET_CREATED) {
            Log::printf("Keyboard event: 0x%.2hhx, 0x%.2hhx, %s\n\r", packet.scancode, packet.keypoint, packet.flags != 0 ? "PRESSED" : "RELEASED");
        }
	}
}