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

#include <kern/memory.hpp>

namespace kern {
    class static_string : public kern::unique_ptr<char[]> {
    public:
        inline constexpr static_string() : kern::unique_ptr<char[]>{} {}
        inline constexpr static_string(char* str, size_t length) : kern::unique_ptr<char[]>{str, length} {}
        inline constexpr static_string(const static_string& other) = delete;
        inline constexpr static_string(static_string&& other) : kern::unique_ptr<char[]>{other.release(), other.size()} {}
        inline constexpr ~static_string() = default;

        inline constexpr size_t size() const { return kern::unique_ptr<char[]>::size(); }
        inline constexpr bool empty() const { return size() == 0; }
        
        inline constexpr bool operator==(const static_string& other) const {
            if (size() != other.size()) {
                return false;
            }

            for (size_t i = 0; i < size(); ++i) {
                if (get()[i] != other.get()[i]) {
                    return false;
                }
            }

            return true;
        }
    };
    
   inline constexpr static_string make_static_string(size_t length) {
        auto* str = static_cast<char*>(Heap::Allocate(length));

        if (str == nullptr) {
            return static_string{};
        }

        return static_string(str, length);
   }
}
