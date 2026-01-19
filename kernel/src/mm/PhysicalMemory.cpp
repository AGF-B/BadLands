#include <cpuid.h>
#include <cstdint>

#include <bit>

#include <shared/efi/efi.h>
#include <shared/memory/defs.hpp>
#include <shared/memory/layout.hpp>

#include <mm/Paging.hpp>
#include <mm/PhysicalMemory.hpp>
#include <mm/VirtualMemory.hpp>
#include <mm/VirtualMemoryLayout.hpp>

namespace ShdMem = Shared::Memory;
namespace VML = ShdMem::Layout;

namespace {
	static uint8_t MAXPHYADDR = 0;

	static uint64_t _setup_filter_address(uint64_t _addr);
	static uint64_t(*_filter_address_handler)(uint64_t _addr) = &_setup_filter_address;	

	static uint64_t _filter_address(uint64_t _addr) {
		return _addr & (((uint64_t)1 << MAXPHYADDR) - 1) & ~0xFFF;
	}
	static uint64_t _setup_filter_address(uint64_t _addr) {
		unsigned int eax = 0, unused = 0;
		__get_cpuid(0x80000008, &eax, &unused, &unused, &unused);
		MAXPHYADDR = eax & 0xFF;
		_filter_address_handler = &_filter_address;
		return PhysicalMemory::FilterAddress(_addr);
	}

	static uint8_t* DMA_bitmap = nullptr;

	static constexpr uint64_t DMA_PAGES			= (VML::DMAZone.limit / ShdMem::FRAME_SIZE);
	static constexpr uint64_t DMA_BITMAP_SIZE	= (DMA_PAGES / 8);

	static constexpr uint64_t MAX_ADDRESSABLE_MEMORY 	= 0x4000000000; // 256GB

	static constexpr uint64_t BITMAP_ENTRIES_4KB 		= MAX_ADDRESSABLE_MEMORY / ShdMem::PTE_COVERAGE;
	static constexpr uint64_t BITMAP_ENTRIES_2MB 		= BITMAP_ENTRIES_4KB / ShdMem::PT_ENTRIES;
	static constexpr uint64_t BITMAP_ENTRIES_32MB 		= BITMAP_ENTRIES_2MB / 16;

	static constexpr uint64_t BITMAP_WORDS_4KB 			= BITMAP_ENTRIES_4KB / 64;
	static constexpr uint64_t BITMAP_WORDS_2MB 			= BITMAP_ENTRIES_2MB / 64;
	static constexpr uint64_t BITMAP_WORDS_32MB 		= BITMAP_ENTRIES_32MB / 64;

	static constexpr uint64_t BITMAP_SIZE_4KB 			= BITMAP_WORDS_4KB * sizeof(uint64_t);
	static constexpr uint64_t BITMAP_SIZE_2MB 			= BITMAP_WORDS_2MB * sizeof(uint64_t);	
	static constexpr uint64_t BITMAP_SIZE_32MB 			= BITMAP_WORDS_32MB * sizeof(uint64_t);

	struct LargeMemoryRegionStatus {
		uint64_t AnyUsed;
		uint64_t AnyFree;
	};

	// Memory Region Tracking Structures:
	// 
	// BitMap32MB: Tracks the state of 32MB regions
	//   - AnyFree: Indicates if any 2MB page within this 32MB region is free
	//   - AnyUsed: Indicates if any 2MB page within this 32MB region is in use
	// 
	// BitMap2MB: Tracks the state of 2MB regions  
	//   - AnyFree: Indicates if any 4KB page within this 2MB region is free
	//   - AnyUsed: Indicates if any 4KB page within this 2MB region is in use
	// 
	// BitMap4KB: Direct bitmap for 4KB pages
	//   - Set bit indicates the 4KB page is currently in use
	//   - Clear bit indicates the 4KB page is free
	// 
	// Cache Structures:
	// - Cached32MB: Stack of free 32MB regions for fast allocation
	// - Cached2MB: Stack of free 2MB regions for fast allocation
	// - Cached4KB: Single cached 4KB region for fast allocation

	static LargeMemoryRegionStatus* const BitMap32MB
		= reinterpret_cast<LargeMemoryRegionStatus*>(VirtualMemoryLayout::PhysicalMemoryMap.start);
	static LargeMemoryRegionStatus* const BitMap2MB
		= reinterpret_cast<LargeMemoryRegionStatus*>(VirtualMemoryLayout::PhysicalMemoryMap.start + 2 * BITMAP_SIZE_32MB);
	static uint64_t* 				const BitMap4KB 
		= reinterpret_cast<uint64_t*>(VirtualMemoryLayout::PhysicalMemoryMap.start + 2 * BITMAP_SIZE_32MB + 2 * BITMAP_SIZE_2MB);

