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

#include <cstdint>

#include <efi/efi_datatypes.h>
#include <efi/efi_misc.hpp>

#include <shared/efi/efi.h>
#include <shared/graphics/basic.hpp>

#include <loader/basic_graphics.hpp>

#include <ldstdio.hpp>

namespace {
    static constexpr EFI_GUID EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID = {
        .Data1 = 0x9042A9DE,
        .Data2 = 0x23DC,
        .Data3 = 0x4A38,
        .Data4 = { 0x96, 0xFB, 0x7A, 0xDE, 0xD0, 0x80, 0x51, 0x6A }
    };
}

Shared::Graphics::BasicGraphics Loader::LoadGraphics() {
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = nullptr;

    if (EFI::sys->BootServices->LocateProtocol(
        const_cast<EFI_GUID*>(&EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID),
        nullptr,
        reinterpret_cast<VOID**>(&gop)
    ) != EFI_SUCCESS) {
        Loader::puts(u"Could not find a suitable graphics output protocol\n\r");
        EFI::Terminate();
    }

    if (gop->Mode == nullptr) {
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* gopInfo = nullptr;
        UINTN gopInfoSize = 0;

        if (gop->QueryMode(gop, 0, &gopInfoSize, &gopInfo) != EFI_SUCCESS) {
            Loader::puts(u"Error retrieving default video mode\n\r");
            EFI::Terminate();
        }

        EFI::sys->BootServices->FreePool(gopInfo);

        if (gop->SetMode(gop, 0) != EFI_SUCCESS) {
            Loader::puts(u"Error configuring default video mode\n\r");
            EFI::Terminate();
        }
    }

    return Shared::Graphics::BasicGraphics {
        .ResX = gop->Mode->Info->HorizontalResolution,
        .ResY = gop->Mode->Info->VerticalResolution,
        .PPSL = gop->Mode->Info->PixelsPerScanLine,
        .PXFMT = gop->Mode->Info->PixelFormat,
        .FBADDR = reinterpret_cast<uint32_t*>(gop->Mode->FrameBufferBase),
        .FBSIZE = gop->Mode->FrameBufferSize
    };
}
