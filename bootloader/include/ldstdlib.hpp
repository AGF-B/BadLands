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

#include <efi/efi_datatypes.h>
#include <shared/efi/efi.h>

namespace Loader {
    INTN itoa(INTN x, CHAR16* buffer, INT32 radix);
    INTN utoa(UINTN x, CHAR16* buffer, INT32 radix);

    int memcmp(const VOID* _buf1, const VOID* _buf2, UINTN size);
    void* memcpy(void* restrict dest, const void* restrict src, size_t count);
    void* memset(void* dest, int ch, size_t count);
}
