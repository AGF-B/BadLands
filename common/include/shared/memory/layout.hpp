#pragma once

#include <cstdint>

namespace Shared {
    namespace Memory {
        namespace Layout {
            struct MemoryZone {
                uint64_t start;
                uint64_t limit;

                inline constexpr uint64_t end() const noexcept {
                    return start + limit;
                }
            };

            inline constexpr MemoryZone DMAZone {
                .start = 0x0000000000000000,
                .limit = 0x0000000001000000
            };

            inline constexpr MemoryZone KernelImage {
                .start = 0xFFFF800000000000,
                .limit = 0x0000000020000000
            };

            inline constexpr MemoryZone OsLoaderFont {
                .start = KernelImage.end(),
                .limit = 0x0000000000080000
            };

            inline constexpr MemoryZone OsLoaderData {
                .start = OsLoaderFont.end(),
                .limit = 0x0000000001F80000
            };

            inline constexpr MemoryZone EfiRuntimeServices {
                .start = OsLoaderData.end(),
                .limit = 0x0000000004000000
            };

            inline constexpr MemoryZone EfiGopFramebuffer {
                .start = EfiRuntimeServices.end(),
                .limit = 0x0000000004000000
            };

            inline constexpr MemoryZone AcpiNvs {
                .start = EfiGopFramebuffer.end(),
                .limit = 0x0000000002000000
            };
        }
    }
}
