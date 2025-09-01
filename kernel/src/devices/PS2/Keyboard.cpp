#include <cstdint>
#include <cstddef>

#include <devices/PS2/Controller.hpp>
#include <devices/PS2/Keyboard.hpp>
#include <devices/PS2/Keypoints.hpp>

#include <interrupts/APIC.hpp>

#include <screen/Log.hpp>

namespace Devices::PS2 {
	namespace {
		// PS/2 KEYBOARD SCAN CODE IDENTIFIERS
		static inline constexpr uint8_t SCAN_CODE_SET_1 = 0x01;
		static inline constexpr uint8_t SCAN_CODE_SET_2 = 0x02;
		static inline constexpr uint8_t SCAN_CODE_SET_3 = 0x03;

		// CONSTANTS
		static inline constexpr uint32_t MAX_RETRY = 3;
		static inline constexpr uint32_t FATAL_ERROR = 0xDEADBEEF;
		static inline constexpr uint32_t INTERNAL_ERROR = 0xBAAAAAAD;
		static inline constexpr uint8_t RESET_PASSED = 0xAA;

		// PS/2 KEYBOARD COMMANDS
		static inline constexpr uint8_t SET_LEDS = 0xED;
		static inline constexpr uint8_t ECHO = 0xEE;
		static inline constexpr uint8_t SCAN_CODE_SET_INTERACT = 0xF0;
		static inline constexpr uint8_t ENABLE_SCANNING = 0xF4;
		static inline constexpr uint8_t KBD_ACK = 0xFA;
		static inline constexpr uint8_t KBD_RESEND = 0xFE;
		static inline constexpr uint8_t KBD_RESET = 0xFF;

		// PS/2 KEYBOARD SUB-COMMANDS
		static inline constexpr uint8_t GET_SCAN_CODE_SET = 0x00;
		static inline constexpr uint8_t SET_SCAN_CODE_SET_1 = 0x01;
		static inline constexpr uint8_t SET_SCAN_CODE_SET_2 = 0x02;
		static inline constexpr uint8_t SET_SCAN_CODE_SET_3 = 0x03;

		static inline constexpr uint8_t SET_SCROLL_LOCK = 0x01;
		static inline constexpr uint8_t SET_NUMBER_LOCK = 0x02;
		static inline constexpr uint8_t SET_CAPS_LOCK = 0x04;

		static inline uint32_t SendCommand(uint8_t command) {
			for (size_t t = 0; t < MAX_RETRY; ++t) {
				uint32_t status = SendBytePS2Port1(command);

				if (status != 0) {
					continue;
				}

				status = RecvBytePS2Port1();

				if (status != KBD_RESEND) {
					return status;
				}
			}

			return INTERNAL_ERROR;
		}

		static inline unsigned int SendCommandData(uint8_t command, uint8_t data) {
			for (size_t t = 0; t < MAX_RETRY; ++t) {
				uint32_t status = SendBytePS2Port1(command);

				if (status != 0) {
					continue;
				}

				status = RecvBytePS2Port1();

				if (status != KBD_ACK) {
					continue;
				}

				status = SendBytePS2Port1(data);

				if (status != 0) {
					continue;
				}

				status = RecvBytePS2Port1();

				if (status != KBD_RESEND) {
					return status;
				}
			}

			return INTERNAL_ERROR;
		}

		static inline void DisableKeyboard() {
			APIC::MaskIRQ(PS2_PORT1_ISA_IRQ_VECTOR);
		}

		static unsigned int errorCount = 0;

		static inline unsigned int HandleInternalError() {
			if (++errorCount >= MAX_RETRY) {
				DisableKeyboard();
				return FATAL_ERROR;
			}
			uint32_t status = SendCommand(KBD_RESET);
			if (status != 0) {
				return HandleInternalError();
			}
			status = RecvBytePS2Port1();
			if (status != RESET_PASSED) {
				return HandleInternalError();
			}
			return 0;
		}

		static inline void MitigateInternalError() {
			if (errorCount > 0) {
				--errorCount;
			}
		}

		static inline unsigned int GetScanCodeSet(unsigned int* scanCodeSet) {
			uint32_t status = SendCommandData(SCAN_CODE_SET_INTERACT, GET_SCAN_CODE_SET);

			if (status == INTERNAL_ERROR) {
				if (HandleInternalError() != 0) {
					return FATAL_ERROR;
				}

				return GetScanCodeSet(scanCodeSet);
			}
			else if (status != KBD_ACK) {
				if (HandleInternalError() != 0) {
					return FATAL_ERROR;
				}

				return GetScanCodeSet(scanCodeSet);
			}

			status = RecvBytePS2Port1();

			if (status > 0xFF) {
				if (HandleInternalError() != 0) {
					return FATAL_ERROR;
				}

				return GetScanCodeSet(scanCodeSet);
			}

			MitigateInternalError();

			*scanCodeSet = status;
			return 0;
		}

