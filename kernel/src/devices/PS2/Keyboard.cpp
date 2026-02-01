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

#include <cstdint>
#include <cstddef>

#include <devices/PS2/Controller.hpp>
#include <devices/PS2/Keyboard.hpp>
#include <devices/PS2/Keypoints.hpp>

#include <interrupts/APIC.hpp>
#include <interrupts/InterruptProvider.hpp>
#include <interrupts/IDT.hpp>

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

		static constexpr uint8_t PS2_PORT1_ISA_IRQ_VECTOR = 1;
		static constexpr uint8_t PS2_PORT2_ISA_IRQ_VECTOR = 12;

		static inline uint32_t SendCommand(uint8_t command) {
			for (size_t t = 0; t < MAX_RETRY; ++t) {
				if (SendBytePort1(command).IsSuccess()) {
					const auto status_wrapper = RecvBytePort1();

					if (status_wrapper.HasValue()) {
						const uint8_t status = status_wrapper.GetValue();

						if (status != KBD_RESEND) {
							return status;
						}
					}
				}
			}

			return INTERNAL_ERROR;
		}

		static inline uint32_t SendCommandData(uint8_t command, uint8_t data) {
			for (size_t t = 0; t < MAX_RETRY; ++t) {
				if (SendBytePort1(command).IsSuccess()) {
					auto status_wrapper = RecvBytePort1();

					if (status_wrapper.HasValue() && status_wrapper.GetValue() == KBD_ACK) {
						if (SendBytePort1(data).IsSuccess()) {
							status_wrapper = RecvBytePort1();

							if (status_wrapper.HasValue()) {
								const uint8_t status = status_wrapper.GetValue();
								
								if (status != KBD_RESEND) {
									return status;
								}
							}
						}
					}
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

			const auto status_wrapper = RecvBytePort1();

			if (!status_wrapper.HasValue() || status_wrapper.GetValue() != RESET_PASSED) {
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

			const auto scan_code_wrapper = RecvBytePort1();

			if (!scan_code_wrapper.HasValue()) {
				if (HandleInternalError() != 0) {
					return FATAL_ERROR;
				}

				return GetScanCodeSet(scanCodeSet);
			}

			MitigateInternalError();

			*scanCodeSet = scan_code_wrapper.GetValue();
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

		EventResponse (*keyboardEventConverter)(uint8_t byte, BasicKeyPacket* buffer);
		FS::IFNode* keyboardMultiplexer;

		void PS2KeyboardEventHandler(void*, uint64_t) {
			Optional<uint8_t> byte_wrapper = RecvBytePort1();

			APIC::SendEOI();

			if (byte_wrapper.HasValue()) {
				const uint8_t byte = byte_wrapper.GetValue();

				BasicKeyPacket packet;

				if (keyboardEventConverter(byte, &packet) == EventResponse::PACKET_CREATED) {
					keyboardMultiplexer->Write(0, sizeof(BasicKeyPacket), reinterpret_cast<uint8_t*>(&packet));
				}
			}
		}

		void PS2FlushSecondChannel(void*,uint64_t) {
			RecvBytePort1();
			APIC::SendEOI();
		}

		static Interrupts::InterruptTrampoline PS2KeyboardTrampoline(&PS2KeyboardEventHandler);
		static Interrupts::InterruptTrampoline PS2FlushSecondChannelTrampoline(&PS2FlushSecondChannel);
	}

	StatusCode InitializeKeyboard(FS::IFNode* keyboardMultiplex) {
		Log::putsSafe("[PS/2] Initializing keyboard\n\r");

		// resets LEDs
		if (ResetLEDS() == FATAL_ERROR) {
			Log::puts("[PS/2] Could not reset keyboard LEDs\n\r");
			return StatusCode::FATAL_ERROR;
		}

		Log::putsSafe("[PS/2] Keyboard LEDs reset\n\r");

		unsigned int scanCodeSet = 0;

		if (ControllerForcesTranslation()) {
			keyboardEventConverter = &KeyboardScanCodeSet1Handler;
			Log::putsSafe("[PS/2] PS/2 controller forces translation to scan code set 1\n\r");
		}
		else {
			if (GetScanCodeSet(&scanCodeSet) == FATAL_ERROR) {
				Log::putsSafe("[PS/2] Could not query keyboard scan code set\n\r");
				return StatusCode::FATAL_ERROR;
			}
			
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
				// sets scan code set 2 as default
				if (SetScanCodeSet(SCAN_CODE_SET_2) == FATAL_ERROR) {
					Log::putsSafe("[PS/2] Could not set keyboard scan code set to 2\n\r");
				}
				else if (GetScanCodeSet(&scanCodeSet) == FATAL_ERROR) {
					Log::putsSafe("[PS/2] Could not query keyboard scan code set\n\r");
					return StatusCode::FATAL_ERROR;
				}
				else if (scanCodeSet == SCAN_CODE_SET_2) {
					keyboardEventConverter = &KeyboardScanCodeSet2Handler;
				}
				else {
					Log::putsSafe("[PS/2] Detected invalid keyboard scan code set\n\r");
					return StatusCode::FATAL_ERROR;
				}
			}

			Log::printfSafe("[PS/2] Detected keyboard scan code set: %u\n\r", scanCodeSet);
		}

		// Performs ECHO to check if the device is still responsive
		if (EchoCheck() == FATAL_ERROR) {
			Log::putsSafe("[PS/2] Keyboard ECHO check failed\n\r");
			return StatusCode::FATAL_ERROR;
		}

		Log::putsSafe("[PS/2] Keyboard ECHO check successful\n\r");

		// Re-enables keyboard scanning
		if (EnableScanning() == FATAL_ERROR) {
			Log::putsSafe("[PS/2] Could not enable keyboard scanning\n\r");
			DisableKeyboard();
			return StatusCode::FATAL_ERROR;
		}

		Log::putsSafe("[PS/2] Keyboard scanning enabled\n\r");

		keyboardMultiplexer = keyboardMultiplex;

		Log::putsSafe("[PS/2] Keyboard initialized\n\r");

		const int vector = Interrupts::ReserveInterrupt();

        if (vector < 0) {
            Log::putsSafe("[PS/2] Could not reserve an interrupt for the keyboard\n\r");
            Log::putsSafe("[PS/2] No keyboard input will be provided\n\r");
            return StatusCode::FATAL_ERROR;
        }

		Interrupts::RegisterIRQ(vector, &PS2KeyboardTrampoline);

        APIC::SetupIRQ(PS2_PORT1_ISA_IRQ_VECTOR, {
            .InterruptVector = static_cast<uint8_t>(vector),
            .Delivery = APIC::IRQDeliveryMode::FIXED,
            .DestinationMode = APIC::IRQDestinationMode::Logical,
            .Polarity = APIC::IRQPolarity::ACTIVE_HIGH,
            .Trigger = APIC::IRQTrigger::EDGE,
            .Masked = false,
            .Destination = APIC::GetLAPICLogicalID()
        });

		if (ControllerForcesPort2Interrupts()) {
			const int vector2 = Interrupts::ReserveInterrupt();

			if (vector2 < 0) {
				Log::putsSafe("[PS/2] Could not reserve an interrupt for the second PS/2 channel\n\r");
				DisableKeyboard();
				Interrupts::ReleaseIRQ(vector);
				return StatusCode::FATAL_ERROR;
			}

			Interrupts::RegisterIRQ(vector2, &PS2FlushSecondChannelTrampoline);

			APIC::SetupIRQ(PS2_PORT2_ISA_IRQ_VECTOR, {
				.InterruptVector = static_cast<uint8_t>(vector2),
				.Delivery = APIC::IRQDeliveryMode::FIXED,
				.DestinationMode = APIC::IRQDestinationMode::Logical,
				.Polarity = APIC::IRQPolarity::ACTIVE_HIGH,
				.Trigger = APIC::IRQTrigger::EDGE,
				.Masked = false,
				.Destination = APIC::GetLAPICLogicalID()
			});

			Log::putsSafe("[PS/2] Created bypass interrupt handler for second PS/2 channel\n\r");
		}

		Log::printfSafe("[PS/2] Keyboard IRQ mapped to vector 0x%02hhx\n\r", static_cast<uint8_t>(vector));

		return StatusCode::SUCCESS;
	}
}
