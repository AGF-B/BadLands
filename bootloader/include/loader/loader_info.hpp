#pragma once

#include <cstdint>

#include <efi/efi.h>

#include <loader/basic_graphics.hpp>

struct EfiMemoryMap {
    UINTN mmap_size;
    EFI_MEMORY_DESCRIPTOR* mmap;
    UINTN mmap_key;
    UINTN desc_size;
    UINT32 desc_ver;
};
