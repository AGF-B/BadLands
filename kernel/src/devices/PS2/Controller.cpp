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

#include <shared/PIO.hpp>
#include <shared/Response.hpp>

#include <devices/PS2/Controller.hpp>

// i8042 Status register
//  - bit 0: output buffer status (set = full, cleared = empty)
//  - bit 1: input buffer status (set = full, cleared = empty)
//  - bit 2: system flag (set = POST passed, cleared = POST failed) [POST := Power-On Self-Test]
//  - bit 3: command/data (set = data written to input buffer is for a PS/2 device, cleared = data written to input buffer is for PS/2 controller)
//  - bit 4: reserved (chipset specific)
//  - bit 5: reserved (chipset specific)
//  - bit 6: time-out error (set = time-out error, cleared = no error)
//  - bit 7: parity error (set = parity error, cleared = no error)
//
// i8042 Controller commands                                      | Response byte meaning
//  - 0x20      : Read byte 0 from internal RAM                     (controller configuration RAM)
//  - 0x21-0x3F : Read byte N from internal RAM                     (unspecified)
//  - 0x60      : Write to byte 0 of internal RAM                   (none)
//  - 0x61-0x7F : Write to byte N of internal RAM                   (none)
//  - 0xA7      : Disable second PS/2 port                          (none)
//  - 0xA8      : Enable second PS/2 port                           (none)
//  - 0xA9      : Test second PS/2 port                             (0x00 = test passed, 0x01 = clock line stuck low, 0x02 = clock line stuck hight,
//                                                                      0x03 = data line stuck low, 0x04 = data line stuck high)
//  - 0xAA      : Test PS/2 controller                              (0x55 = test passed, 0xFC = test failed)
//  - 0xAB      : Test first PS/2 port                              (0x00 = test passed, 0x01 = clock line stuck low, 0x02 = clock line stuck hight,
//                                                                      0x03 = data line stuck low, 0x04 = data line stuck high)
//  - 0xAC      : diagnostic dump (Read all RAM)                    (unspecified)
//  - 0xAD      : disable first PS/2 port                           (none)
//  - 0xAE      : enable first PS/2 port                            (none)
//  - 0xC0      : read controller input port                        (unspecified)
//  - 0xC1      : copy bits 0-3 of input port to status bits 4-7    (none)
//  - 0xC2      : copy bits 4-7 of input port to status bits 4-7    (none)
//  - 0xD0      : read controller output port                       (controller output port)
//  - 0xD1      : write next byte to controller output port         (none)
//  - 0xD2      : write next byte to first PS/2 port output buffer  (none)
//  - 0xD3      : write next byte to second PS/2 port output buffer (none)
//  - 0xD4      : write next byte to second PS/2 port input buffer  (none)
//  - 0xF0-0xFF : pulse output low for 6 ms                         (none)
//                  (bits 0-3 mask the different output lines, 0 = pulse line, 1 = don't pulse line ; bit 0 is the reset line)
//
// i8042 Controller Configuration Byte
//  - bit 0: first PS/2 port interrupt (set = enabled, cleared = disabled)
//  - bit 1: second PS/2 port interrupt (set = enabled, cleared = disabled)
//  - bit 2: system flag (set = system passed POST, cleared = system failed POST)
//  - bit 3: reserved (should be 0)
//  - bit 4: first PS/2 port clock (set = disable, cleared = disabled)
//  - bit 5: second PS/2 port clock (set = disabled, cleared = disabled)
//  - bit 6: first PS/2 port translation (set = enabled, cleared = disabled)
//  - bit 7: reserved (must be 0)
//
// i8042 Controller Output Port
//  - bit 0: system reset (always set)
//  - bit 1: A20 gate (set = enabled, cleared = disabled)
//  - bit 2: second PS/2 port clock
//  - bit 3: second PS/2 port data
//  - bit 4: output buffer full with byte from first PS/2 port (IRQ1)
//  - bit 5: output buffer full with byte from second PS/2 port (IRQ12)
//  - bit 6: first PS/2 port clock
//  - bit 7: first PS/2 port data

namespace Devices::PS2 {
    namespace {
        // i8042 IO Ports
        static constexpr uint16_t PS2_COMMAND_PORT  = 0x64; // when written to
        static constexpr uint16_t PS2_STATUS_PORT   = 0x64; // when read from
        static constexpr uint16_t PS2_DATA_PORT     = 0x60; // read/write

