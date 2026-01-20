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

#pragma once

#include <cstddef>
#include <cstdint>

namespace Framebuffer {
    struct Info {
        uint64_t Size;
        uint32_t XResolution;
        uint32_t YResolution;
        uint32_t PixelsPerScanLine;        
    };

    void Setup();
    Info RequestInfo();
    void WriteAndFlush(uint32_t x, uint32_t y, uint32_t p);
    uint32_t Read(uint32_t x, uint32_t y);
    void Write(uint32_t x, uint32_t y, uint32_t p);
    void FlushRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    void Flush();
    void Scroll(uint64_t dy);
}
