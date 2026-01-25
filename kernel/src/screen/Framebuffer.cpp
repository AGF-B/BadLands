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

#include <cstddef>
#include <cstdint>

#include <shared/graphics/basic.hpp>
#include <shared/memory/layout.hpp>

#include <screen/Framebuffer.hpp>
#include <screen/Log.hpp>

namespace VML = Shared::Memory::Layout;

namespace {
    static uint32_t* address = nullptr;
    static uint32_t* back = nullptr;
    static Framebuffer::Info info;
    static uint64_t y_disp = 0;
}

namespace Framebuffer {
    void Setup() {
        Shared::Graphics::BasicGraphics* GFXData = reinterpret_cast<Shared::Graphics::BasicGraphics*>(
            VML::OsLoaderData.start + VML::OsLoaderDataOffsets.GFXData
        );

        info.Size = GFXData->FBSIZE;
        address = GFXData->FBADDR;

		info.XResolution = GFXData->ResX;
		info.YResolution = GFXData->ResY;
		info.PixelsPerScanLine = GFXData->PPSL;

        back = (uint32_t*)Shared::Memory::Layout::ScreenBackBuffer.start;

        for (size_t r = 0; r < info.YResolution; ++r) {
            for(size_t c = 0; c < info.XResolution; ++c) {
                address[r * info.PixelsPerScanLine + c] = 0;
                back[r * info.PixelsPerScanLine + c] = 0;
            }
        }
    }

    Info RequestInfo() {
        return info;
    }

    void WriteAndFlush(uint32_t x, uint32_t y, uint32_t p) {
        back[((y + y_disp) % info.YResolution) * info.PixelsPerScanLine + x] = p;
        address[y * info.PixelsPerScanLine + x] = p;
    }

    uint32_t Read(uint32_t x, uint32_t y) {
        return back[((y + y_disp) % info.YResolution) * info.PixelsPerScanLine + x];
    }

    void Write(uint32_t x, uint32_t y, uint32_t p) {
        back[((y + y_disp) % info.YResolution) * info.PixelsPerScanLine + x] = p;
    }

    void FlushRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
        if (y + height > info.YResolution) {
            height = info.YResolution - y;
        }

        if (x + width > info.XResolution) {
            width = info.XResolution - x;
        }
        
        for (uint32_t row = 0; row < height; ++row) {
            uint32_t screen_y = y + row;
            
            uint32_t* dest_row = &address[screen_y * info.PixelsPerScanLine + x];
            uint32_t* src_row = &back[((screen_y + y_disp) % info.YResolution) * info.PixelsPerScanLine + x];
            uint32_t bytes_to_copy = width * sizeof(uint32_t);

            __asm__ volatile("cld");
            __asm__ volatile(
                "rep movsb"
                : "=D"(dest_row), "=S"(src_row), "=c"(bytes_to_copy)
                : "0"(dest_row), "1"(src_row), "2"(bytes_to_copy)
                : "memory"
            );
        }
    }

    void Flush() {
        for (size_t y = 0; y < info.YResolution; ++y) {
            uint32_t* dest_row = &address[y * info.PixelsPerScanLine];
            uint32_t* src_row  = &back[((y + y_disp) % info.YResolution) * info.PixelsPerScanLine];
            uint64_t bytes_to_copy = info.XResolution / 2;

            __asm__ volatile("cld");
            __asm__ volatile(
                "rep movsq"
                : "=D"(dest_row), "=S"(src_row), "=c"(bytes_to_copy)
                : "0"(dest_row), "1"(src_row), "2"(bytes_to_copy)
                : "memory"
            );
        }
    }

    void Clear() {
        for (size_t y = 0; y < info.YResolution; ++y) {
            uint32_t* hw_buffer_row = &address[y * info.PixelsPerScanLine];
            uint32_t* back_buffer_row = &back[y * info.PixelsPerScanLine];

            const uint64_t pixels_to_clear = info.XResolution / 2;
            uint64_t count = pixels_to_clear;

            __asm__ volatile("cld");
            
            __asm__ volatile(
                "rep stosq"
                : "=D"(hw_buffer_row), "=c"(count)
                : "a"(0), "0"(hw_buffer_row), "1"(count)
                : "memory"
            );

            count = pixels_to_clear;

            __asm__ volatile(
                "rep stosq"
                : "=D"(back_buffer_row), "=c"(count)
                : "a"(0), "0"(back_buffer_row), "1"(count)
                : "memory"
            );
        }
    }

    void Scroll(uint64_t dy) {
        y_disp = (y_disp + dy) % info.YResolution;
    }
}