	static constexpr uint64_t TOTAL_METADATA_SIZE 	= 2 * BITMAP_SIZE_32MB + 2 * BITMAP_SIZE_2MB + BITMAP_SIZE_4KB;
	static constexpr uint64_t TOTAL_METADATA_PAGES 	= (TOTAL_METADATA_SIZE + ShdMem::FRAME_SIZE - 1) / ShdMem::FRAME_SIZE;

	static constexpr uint64_t UNIT = 1;

	class LargeMemoryCache {
	private:
		static constexpr size_t CACHE_SIZE = 64;

		uint64_t cache[CACHE_SIZE] = { 0 };
		size_t head = 0;
		size_t tail = 0;
		size_t count = 0;

	public:
		constexpr LargeMemoryCache() = default;

		constexpr bool IsEmpty() const noexcept {
			return count == 0;
		}

		constexpr bool IsFull() const noexcept {
			return count == CACHE_SIZE;
		}

		constexpr void Push(uint32_t entry) noexcept {
			if (!IsFull()) {
				cache[tail] = entry;
				tail = (tail + 1) % CACHE_SIZE;
				++count;
			}
		}

		constexpr Optional<uint32_t> Pop() noexcept {
			if (!IsEmpty()) {
				uint32_t entry = cache[head];
				head = (head + 1) % CACHE_SIZE;
				--count;
				return Optional(entry);
			}

			return Optional<uint32_t>();
		}
	};

	static LargeMemoryCache 		Cache32MB{};
	static LargeMemoryCache 		Cache2MB{};
	static Optional<uint64_t> 		Cached4KB{};

	static constexpr bool IsAddressable(uint64_t address) {
		return address < MAX_ADDRESSABLE_MEMORY;
	}

	static constexpr bool IsDMAAddress(uint64_t address) {
		return address >= VML::DMAZone.start && address < VML::DMAZone.end();
	}

	static constexpr uint64_t GetDMAAddressPage(uint64_t address) {
		return address / ShdMem::FRAME_SIZE;
	}

	static constexpr uint64_t Get2MBParent32MB(uint64_t region_2mb) {
		return region_2mb / 16;
	}

	static constexpr uint64_t Get4KBParent2MB(uint64_t region_4kb) {
		return region_4kb / ShdMem::PT_ENTRIES;
	}

	static constexpr uint64_t Get4KBParent32MB(uint64_t region_4kb) {
		return Get2MBParent32MB(Get4KBParent2MB(region_4kb));
	}

	static constexpr Optional<uint64_t> Try4KBCache() {
		if (Cached4KB.HasValue()) {
			const auto region = Cached4KB.GetValueAndClear();

			// Check parent 2MB page still has free pages, i.e., has not been allocated since caching
			const size_t parent_2mb = Get4KBParent2MB(region);
			const size_t word_2mb 	= parent_2mb / 64;
			const size_t bit_2mb 	= parent_2mb % 64;

			if ((BitMap2MB[word_2mb].AnyFree & (UNIT << bit_2mb)) != 0) {
				return Optional(region);
			}
		}

		return Optional<uint64_t>();
	}

	static void Mark32MBRegionUsed(uint64_t region) {
		const size_t word_idx = region / 64;
		const size_t bit = region % 64;

		BitMap32MB[word_idx].AnyUsed |= (UNIT << bit);

		const size_t first_2mb = region * 16;
		bool all_used = true;

		for (size_t i = 0; i < 16; i++) {
			const size_t check_region 	= first_2mb + i;
			const size_t check_word 	= check_region / 64;
			const size_t check_bit 		= check_region % 64;

			if ((BitMap2MB[check_word].AnyUsed & (UNIT << check_bit)) == 0) {
				all_used = false;
				break;
			}
		}

		if (all_used) {
			BitMap32MB[word_idx].AnyFree &= ~(UNIT << bit);
		}
	}

	static void Mark2MBRegionUsed(uint64_t region) {
		const size_t word_idx = region / 64;
		const size_t bit = region % 64;

		BitMap2MB[word_idx].AnyUsed |= (UNIT << bit);

		const size_t parent_32mb = Get2MBParent32MB(region);
		Mark32MBRegionUsed(parent_32mb);

		// check if there is still a free 4KB page in this 2MB region
		const size_t first_4kb = region * ShdMem::PT_ENTRIES;
		bool all_used = true;

		for (size_t i = 0; i < ShdMem::PT_ENTRIES; i++) {
			const size_t check_region 	= first_4kb + i;
			const size_t check_word 	= check_region / 64;
			const size_t check_bit 		= check_region % 64;

			if ((BitMap4KB[check_word] & (UNIT << check_bit)) == 0) {
				all_used = false;
				break;
			}
		}

		if (all_used) {
			BitMap2MB[word_idx].AnyFree &= ~(UNIT << bit);
		}
	}

