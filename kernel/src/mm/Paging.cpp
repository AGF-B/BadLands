#include <cstddef>
#include <cstdint>

#include <shared/Response.hpp>
#include <shared/memory/defs.hpp>
#include <shared/memory/layout.hpp>

#include <mm/Paging.hpp>
#include <mm/PhysicalMemory.hpp>
#include <mm/VirtualMemory.hpp>
#include <mm/VirtualMemoryLayout.hpp>

namespace ShdMem = Shared::Memory;

namespace Paging {
    namespace {
        namespace Secondary {
            static constexpr uint64_t PAGING_LOOP_MASK  = (VirtualMemoryLayout::SecondaryRecursiveMapping.start >> 39) & 0x1FF;
            static constexpr uint64_t PAGING_LOOP_1		= VirtualMemoryLayout::SecondaryRecursiveMapping.start;
            static constexpr uint64_t PAGING_LOOP_2     = PAGING_LOOP_1 | (PAGING_LOOP_MASK << 30);
            static constexpr uint64_t PAGING_LOOP_3     = PAGING_LOOP_2 | (PAGING_LOOP_MASK << 21);
            static constexpr uint64_t PAGING_LOOP_4     = PAGING_LOOP_3 | (PAGING_LOOP_MASK << 12);
        }

        static constexpr auto primaryMapping = ShdMem::ParseVirtualAddress(
            ShdMem::Layout::RecursiveMemoryMapping.start
        );        
        static constexpr auto secondaryMapping = ShdMem::ParseVirtualAddress(
            VirtualMemoryLayout::SecondaryRecursiveMapping.start
        );

        static constexpr uint16_t SECONDARY_PML4_INDEX = secondaryMapping.PML4_offset;
		static constexpr uint16_t PRIMARY_PML4_INDEX = primaryMapping.PML4_offset;

        static constexpr auto dma_mapping = ShdMem::ParseVirtualAddress(ShdMem::Layout::DMAZone.start);
        static constexpr auto dma_end_mapping = ShdMem::ParseVirtualAddress(
            ShdMem::Layout::DMAZone.start + ShdMem::Layout::DMAZone.limit
        );

        static Success ShareKernelMemoryToSecondaryMapping() {
            // share main kernel mappings
            for (uint16_t i = 256; i < SECONDARY_PML4_INDEX; ++i) {
                GetPML4Address(false)[i] = GetPML4Address(true)[i];
            }

            // share DMA pages

            static_assert(dma_mapping.PML4_offset == dma_end_mapping.PML4_offset);
            static_assert(dma_mapping.PDPT_offset == dma_end_mapping.PDPT_offset);
            static_assert(dma_mapping.PD_offset == 0);

            // DMA is mapped with 2MB pages 

            static constexpr uint16_t dma_pds = dma_end_mapping.PD_offset - dma_mapping.PD_offset;

            static_assert(dma_pds == 8);

            const auto dma_pml4e    = GetPML4EAddress(dma_mapping, false);
            const auto dma_pdpte    = GetPDPTEAddress(dma_mapping, false);

            void* phys_first_pml4e = PhysicalMemory::Allocate();

            if (phys_first_pml4e == nullptr) {
                return Failure();
            }

            SetPML4EInfo(dma_pml4e, {
                .present = 1,
                .readWrite = 1,
                .address = PhysicalMemory::FilterAddress(phys_first_pml4e)
            });

            InvalidatePage(dma_pdpte);

            void* phys_first_pdpte = PhysicalMemory::Allocate();

            if (phys_first_pdpte == nullptr) {
                PhysicalMemory::Free(phys_first_pml4e);
                return Failure();
            }

            SetPDPTEInfo(dma_pdpte, {
                .present = 1,
                .readWrite = 1,
                .address = PhysicalMemory::FilterAddress(phys_first_pdpte)
            });

            for (size_t i = 0; i < dma_pds; ++i) {
                auto pdpte = dma_mapping;
                pdpte.PDPT_offset += i;

                auto* const src_pdpte = GetPDPTEAddress(pdpte, true);
                auto* const dst_pdpte = GetPDPTEAddress(pdpte, false);

                SetPDPTEInfo(dst_pdpte, GetPDPTEInfo(src_pdpte));
            }

            return Success();
        }

