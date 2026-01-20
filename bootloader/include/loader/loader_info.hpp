// SPDX-License-Identifier: GPL-3.0-only
//
// Copyright (C) 2026 Alexandre Boissiere
// This file is part of the BadLands operating system.
//
// This program is free software: you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation, version 3.
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
// without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with this program.
// If not, see <https://www.gnu.org/licenses/>. 

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
