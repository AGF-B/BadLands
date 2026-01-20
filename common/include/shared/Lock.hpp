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

#include <shared/SimpleAtomic.hpp>

namespace Utils {
    class Lock{
    public:
        inline bool trylock() noexcept {
            static constexpr bool desired = true;
            bool expected = false;

            if (!pvt_lock.compare_exchange(expected, desired)) {
                return false;
            }

            return true;
        }

        inline void lock() noexcept {
            while (!trylock()) { __asm__ volatile("pause"); }
        }

        inline void unlock() noexcept {
            pvt_lock.store(false);
        }

        inline Lock& operator=(const Lock& m) {
            if (this != &m) {
                pvt_lock.store(false);
            }

            return *this;
        }

    private:
        SimpleAtomic<bool> pvt_lock{false};
    };
}
