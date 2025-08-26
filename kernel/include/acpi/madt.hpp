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

        struct APICOverride {
            static constexpr uint8_t _Type = 0x05;
            uint8_t Type;
            uint8_t Length;
            uint8_t Reserved[2];
            uint64_t LocalApicAddress;
        };
    };
}