        // i8042 Commands used here
        static constexpr uint8_t PS2_READ_CONFIG     = 0x20;
        static constexpr uint8_t PS2_WRITE_CONFIG    = 0x60;
        static constexpr uint8_t PS2_TEST_PORT_1     = 0xAB;
        static constexpr uint8_t PS2_DISABLE_PORT_1  = 0xAD;
        static constexpr uint8_t PS2_ENABLE_PORT_1   = 0xAE;
        static constexpr uint8_t PS2_DISABLE_PORT_2  = 0xA7;
        static constexpr uint8_t PS2_ENABLE_PORT_2   = 0xA8;

        // PS2 Devices commands
        static constexpr uint8_t PS2_IDENTIFY        = 0xF2;
        static constexpr uint8_t PS2_ENABLE_SCAN     = 0xF4;
        static constexpr uint8_t PS2_DISABLE_SCAN    = 0xF5;
        static constexpr uint8_t PS2_ACK             = 0xFA;
        static constexpr uint8_t PS2_RESET           = 0xFF;
        static constexpr uint8_t PS2_RESET_FAILED_0  = 0xFC;
        static constexpr uint8_t PS2_RESET_FAILED_1  = 0xFD;
        static constexpr uint8_t PS2_RESET_PASSED    = 0xAA;

        // Constants
        static constexpr uint8_t OUTPUT_BUFFER_STATUS   = 0x01;
        static constexpr uint8_t INPUT_BUFFER_STATUS    = 0x02;
        // keeps second channel bits intact, disables interrupts on first channel
        static constexpr uint8_t PS2_CONFIG_MASK        = 0x26;
        static constexpr uint8_t PS2_CONFIG_INT_1       = 0x01;
        static constexpr uint8_t PS2_CONFIG_INT_2      = 0x02;
        static constexpr uint8_t PS2_CONFIG_PORT_2_CLK  = 0x20;
        static constexpr uint8_t PS2_CONFIG_TRANSLATION = 0x40;
        static constexpr int PS2_ERROR                  = -1;
        static constexpr uint16_t TIMEOUT               = 1000;

        static void Delay() {
            outb(0x80, 0x20);
        }

        static uint8_t ReadStatus() {
            return inb(PS2_STATUS_PORT);
        }

        static bool IsOutputAvailable() {
            return (ReadStatus() & OUTPUT_BUFFER_STATUS) != 0;
        }

        static bool IsInputAvailable() {
            return (ReadStatus() & INPUT_BUFFER_STATUS) == 0;
        }

        static void WaitForOutput() {
            while (!IsOutputAvailable());
        }

        static void WaitForInput() {
            while (!IsInputAvailable());
        }

        static void SendCommand(uint8_t command) {
            outb(PS2_COMMAND_PORT, command);
        }

        static uint8_t ReadData() {
            return inb(PS2_DATA_PORT);
        }

        static uint8_t WaitReadData() {
            WaitForOutput();
            return ReadData();
        }

        static void SendData(uint8_t data) {
            outb(PS2_DATA_PORT, data);
        }

        static void WaitSendData(uint8_t data) {
            WaitForInput();
            SendData(data);
        }

        static uint8_t ReadConfig() {
            SendCommand(PS2_READ_CONFIG);
            return WaitReadData();
        }

        static void WriteConfig(uint8_t config) {
            SendCommand(PS2_WRITE_CONFIG);
            WaitSendData(config);
        }

        static Success TryTimeout(uint16_t timeout, bool (*pred)(), void (*callback)(void* data), void* data = nullptr) {
            while (timeout--) {
                if (pred()) {
                    callback(data);
                    return Success();
                }
                Delay();
            }

            return Failure();
        }

        static Success WaitForAck(uint16_t timeout) {
            return TryTimeout(timeout, []() {
                if (!IsOutputAvailable()) {
                    return false;
                }

                return ReadData() == PS2_ACK;
            }, [](void*){});
        }

        static Success SendCommandAndWaitAck(uint16_t timeout, bool (*pred)(), uint8_t command) {
            if (!TryTimeout(timeout, pred, [](void*){}).IsSuccess()) {
                return Failure();
            }

            SendData(command);

            return WaitForAck(timeout);
        }

        static bool forces_translation = false;
        static bool forces_port2_interrupts = false;
    }

