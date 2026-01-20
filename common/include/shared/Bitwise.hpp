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

#include <type_traits>

template<typename T> concept integral_t = std::is_integral_v<T>;

template<integral_t T>
constexpr T& ModifyPacked(T& target, T mask, uint8_t shift, T value) {
    target = (target & ~mask) | ((value << shift) & mask);
    return target;
}

template<integral_t T, integral_t R>
constexpr T& ModifyPacked(T& target, T mask, uint8_t shift, R value) {
    return ModifyPacked<T>(target, mask, shift, static_cast<T>(value));
}

template<integral_t T>
constexpr T GetPacked(const T& source, T mask, uint8_t shift) {
    return (source & mask) >> shift;
}

template<integral_t T, integral_t R>
constexpr R GetPacked(const T& source, T mask, uint8_t shift) {
    return static_cast<R>(GetPacked<T>(source, mask, shift));
}