	static bool Update32MBFlags(uint64_t region_32mb) {
		const size_t word_idx = region_32mb / 64;
		const size_t bit = region_32mb % 64;

		bool any_used = false;
		bool any_free = false;

		const size_t first_2mb = region_32mb * 16;

		for (size_t i = 0; i < 16; ++i) {
			const size_t child 	= first_2mb + i;
			const size_t w 		= child / 64;
			const size_t b 		= child % 64;

			if ((BitMap2MB[w].AnyUsed & (UNIT << b)) != 0) {
				any_used = true;
			}
			if ((BitMap2MB[w].AnyFree & (UNIT << b)) != 0) {
				any_free = true;
			}
		}

		auto& any_used_word = BitMap32MB[word_idx].AnyUsed;
		auto& any_free_word = BitMap32MB[word_idx].AnyFree;

		if (any_used) {
			any_used_word |= (UNIT << bit);
		} else {
			any_used_word &= ~(UNIT << bit);
		}

		if (any_free) {
			any_free_word |= (UNIT << bit);
		} else {
			any_free_word &= ~(UNIT << bit);
		}

		return !any_used;
	}

	static void Mark2MBRegionFreed(uint64_t region) {
		const size_t word_idx 	= region / 64;
		const size_t bit 		= region % 64;

		// Clear 4KB usage inside this 2MB region
		const size_t start_word_4kb = (region * ShdMem::PT_ENTRIES) / 64;
		const size_t end_word_4kb 	= ((region + 1) * ShdMem::PT_ENTRIES) / 64;

		for (size_t w = start_word_4kb; w < end_word_4kb; ++w) {
			BitMap4KB[w] = 0;
		}

		BitMap2MB[word_idx].AnyUsed &= ~(UNIT << bit);
		BitMap2MB[word_idx].AnyFree |= (UNIT << bit);

		const uint64_t parent_32mb = Get2MBParent32MB(region);		
		const bool parent_free = Update32MBFlags(parent_32mb);

		if (parent_free) {
			Cache32MB.Push(parent_32mb);
		}
		
		Cache2MB.Push(region);
	}

	static bool Region2MBHasUsed(uint64_t region) {
		const size_t start_word_4kb = (region * ShdMem::PT_ENTRIES) / 64;
		const size_t end_word_4kb = ((region + 1) * ShdMem::PT_ENTRIES) / 64;

		for (size_t w = start_word_4kb; w < end_word_4kb; ++w) {
			if (BitMap4KB[w] != 0) {
				return true;
			}
		}

		return false;
	}

	static void* AllocateCached32MBPage() {
		const auto region_wrapper = Cache32MB.Pop();

		if (region_wrapper.HasValue()) {
			const uint64_t region = region_wrapper.GetValue();
			const size_t word_idx = region / 64;
			const size_t bit = region % 64;

			if ((BitMap32MB[word_idx].AnyUsed & (UNIT << bit)) == 0) {
				Mark32MBRegionUsed(region);
				return reinterpret_cast<void*>(region * 16 * ShdMem::PDE_COVERAGE);
			}
		}

		return nullptr;
	}

	static void* AllocateCached2MBPage() {
		const auto region_wrapper = Cache2MB.Pop();

		if (region_wrapper.HasValue()) {
			const uint32_t 	region 		= region_wrapper.GetValue();        
			const size_t 	word_idx 	= region / 64;
			const size_t 	bit 		= region % 64;

			const size_t	parent_32mb = Get2MBParent32MB(region);

			auto& any_used = BitMap2MB[word_idx].AnyUsed;
			auto& any_free = BitMap2MB[word_idx].AnyFree;

			if ((any_used & (UNIT << bit)) == 0) {        
				any_used |= (UNIT << bit);
				any_free &= ~(UNIT << bit);
				
				Mark32MBRegionUsed(parent_32mb);
				
				return (void*)(region * ShdMem::PDE_COVERAGE);
			}
		}

		return nullptr;
	}
}

uint64_t PhysicalMemory::FilterAddress(uint64_t address) {
	return _filter_address_handler(address);
}

uint64_t PhysicalMemory::FilterAddress(void* address) {
	return _filter_address_handler(reinterpret_cast<uint64_t>(address));
}