        // Frees a PDE/PT and all its children
        static Success FreePDE(const ShdMem::VirtualAddress& mapping, PDE* pde) {
            for (size_t i = 0; i < ShdMem::PD_ENTRIES; ++i) {
                auto map = mapping;
                map.PT_offset = i;

                const auto pte = GetPTEAddress(map, false);
                const auto pte_info = GetPTEInfo(pte);

                if (pte_info.present) {
                    const auto address = pte_info.address;
                    
                    if (!PhysicalMemory::Free(reinterpret_cast<void*>(address)).IsSuccess()) {
                        return Failure();
                    }
                }
            }

            const auto pde_info = GetPDEInfo(pde);
            PhysicalMemory::Free(reinterpret_cast<void*>(pde_info.address));

            return Success();
        }

        // Frees a PDPTE/PD and all its children
        static Success FreePDPTE(const ShdMem::VirtualAddress& mapping, PDPTE* pdpte) {
            for (size_t i = 0; i < ShdMem::PD_ENTRIES; ++i) {
                auto map = mapping;
                map.PD_offset = i;

                const auto pde = GetPDEAddress(map, false);
                const auto pde_info = GetPDEInfo(pde);

                if (pde_info.present) {
                    if (pde_info.pageSize) {
                        const auto address = pde_info.address;
                        
                        if (!PhysicalMemory::Free2MB(reinterpret_cast<void*>(address)).IsSuccess()) {
                            return Failure();
                        }
                    }
                    else {
                        if (!FreePDE(map, pde).IsSuccess()) {
                            return Failure();
                        }
                    }
                }
            }

            const auto pdpte_info = GetPDPTEInfo(pdpte);

            return PhysicalMemory::Free(reinterpret_cast<void*>(pdpte_info.address));
        }

        // Frees a PML4E/PDPT and all its children
        static Success FreePML4E(const ShdMem::VirtualAddress& mapping, PML4E* pml4e) {
            for (size_t i = 0; i < ShdMem::PDPT_ENTRIES; ++i) {
                auto map = mapping;
                map.PDPT_offset = i;

                const auto pdpte = GetPDPTEAddress(map, false);
                const auto pdpte_info = GetPDPTEInfo(pdpte);

                if (pdpte_info.present) {
                    if (pdpte_info.pageSize) {
                        const auto address = pdpte_info.address;
                        
                        if (!PhysicalMemory::Free1GB(reinterpret_cast<void*>(address)).IsSuccess()) {
                            return Failure();
                        }
                    }
                    else {
                        if (!FreePDPTE(map, pdpte).IsSuccess()) {
                            return Failure();
                        }
                    }
                }
            }

            const auto pml4e_info = GetPML4EInfo(pml4e);

            return PhysicalMemory::Free(reinterpret_cast<void*>(pml4e_info.address));
        }
    }

    static Success FreeFirstPML4E() {
        const auto primary_pml4e = GetPML4EAddress(primaryMapping, false);
        const auto primary_pml4e_info = GetPML4EInfo(primary_pml4e);
        
        if (!primary_pml4e_info.present) {
            return Success();
        }

        // Free the first PDPT
        auto mapping = dma_end_mapping;

        for (size_t i = dma_end_mapping.PD_offset; i < ShdMem::PD_ENTRIES; ++i) {
            mapping.PD_offset = i;

            const auto pde = GetPDEAddress(mapping, false);
            const auto pde_info = GetPDEInfo(pde);

            if (pde_info.present) {
                if (pde_info.pageSize) {
                    const auto address = pde_info.address;
                    
                    if (!PhysicalMemory::Free2MB(reinterpret_cast<void*>(address)).IsSuccess()) {
                        return Failure();
                    }
                }
                else {
                    if (!FreePDE(mapping, pde).IsSuccess()) {
                        return Failure();
                    }
                }
            }
        }

        // Free the other PDPTs
        for (size_t i = dma_end_mapping.PDPT_offset + 1; i < ShdMem::PDPT_ENTRIES; ++i) {
            mapping = dma_end_mapping;
            mapping.PDPT_offset = i;

            const auto pdpte = GetPDPTAddress(mapping, false);
            const auto pdpte_info = GetPDPTEInfo(pdpte);

            if (pdpte_info.present) {
                if (pdpte_info.pageSize) {
                    const auto address = pdpte_info.address;
                    
                    if (!PhysicalMemory::Free1GB(reinterpret_cast<void*>(address)).IsSuccess()) {
                        return Failure();
                    }
                }
                else {
                    if (!FreePDPTE(mapping, pdpte).IsSuccess()) {
                        return Failure();
                    }
                }
            }
        }

        return Success();
    }

