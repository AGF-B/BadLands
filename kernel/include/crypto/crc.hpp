#pragma once

#include <cstddef>
#include <cstdint>

namespace Crypto {
    uint32_t CRC32(const uint8_t* data, size_t length);
    
    class CRC32Engine {
    private:
        uint32_t crc32;

    public:
        CRC32Engine() : crc32(0xFFFFFFFF) {}

        void Update(const uint8_t* data, size_t length);
        uint32_t Finalize();
    };
}