Success PhysicalMemory::Setup() {
	const uint64_t mmapSize = *reinterpret_cast<uint64_t*>(VML::OsLoaderData.start + VML::OsLoaderDataOffsets.MmapSize);
	const uint64_t descriptorSize = *reinterpret_cast<uint64_t*>(VML::OsLoaderData.start + VML::OsLoaderDataOffsets.MmapDescSize);
	uint8_t* const mmap = reinterpret_cast<uint8_t*>(VML::OsLoaderData.start + VML::OsLoaderDataOffsets.Mmap);
	const size_t descriptorCount = mmapSize / descriptorSize;

	DMA_bitmap = reinterpret_cast<uint8_t*>(VML::OsLoaderData.start + VML::OsLoaderDataOffsets.DMABitMap);
	DMA_bitmap[0] |= 1; // reserve the first DMA page to make NULL pointers invalid

	uint64_t bitmap_virt_addr = VirtualMemoryLayout::PhysicalMemoryMap.start;

	uint64_t bytes_mapped = 0;
	const uint64_t bytes_needed = TOTAL_METADATA_SIZE;

	bool bitmaps_mapped = false;
	bool bitmaps_initialized = false;

	for (size_t i = 0; i < descriptorCount; ++i) {
		EFI_MEMORY_DESCRIPTOR* descriptor = reinterpret_cast<EFI_MEMORY_DESCRIPTOR*>(mmap + i * descriptorSize);

		if (descriptor->Type == EfiConventionalMemory || descriptor->Type == EfiLoaderCode || descriptor->Type == EfiLoaderData
			|| descriptor->Type == EfiBootServicesCode || descriptor->Type == EfiBootServicesData
		) {
			uint64_t phys_addr = descriptor->PhysicalStart;
			uint64_t num_pages = descriptor->NumberOfPages;

			// Skip DMA zone
			if (phys_addr < VML::DMAZone.limit) {
				int64_t endDMAOffset = phys_addr + ShdMem::FRAME_SIZE * num_pages - VML::DMAZone.limit;

				if (endDMAOffset <= 0) {
					continue;
				}

				num_pages = endDMAOffset / ShdMem::FRAME_SIZE;
				phys_addr = VML::DMAZone.limit;
			}

			if (num_pages == 0) {
				continue;
			}

			// Allocate memory for the bitmaps and map it
			while (num_pages > 0 && bytes_mapped < bytes_needed) {
				const uint64_t bytes_remaining = bytes_needed - bytes_mapped;
				const auto mapping = ShdMem::ParseVirtualAddress(bitmap_virt_addr);
				
				// Take page table pages from the end of the descriptor
				uint64_t pt_phys_base = phys_addr + (num_pages - 1) * ShdMem::FRAME_SIZE;

				// Try to use 2MB page if aligned and enough space
				if ((phys_addr % ShdMem::PDE_COVERAGE) == 0 &&
					(bitmap_virt_addr % ShdMem::PDE_COVERAGE) == 0 &&
					bytes_remaining >= ShdMem::PDE_COVERAGE &&
					num_pages >= (ShdMem::PDE_COVERAGE / ShdMem::FRAME_SIZE)) {
					
					const auto pml4e = Paging::GetPML4EAddress(mapping);

					if (!Paging::GetPML4EInfo(pml4e).present) {
						Paging::SetPML4EInfo(pml4e, {
							.present = 1,
							.readWrite = 1,
							.address = FilterAddress(pt_phys_base)
						});

						const auto pdpt = Paging::GetPDPTAddress(mapping);

						Paging::InvalidatePage(pdpt);
						ShdMem::ZeroPage(pdpt);

						pt_phys_base -= ShdMem::FRAME_SIZE;

						if (--num_pages <= (ShdMem::PDE_COVERAGE / ShdMem::FRAME_SIZE)) break;
					}

					const auto pdpte = Paging::GetPDPTEAddress(mapping);

					if (!Paging::GetPDPTEInfo(pdpte).present) {
						Paging::SetPDPTEInfo(pdpte, {
							.present = 1,
							.readWrite = 1,
							.address = FilterAddress(pt_phys_base)
						});

						const auto pd = Paging::GetPDAddress(mapping);
						Paging::InvalidatePage(pd);
						ShdMem::ZeroPage(pd);

						pt_phys_base -= ShdMem::FRAME_SIZE;

						if (--num_pages <= (ShdMem::PDE_COVERAGE / ShdMem::FRAME_SIZE)) break;
					}

					// Map 2MB page
					const auto pde = Paging::GetPDEAddress(mapping);

					Paging::SetPDEInfo(pde, {
						.present = 1,
						.readWrite = 1,
						.pageSize = 1,
						.executeDisable = 1,
						.address = FilterAddress(phys_addr)
					});
					
					Paging::InvalidatePage(reinterpret_cast<void*>(bitmap_virt_addr));

					phys_addr += ShdMem::PDE_COVERAGE;
					bitmap_virt_addr += ShdMem::PDE_COVERAGE;
					bytes_mapped += ShdMem::PDE_COVERAGE;
					num_pages -= (ShdMem::PDE_COVERAGE / ShdMem::FRAME_SIZE);
				}
				else {
					const auto pml4e = Paging::GetPML4EAddress(mapping);

					if (!Paging::GetPML4EInfo(pml4e).present) {
						Paging::SetPML4EInfo(pml4e, {
							.present = 1,
							.readWrite = 1,
							.address = FilterAddress(pt_phys_base)
						});

						const auto pdpt = Paging::GetPDPTAddress(mapping);

						Paging::InvalidatePage(pdpt);
						ShdMem::ZeroPage(pdpt);

						pt_phys_base -= ShdMem::FRAME_SIZE;
						if (--num_pages == 0) break;
					}

					const auto pdpte = Paging::GetPDPTEAddress(mapping);

					if (!Paging::GetPDPTEInfo(pdpte).present) {
						Paging::SetPDPTEInfo(pdpte, {
							.present = 1,
							.readWrite = 1,
							.address = FilterAddress(pt_phys_base)
						});

						const auto pd = Paging::GetPDAddress(mapping);

						Paging::InvalidatePage(pd);
						ShdMem::ZeroPage(pd);

						pt_phys_base -= ShdMem::FRAME_SIZE;

						if (--num_pages == 0) break;
					}

					const auto pde = Paging::GetPDEAddress(mapping);

					if (!Paging::GetPDEInfo(pde).present) {
						Paging::SetPDEInfo(pde, {
							.present = 1,
							.readWrite = 1,
							.address = FilterAddress(pt_phys_base)
						});

						const auto pt = Paging::GetPTAddress(mapping);

						Paging::InvalidatePage(pt);
						ShdMem::ZeroPage(pt);

						pt_phys_base -= ShdMem::FRAME_SIZE;
						if (--num_pages == 0) break;
					}

					const auto pte = Paging::GetPTEAddress(mapping);

					Paging::SetPTEInfo(pte, {
						.present = 1,
						.readWrite = 1,
						.executeDisable = 1,
						.address = FilterAddress(phys_addr)
					});

					Paging::InvalidatePage(reinterpret_cast<void*>(bitmap_virt_addr));
					ShdMem::ZeroPage(reinterpret_cast<void*>(bitmap_virt_addr));

					phys_addr += ShdMem::FRAME_SIZE;
					bitmap_virt_addr += ShdMem::FRAME_SIZE;
					bytes_mapped += ShdMem::FRAME_SIZE;
					--num_pages;
				}

				if (bytes_mapped >= bytes_needed) {
					bitmaps_mapped = true;
					break;
				}
			}

			if (bitmaps_mapped) {
				if (!bitmaps_initialized) {
					// Initialize all bitmap bits to 1 (all pages marked as used initially)
					for (size_t i = 0; i < BITMAP_WORDS_32MB; ++i) {
						BitMap32MB[i].AnyUsed = ~0ULL;
						BitMap32MB[i].AnyFree = 0;
					}

					for (size_t i = 0; i < BITMAP_WORDS_2MB; ++i) {
						BitMap2MB[i].AnyUsed = ~0ULL;
						BitMap2MB[i].AnyFree = 0;
					}

					for (size_t i = 0; i < BITMAP_WORDS_4KB; ++i) {
						BitMap4KB[i] = ~0ULL;
					}

					bitmaps_initialized = true;
				}
				
				// Mark remaining pages in this descriptor as free
				for (uint64_t page = 0; page < num_pages; ++page) {
					const uint64_t addr = phys_addr + page * ShdMem::FRAME_SIZE;
					
					if (IsAddressable(addr)) {
						const uint64_t region_4kb = addr / ShdMem::FRAME_SIZE;
						const size_t word_4kb = region_4kb / 64;
						const size_t bit_4kb = region_4kb % 64;

						// Clear the 4KB bit (mark as free)
						BitMap4KB[word_4kb] &= ~(UNIT << bit_4kb);
					}
				}
			}
		}
	}

	if (!bitmaps_mapped) {
		return Failure(); // Not enough memory to map bitmaps
	}

	// Update 2MB and 32MB hierarchical flags based on 4KB bitmap
	for (size_t region_2mb = 0; region_2mb < BITMAP_ENTRIES_2MB; ++region_2mb) {
		const size_t start_4kb = (region_2mb * ShdMem::PT_ENTRIES) / 64;
		const size_t end_4kb = ((region_2mb + 1) * ShdMem::PT_ENTRIES) / 64;

		bool has_free = false;
		bool has_used = false;

		for (size_t w = start_4kb; w < end_4kb; ++w) {
			if (BitMap4KB[w] != 0) {
				has_used = true;
			}
			if (BitMap4KB[w] != ~0ULL) {
				has_free = true;
			}
		}

		const size_t word_2mb = region_2mb / 64;
		const size_t bit_2mb = region_2mb % 64;

		if (has_used) {
			BitMap2MB[word_2mb].AnyUsed |= (UNIT << bit_2mb);
		} else {
			BitMap2MB[word_2mb].AnyUsed &= ~(UNIT << bit_2mb);
		}

		if (has_free) {
			BitMap2MB[word_2mb].AnyFree |= (UNIT << bit_2mb);
		} else {
			BitMap2MB[word_2mb].AnyFree &= ~(UNIT << bit_2mb);
		}
	}

	// Update 32MB flags based on 2MB bitmap
	for (size_t region_32mb = 0; region_32mb < BITMAP_ENTRIES_32MB; ++region_32mb) {
		const size_t first_2mb = region_32mb * 16;
		bool has_free = false;
		bool has_used = false;

		for (size_t i = 0; i < 16; ++i) {
			const size_t child_2mb = first_2mb + i;
			const size_t word_2mb = child_2mb / 64;
			const size_t bit_2mb = child_2mb % 64;

			if ((BitMap2MB[word_2mb].AnyUsed & (UNIT << bit_2mb)) != 0) {
				has_used = true;
			}
			if ((BitMap2MB[word_2mb].AnyFree & (UNIT << bit_2mb)) != 0) {
				has_free = true;
			}
		}

		const size_t word_32mb = region_32mb / 64;
		const size_t bit_32mb = region_32mb % 64;

		if (has_used) {
			BitMap32MB[word_32mb].AnyUsed |= (UNIT << bit_32mb);
		} else {
			BitMap32MB[word_32mb].AnyUsed &= ~(UNIT << bit_32mb);
		}

		if (has_free) {
			BitMap32MB[word_32mb].AnyFree |= (UNIT << bit_32mb);
		} else {
			BitMap32MB[word_32mb].AnyFree &= ~(UNIT << bit_32mb);
		}
	}

	return Success();
}

