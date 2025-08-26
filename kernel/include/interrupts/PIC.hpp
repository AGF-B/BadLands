#pragma once

namespace PIC {
	extern "C" {
		static inline constexpr uint8_t MasterPIC_IRQ_Remap = 0x20;
		static inline constexpr uint8_t SlavePIC_IRQ_Remap = 0x28;

		extern void InitializePIC();
		extern void SpuriousPIC();
	}
}
