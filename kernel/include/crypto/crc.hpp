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
