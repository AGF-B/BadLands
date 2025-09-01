#pragma once

#include <cstdint>

#include <devices/KeyboardDispatcher/Keypacket.hpp>

#include <fs/IFNode.hpp>

namespace Devices {
	namespace PS2 {
		using BasicKeyPacket = Devices::KeyboardDispatcher::BasicKeyPacket;

		enum class EventResponse {
			IGNORE,
			PACKET_CREATED
		};

		inline constexpr unsigned int KEY_PRESSED = 0x01;

		EventResponse KeyboardScanCodeSet1Handler(uint8_t byte, BasicKeyPacket* buffer);
		EventResponse KeyboardScanCodeSet2Handler(uint8_t byte, BasicKeyPacket* buffer);
		EventResponse KeyboardScanCodeSet3Handler(uint8_t byte, BasicKeyPacket* buffer);

		extern EventResponse(*keyboardEventConverter)(uint8_t byte, BasicKeyPacket* buffer);
		extern FS::IFNode* keyboardMultiplexer;
	}
}