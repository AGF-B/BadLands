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

namespace APIC {
	enum class IRQDeliveryMode {
		FIXED,
		LOWEST_PRIORITY,
		SMI,
		NMI,
		INIT,
		EXT_INIT
	};

	enum class IRQDestinationMode {
		Physical,
		Logical
	};

	enum class IRQPolarity {
		ACTIVE_HIGH,
		ACTIVE_LOW,
		RESERVED
	};

	enum class IRQTrigger {
		EDGE,
		LEVEL,
		RESERVED
	};

	struct IRQDescriptor {
		uint8_t InterruptVector;
		IRQDeliveryMode Delivery;
		IRQDestinationMode DestinationMode;
		IRQPolarity Polarity;
		IRQTrigger Trigger;
		bool Masked;
		uint8_t Destination;
	};

	namespace Timer {
		enum class Mode {
			ONE_SHOT,
			PERIODIC,
			TSC_DEADLINE
		};

		enum class DivideConfiguration {
			BY_1,
			BY_2,
			BY_4,
			BY_8,
			BY_16,
			BY_32,
			BY_64,
			BY_128
		};

		void SetTimerLVT(uint8_t vector, Mode mode);
		void SetTimerDivideConfiguration(DivideConfiguration config);
		void SetTimerInitialCount(uint32_t count);
		uint32_t GetTimerCurrentCount();
		void UnmaskTimerLVT();
		void MaskTimerLVT();
	};

	void Initialize();

	void SetupLocalAPIC();
	uint8_t GetLAPICLogicalID();
	uint8_t GetLAPICID();
	void SendEOI();

	void MaskIRQ(uint32_t irq);
	void UnmaskIRQ(uint32_t irq);
	void SetupIRQ(uint32_t irq, IRQDescriptor descriptor);
}
