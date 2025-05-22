#include <cstddef>
#include <cstdint>

#include <shared/graphics/basic.hpp>
#include <shared/memory/layout.hpp>

#include <screen/Framebuffer.hpp>

namespace VML = Shared::Memory::Layout;

namespace {
    static Framebuffer::Info info;
}

namespace Framebuffer {
    Info Setup() {
        Shared::Graphics::BasicGraphics* GFXData = reinterpret_cast<Shared::Graphics::BasicGraphics*>(
            VML::OsLoaderData.start + VML::OsLoaderDataOffsets.GFXData
        );

        info.Size = GFXData->FBSIZE;
        info.Address = GFXData->FBADDR;

		info.XResolution = GFXData->ResX;
		info.YResolution = GFXData->ResY;
		info.PixelsPerScanLine = GFXData->PPSL;

        for (size_t r = 0; r < info.YResolution; ++r) {
            for(size_t c = 0; c < info.XResolution; ++c) {
                info.Address[r * info.PixelsPerScanLine + c] = 0;
            }
        }

        return info;
    }

    Info RequestInfo() {
        return info;
    }
}
