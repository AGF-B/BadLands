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

#include <shared/Response.hpp>
#include <shared/memory/defs.hpp>

namespace Paging {
    using PTE = Shared::Memory::PTE;
    using PDE = Shared::Memory::PDE;
    using PDPTE = Shared::Memory::PDPTE;
    using PML4E = Shared::Memory::PML4E;

    struct PTEInfo {
        bool present        : 1 = false;
        bool readWrite      : 1 = false;
        bool userMode       : 1 = false;
        bool PWT            : 1 = false;
        bool PCD            : 1 = false;
        bool accessed       : 1 = false;
        bool dirty          : 1 = false;
        bool PAT            : 1 = false;
        bool global         : 1 = false;
        bool executeDisable : 1 = false;
        uint64_t address = 0;
    };

    struct PDEInfo {
        bool present        : 1 = false;
        bool readWrite      : 1 = false;
        bool userMode       : 1 = false;
        bool PWT            : 1 = false;
        bool PCD            : 1 = false;
        bool accessed       : 1 = false;
        bool dirty          : 1 = false;
        bool pageSize     	: 1 = false;
        bool global         : 1 = false;
        bool PAT            : 1 = false;
        bool executeDisable : 1 = false;
        uint64_t address = 0;
    };

    struct PDPTEInfo {
        bool present        : 1 = false;
        bool readWrite      : 1 = false;
        bool userMode       : 1 = false;
        bool PWT            : 1 = false;
        bool PCD            : 1 = false;
        bool accessed       : 1 = false;
        bool dirty          : 1 = false;
        bool pageSize     	: 1 = false;
        bool global         : 1 = false;
        bool PAT            : 1 = false;
        bool executeDisable : 1 = false;
        uint64_t address = 0;
    };

    struct PML4EInfo {
        bool present        : 1 = false;
        bool readWrite      : 1 = false;
        bool userMode       : 1 = false;
        bool PWT            : 1 = false;
        bool PCD            : 1 = false;
        bool accessed       : 1 = false;
        bool executeDisable : 1 = false;
        uint64_t address = 0;
    };

    PTE* GetPTAddress(const Shared::Memory::VirtualAddress& address, bool usePrimary = true);
    PDE* GetPDAddress(const Shared::Memory::VirtualAddress& address, bool usePrimary = true);
    PDPTE* GetPDPTAddress(const Shared::Memory::VirtualAddress& address, bool usePrimary = true);
    PML4E* GetPML4Address(bool usePrimary = true);

    PTE* GetPTEAddress(const Shared::Memory::VirtualAddress& address, bool usePrimary = true);
    PDE* GetPDEAddress(const Shared::Memory::VirtualAddress& address, bool usePrimary = true);
    PDPTE* GetPDPTEAddress(const Shared::Memory::VirtualAddress& address, bool usePrimary = true);
    PML4E* GetPML4EAddress(const Shared::Memory::VirtualAddress& address, bool usePrimary = true);

    PTEInfo GetPTEInfo(PTE* pte);
    void    SetPTEInfo(PTE* pte, const PTEInfo& info);
    void    UnmapPTE(PTE* pte);

    PDEInfo GetPDEInfo(PDE* pde);
    void    SetPDEInfo(PDE* pde, const PDEInfo& info);
    void    UnmapPDE(PDE* pde);

    PDPTEInfo GetPDPTEInfo(PDPTE* pdpte);
    void      SetPDPTEInfo(PDPTE* pdpte, const PDPTEInfo& info);
    void      UnmapPDPTE(PDPTE* pdpte);

    PML4EInfo GetPML4EInfo(PML4E* pml4e);
    void      SetPML4EInfo(PML4E* pml4e, const PML4EInfo& info);
    void      UnmapPML4E(PML4E* pml4e);

    Optional<void*> GetPhysicalAddress(const void* virtual_address, bool usePrimary = true);

    void InvalidatePage(const void* virtual_address);
    void InvalidateTLB();

    bool IsMapped(const void* virtual_address, bool usePrimary = true);

    Success CreateSecondaryRecursiveMapping(void* CR3);
    void UpdateSecondaryRecursiveMapping(void* CR3);
    Success FreeSecondaryRecursiveMapping();
}
