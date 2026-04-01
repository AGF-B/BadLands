#pragma once

#include <cstddef>
#include <cstdint>

namespace Crypto {
    uint32_t CRC32(const uint8_t* data, size_t length);
}