PhysicalMemory::StatusCode PhysicalMemory::QueryDMAAddress(uint64_t address) {
	if (address >= VML::DMAZone.limit) {
		return StatusCode::INVALID_PARAMETER;
	}

    const uint64_t page = address / ShdMem::FRAME_SIZE;
	return ((DMA_bitmap[page / 8] & (1 << (page % 8))) == 0) ? StatusCode::FREE : StatusCode::ALLOCATED;
}

void* PhysicalMemory::AllocateDMA(uint64_t pages) {
	if (pages == 0) {
		return nullptr;
	}

	uint64_t startPage = 0;
	uint64_t pagesFound = 0;

	for (size_t i = 0; i < DMA_BITMAP_SIZE; ++i) {
		uint8_t byte = DMA_bitmap[i];

		for (size_t j = 0; j < 8; ++j) {
			if ((byte & (1 << j)) != 0) {
				pagesFound = 0;
			}
			else {
				if (pagesFound++ == 0) {
					startPage = static_cast<uint64_t>(8) * i + j;
				}
			}

			if (pagesFound >= pages) {
				for (size_t p = startPage; p < startPage + pages; ++p) {
					size_t x = p / 8;
					size_t y = p % 8;

					DMA_bitmap[x] |= 1 << y;
				}

				return reinterpret_cast<void*>(startPage * ShdMem::FRAME_SIZE);
			}
		}
	}

	return nullptr;
}

