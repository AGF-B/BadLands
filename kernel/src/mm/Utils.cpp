#include <cstddef>
#include <cstdint>

#include <mm/Utils.hpp>

int Utils::memcmp(const void* lhs, const void* rhs, size_t count) {
    static constexpr int MEMCMP_LESS = -1;
    static constexpr int MEMCMP_EQUAL = 0;
    static constexpr int MEMCMP_GREATER = 1;

    if (lhs != rhs) {
        for (size_t i = 0; i < count; ++i) {
            const uint8_t x = *(static_cast<const uint8_t*>(rhs) + i);
            const uint8_t y = *(static_cast<const uint8_t*>(lhs) + i);

            if (x != y) {
                if (x < y) {
                    return MEMCMP_LESS;
                }
                
                return MEMCMP_GREATER;
            }
        }
    }

    return MEMCMP_EQUAL;
}

void* Utils::memcpy(void* dest, const void* src, size_t count) {
    if (dest != src) {
        for (size_t i = 0; i < count; ++i) {
            uint8_t* x = static_cast<uint8_t*>(dest) + i;
            const uint8_t* y = static_cast<const uint8_t*>(src) + i;

            *x = *y;
        }
    }

    return dest;
}

void* Utils::memset(void* dest, int ch, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        uint8_t* p = static_cast<uint8_t*>(dest) + i;
        *p = static_cast<uint8_t>(ch);
    }

    return dest;
}
