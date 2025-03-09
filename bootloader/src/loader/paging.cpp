#include <cstdint>

#include <efi/efi_misc.hpp>
#include <efi/efi.h>

#include <loader/paging.hpp>

#include <ldstdio.hpp>
#include <ldstdlib.hpp>

EfiMemoryMap Loader::GetEfiMemoryMap() {
    EfiMemoryMap efi_mmap = {
        .mmap_size  = 0,
        .mmap       = nullptr,
        .mmap_key   = 0,
        .desc_size  = 0,
        .desc_ver   = 0
    };
    EFI::sys->BootServices->GetMemoryMap(
        &efi_mmap.mmap_size,
        efi_mmap.mmap,
        &efi_mmap.mmap_key,
        &efi_mmap.desc_size,
        &efi_mmap.desc_ver
    );
    efi_mmap.mmap_size += 2 * efi_mmap.desc_size;
    // another +2 because mapping the loader info requires memory allocations that may fragment the memory map
    EFI::sys->BootServices->AllocatePool(
        EfiLoaderData,
        efi_mmap.mmap_size + 2 * efi_mmap.desc_size,
        reinterpret_cast<VOID**>(&efi_mmap.mmap)
    );
    EFI::sys->BootServices->GetMemoryMap(
        &efi_mmap.mmap_size,
        efi_mmap.mmap,
        &efi_mmap.mmap_key,
        &efi_mmap.desc_size,
        &efi_mmap.desc_ver
    );
    return efi_mmap;
}
