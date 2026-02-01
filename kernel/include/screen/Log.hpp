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

#include <cstdarg>
#include <cstddef>
#include <cstdint>

namespace Log {
	void Setup();

	void putcAt(char c, uint32_t x, uint32_t y);
	void putc(char c);
	void puts(const char* s);
	void vprintf(const char* format, va_list args);
	void printf(const char* format, ...);

	void putAtSafe(char c, uint32_t x, uint32_t y);
	void putcSafe(char c);
	void putsSafe(const char* s);
	void vprintfSafe(const char* format, va_list args);
	void printfSafe(const char* format, ...);

	void clear();
}
