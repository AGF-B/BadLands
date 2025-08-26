#pragma once

#include <cstdint>

namespace ACPI {
    struct RSDP {
        uint8_t Signature[8];
        uint8_t Checksum;
        uint8_t OEMID[6];
        uint8_t Revision;
        uint32_t RsdtAddress;
        uint32_t Length;
        uint64_t XsdtAddress;
        uint8_t ExtChecksum;
        uint8_t Reserved[3];
    };
}
