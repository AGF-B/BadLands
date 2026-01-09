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