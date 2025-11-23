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
