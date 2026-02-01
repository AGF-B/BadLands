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

#include <shared/Response.hpp>

namespace Devices {
	namespace PS2 {
		Success InitializeController();
		bool ControllerForcesTranslation();
		bool ControllerForcesPort2Interrupts();
		Optional<uint16_t> IdentifyPort1();
		Success SendBytePort1(uint8_t data);
		Optional<uint8_t> RecvBytePort1();
	}
}