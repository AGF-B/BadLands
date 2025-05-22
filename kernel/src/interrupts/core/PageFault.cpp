#include <cstddef>
#include <cstdint>

#include <shared/memory/defs.hpp>

#include <interrupts/Panic.hpp>
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

    extern "C" void page_fault_handler(uint64_t errv) {
        if ((errv & PF_PRESENT) == 1) {
            Panic::Panic("PAGE FAULT VIOLATION\n\r", errv);
        }
        else {
            uint64_t CR2 = 0;
            __asm__ volatile("mov %%cr2, %0" : "=r"(CR2));

            ShdMem::VirtualAddress mapping = ShdMem::ParseVirtualAddress(CR2);
            VirtualMemory::PML4E* pml4e = VirtualMemory::GetPML4EAddress(mapping.PML4_offset);
            VirtualMemory::PDPTE* pdpte = VirtualMemory::GetPDPTEAddress(mapping.PML4_offset, mapping.PDPT_offset);
            VirtualMemory::PDE* pde = VirtualMemory::GetPDEAddress(
                mapping.PML4_offset,
                mapping.PDPT_offset,
                mapping.PD_offset
            );
            VirtualMemory::PTE* pte = VirtualMemory::GetPTEAddress(
                mapping.PML4_offset,
                mapping.PDPT_offset,
                mapping.PD_offset,
                mapping.PT_offset
            );

            if ((*pml4e & ShdMem::PML4E_PRESENT) == 0
                || (*pdpte & ShdMem::PDPTE_PRESENT) == 0
                || (*pde & ShdMem::PDE_PRESENT) == 0
                || *pte == 0) {
                Panic::Panic("WHAT DID YOU THINK WOULD HAPPEN??\n\r", errv);
            }
            
            uint64_t PRESENT = ShdMem::PTE_PRESENT;
            uint64_t READWRITE = *pte & VirtualMemory::NP_READWRITE;
            uint64_t USERMODE = *pte & VirtualMemory::NP_USERMODE;
            uint64_t PWT = *pte & VirtualMemory::NP_PWT;
            uint64_t PCD = *pte & VirtualMemory::NP_PCD;
            uint64_t PAT = (*pte & VirtualMemory::NP_PAT) << 2;
            uint64_t GLOBAL = (*pte & VirtualMemory::NP_GLOBAL) << 2;
            uint64_t PK = (*pte & VirtualMemory::NP_PK) << 34;
            
            if ((*pte & VirtualMemory::NP_ON_DEMAND) == 0) {
                Panic::Panic("MEMORY SWAPPING UNSUPPORTED\n\r", errv);
            }
            else {
                void* page = PhysicalMemory::Allocate();
                if (page == nullptr) {
                    Panic::Panic("THE COCONUT WENT NUTS (OUT OF MEMORY)\n\r", errv);
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