    PTE* GetPTAddress(const ShdMem::VirtualAddress& address, bool usePrimary) {
        const uint64_t pml4_offset = address.PML4_offset;
        const uint64_t pdpt_offset = address.PDPT_offset;
        const uint64_t pd_offset   = address.PD_offset;

        if (usePrimary) {
			return reinterpret_cast<PTE*>(ShdMem::Layout::PAGING_LOOP_1
				| (pml4_offset << 30) | (pdpt_offset << 21) | (pd_offset << 12)
			);
		}
		else {
			return reinterpret_cast<PTE*>(Secondary::PAGING_LOOP_1
				| (pml4_offset << 30) | (pdpt_offset << 21) | (pd_offset << 12)
			);
		}
    }

    PDE* GetPDAddress(const ShdMem::VirtualAddress& address, bool usePrimary) {
        const uint64_t pml4_offset = address.PML4_offset;
        const uint64_t pdpt_offset = address.PDPT_offset;

        if (usePrimary) {
			return reinterpret_cast<PDE*>(ShdMem::Layout::PAGING_LOOP_2
				| (pml4_offset << 21) | (pdpt_offset << 12)
			);
		}
		else {
			return reinterpret_cast<PDE*>(Secondary::PAGING_LOOP_2
				| (pml4_offset << 21) | (pdpt_offset << 12)
			);
		}
    }

    PDPTE* GetPDPTAddress(const ShdMem::VirtualAddress& address, bool usePrimary) {
        const uint64_t pml4_offset = address.PML4_offset;

        if (usePrimary) {
			return reinterpret_cast<PDPTE*>(ShdMem::Layout::PAGING_LOOP_3 | (pml4_offset << 12));
		}
		else {
			return reinterpret_cast<PDPTE*>(Secondary::PAGING_LOOP_3 | (pml4_offset << 12));
		}
    }

    PML4E* GetPML4Address(bool usePrimary) {
        if (usePrimary) {
			return reinterpret_cast<PML4E*>(ShdMem::Layout::PAGING_LOOP_4);
		}
		else {
			return reinterpret_cast<PML4E*>(Secondary::PAGING_LOOP_4);
		}
    }

    PTE* GetPTEAddress(const ShdMem::VirtualAddress& address, bool usePrimary) {
        return GetPTAddress(address, usePrimary) + address.PT_offset;
	};

    PDE* GetPDEAddress(const ShdMem::VirtualAddress& address, bool usePrimary) {
		return GetPDAddress(address, usePrimary) + address.PD_offset;
	};

	PDPTE* GetPDPTEAddress(const ShdMem::VirtualAddress& address, bool usePrimary) {
		return GetPDPTAddress(address, usePrimary) + address.PDPT_offset;
	};

	PML4E* GetPML4EAddress(const ShdMem::VirtualAddress& address, bool usePrimary) {
		return GetPML4Address(usePrimary) + address.PML4_offset;
	};

    PTEInfo GetPTEInfo(PTE* pte) {
        return PTEInfo {
            .present        = (*pte & ShdMem::PTE_PRESENT) != 0,
            .readWrite      = (*pte & ShdMem::PTE_READWRITE) != 0,
            .userMode       = (*pte & ShdMem::PTE_USERMODE) != 0,
            .PWT            = (*pte & ShdMem::PTE_PWT) != 0,
            .PCD            = (*pte & ShdMem::PTE_PCD) != 0,
            .accessed       = (*pte & ShdMem::PTE_ACCESSED) != 0,
            .dirty          = (*pte & ShdMem::PTE_DIRTY) != 0,
            .PAT            = (*pte & ShdMem::PTE_PAT) != 0,
            .global         = (*pte & ShdMem::PTE_GLOBAL) != 0,
            .executeDisable = (*pte & ShdMem::PTE_XD) != 0,
            .address        = (*pte & ShdMem::PTE_ADDRESS)
        };
    }

