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

#include <cstdint>

#include <shared/Response.hpp>

namespace Devices {
    namespace Block {
        class Interface {
        public:
            virtual uint64_t GetBlocksCount() const = 0;
            virtual uint64_t GetBlockSize() const = 0;
            
            virtual Success ReadBlocks(uint64_t startBlock, uint64_t blocksCount, uint8_t* buffer) = 0;
            virtual Success WriteBlocks(uint64_t startBlock, uint64_t blocksCount, const uint8_t* buffer) = 0;
        };
    }
}
