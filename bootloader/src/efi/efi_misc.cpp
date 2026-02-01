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

#include <efi/efi_datatypes.h>
#include <efi/efi_misc.hpp>
#include <shared/efi/efi.h>

#include <ldstdio.hpp>
#include <ldstdlib.hpp>

int Loader::guidcmp(const EFI_GUID* _guid1, const EFI_GUID* _guid2) {
    if constexpr (sizeof(EFI_GUID) % sizeof(uint64_t) == 0) {
        for (size_t i = 0; i < sizeof(EFI_GUID) / sizeof(uint64_t); ++i) {
            if (*(reinterpret_cast<const uint64_t*>(_guid1) + i) !=
                *(reinterpret_cast<const uint64_t*>(_guid2) + i)
            ) {
                return 0;
            }
        }

        return 1;
    }
    else {
        return Loader::memcmp(
            reinterpret_cast<const VOID*>(_guid1),
            reinterpret_cast<const VOID*>(_guid2),
            sizeof(EFI_GUID)
        );
    }
}

[[noreturn]] void EFI::Terminate(void) {
    Loader::puts(u"\n\rPress a key to terminate.\n\r");
    readkey();
    EFI::sys->RuntimeServices->ResetSystem(EfiResetShutdown, EFI_ABORTED, 0, nullptr);
    Loader::puts(u"System shutdown failed, press the power button for an extended period of time.\n\r");
    while (1);
}
