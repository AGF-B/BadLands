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

#include <shared/Lock.hpp>
#include <shared/LockGuard.hpp>
#include <shared/SimpleAtomic.hpp>

#include <interrupts/APIC.hpp>
#include <interrupts/IDT.hpp>
#include <interrupts/InterruptProvider.hpp>
#include <interrupts/PIT.hpp>
#include <interrupts/Panic.hpp>

// 8254 PIT Command Byte fields:
// bit 6 - 7:
//      0: channel 0 (the channel selected here)
//      1: channel 1
//      2: channel 2
//      3: read-back
// bit 4 - 5:
//      0: latch count value
//      1: low byte only
//      2: high byte only
//      3: low byte followed by high byte (the access mode chosen here)
// bit 1 - 3:
//      0: interrupt on terminal count
//      1: hardware re-triggerable one-shot
//      2: rate generator (the operating mode selected here)
//      3: square wave generator
//      4: software triggered strobe
//      5: hardware triggered strobe
//      6: rate generator, same as 2
//      7: square wave generator, same as 3
// bit 0:
//      0: 16-bit binary (the encoding chosen here)
//      1: four-digit BCD

namespace {
    static constexpr uint16_t PIT_CHANNEL_0_DATA = 0x40;
    static constexpr uint16_t PIT_CHANNEL_1_DATA = 0x41;
    static constexpr uint16_t PIT_CHANNEL_2_DATA = 0x42;
    static constexpr uint16_t PIT_MODE_REGISTER  = 0x43;

    static constexpr uint8_t PIT_COMMAND_BYTE = 0x34;
    static constexpr uint16_t PIT_RELOAD_VALUE = 1193;

    static constexpr uint8_t ISA_IRQ_PORT = 0;

    static constexpr uint8_t PIT_INTERVAL_MILLIS = 1;

    static int vector = -1;
    static bool enabled = false;
    static Utils::Lock enable_lock;
    static Utils::SimpleAtomic<uint64_t> users{0};
    static void (*pit_handler)() = nullptr;
    static volatile uint64_t millis_counter = 0;

    static void trampoline([[maybe_unused]] void* sp, [[maybe_unused]] uint64_t errv) {
        PIT::HandleInterrupt();
    }

    static Interrupts::InterruptTrampoline PIT_trampoline(trampoline);
}

#include <screen/Log.hpp>

namespace PIT {
    void Initialize() {
        millis_counter = 0;

        vector = Interrupts::ReserveInterrupt();

        if (vector >= 0) {
            APIC::SetupIRQ(ISA_IRQ_PORT, {
                .InterruptVector = static_cast<uint8_t>(vector),
                .Delivery = APIC::IRQDeliveryMode::FIXED,
                .DestinationMode = APIC::IRQDestinationMode::Logical,
                .Polarity = APIC::IRQPolarity::ACTIVE_HIGH,
                .Trigger = APIC::IRQTrigger::EDGE,
                .Masked = true,
                .Destination = APIC::GetLAPICLogicalID()
            });

            __asm__ volatile("outb %%al, %1" :: "a"(PIT_COMMAND_BYTE), "Nd"(PIT_MODE_REGISTER));
            __asm__ volatile("outb %%al, %1" :: "a"(PIT_RELOAD_VALUE & 0xFF), "Nd"(PIT_CHANNEL_0_DATA));
            __asm__ volatile("outb %%al, %1" :: "a"((PIT_RELOAD_VALUE >> 8) & 0xFF), "Nd"(PIT_CHANNEL_0_DATA));

            Interrupts::RegisterIRQ(vector, &PIT_trampoline);
        }
        else {
            Panic::PanicShutdown("COULD NOT RESERVE A BASIC TIMER INTERRUPT\n\r");
        }
    }

    bool IsEnabled() {
        return enabled;
    }

    void Enable() {
        ++users;
        Utils::LockGuard _{enable_lock};
        APIC::UnmaskIRQ(ISA_IRQ_PORT);
        enabled = true;
    }

    void Disable() {
        Utils::LockGuard _{enable_lock};
        
        if (--users == 0) {
            APIC::MaskIRQ(ISA_IRQ_PORT);
            enabled = false;
        }
    }

    void ReattachIRQ(void (*handler)()) {
        if (vector != -1) {
            Interrupts::ForceIRQHandler(vector, reinterpret_cast<void*>(handler));
        }
    }

    void ReleaseIRQ() {
        if (vector != -1) {
            Interrupts::ReleaseIRQ(vector);
        }
    }

    void SignalIRQ() {
        millis_counter += PIT_INTERVAL_MILLIS;
    }

    void SendEOI() {
        APIC::SendEOI();
    }

    void SetHandler(void (*handler)()) {
        pit_handler = handler;
    }

    void HandleInterrupt() {
        SignalIRQ();

        if (pit_handler != nullptr) {
            pit_handler();
        }

        SendEOI();
    }

    uint64_t GetCountMicros() {
        return millis_counter * 1000;
    }

    uint64_t GetCountMillis() {
        return millis_counter;
    }
}
