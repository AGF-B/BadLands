#pragma once

#include <cstdint>

namespace Devices {
	namespace KeyboardDispatcher {
		struct BasicKeyPacket {
			uint8_t scancode;
			uint8_t keypoint;
			uint8_t flags;
			uint8_t reserved = 0;
		};
	}
}
