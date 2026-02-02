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

#include <cstddef>
#include <cstdint>

#include <interrupts/InterruptProvider.hpp>

namespace Interrupts {
	void kernel_idt_setup(void);
	void ForceIRQHandler(unsigned int interruptVector, void* handler);
	void ReleaseIRQ(unsigned int interruptVector);
	void RegisterIRQ(unsigned int interruptVector, InterruptProvider* provider);
	int ReserveInterrupt();
	void ReleaseInterrupt(int i);

	static inline constexpr uint8_t SOFTWARE_YIELD_IRQ = 0x21;
}
