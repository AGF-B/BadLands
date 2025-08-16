#pragma once

#include <shared/memory/defs.hpp>

#include <mm/VirtualMemoryLayout.hpp>

namespace VirtualMemory {
	namespace Secondary {
		inline constexpr uint64_t PAGING_LOOP_MASK  = (VirtualMemoryLayout::SecondaryRecursiveMapping.start >> 39) & 0x1FF;
		inline constexpr uint64_t PAGING_LOOP_1		= VirtualMemoryLayout::SecondaryRecursiveMapping.start;
		inline constexpr uint64_t PAGING_LOOP_2 = PAGING_LOOP_1 | (PAGING_LOOP_MASK << 30);
		inline constexpr uint64_t PAGING_LOOP_3 = PAGING_LOOP_2 | (PAGING_LOOP_MASK << 21);
		inline constexpr uint64_t PAGING_LOOP_4 = PAGING_LOOP_3 | (PAGING_LOOP_MASK << 12);
	}

    enum class StatusCode {
		SUCCESS,
		OUT_OF_MEMORY,
		INVALID_PARAMETER
	};

    using PTE = Shared::Memory::PTE;
    using PDE = Shared::Memory::PDE;
    using PDPTE = Shared::Memory::PDPTE;
    using PML4E = Shared::Memory::PML4E;

    template<bool usePrimary = true>
	constexpr PTE* GetPTAddress(uint64_t pml4_offset, uint64_t pdpt_offset, uint64_t pd_offset) {
		if constexpr (usePrimary) {
			return reinterpret_cast<PTE*>(Shared::Memory::Layout::PAGING_LOOP_1
				| (pml4_offset << 30) | (pdpt_offset << 21) | (pd_offset << 12)
			);
		}
		else {
			return reinterpret_cast<PTE*>(Secondary::PAGING_LOOP_1
				| (pml4_offset << 30) | (pdpt_offset << 21) | (pd_offset << 12)
			);
		}
	};

	template<bool usePrimary = true>
	constexpr PDE* GetPDAddress(uint64_t pml4_offset, uint64_t pdpt_offset) {
		if constexpr (usePrimary) {
			return reinterpret_cast<PDE*>(Shared::Memory::Layout::PAGING_LOOP_2
				| (pml4_offset << 21) | (pdpt_offset << 12)
			);
		}
		else {
			return reinterpret_cast<PDE*>(Secondary::PAGING_LOOP_2
				| (pml4_offset << 21) | (pdpt_offset << 12)
			);
		}
	};

	template<bool usePrimary = true>
	constexpr PDPTE* GetPDPTAddress(uint64_t pml4_offset) {
		if constexpr (usePrimary) {
			return reinterpret_cast<PDPTE*>(Shared::Memory::Layout::PAGING_LOOP_3 | (pml4_offset << 12));
		}
		else {
			return reinterpret_cast<PDPTE*>(Secondary::PAGING_LOOP_3 | (pml4_offset << 12));
		}
	};
    
	template<bool usePrimary = true>
	constexpr PML4E* GetPML4Address() {
		if constexpr (usePrimary) {
			return reinterpret_cast<PML4E*>(Shared::Memory::Layout::PAGING_LOOP_4);
		}
		else {
			return reinterpret_cast<PML4E*>(Secondary::PAGING_LOOP_4);
		}
	};
	
	template<bool usePrimary = true>
	constexpr PTE* GetPTEAddress(uint64_t pml4_offset, uint64_t pdpt_offset, uint64_t pd_offset, uint64_t pt_offset) {
		return GetPTAddress<usePrimary>(pml4_offset, pdpt_offset, pd_offset) + pt_offset;
	};

	template<bool usePrimary = true>
	constexpr PDE* GetPDEAddress(uint64_t pml4_offset, uint64_t pdpt_offset, uint64_t pd_offset) {
		return GetPDAddress<usePrimary>(pml4_offset, pdpt_offset) + pd_offset;
	};

	template<bool usePrimary = true>
	constexpr PDPTE* GetPDPTEAddress(uint64_t pml4_offset, uint64_t pdpt_offset) {
		return GetPDPTAddress<usePrimary>(pml4_offset) + pdpt_offset;
	};

	template<bool usePrimary = true>
	constexpr PML4E* GetPML4EAddress(uint64_t pml4_offset) {
		return GetPML4Address<usePrimary>() + pml4_offset;
	};

	/// Paging constants

	// Custom values (when the page is present and valid)

	inline constexpr uint64_t PTE_LOCK			= 0x0000000000000200;

	// Masks used for non-present entries (if the page entry is 0, it is invalid)

	// reserved, must be 0
	inline constexpr uint64_t NP_PRESENT	= 0x0000000000000001;
	inline constexpr uint64_t NP_READWRITE	= 0x0000000000000002;
	inline constexpr uint64_t NP_USERMODE	= 0x0000000000000004;
	inline constexpr uint64_t NP_PWT		= 0x0000000000000008;
	inline constexpr uint64_t NP_PCD		= 0x0000000000000010;
	inline constexpr uint64_t NP_PAT		= 0x0000000000000020;
	inline constexpr uint64_t NP_GLOBAL		= 0x0000000000000040;
	inline constexpr uint64_t NP_PK			= 0x0000000000000780;
	// Cleared if the page entry is invalid, or in the swap file,
	// Set if the page is reserved for on-demand mapping
	inline constexpr uint64_t NP_ON_DEMAND	= 0x0000000000000800;
	// Index of the page in the swap file, this field is ignored if NP_ON_DEMAND is set
	inline constexpr uint64_t NP_INDEX		= 0xFFFFFFFFFFFFE000;

	StatusCode Setup();

	void* AllocateDMA(uint64_t pages);
	void* AllocateKernelHeap(uint64_t pages);
	void* AllocateUserPages(uint64_t pages);
	void* AllocateUserPagesAt(uint64_t pages, void* ptr);

	StatusCode FreeDMA(void* ptr, uint64_t pages);
	StatusCode FreeKernelHeap(void* ptr, uint64_t pages);
	StatusCode FreeUserPages(void* ptr, uint64_t pages);

	void* MapGeneralPage(void* pageAddress);
	StatusCode UnmapGeneralPage(void* vpage);
}
