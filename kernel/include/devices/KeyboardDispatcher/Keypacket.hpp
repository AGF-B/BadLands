// SPDX-License-Identifier: GPL-3.0-only
//
// Copyright (C) 2026 Alexandre Boissiere
// This file is part of the BadLands operating system.
//
// This program is free software: you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation, version 3.
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
// without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with this program.
// If not, see <https://www.gnu.org/licenses/>. 

#pragma once

#include <cstdint>

namespace Devices {
	namespace KeyboardDispatcher {
		static inline constexpr uint16_t FLAG_LEFT_CONTROL 	= 0x01;
		static inline constexpr uint16_t FLAG_LEFT_SHIFT  	= 0x02;
		static inline constexpr uint16_t FLAG_LEFT_ALT    	= 0x04;
		static inline constexpr uint16_t FLAG_LEFT_GUI    	= 0x08;
		static inline constexpr uint16_t FLAG_RIGHT_CONTROL	= 0x10;
		static inline constexpr uint16_t FLAG_RIGHT_SHIFT 	= 0x20;
		static inline constexpr uint16_t FLAG_RIGHT_ALT   	= 0x40;
		static inline constexpr uint16_t FLAG_RIGHT_GUI   	= 0x80;
		static inline constexpr uint16_t FLAG_KEY_PRESSED	= 0x100;

		struct BasicKeyPacket {
			uint8_t scancode;
			uint8_t keypoint;
			uint16_t flags;
		};
		
		struct VirtualKeyPacket {
            uint8_t keypoint;
            uint8_t keycode;
            uint16_t flags;
        };
	}
}
