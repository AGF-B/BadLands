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

#include <devices/KeyboardDispatcher/Keypacket.hpp>

#include <fs/IFNode.hpp>

namespace Devices {
	namespace PS2 {
		using BasicKeyPacket = Devices::KeyboardDispatcher::BasicKeyPacket;

		enum class EventResponse {
			IGNORE,
			PACKET_CREATED
		};

		EventResponse KeyboardScanCodeSet1Handler(uint8_t byte, BasicKeyPacket* buffer);
		EventResponse KeyboardScanCodeSet2Handler(uint8_t byte, BasicKeyPacket* buffer);
		EventResponse KeyboardScanCodeSet3Handler(uint8_t byte, BasicKeyPacket* buffer);
	}
}