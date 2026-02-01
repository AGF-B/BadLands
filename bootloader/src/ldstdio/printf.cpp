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

#include <shared/efi/efi.h>
#include <efi/efi_datatypes.h>

#include <ldstdio.hpp>
#include <ldstdlib.hpp>

size_t Loader::printf(const CHAR16* restrict format, ...) {
    CHAR16* buffer;
    EFI::sys->BootServices->AllocatePool(
        EfiLoaderData,
        sizeof(CHAR16) * 512,
        reinterpret_cast<VOID**>(&buffer)
    );

    va_list args;
    va_start(args, format);

    size_t n = Loader::vsnprintf(buffer, 512, format, args);

    va_end(args);
    Loader::puts(buffer);

    EFI::sys->BootServices->FreePool(buffer);
    return n;
}
