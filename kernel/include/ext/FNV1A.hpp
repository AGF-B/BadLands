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

namespace Ext {
	template<class T> constexpr uint32_t FNV1A32(T x) {
		static constexpr uint32_t FNV_prime = 0x01000193;
		static constexpr uint32_t FNV_offset_basis = 0x811c9dc5;

		uint32_t hash = FNV_offset_basis;

		const uint8_t* const raw = reinterpret_cast<const uint8_t*>(&x);

		for (size_t i = 0; i < sizeof(T); ++i) {
			hash ^= *(raw + i);
			hash *= FNV_prime;
		}

		return hash;
	}

	template<> constexpr uint32_t FNV1A32<const char*>(const char* x) {
		static constexpr uint32_t FNV_prime = 0x01000193;
		static constexpr uint32_t FNV_offset_basis = 0x811c9dc5;

		uint32_t hash = FNV_offset_basis;

		while (*x != 0) {
			hash ^= *x++;
			hash *= FNV_prime;
		}

		return hash;
	}
}
