#pragma once

#include <cstdint>

#include <shared/efi/efi.h>
#include <shared/graphics/basic.hpp>
#include <shared/memory/defs.hpp>

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

            inline constexpr MemoryZone ScreenBackBuffer {
                .start = EfiGopFramebuffer.end(),
                .limit = 0x0000000004000000
            };

            inline constexpr MemoryZone AcpiNvs {
                .start = ScreenBackBuffer.end(),
                .limit = 0x0000000002000000
            };

            inline constexpr uint64_t UnmappedMemoryStart = AcpiNvs.end();

            inline constexpr MemoryZone RecursiveMemoryMapping {
                .start = 0xFFFFFF0000000000,
                .limit = 0x0000008000000000
            };

            inline constexpr uint64_t DMA_BITMAP_ENTRIES = DMAZone.limit / (Shared::Memory::FRAME_SIZE * 8);
            
            struct DMAZoneInfo {
                uint8_t bitmap[Shared::Memory::Layout::DMA_BITMAP_ENTRIES];
            };

            inline constexpr struct {
                size_t DMABitMap    = 0;
                size_t GFXData      = DMABitMap     + sizeof(DMAZoneInfo);
                size_t RTServices   = GFXData       + sizeof(Shared::Graphics::BasicGraphics);
                size_t PCIeECAM0    = RTServices    + sizeof(EFI_RUNTIME_SERVICES*);
                size_t AcpiRevision = PCIeECAM0     + sizeof(EFI_PHYSICAL_ADDRESS);
                size_t AcpiRSDP     = AcpiRevision  + sizeof(uint64_t);
                size_t MmapSize     = AcpiRSDP      + sizeof(EFI_PHYSICAL_ADDRESS);
                size_t MmapDescSize = MmapSize      + sizeof(uint64_t);
                size_t Mmap         = MmapDescSize  + sizeof(uint64_t);
            } OsLoaderDataOffsets;

            inline constexpr uint64_t PAGING_LOOP_MASK  = (RecursiveMemoryMapping.start >> 39) & 0x1FF;
            inline constexpr uint64_t PAGING_LOOP_1     = RecursiveMemoryMapping.start;
            inline constexpr uint64_t PAGING_LOOP_2     = PAGING_LOOP_1 | (PAGING_LOOP_MASK << 30);
            inline constexpr uint64_t PAGING_LOOP_3     = PAGING_LOOP_2 | (PAGING_LOOP_MASK << 21);
            inline constexpr uint64_t PAGING_LOOP_4     = PAGING_LOOP_3 | (PAGING_LOOP_MASK << 12);
        }
    }
}