		static inline unsigned int SetScanCodeSet(uint8_t scanCodeSet) {
			uint32_t status = SendCommandData(SCAN_CODE_SET_INTERACT, scanCodeSet);

			if (status == INTERNAL_ERROR) {
				if (HandleInternalError() != 0) {
					return FATAL_ERROR;
				}

				return SetScanCodeSet(scanCodeSet);
			}
			else if (status != KBD_ACK) {
				if (HandleInternalError() != 0) {
					return FATAL_ERROR;
				}

				return SetScanCodeSet(scanCodeSet);
			}

			MitigateInternalError();

			return 0;
		}

		static inline unsigned int ResetLEDS(void) {
			uint32_t status = SendCommandData(SET_LEDS, 0);

			if (status == INTERNAL_ERROR) {
				if (HandleInternalError() != 0) {
					return FATAL_ERROR;
				}

				return ResetLEDS();
			}
			else if (status != KBD_ACK) {
				if (HandleInternalError() != 0) {
					return FATAL_ERROR;
				}

				return ResetLEDS();
			}

			MitigateInternalError();

			return 0;
		}

		static inline unsigned int EchoCheck(void) {
			uint32_t status = SendCommand(ECHO);

			if (status == INTERNAL_ERROR) {
				if (HandleInternalError() != 0) {
					return FATAL_ERROR;
				}

				return EchoCheck();
			}
			else if (status != ECHO) {
				if (HandleInternalError() != 0) {
					return FATAL_ERROR;
				}

				return EchoCheck();
			}

			MitigateInternalError();

			return 0;
		}

		static inline unsigned int EnableScanning(void) {
			uint32_t status = SendCommand(ENABLE_SCANNING);

			if (status == INTERNAL_ERROR) {
				if (HandleInternalError() == 0) {
					return FATAL_ERROR;
				}

				return EnableScanning();
			}
			else if (status != KBD_ACK) {
				if (HandleInternalError() != 0) {
					return FATAL_ERROR;
				}

				return FATAL_ERROR;
			}

			MitigateInternalError();

			return 0;
		}
	}

	StatusCode InitializeKeyboard(FS::IFNode* keyboardMultiplex) {
		Log::puts("[PS/2] Initializing keyboard\n\r");

		// resets LEDs
		if (ResetLEDS() == FATAL_ERROR) {
			Log::puts("[PS/2] Could not reset keyboard LEDs\n\r");
			return StatusCode::FATAL_ERROR;
		}

		Log::puts("[PS/2] Keyboard LEDs reset\n\r");

		// tries to set scan code set 1, otherwise adapts to the current one
		unsigned int scanCodeSet = 0;

		if (GetScanCodeSet(&scanCodeSet) == FATAL_ERROR) {
			Log::puts("[PS/2] Could not query keyboard scan code set\n\r");
			return StatusCode::FATAL_ERROR;
		}
		else if (scanCodeSet != SCAN_CODE_SET_1) {
			if (SetScanCodeSet(SCAN_CODE_SET_1) == FATAL_ERROR) {
				Log::puts("[PS/2] Could not set keyboard scan code set\n\r");
				return StatusCode::FATAL_ERROR;
			}
			if (GetScanCodeSet(&scanCodeSet) == FATAL_ERROR) {
				Log::puts("[PS/2] Could not verify keyboard scan code set\n\r");
				return StatusCode::FATAL_ERROR;
			}
		}

		Log::printf("[PS/2] Detected keyboard scan code set: %u\n\r", scanCodeSet);

		// Performs ECHO to check if the device is still responsive
		if (EchoCheck() == FATAL_ERROR) {
			Log::puts("[PS/2] Keyboard ECHO check failed\n\r");
			return StatusCode::FATAL_ERROR;
		}

		Log::puts("[PS/2] Keyboard ECHO check successful\n\r");

		// selects the correct scan code converter
		if (scanCodeSet == SCAN_CODE_SET_1) {
			keyboardEventConverter = &KeyboardScanCodeSet1Handler;
		}
		else if (scanCodeSet == SCAN_CODE_SET_2) {
			keyboardEventConverter = &KeyboardScanCodeSet2Handler;
		}
		else if (scanCodeSet == SCAN_CODE_SET_3) {
			keyboardEventConverter = &KeyboardScanCodeSet3Handler;
		}
		else {
			Log::puts("[PS/2] Detected invalid keyboard scan code set\n\r");
			DisableKeyboard();
			return StatusCode::FATAL_ERROR;
		}

		Log::puts("[PS/2] Internal scan code selector initialized\n\r");

		// Re-enables keyboard scanning
		if (EnableScanning() == FATAL_ERROR) {
			Log::puts("[PS/2] Could not enable keyboard scanning\n\r");
			DisableKeyboard();
			return StatusCode::FATAL_ERROR;
		}

		Log::puts("[PS/2] Keyboard scanning enabled\n\r");

		keyboardMultiplexer = keyboardMultiplex;

		Log::puts("[PS/2] Keyboard initialized\n\r");

		return StatusCode::SUCCESS;
	}
}
