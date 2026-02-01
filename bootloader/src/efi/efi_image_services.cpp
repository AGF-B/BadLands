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
#include <efi/efi_image_services.hpp>
#include <efi/efi_misc.hpp>
#include <shared/efi/efi.h>

#include <ldstdio.hpp>

namespace {
    static constexpr EFI_GUID EFI_LOADED_IMAGE_PROTOCOL_GUID = {
        .Data1 = 0x5B1B31A1,
        .Data2 = 0x9562,
        .Data3 = 0x11D2,
        .Data4 = { 0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B }
    };
}

EFI_LOADED_IMAGE_PROTOCOL* EFI::getLoadedImageProtocol(EFI_HANDLE ImageHandle) {
    EFI_GUID** ImageProtocols = nullptr;
    UINTN ProtocolCount = 0;

    auto protocolBuffer = &ImageProtocols;
    auto protocolBufferCount = &ProtocolCount;

    EFI_STATUS status;

    if ((status = EFI::sys->BootServices->ProtocolsPerHandle(
        ImageHandle,
        protocolBuffer,
        protocolBufferCount
    )) != EFI_SUCCESS) {
        Loader::puts(u"Error retrieving system loader image protocols\n\r");
        EFI::Terminate();
    }

    for (UINTN i = 0; i < ProtocolCount; ++i) {
        if (Loader::guidcmp(ImageProtocols[i], &EFI_LOADED_IMAGE_PROTOCOL_GUID)) {
            EFI_LOADED_IMAGE_PROTOCOL* efi_lip = nullptr;

            if (EFI::sys->BootServices->OpenProtocol(
                ImageHandle,
                ImageProtocols[i],
                reinterpret_cast<VOID**>(&efi_lip),
                ImageHandle,
                nullptr,
                EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL
            ) != EFI_SUCCESS) {
                Loader::puts(u"Error retrieving system loader image information\n\r");
                EFI::Terminate();
            }

            EFI::sys->BootServices->FreePool(ImageProtocols);
            return efi_lip;
        }
    }

    EFI::sys->BootServices->FreePool(ImageProtocols);
    return nullptr;
}
