#pragma once

#include <cstddef>
#include <cstdint>

#include <type_traits>

namespace Shared {
    namespace Memory {
        inline constexpr uint64_t PAGE_SIZE          = 0x1000;
        inline constexpr uint64_t FRAME_SIZE         = 0x1000;

        inline constexpr uint64_t PML4_ENTRIES       = 0x200;
        inline constexpr uint64_t PDPT_ENTRIES       = 0x200;
        inline constexpr uint64_t PD_ENTRIES         = 0x200;
        inline constexpr uint64_t PT_ENTRIES         = 0x200;

        inline constexpr uint64_t ENTRY_SIZE         = 8;

        inline constexpr uint64_t PML4E_COVERAGE	= 0x0000008000000000;
        inline constexpr uint64_t PDPTE_COVERAGE	= 0x0000000040000000;
        inline constexpr uint64_t PDE_COVERAGE		= 0x0000000000200000;
        inline constexpr uint64_t PTE_COVERAGE		= 0x0000000000001000;

        inline constexpr uint64_t PML4E_PRESENT      = 0x0000000000000001;
        inline constexpr uint64_t PML4E_READWRITE    = 0x0000000000000002;
        inline constexpr uint64_t PML4E_USERMODE     = 0x0000000000000004;
        inline constexpr uint64_t PML4E_PWT          = 0x0000000000000008;
        inline constexpr uint64_t PML4E_PCD          = 0x0000000000000010;
        inline constexpr uint64_t PML4E_ACCESSED     = 0x0000000000000020;
        inline constexpr uint64_t PML4E_ADDRESS      = 0x000FFFFFFFFFF000;
        inline constexpr uint64_t PML4E_XD           = 0x8000000000000000;

        inline constexpr uint64_t PDPTE_PRESENT      = 0x0000000000000001;
        inline constexpr uint64_t PDPTE_READWRITE    = 0x0000000000000002;
        inline constexpr uint64_t PDPTE_USERMODE     = 0x0000000000000004;
        inline constexpr uint64_t PDPTE_PWT          = 0x0000000000000008;
        inline constexpr uint64_t PDPTE_PCD          = 0x0000000000000010;
        inline constexpr uint64_t PDPTE_ACCESSED     = 0x0000000000000020;
        inline constexpr uint64_t PDPTE_DIRTY        = 0x0000000000000040;
        inline constexpr uint64_t PDPTE_PAGE_SIZE    = 0x0000000000000080;
        inline constexpr uint64_t PDPTE_GLOBAL       = 0x0000000000000100;
        inline constexpr uint64_t PDPTE_ADDRESS      = 0x000FFFFFFFFFF000;
        inline constexpr uint64_t PDPTE_XD           = 0x8000000000000000;

        inline constexpr uint64_t PDE_PRESENT        = 0x0000000000000001;
        inline constexpr uint64_t PDE_READWRITE      = 0x0000000000000002;
        inline constexpr uint64_t PDE_USERMODE       = 0x0000000000000004;
        inline constexpr uint64_t PDE_PWT            = 0x0000000000000008;
        inline constexpr uint64_t PDE_PCD            = 0x0000000000000010;
        inline constexpr uint64_t PDE_ACCESSED       = 0x0000000000000020;
        inline constexpr uint64_t PDE_DIRTY          = 0x0000000000000040;
        inline constexpr uint64_t PDE_PAGE_SIZE      = 0x0000000000000080;
        inline constexpr uint64_t PDE_GLOBAL         = 0x0000000000000100;
        inline constexpr uint64_t PDE_ADDRESS        = 0x000FFFFFFFFFF000;
        inline constexpr uint64_t PDE_PK             = 0x7800000000000000;
        inline constexpr uint64_t PDE_XD             = 0x8000000000000000;

        inline constexpr uint64_t PTE_PRESENT        = 0x0000000000000001;
        inline constexpr uint64_t PTE_READWRITE      = 0x0000000000000002;
        inline constexpr uint64_t PTE_USERMODE       = 0x0000000000000004;
        inline constexpr uint64_t PTE_PWT            = 0x0000000000000008;
        inline constexpr uint64_t PTE_PCD            = 0x0000000000000010;
        inline constexpr uint64_t PTE_ACCESSED       = 0x0000000000000020;
        inline constexpr uint64_t PTE_DIRTY          = 0x0000000000000040;
        inline constexpr uint64_t PTE_PAT            = 0x0000000000000080;
        inline constexpr uint64_t PTE_GLOBAL         = 0x0000000000000100;
        inline constexpr uint64_t PTE_ADDRESS        = 0x000FFFFFFFFFF000;
        inline constexpr uint64_t PTE_PK             = 0x7800000000000000;
        inline constexpr uint64_t PTE_XD             = 0x8000000000000000;

        inline constexpr uint64_t PTE_WRITEBACK         = 0;
        inline constexpr uint64_t PTE_WRITETHROUGH      = PTE_PWT;
        inline constexpr uint64_t PTE_WEAK_UNCACHEABLE  = PTE_PCD;
        inline constexpr uint64_t PTE_UNCACHEABLE       = PTE_PCD | PTE_PWT;
        inline constexpr uint64_t PTE_WRITECOMBINING    = PTE_PAT;

        typedef uint64_t PTE;
        typedef uint64_t PDE;
        typedef uint64_t PDPTE;
        typedef uint64_t PML4E;

        struct VirtualAddress {
            uint16_t PML4_offset;
            uint16_t PDPT_offset;
            uint16_t PD_offset;
            uint16_t PT_offset;
            uint16_t offset;
        };

        template<typename T> concept AddressType = std::is_same_v<T, uint64_t> || std::is_pointer<T>::value;
        template<AddressType T> constexpr VirtualAddress ParseVirtualAddress(const T address) {
            if constexpr (std::is_same_v<T, uint64_t>) {
                return VirtualAddress{
                    .PML4_offset    = static_cast<uint16_t>((address >> 39) & 0x1FF),
                    .PDPT_offset    = static_cast<uint16_t>((address >> 30) & 0x1FF),
                    .PD_offset      = static_cast<uint16_t>((address >> 21) & 0x1FF),
                    .PT_offset      = static_cast<uint16_t>((address >> 12) & 0x1FF),
                    .offset         = static_cast<uint16_t>(address & 0xFFF)
                };
            }
            else {
                return ParseVirtualAddress(reinterpret_cast<uint64_t>(address));
            }
        };

        template<AddressType T> void ZeroPage(T address) {
            if constexpr (std::is_same_v<T, uint64_t>) {
                uint64_t* pagePtr = reinterpret_cast<uint64_t*>(address);
                for (size_t qword = 0; qword < FRAME_SIZE / sizeof(uint64_t); ++qword) {
                    *(pagePtr + qword) = 0;
                }
            }
            else {
                ZeroPage(reinterpret_cast<uint64_t>(address));
            }
        };
    }
}
