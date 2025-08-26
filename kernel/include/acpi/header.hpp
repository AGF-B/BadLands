#pragma once

#include <cstdint>

namespace ACPI {
    struct Header {
        uint8_t Signature[4];
        uint32_t Length;
        uint8_t Revision;
        uint8_t Checksum;
        uint8_t OEMID[6];
        uint8_t OEMTableID[8];
        uint32_t OEMRevision;
        uint8_t CreatorID[4];
        uint32_t CreatorRevision;
    };
}
