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