void* PhysicalMemory::Allocate() {
    const auto cached = Try4KBCache();

	if (cached.HasValue()) {
		const auto region = cached.GetValue();

		const size_t word_idx = region / 64;
		const size_t bit = region % 64;

		BitMap4KB[word_idx] |= (UNIT << bit);

		const size_t parent_2mb = Get4KBParent2MB(region);
		Mark2MBRegionUsed(parent_2mb);
		
		return reinterpret_cast<void*>(region * ShdMem::FRAME_SIZE);
	}
	
	for (size_t i = 0; i < BITMAP_WORDS_32MB; i++) {
		const auto word_32mb = BitMap32MB[i];
		
		// Look for regions with free pages
		const uint64_t has_free = word_32mb.AnyFree;
		
		if (has_free != 0) {
			// Prefer partial regions (already have some pages used)
			const uint64_t partial = word_32mb.AnyUsed & word_32mb.AnyFree;
			const uint64_t target = (partial != 0) ? partial : has_free;
			
			const int bit = std::countr_zero(target);
			const size_t region_32mb = i * 64 + bit;
			const size_t first_2mb = region_32mb * 16;
			
			// Search 2MB children
			for (int j = 0; j < 16; j++) {
				const size_t region_2mb = first_2mb + j;
				const size_t word_idx_2mb = region_2mb / 64;
				const int bit_2mb = region_2mb % 64;
				
				const auto word_2mb = BitMap2MB[word_idx_2mb];
				
				// Check 2MB region has available pages
				if ((word_2mb.AnyFree & (UNIT << bit_2mb)) != 0) {
					Mark2MBRegionUsed(region_2mb);
					
					// Search 4KB pages within this 2MB region
					const size_t start_4kb = (region_2mb * ShdMem::PT_ENTRIES) / 64;
					const size_t end_4kb = ((region_2mb + 1) * ShdMem::PT_ENTRIES) / 64;
					
					for (size_t kb4_idx = start_4kb; kb4_idx < end_4kb; kb4_idx++) {
						const uint64_t word = BitMap4KB[kb4_idx];
						
						if (word != ~0ULL) {
							const int page_bit = std::countr_zero(~word);
							const size_t page_region = kb4_idx * 64 + page_bit;
							
							BitMap4KB[kb4_idx] |= (UNIT << page_bit);
							
							// Cache the next potential free page in this word
							if (BitMap4KB[kb4_idx] != ~0ULL) {
								const int next_bit = std::countr_zero(~BitMap4KB[kb4_idx]);
								Cached4KB = Optional<uint64_t>(kb4_idx * 64 + next_bit);
							}
							
							return reinterpret_cast<void*>(page_region * ShdMem::FRAME_SIZE);
						}
					}
				}
			}
		}
	}
    
	return nullptr;
}

