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

#include <ldstdlib.hpp>

// basic and non-optimized code (no need for the loader to use more optimized versions)

int Loader::memcmp(const VOID* _buf1, const VOID* _buf2, UINTN size) {
    for (size_t i = 0; i < size; ++i) {
        if (*(static_cast<const uint8_t*>(_buf1) + i) != *(static_cast<const uint8_t*>(_buf2) + i)) {
            return 0;
        }
    }

    return 1;
}

void* Loader::memcpy(void* restrict dest, const void* restrict src, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        *(static_cast<uint8_t*>(dest) + i) = *(static_cast<const uint8_t*>(src) + i);
    }

    return dest;
}

void* Loader::memset(void* dest, int ch, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        *(static_cast<uint8_t*>(dest) + i) = static_cast<uint8_t>(ch);
    }

    return dest;
}