    Optional<uint16_t> IdentifyPort1() {
        if (!SendCommandAndWaitAck(TIMEOUT, &IsInputAvailable, PS2_DISABLE_SCAN).IsSuccess()) {
            return Optional<uint16_t>();
        }

        if (!SendCommandAndWaitAck(TIMEOUT, &IsInputAvailable, PS2_IDENTIFY).IsSuccess()) {
            return Optional<uint16_t>();
        }

        uint16_t identity = 0;

        if (!TryTimeout(TIMEOUT, &IsOutputAvailable, [](void* ptr) {
            uint16_t* id = static_cast<uint16_t*>(ptr);
            *id = ReadData();
        }, &identity).IsSuccess()) {
            return Optional<uint16_t>();
        }

        TryTimeout(TIMEOUT, &IsOutputAvailable, [](void* ptr) {
            uint16_t* id = static_cast<uint16_t*>(ptr);
            *id |= static_cast<uint16_t>(ReadData()) << 8;
        }, &identity);

        return Optional<uint16_t>(identity);
    }

    Success ResetPort1() {
        auto identity = IdentifyPort1();

        if (!identity.HasValue() || identity.GetValue() == 0xFFFF) {
            return Failure();
        }

        if (!TryTimeout(TIMEOUT, &IsInputAvailable, [](void*) {
            SendData(PS2_RESET);
        }).IsSuccess()) {
            return Failure();
        }

        if (!TryTimeout(TIMEOUT, [](){ 
            if (!IsOutputAvailable()) {
                return false;
            }

            return ReadData() == PS2_ACK;
        }, [](void*){}).IsSuccess()) {
            return Failure();
        }

        if (!TryTimeout(TIMEOUT, []() {
            if (!IsOutputAvailable()) {
                return false;
            }

            uint8_t response = ReadData();
            return response == PS2_RESET_PASSED;
        }, [](void*){}).IsSuccess()) {
            return Failure();
        }

        return Success();
    }

    Success InitializeController() {
        // Disable all device channels
        SendCommand(PS2_DISABLE_PORT_1);
        SendCommand(PS2_DISABLE_PORT_2);

        // Flush the controller buffer
        ReadData();

        // Disable interrupts and translation
        uint8_t config = ReadConfig();
        config &= PS2_CONFIG_MASK;
        WriteConfig(config);

        // Restore configuration
        WriteConfig(config);

        // detect and disable second PS/2 port if necessary
        SendCommand(PS2_ENABLE_PORT_2);
        config = ReadConfig();

        if ((config & PS2_CONFIG_PORT_2_CLK) == 0) {
            // Second channel exists, disable it again
            SendCommand(PS2_DISABLE_PORT_2);
            config = ReadConfig();
            config &= ~(PS2_CONFIG_INT_2 | PS2_CONFIG_PORT_2_CLK);
            WriteConfig(config);
        }

        // Test first PS/2 port
        SendCommand(PS2_TEST_PORT_1);

        if (WaitReadData() != 0x00) {
            return Failure();
        }

        // Re-enable first PS/2 port and its interrupts
        SendCommand(PS2_ENABLE_PORT_1);
        config = ReadConfig();
        config |= PS2_CONFIG_INT_1;
        WriteConfig(config);

        if (ReadConfig() & PS2_CONFIG_TRANSLATION) {
            forces_translation = true;
        } else {
            forces_translation = false;
        }

        if (ReadConfig() & PS2_CONFIG_INT_2) {
            forces_port2_interrupts = true;
        } else {
            forces_port2_interrupts = false;
        }

        // Reset device on the first PS/2 port
        return ResetPort1();
    }

    bool ControllerForcesTranslation() {
        return forces_translation;
    }

    bool ControllerForcesPort2Interrupts() {
        return forces_port2_interrupts;
    }

    Success SendBytePort1(uint8_t data) {
        if (!TryTimeout(TIMEOUT, &IsInputAvailable, [](void*){}).IsSuccess()) {
            return Failure();
        }

        SendData(data);

        return Success();
    }

    Optional<uint8_t> RecvBytePort1() {
        if (!TryTimeout(TIMEOUT, &IsOutputAvailable, [](void*){}).IsSuccess()) {
            return Optional<uint8_t>();
        }

        return Optional<uint8_t>(ReadData());
    }
}