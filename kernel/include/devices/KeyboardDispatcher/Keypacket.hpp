#pragma once

#include <cstdint>

namespace Devices {
	namespace KeyboardDispatcher {
		static inline constexpr uint8_t FLAG_LEFT_CONTROL 	= 0x01;
		static inline constexpr uint8_t FLAG_LEFT_SHIFT  	= 0x02;
		static inline constexpr uint8_t FLAG_LEFT_ALT    	= 0x04;
		static inline constexpr uint8_t FLAG_LEFT_GUI    	= 0x08;
		static inline constexpr uint8_t FLAG_RIGHT_CONTROL	= 0x10;
		static inline constexpr uint8_t FLAG_RIGHT_SHIFT 	= 0x20;
		static inline constexpr uint8_t FLAG_RIGHT_ALT   	= 0x40;
		static inline constexpr uint8_t FLAG_RIGHT_GUI   	= 0x80;

		struct BasicKeyPacket {
			uint8_t scancode;
			uint8_t keypoint;
			uint8_t flags;
			uint8_t reserved = 0;
		};
		
		struct VirtualKeyPacket {
            uint8_t keypoint;
            uint8_t keycode;
            uint8_t flags;
        };
	}
}
