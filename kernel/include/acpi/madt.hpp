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

#include <acpi/header.hpp>

namespace ACPI {
    struct MADT {
        Header hdr;
        uint32_t LocalInterruptControlAddress;
        uint32_t Flags;

        struct LocalAPIC {
            static constexpr uint8_t _Type = 0x00;
            uint8_t Type;
            uint8_t Length;
            uint8_t ACPI_Processor_UID;
            uint8_t APIC_ID;
            uint32_t Flags;
        };

        struct IOAPIC {
            static constexpr uint8_t _Type = 0x01;
            uint8_t Type;
            uint8_t Length;
            uint8_t IOAPIC_ID;
            uint8_t Reserved;
            uint32_t IOAPIC_Address;
            uint32_t GlobalSystemInterruptBase;
        };

        struct InterruptSourceOverride {
            static constexpr uint8_t _Type = 0x02;
            uint8_t Type;
            uint8_t Length;
            uint8_t Bus;
            uint8_t Source;
            uint32_t GlobalSystemInterrupt;
            uint32_t Flags;
        };

        struct APICOverride {
            static constexpr uint8_t _Type = 0x05;
            uint8_t Type;
            uint8_t Length;
            uint8_t Reserved[2];
            uint64_t LocalApicAddress;
        };
    };
}
