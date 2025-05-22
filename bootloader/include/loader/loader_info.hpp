#pragma once

#include <cstdint>

#include <shared/efi/efi.h>
#include <shared/graphics/basic.hpp>
#include <shared/memory/layout.hpp>

#include <loader/kernel_loader.hpp>

struct EfiMemoryMap {
    UINTN mmap_size;
    EFI_MEMORY_DESCRIPTOR* mmap;
    UINTN mmap_key;
    UINTN desc_size;
    UINT32 desc_ver;
};

struct LoaderInfo {
    Shared::Memory::Layout::DMAZoneInfo dmaInfo;    //  describes the DMA Legacy memory region (first 16 MB)
    Shared::Graphics::BasicGraphics gfxData;        //  all the basic graphics data the kernel may need to know
    EFI_RUNTIME_SERVICES* rtServices;               //  EFI runtime services table location
    EFI_PHYSICAL_ADDRESS PCIe_ECAM_0;               //  physical address of the first ECAM entry in the MCFG ACPI table
    uint64_t AcpiRevision;                          //  ACPI Revision
    void* RSDP;                                     //  ACPI RSDP
};
