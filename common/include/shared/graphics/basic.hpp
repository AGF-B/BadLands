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