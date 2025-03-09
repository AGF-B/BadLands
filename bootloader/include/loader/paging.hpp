#pragma once

#include <loader/loader_info.hpp>

#include <shared/memory/defs.hpp>
#include <shared/memory/layout.hpp>

inline constexpr DMA_ZONE_START     = 0x0000000000000000;
inline constexpr DMA_ZONE_LIMIT     = 0x0000000001000000;

inline constexpr EFI_RT_SVC_REMAP   = 0xFFFF800020000000;
inline constexpr EFI_RT_SVC_LIMIT   = 0x0000000004000000;

inline constexpr EFI_GOP_REMAP      = 0xFFFF800024000000;
inline constexpr EFI_GOP_LIMIT      = 0x0000000004000000;

inline constexpr EFI_ACPI_REMAP     = 0xFFFF800028000000;
inline constexpr EFI_ACPI_LIMIT     = 0x0000000010000000;

inline constexpr PSF_FONT_REMAP     = 0xFFFF800138000000;
inline constexpr PSF_FONT_LIMIT     = 0x0000000000080000;

inline constexpr LOADER_INFO_REMAP  = 0xFFFF800138080000;
inline constexpr LOADER_INFO_LIMIT  = 0x0000000001F80000;

struct PagingInformation {
    unsigned int PAT_support;
    uint8_t MAXPHYADDR;
};

struct PTE {
    uint64_t raw;
};

struct PDE {
    uint64_t raw;
};

struct PDPTE {
    uint64_t raw;
};

struct PML4E {
    uint64_t raw;
};

struct RawVirtualAddress {
    uint16_t PML4_offset;
    uint16_t PDPT_offset;
    uint16_t PD_offset;
    uint16_t PT_offset;
    uint16_t offset;
};

namespace Loader {
    EfiMemoryMap GetEfiMemoryMap(void);

    // PML4E* setupBasicPaging(const PagingInformation* PI);
    // void prepareEFIRemap(PML4E* pml4, PagingInformation* PI);

    // void remapRuntimeServices(PML4E* pml4, EFI_MEMORY_DESCRIPTOR* rt_desc, const PagingInformation* PI);
    // void remapACPINVS(PML4E* pml4, EFI_MEMORY_DESCRIPTOR* acpi_desc, const PagingInformation* PI);
    // void mapKernel(PML4E* pml4, void* _source, void* _dest, size_t size, const PagingInformation* PI);
    // void mapLoader(PML4E* pml4, const PagingInformation* PI);
    // void remapGOP(PML4E* pml4, BasicGraphics* BasicGFX, const PagingInformation* PI);
    // void mapPSFFont(PML4E* pml4, const void** pcf_font, size_t size, const PagingInformation* PI);
    // void* setupLoaderInfo(PML4E* pml4, LoaderInfo* linfo, const PagingInformation* PI);
}
