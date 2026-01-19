#pragma once

#include <shared/Response.hpp>
#include <shared/memory/defs.hpp>

#include <mm/VirtualMemoryLayout.hpp>

namespace VirtualMemory {
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

	Success Setup();
	void* DeriveNewFreshCR3();

	void* AllocateDMA(uint64_t pages);
	void* AllocateKernelHeap(uint64_t pages);
	void* AllocateUserPages(uint64_t pages);
	void* AllocateUserPagesAt(uint64_t pages, void* ptr);

	Success FreeDMA(void* ptr, uint64_t pages);
	Success FreeKernelHeap(void* ptr, uint64_t pages);
	Success FreeUserPages(void* ptr, uint64_t pages);

	Success ChangeMappingFlags(void* _ptr, uint64_t flags, uint64_t pages = 1);

	void* MapGeneralPages(void* pageAddress, size_t pages, uint64_t flags = 0);
	Success UnmapGeneralPages(void* vpage, size_t pages);
}