void* PhysicalMemory::Allocate2MB() {
	const auto cached_page = AllocateCached2MBPage();

	if (cached_page != nullptr) {
		return cached_page;
	}
    
    for (size_t i = 0; i < BITMAP_WORDS_32MB; i++) {
        const auto word_mb32 = BitMap32MB[i];
        
        uint64_t partial_mask = word_mb32.AnyUsed & word_mb32.AnyFree;
        
        if (partial_mask != 0) {
            while (partial_mask != 0 && !Cache2MB.IsFull()) {
                const int bit = std::countr_zero(partial_mask);
                partial_mask &= ~(UNIT << bit);
                
                const size_t region_32mb 	= i * 64 + bit;
                const size_t first_2mb 		= region_32mb * 16;
                
                for (int j = 0; j < 16; j++) {
                    const size_t region_2mb 	= first_2mb + j;
                    const size_t word_idx_2mb 	= region_2mb / 64;
                    const size_t bit_2mb 		= region_2mb % 64;
                    
                    if ((BitMap2MB[word_idx_2mb].AnyUsed & (UNIT << bit_2mb)) == 0) {
						Cache2MB.Push(region_2mb);
						           
                        if (Cache2MB.IsFull()) {
							break;
						}
                    }
                }
            }
            
            if (!Cache2MB.IsEmpty()) {
                return AllocateCached2MBPage();
            }
        }
    }
    
    for (size_t i = 0; i < BITMAP_WORDS_32MB; i++) {
        const auto 		word 			= BitMap32MB[i];
        const uint64_t 	free_regions 	= ~word.AnyUsed;
        
        if (free_regions != 0) {
            const size_t bit 			= std::countr_zero(free_regions);
            const size_t region_32mb 	= i * 64 + bit;
            const size_t first_2mb 		= region_32mb * 16;

			Cache2MB.Push(first_2mb);
            
            return AllocateCached2MBPage();
        }
    }
    
    return nullptr;
}

void* PhysicalMemory::Allocate32MB() {
	const auto cached_page = AllocateCached32MBPage();

	if (cached_page != nullptr) {
		return cached_page;
	}
	
	for (size_t i = 0; i < BITMAP_WORDS_32MB && !Cache32MB.IsFull(); i++) {
		const auto word = BitMap32MB[i];
		uint64_t free_regions = ~word.AnyUsed;

		while (free_regions != 0 && !Cache32MB.IsFull()) {
			const int bit = std::countr_zero(free_regions);
			free_regions &= ~(UNIT << bit);

			const uint64_t region = i * 64 + bit;

			if (region >= BITMAP_ENTRIES_32MB) {
				break;
			}

			Cache32MB.Push(region);
		}
	}

	return AllocateCached32MBPage();
}

Success PhysicalMemory::FreeDMA(void* ptr, uint64_t pages) {
	const uint64_t address = reinterpret_cast<uint64_t>(ptr);
	const uint64_t end = address + ShdMem::FRAME_SIZE * (pages - 1);

	if (!IsDMAAddress(address) || !IsDMAAddress(end)) {
		Failure();
	}

	const uint64_t firstPage = address / ShdMem::FRAME_SIZE;

	for (size_t p = firstPage; p < firstPage + pages; ++p) {
		size_t x = p / 8;
		size_t y = p % 8;

		DMA_bitmap[x] &= ~(1 << y);
	}

	return Success();
}