    void SetPTEInfo(PTE* pte, const PTEInfo& info) {
        uint64_t entry = 0;

        if (info.present)        entry |= ShdMem::PTE_PRESENT;
        if (info.readWrite)      entry |= ShdMem::PTE_READWRITE;
        if (info.userMode)       entry |= ShdMem::PTE_USERMODE;
        if (info.PWT)            entry |= ShdMem::PTE_PWT;
        if (info.PCD)            entry |= ShdMem::PTE_PCD;
        if (info.PAT)            entry |= ShdMem::PTE_PAT;
        if (info.global)         entry |= ShdMem::PTE_GLOBAL;
        if (info.executeDisable) entry |= ShdMem::PTE_XD;

        entry |= info.address & ShdMem::PTE_ADDRESS;

        *pte = entry;
    }

    void UnmapPTE(PTE* pte) {
        *pte = 0;
    }

    PDEInfo GetPDEInfo(PDE* pde) {
        PDEInfo info {
            .present = (*pde & ShdMem::PDE_PRESENT) != 0,
            .readWrite = (*pde & ShdMem::PDE_READWRITE) != 0,
            .userMode = (*pde & ShdMem::PDE_USERMODE) != 0,
            .PWT = (*pde & ShdMem::PDE_PWT) != 0,
            .PCD = (*pde & ShdMem::PDE_PCD) != 0,
            .accessed = (*pde & ShdMem::PDE_ACCESSED) != 0,
            .pageSize = (*pde & ShdMem::PDE_PAGE_SIZE) != 0,
            .executeDisable = (*pde & ShdMem::PDE_XD) != 0
        };

        if (info.pageSize) {
            info.dirty      = (*pde & ShdMem::PDE_DIRTY) != 0;
            info.global     = (*pde & ShdMem::PDE_GLOBAL) != 0;
            info.PAT        = (*pde & ShdMem::PDE_PAT) != 0;
            info.address    = (*pde & ShdMem::PDE_2MB_ADDRESS);
        }
        else {
            info.dirty      = false;
            info.global     = false;
            info.PAT        = false;
            info.address    = (*pde & ShdMem::PDE_ADDRESS);
        }

        return info;
    }

    void SetPDEInfo(PDE* pde, const PDEInfo& info) {
        uint64_t entry = 0;

        if (info.present)        entry |= ShdMem::PDE_PRESENT;
        if (info.readWrite)      entry |= ShdMem::PDE_READWRITE;
        if (info.userMode)       entry |= ShdMem::PDE_USERMODE;
        if (info.PWT)            entry |= ShdMem::PDE_PWT;
        if (info.PCD)            entry |= ShdMem::PDE_PCD;
        if (info.pageSize)       entry |= ShdMem::PDE_PAGE_SIZE;
        if (info.executeDisable) entry |= ShdMem::PDE_XD;

        if (info.pageSize) {
            if (info.global)    entry |= ShdMem::PDE_GLOBAL;
            if (info.PAT)       entry |= ShdMem::PDE_PAT;
            entry |= info.address & ShdMem::PDE_2MB_ADDRESS;
        }
        else {
            entry |= info.address & ShdMem::PDE_ADDRESS;
        }

        *pde = entry;
    }

    void UnmapPDE(PDE* pde) {
        *pde = 0;
    }

    PDPTEInfo GetPDPTEInfo(PDPTE* pdpte) {
        PDPTEInfo info {
            .present = (*pdpte & ShdMem::PDPTE_PRESENT) != 0,
            .readWrite = (*pdpte & ShdMem::PDPTE_READWRITE) != 0,
            .userMode = (*pdpte & ShdMem::PDPTE_USERMODE) != 0,
            .PWT = (*pdpte & ShdMem::PDPTE_PWT) != 0,
            .PCD = (*pdpte & ShdMem::PDPTE_PCD) != 0,
            .accessed = (*pdpte & ShdMem::PDPTE_ACCESSED) != 0,
            .pageSize = (*pdpte & ShdMem::PDPTE_PAGE_SIZE) != 0,
            .executeDisable = (*pdpte & ShdMem::PDPTE_XD) != 0
        };

        if (info.pageSize) {
            info.dirty      = (*pdpte & ShdMem::PDPTE_DIRTY) != 0;
            info.global     = (*pdpte & ShdMem::PDPTE_GLOBAL) != 0;
            info.PAT        = (*pdpte & ShdMem::PDPTE_PAT) != 0;
            info.address    = (*pdpte & ShdMem::PDPTE_1GB_ADDRESS);
        }
        else {
            info.dirty      = false;
            info.global     = false;
            info.PAT        = false;
            info.address    = (*pdpte & ShdMem::PDPTE_ADDRESS);
        }

        return info;
    }

