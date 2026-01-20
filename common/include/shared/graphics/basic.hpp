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

#include <shared/efi/efi.h>

namespace Shared::Graphics {
    struct BasicGraphics {
        uint32_t ResX;                      // horizontal resolution
        uint32_t ResY;                      // vertical resolution
        uint32_t PPSL;                      // pixels per scan line
        EFI_GRAPHICS_PIXEL_FORMAT PXFMT;    // pixel format
        uint32_t* FBADDR;                   // frame buffer address
        uint64_t FBSIZE;                    // frame buffer size
    };
}