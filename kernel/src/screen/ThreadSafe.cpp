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

#include <cstdarg>
#include <cstdint>

#include <shared/Lock.hpp>
#include <shared/LockGuard.hpp>

#include <screen/Log.hpp>

namespace {
    static Utils::Lock globalLogLock;
}

namespace Log {
    void putAtSafe(char c, uint32_t x, uint32_t y) {
        Utils::LockGuard _(globalLogLock);
        Log::putcAt(c, x, y);
    }

    void putcSafe(char c) {
        Utils::LockGuard _(globalLogLock);
        Log::putc(c);
    }

    void putsSafe(const char* s) {
        Utils::LockGuard _(globalLogLock);
        Log::puts(s);
    }

    void vprintfSafe(const char* format, va_list args) {
        Utils::LockGuard _(globalLogLock);
        Log::vprintf(format, args);
    }

    void printfSafe(const char* format, ...) {
        Utils::LockGuard _(globalLogLock);
        
        va_list args;
        va_start(args, format);
        Log::vprintf(format, args);
        va_end(args);
    }
}