Success PhysicalMemory::Free(void* ptr) {
	if (ptr == nullptr) {
		return Failure();
	}

	uint64_t address = reinterpret_cast<uint64_t>(ptr);

	if (!IsAddressable(address) || IsDMAAddress(address)) {
		return Failure();
	}
	else if ((address % ShdMem::FRAME_SIZE) != 0) {
		address = address - (address % ShdMem::FRAME_SIZE);
	}

	const uint64_t region 	= address / ShdMem::FRAME_SIZE;
	const size_t word_idx 	= region / 64;
	const size_t bit 		= region % 64;

	if ((BitMap4KB[word_idx] & (UNIT << bit)) == 0) {
		return Success();
	}

	BitMap4KB[word_idx] &= ~(UNIT << bit);

	const uint64_t parent_2mb 	= Get4KBParent2MB(region);
	const size_t word_2mb 		= parent_2mb / 64;
	const size_t bit_2mb 		= parent_2mb % 64;

	BitMap2MB[word_2mb].AnyFree |= (UNIT << bit_2mb);

	if (!Region2MBHasUsed(parent_2mb)) {
		BitMap2MB[word_2mb].AnyUsed &= ~(UNIT << bit_2mb);
		Cache2MB.Push(parent_2mb);
	}

	const uint64_t parent_32mb = Get2MBParent32MB(parent_2mb);
	const bool fully_free = Update32MBFlags(parent_32mb);

	if (fully_free) {
		Cache32MB.Push(parent_32mb);
	}

	Cached4KB = Optional<uint64_t>(region);

	return Success();
}

Success PhysicalMemory::Free2MB(void* ptr) {
	if (ptr == nullptr) {
		return Failure();
	}

	uint64_t address = reinterpret_cast<uint64_t>(ptr);

	if (!IsAddressable(address) || IsDMAAddress(address)) {
		return Failure();
	}
	else if ((address % ShdMem::PDE_COVERAGE) != 0) {
		address = address - (address % ShdMem::PDE_COVERAGE);
	}

	const uint64_t region 	= address / ShdMem::PDE_COVERAGE;
	const size_t word_idx 	= region / 64;
	const size_t bit 		= region % 64;

	if ((BitMap2MB[word_idx].AnyUsed & (UNIT << bit)) == 0) {
		return Failure();
	}

	Mark2MBRegionFreed(region);

	return Success();
}

Success PhysicalMemory::Free32MB(void* ptr) {
	if (ptr == nullptr) {
		Failure();
	}

	uint64_t address = reinterpret_cast<uint64_t>(ptr);

	if (!IsAddressable(address) || IsDMAAddress(address)) {
		return Failure();
	}
	else if ((address % (16 * ShdMem::PDE_COVERAGE)) != 0) {
		address = address - (address % (16 * ShdMem::PDE_COVERAGE));
	}

	const uint64_t region 	= address / (16 * ShdMem::PDE_COVERAGE);
	const size_t word_idx 	= region / 64;
	const size_t bit 		= region % 64;

	if ((BitMap32MB[word_idx].AnyUsed & (UNIT << bit)) == 0) {
		return Failure();
	}

	// Free all 2MB children
	const size_t first_2mb = region * 16;

	for (size_t i = 0; i < 16; ++i) {
		const uint64_t child_2mb = first_2mb + i;
		const size_t word_2mb = child_2mb / 64;
		const size_t bit_2mb = child_2mb % 64;

		// Clear all 4KB pages within this 2MB region
		const size_t start_word_4kb = (child_2mb * ShdMem::PT_ENTRIES) / 64;
		const size_t end_word_4kb = ((child_2mb + 1) * ShdMem::PT_ENTRIES) / 64;

		for (size_t w = start_word_4kb; w < end_word_4kb; ++w) {
			BitMap4KB[w] = 0;
		}

		BitMap2MB[word_2mb].AnyUsed &= ~(UNIT << bit_2mb);
		BitMap2MB[word_2mb].AnyFree |= (UNIT << bit_2mb);

		Cache2MB.Push(child_2mb);
	}

	// Update parent 32MB flags
	BitMap32MB[word_idx].AnyUsed &= ~(UNIT << bit);
	BitMap32MB[word_idx].AnyFree |= (UNIT << bit);

	// Add to cache if space available
	Cache32MB.Push(region);

	return Success();
}

Success PhysicalMemory::Free1GB(void* ptr) {
	static constexpr size_t SUB_32MB_PAGES = ShdMem::PDPTE_COVERAGE / (16 * ShdMem::PDE_COVERAGE);

	const uint64_t address = reinterpret_cast<uint64_t>(ptr);

	for (size_t i = 0; i < SUB_32MB_PAGES; ++i) {
		const uint64_t sub_address = address + i * (16 * ShdMem::PDE_COVERAGE);

		if (!Free32MB(reinterpret_cast<void*>(sub_address)).IsSuccess()) {
			return Failure();
		}
	}

	return Success();
}
