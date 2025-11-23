#pragma once

#include <cstdint>

namespace Utils {
    enum class MemoryOrder : uint8_t {
        RELAXED = 0,
        CONSUME = 1,
        ACQUIRE = 2,
        RELEASE = 3,
        ACQ_REL = 4,
        SEQ_CST = 5
    };
}
