#pragma once

#include <cstdint>

namespace Devices {
	namespace PS2 {
		extern "C" {
			extern uint32_t InitializePS2Controller();
			extern uint16_t IdentifyPS2Port1();
			extern uint32_t SendBytePS2Port1(uint8_t data);
			extern uint32_t RecvBytePS2Port1();
		}
	}
}