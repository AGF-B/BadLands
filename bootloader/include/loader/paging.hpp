#pragma once

#include <shared/graphics/basic.hpp>
#include <shared/memory/defs.hpp>
#include <shared/memory/layout.hpp>

struct EfiMemoryMap;
struct LoaderInfo;

#include <loader/loader_info.hpp>

struct PagingInformation {
    uint8_t MAXPHYADDR;
};

typedef uint64_t PTE;
typedef uint64_t PDE;
typedef uint64_t PDPTE;
typedef uint64_t PML4E;

namespace Loader {
    EfiMemoryMap GetEfiMemoryMap(void);

    PML4E* SetupBasicPaging(const PagingInformation& PI);
    void PrepareEFIRemap(PML4E* pml4, const PagingInformation& PI);

    void RemapRuntimeServices(PML4E* pml4, EFI_MEMORY_DESCRIPTOR* rt_desc, const PagingInformation& PI);
    void RemapACPINVS(PML4E* pml4, EFI_MEMORY_DESCRIPTOR* acpi_desc, const PagingInformation& PI);
    void MapKernel(PML4E* pml4, void* _source, void* _dest, size_t size, const PagingInformation& PI);
    void MapLoader(PML4E* pml4, const PagingInformation& PI);
    void RemapGOP(PML4E* pml4, Shared::Graphics::BasicGraphics& BasicGFX, const PagingInformation& PI);
    void MapPSFFont(PML4E* pml4, void*& pcf_font, size_t size, const PagingInformation& PI);
    void SetupLoaderInfo(PML4E* pml4, const LoaderInfo& linfo, const PagingInformation& PI, EfiMemoryMap& mmap);
}
