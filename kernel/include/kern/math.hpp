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

#include <limits>
#include <type_traits>

namespace kern {
    inline constexpr int64_t log(int64_t value, int64_t base) {
        if (base <= 1) {
            return std::numeric_limits<int64_t>::max();
        }
        else if (value <= 0) {
            return std::numeric_limits<int64_t>::min();
        }
        else {
            int64_t result = 0;

            while (value >= base) {
                value /= base;
                ++result;
            }

            return result;
        }
    }

    inline constexpr uint64_t log(uint64_t value, uint64_t base) {
        if (base <= 1) {
            return std::numeric_limits<uint64_t>::max();
        }
        else {
            uint64_t result = 0;

            while (value >= base) {
                value /= base;
                ++result;
            }

            return result;
        }
    }
}
