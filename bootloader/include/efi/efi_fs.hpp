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

#ifndef __EFI_STANDALONE__

#include <efi/efi_datatypes.h>
#include <shared/efi/efi.h>

namespace EFI {
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* getDeviceSFSP(EFI_HANDLE ImageHandle, EFI_HANDLE DeviceHandle);
    EFI_FILE_PROTOCOL* openDeviceVolume(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* SFSP);
    EFI_FILE_PROTOCOL* openReadOnlyFile(EFI_FILE_PROTOCOL* Volume, CHAR16* FilePath);
    EFI_FILE_INFO* getFileInfo(EFI_FILE_PROTOCOL* File);
};

#endif
