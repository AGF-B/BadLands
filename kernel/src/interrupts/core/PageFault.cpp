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

#include <cstddef>
#include <cstdint>

#include <shared/memory/defs.hpp>

#include <interrupts/InterruptProvider.hpp>
#include <interrupts/Panic.hpp>

#include <mm/Paging.hpp>
#include <mm/PhysicalMemory.hpp>
#include <mm/VirtualMemory.hpp>

namespace ShdMem = Shared::Memory;

namespace {
    static inline constexpr uint64_t PF_PRESENT                     = 0x00000001;
    static inline constexpr uint64_t PF_WRITE                       = 0x00000002;
    static inline constexpr uint64_t PF_USERMODE                    = 0x00000004;
    static inline constexpr uint64_t PF_RESERVED_VIOLATION          = 0x00000008;
    static inline constexpr uint64_t PF_INSTRUCTION_FETCH           = 0x00000010;
    static inline constexpr uint64_t PF_PROTECTION_KEY_VIOLATION    = 0x00000020;
    static inline constexpr uint64_t PF_SHADOW_STACK_ACCESS         = 0x00000040;
    static inline constexpr uint64_t PF_HLAT                        = 0x00000080;
    static inline constexpr uint64_t PF_SGX_VIOLATION               = 0x00008000;

    static void CheckUnmappedAccess(void* sp, const Shared::Memory::VirtualAddress& mapping, uint64_t errv) {
        const auto pml4e = Paging::GetPML4EAddress(mapping);
        const auto pdpte = Paging::GetPDPTEAddress(mapping);
        const auto pde = Paging::GetPDEAddress(mapping);
        const auto pte = Paging::GetPTEAddress(mapping);

        const auto pml4e_info = Paging::GetPML4EInfo(pml4e);

        if (pml4e_info.present){
            const auto pdpte_info = Paging::GetPDPTEInfo(pdpte);

            if (pdpte_info.present) {
                const auto pde_info = Paging::GetPDEInfo(pde);

                if (pde_info.present) {
                    if (!pde_info.pageSize) {
                        if (*pte != 0) {
                            return;
                        }
                    }
                    else {
                        return;
                    }
                }
            }
        }

        Panic::Panic(sp, "UNMAPPED MEMORY ACCESS\n\r", errv);
    }

    static void PageFaultHandler(void* sp, uint64_t errv) {
        if ((errv & PF_PRESENT) == 1) {
            Panic::Panic(sp, "PAGE FAULT VIOLATION\n\r", errv);
        }
        else {
            uint64_t CR2 = 0;
            __asm__ volatile("mov %%cr2, %0" : "=r"(CR2));

            const auto mapping = ShdMem::ParseVirtualAddress(CR2);

            CheckUnmappedAccess(sp, mapping, errv);

            const auto pde = Paging::GetPDEAddress(mapping);
            const auto pde_info = Paging::GetPDEInfo(pde);

            if (pde_info.pageSize) {
                Panic::Panic(sp, "HUGE PAGE ERROR\n\r", errv);
            }
            else {
                const auto pte = Paging::GetPTEAddress(mapping);

                const uint64_t PRESENT = ShdMem::PTE_PRESENT;
                const uint64_t READWRITE = *pte & VirtualMemory::NP_READWRITE;
                const uint64_t USERMODE = *pte & VirtualMemory::NP_USERMODE;
                const uint64_t PWT = *pte & VirtualMemory::NP_PWT;
                const uint64_t PCD = *pte & VirtualMemory::NP_PCD;
                const uint64_t PAT = (*pte & VirtualMemory::NP_PAT) << 2;
                const uint64_t GLOBAL = (*pte & VirtualMemory::NP_GLOBAL) << 2;
                const uint64_t PK = (*pte & VirtualMemory::NP_PK) << 34;
                
                if ((*pte & VirtualMemory::NP_ON_DEMAND) == 0) {
                    Panic::Panic(sp, "MEMORY SWAPPING UNSUPPORTED\n\r", errv);
                }
                else {
                    void* page = PhysicalMemory::Allocate();

                    if (page == nullptr) {
                        Panic::Panic(sp, "KERNEL OUT OF MEMORY\n\r", errv);
                    }

                    *pte = PK
                        | (PhysicalMemory::FilterAddress(page) & ShdMem::PTE_ADDRESS)
                        | GLOBAL
                        | PAT
                        | PCD
                        | PWT
                        | USERMODE
                        | READWRITE
                        | PRESENT;
                }
            }
        }
    }
}

namespace Interrupts::Core {
    InterruptTrampoline page_fault_trampoline(PageFaultHandler);
}
