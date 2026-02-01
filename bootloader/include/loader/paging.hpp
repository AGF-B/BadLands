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