    void SetPDPTEInfo(PDPTE* pdpte, const PDPTEInfo& info) {
        uint64_t entry = 0;

        if (info.present)        entry |= ShdMem::PDPTE_PRESENT;
        if (info.readWrite)      entry |= ShdMem::PDPTE_READWRITE;
        if (info.userMode)       entry |= ShdMem::PDPTE_USERMODE;
        if (info.PWT)            entry |= ShdMem::PDPTE_PWT;
        if (info.PCD)            entry |= ShdMem::PDPTE_PCD;
        if (info.pageSize)       entry |= ShdMem::PDPTE_PAGE_SIZE;
        if (info.executeDisable) entry |= ShdMem::PDPTE_XD;

        if (info.pageSize) {
            if (info.global)    entry |= ShdMem::PDPTE_GLOBAL;
            if (info.PAT)       entry |= ShdMem::PDPTE_PAT;
            entry |= info.address & ShdMem::PDPTE_1GB_ADDRESS;
        }
        else {
            entry |= info.address & ShdMem::PDPTE_ADDRESS;
        }

        *pdpte = entry;
    }

    void UnmapPDPTE(PDPTE* pdpte) {
        *pdpte = 0;
    }

    PML4EInfo GetPML4EInfo(PML4E* pml4e) {
        return PML4EInfo {
            .present        = (*pml4e & ShdMem::PML4E_PRESENT) != 0,
            .readWrite      = (*pml4e & ShdMem::PML4E_READWRITE) != 0,
            .userMode       = (*pml4e & ShdMem::PML4E_USERMODE) != 0,
            .PWT            = (*pml4e & ShdMem::PML4E_PWT) != 0,
            .PCD            = (*pml4e & ShdMem::PML4E_PCD) != 0,
            .accessed       = (*pml4e & ShdMem::PML4E_ACCESSED) != 0,
            .executeDisable = (*pml4e & ShdMem::PML4E_XD) != 0,
            .address        = (*pml4e & ShdMem::PML4E_ADDRESS)
        };
    }

    void SetPML4EInfo(PML4E* pml4e, const PML4EInfo& info) {
        uint64_t entry = 0;

        if (info.present)        entry |= ShdMem::PML4E_PRESENT;
        if (info.readWrite)      entry |= ShdMem::PML4E_READWRITE;
        if (info.userMode)       entry |= ShdMem::PML4E_USERMODE;
        if (info.PWT)            entry |= ShdMem::PML4E_PWT;
        if (info.PCD)            entry |= ShdMem::PML4E_PCD;
        if (info.executeDisable) entry |= ShdMem::PML4E_XD;

        entry |= info.address & ShdMem::PML4E_ADDRESS;

        *pml4e = entry;
    }

    void UnmapPML4E(PML4E* pml4e) {
        *pml4e = 0;
    }

    Optional<void*> GetPhysicalAddress(const void* virtual_address, bool usePrimary) {
		auto mapping = ShdMem::ParseVirtualAddress(reinterpret_cast<uint64_t>(virtual_address));

        if (!IsMapped(virtual_address, usePrimary)) {
            return Optional<void*>();
        }
        else {
            PDEInfo pde_info = GetPDEInfo(GetPDEAddress(mapping, usePrimary));

            if (pde_info.pageSize) {
                const uint64_t frame_offset = reinterpret_cast<uint64_t>(virtual_address) & (ShdMem::PDE_COVERAGE - 1);
                return Optional(reinterpret_cast<void*>(pde_info.address | frame_offset));
            }
            else {
                PTEInfo pte_info = GetPTEInfo(GetPTEAddress(mapping, usePrimary));

                if (!pte_info.present) {
                    return Optional<void*>();
                }
                else {
                    const uint64_t frame_offset = reinterpret_cast<uint64_t>(virtual_address) & (ShdMem::PTE_COVERAGE - 1);

                    return Optional(reinterpret_cast<void*>(pte_info.address | frame_offset));
                }
            }
        }
	}

