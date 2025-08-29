#pragma once

namespace Devices {
	namespace PS2 {
		enum class StatusCode {
			SUCCESS,
			FATAL_ERROR
		};

		static inline constexpr uint8_t PS2_PORT1_ISA_IRQ_VECTOR = 1;

		StatusCode InitializeKeyboard();
		extern "C" void PS2_IRQ1_Handler();
	}
}