    void InvalidatePage(const void* virtual_address) {
        __asm__ volatile("invlpg (%0)" :: "r"(virtual_address) : "memory");
    }

    void InvalidateTLB() {
        __asm__ volatile(
            "mov %%cr3, %%rax\n"
            "mov %%rax, %%cr3\n"
            ::: "rax", "memory"
        );
    }

    bool IsMapped(const void* virtual_address, bool usePrimary) {
        const auto mapping = ShdMem::ParseVirtualAddress(reinterpret_cast<uint64_t>(virtual_address));

        const PML4EInfo pml4e_info = GetPML4EInfo(GetPML4EAddress(mapping, usePrimary));

        if (!pml4e_info.present) {
            return false;
        }

        const PDPTEInfo pdpte_info = GetPDPTEInfo(GetPDPTEAddress(mapping, usePrimary));

        if (!pdpte_info.present) {
            return false;
        }

        const PDEInfo pde_info = GetPDEInfo(GetPDEAddress(mapping, usePrimary));

        if (!pde_info.present) {
            return false;
        }
        else if (pde_info.pageSize) {
            return true;
        }

        const PTEInfo pte_info = GetPTEInfo(GetPTEAddress(mapping, usePrimary));

        return pte_info.present;
    }

    Success CreateSecondaryRecursiveMapping(void* CR3) {
        PML4E* const vroot = static_cast<PML4E*>(
            VirtualMemory::MapGeneralPages(CR3, 1, ShdMem::PTE_PRESENT | ShdMem::PTE_READWRITE)
        );

		if (vroot == nullptr) {
			return Failure();
		}

		ShdMem::ZeroPage(vroot);

		vroot[SECONDARY_PML4_INDEX] = vroot[PRIMARY_PML4_INDEX] = 
			ShdMem::PML4E_PRESENT
			| ShdMem::PML4E_READWRITE
            | (PhysicalMemory::FilterAddress(CR3) & ShdMem::PML4E_ADDRESS);

		if (!VirtualMemory::UnmapGeneralPages(vroot, 1).IsSuccess()) {
            return Failure();
        }

		UpdateSecondaryRecursiveMapping(CR3);

        return ShareKernelMemoryToSecondaryMapping();
    }

    void UpdateSecondaryRecursiveMapping(void* CR3) {
		SetPML4EInfo(GetPML4EAddress(secondaryMapping), {
			.present = 1,
			.readWrite = 1,
			.address = PhysicalMemory::FilterAddress(CR3)
		});

        InvalidateTLB();
    }

    Success FreeSecondaryRecursiveMapping() {
        const auto secondary_pml4e = GetPML4EAddress(secondaryMapping, false);
        const auto secondary_pml4e_info = GetPML4EInfo(secondary_pml4e);

        if (!secondary_pml4e_info.present) {
            return Success();
        }

        // Free all user space memory
        FreeFirstPML4E();

        static constexpr size_t first_user_pml4e = 1;
        static constexpr size_t last_user_pml4e = 256;

        for (size_t i = first_user_pml4e; i < last_user_pml4e; ++i) {
            const auto mapping = ShdMem::VirtualAddress{
                .PML4_offset = static_cast<uint16_t>(i),
                .PDPT_offset = 0,
                .PD_offset   = 0,
                .PT_offset   = 0,
                .offset      = 0
            };

            const auto pml4e = GetPML4EAddress(mapping, false);
            const auto pml4e_info = GetPML4EInfo(pml4e);

            if (pml4e_info.present) {
                FreePML4E(mapping, pml4e);
            }
        }

        // Free the task memory PML4E
        const auto task_mapping = ShdMem::ParseVirtualAddress(
            VirtualMemoryLayout::TaskMemory.start
        );

        const auto task_pml4e = GetPML4EAddress(task_mapping, false);
        const auto task_pml4e_info = GetPML4EInfo(task_pml4e);

        if (task_pml4e_info.present) {
            FreePML4E(task_mapping, task_pml4e);
        }

        // Free the secondary recursive mapping PML4E        
        PhysicalMemory::Free(reinterpret_cast<void*>(secondary_pml4e_info.address));

        // Unmap the recursive mapping
        UnmapPML4E(GetPML4EAddress(secondaryMapping, false));

        InvalidateTLB();

        return Success();
    }
}
