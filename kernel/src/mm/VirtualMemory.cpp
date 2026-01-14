#include <cstddef>
#include <cstdint>

#include <shared/memory/defs.hpp>

#include <mm/PhysicalMemory.hpp>
#include <mm/VirtualMemory.hpp>
#include <mm/VirtualMemoryLayout.hpp>

namespace ShdMem = Shared::Memory;

struct VMemMapBlock {
	uint64_t virtualStart;
	uint64_t availablePages;
};

struct MemoryContext {
	uint64_t availableMemory;
	uint64_t availableBlockMemory;
	uint64_t storedBlocks;
};

enum class AccessPrivilege {
	HIGH,		// every paging structure has the user mode bit cleared
	MEDIUM,		// intermediate paging structures have the user bit set,
                // the PTE has the user bit cleared (should only be used for the legacy DMA zone)
	LOW			// every intermediate paging structure has the user bit set
};

namespace VirtualMemory {
    namespace {
        static MemoryContext kernelContext {
			.availableMemory = VirtualMemoryLayout::KernelHeap.limit,
			.availableBlockMemory = 0,
			.storedBlocks = 0
		};

		static MemoryContext* const userContext = reinterpret_cast<MemoryContext*>(
			VirtualMemoryLayout::UserVMemManagement.start
		);

        template<bool usePrimary = true>
        static inline StatusCode MapPage(uint64_t _physicalAddress, uint64_t _virtualAddress, AccessPrivilege privilege) {
            if (_physicalAddress % ShdMem::FRAME_SIZE != 0 || _virtualAddress % ShdMem::FRAME_SIZE != 0) {
                return StatusCode::INVALID_PARAMETER;
            }

            ShdMem::VirtualAddress mapping = ShdMem::ParseVirtualAddress(_virtualAddress);
            PML4E* pml4e = GetPML4EAddress<usePrimary>(mapping.PML4_offset);

            if ((*pml4e & ShdMem::PML4E_PRESENT) == 0) {
                void* page = PhysicalMemory::Allocate();
                if (page == nullptr) {
                    return StatusCode::OUT_OF_MEMORY;
                }
                *pml4e = (PhysicalMemory::FilterAddress(page) & ShdMem::PML4E_ADDRESS)
                    | (privilege == AccessPrivilege::HIGH ? 0 : ShdMem::PML4E_USERMODE)
                    | ShdMem::PML4E_READWRITE
                    | ShdMem::PML4E_PRESENT;

                PDPTE* pdpt = GetPDPTAddress<usePrimary>(mapping.PML4_offset);
                __asm__ volatile("invlpg (%0)" :: "r"(pdpt));
                ShdMem::ZeroPage(pdpt);
            }

            PDPTE* pdpte = GetPDPTEAddress<usePrimary>(mapping.PML4_offset, mapping.PDPT_offset);

            if ((*pdpte & ShdMem::PDPTE_PRESENT) == 0) {
                void* page = PhysicalMemory::Allocate();
                if (page == nullptr) {
                    return StatusCode::OUT_OF_MEMORY;
                }
                *pdpte = (PhysicalMemory::FilterAddress(page) & ShdMem::PDPTE_ADDRESS)
                    | (privilege == AccessPrivilege::HIGH ? 0 : ShdMem::PDPTE_USERMODE)
                    | ShdMem::PDPTE_READWRITE
                    | ShdMem::PDPTE_PRESENT;

                PDE* pd = GetPDAddress<usePrimary>(mapping.PML4_offset, mapping.PDPT_offset);
                __asm__ volatile("invlpg (%0)" :: "r"(pd));
                ShdMem::ZeroPage(pd);
            }

            PDE* pde = GetPDEAddress<usePrimary>(mapping.PML4_offset, mapping.PDPT_offset, mapping.PD_offset);

            if ((*pde & ShdMem::PDE_PRESENT) == 0) {
                void* page = PhysicalMemory::Allocate();
                if (page == nullptr) {
                    return StatusCode::OUT_OF_MEMORY;
                }
                *pde = (PhysicalMemory::FilterAddress(page) & ShdMem::PDE_ADDRESS)
                    | (privilege == AccessPrivilege::HIGH ? 0 : ShdMem::PDE_USERMODE)
                    | ShdMem::PDE_READWRITE
                    | ShdMem::PDE_PRESENT;
                
                PTE* pt = GetPTAddress<usePrimary>(mapping.PML4_offset, mapping.PDPT_offset, mapping.PD_offset);
                __asm__ volatile("invlpg (%0)" :: "r"(pt));
                ShdMem::ZeroPage(pt);
            }

            PTE* pte = GetPTEAddress<usePrimary>(mapping.PML4_offset, mapping.PDPT_offset, mapping.PD_offset, mapping.PT_offset);
            *pte = (PhysicalMemory::FilterAddress(_physicalAddress) & ShdMem::PTE_ADDRESS)
                | (privilege == AccessPrivilege::LOW ? ShdMem::PTE_USERMODE : 0)
                | ShdMem::PTE_READWRITE
                | ShdMem::PTE_PRESENT;

            __asm__ volatile("invlpg (%0)" :: "r"(_virtualAddress));

            return StatusCode::SUCCESS;
        }

        template<bool usePrimary = true>
		static inline StatusCode MapOnDemand(const void* _address, uint64_t pages, AccessPrivilege privilege) {
			const uint8_t* address = static_cast<const uint8_t*>(_address);

			for (size_t i = 0; i < pages; ++i) {
				ShdMem::VirtualAddress mapping = ShdMem::ParseVirtualAddress(address);

				PML4E* pml4e = GetPML4EAddress<usePrimary>(mapping.PML4_offset);

				if ((*pml4e & ShdMem::PML4E_PRESENT) == 0) {
					void* page = PhysicalMemory::Allocate();
					if (page == nullptr) {
						return StatusCode::OUT_OF_MEMORY;
					}
					*pml4e = (PhysicalMemory::FilterAddress(page) & ShdMem::PML4E_ADDRESS)
						| ShdMem::PML4E_READWRITE
						| ShdMem::PML4E_PRESENT;

					PDPTE* pdpt = GetPDPTAddress<usePrimary>(mapping.PML4_offset);
					__asm__ volatile("invlpg (%0)" :: "r"(pdpt));
					ShdMem::ZeroPage(pdpt);
				}

				PDPTE* pdpte = GetPDPTEAddress<usePrimary>(mapping.PML4_offset, mapping.PDPT_offset);

				if ((*pdpte & ShdMem::PDPTE_PRESENT) == 0) {
					void* page = PhysicalMemory::Allocate();
					if (page == nullptr) {
						return StatusCode::OUT_OF_MEMORY;
					}
					*pdpte = (PhysicalMemory::FilterAddress(page) & ShdMem::PDPTE_ADDRESS)
						| ShdMem::PDPTE_READWRITE
						| ShdMem::PDPTE_PRESENT;

					PDE* pd = GetPDAddress<usePrimary>(mapping.PML4_offset, mapping.PDPT_offset);
					__asm__ volatile("invlpg (%0)" :: "r"(pd));
					ShdMem::ZeroPage(pd);
				}

				PDE* pde = GetPDEAddress<usePrimary>(mapping.PML4_offset, mapping.PDPT_offset, mapping.PD_offset);

				if ((*pde & ShdMem::PDE_PRESENT) == 0) {
					void* page = PhysicalMemory::Allocate();
					if (page == nullptr) {
						return StatusCode::OUT_OF_MEMORY;
					}
					*pde = (PhysicalMemory::FilterAddress(page) & ShdMem::PDE_ADDRESS)
						| ShdMem::PDE_READWRITE
						| ShdMem::PDE_PRESENT;

					PTE* pt = GetPTAddress<usePrimary>(mapping.PML4_offset, mapping.PDPT_offset, mapping.PD_offset);
					__asm__ volatile("invlpg (%0)" :: "r"(pt));
					ShdMem::ZeroPage(pt);
				}

				PTE* pte = GetPTEAddress<usePrimary>(mapping.PML4_offset, mapping.PDPT_offset, mapping.PD_offset, mapping.PT_offset);
				*pte = NP_ON_DEMAND
					| (privilege == AccessPrivilege::LOW ? NP_USERMODE : 0)
					| NP_READWRITE;

				__asm__ volatile("invlpg (%0)" :: "r"(address));

				address += ShdMem::FRAME_SIZE;
			}

			return StatusCode::SUCCESS;
		}

        static inline StatusCode ExpandVirtualMemoryMap(uint64_t start, uint64_t blocks) {
			void* page = PhysicalMemory::Allocate();
			if (page == nullptr) {
				return StatusCode::OUT_OF_MEMORY;
			}

			return MapPage(reinterpret_cast<uint64_t>(page), start + blocks * sizeof(VMemMapBlock), AccessPrivilege::HIGH);
		}

		// sorts (to the right) by descending number of available pages
		static inline void SortVirtualMemoryMap(VMemMapBlock* start, size_t n) {
			if (n > 2) {
				VMemMapBlock temp;
				for (size_t i = 0; i < n - 1 && (start + 1)->availablePages > start->availablePages; ++i) {
					temp = *start;
					*start = *(start + 1);
					*++start = temp;
				}
			}
		}
		
		// reverse sorts (to the left) by ascending number of available pages
		static inline void RSortVirtualMemoryMap(VMemMapBlock* start, size_t n) {
			if (n > 2) {
				VMemMapBlock temp;
				for (size_t i = 0; i < n - 1 && (start - 1)->availablePages < start->availablePages; ++i) {
					temp = *start;
					*start = *(start - 1);
					*--start = temp;
				}
			}
		}

        template<AccessPrivilege privilege>
		static inline void* AllocateHintCore(VMemMapBlock* block, uint64_t offset, uint64_t pages) {
			MemoryContext* ctx = privilege == AccessPrivilege::HIGH ? &kernelContext : userContext;

			void* pagesStart = reinterpret_cast<void*>(block->virtualStart);

			if (MapOnDemand(pagesStart, pages, AccessPrivilege::LOW) != StatusCode::SUCCESS) {
				return nullptr;
			}

			block->availablePages -= pages;
			block->virtualStart += pages * ShdMem::FRAME_SIZE;

			SortVirtualMemoryMap(block, ctx->storedBlocks - offset);

			ctx->availableMemory -= pages * ShdMem::FRAME_SIZE;
			return pagesStart;
		}

        template<AccessPrivilege privilege, bool useHint = false>
		static inline void* AllocateCore(uint64_t pages, [[maybe_unused]] void* hintPtr, bool contiguous = false) {
			constexpr uint64_t managementBase = privilege == AccessPrivilege::HIGH
				? VirtualMemoryLayout::KernelHeapManagement.start
				: (VirtualMemoryLayout::UserVMemManagement.start
					+ (sizeof(*userContext) + sizeof(VMemMapBlock) - 1) / sizeof(VMemMapBlock));
			
			MemoryContext* ctx = privilege == AccessPrivilege::HIGH ? &kernelContext : userContext;

			VMemMapBlock* vmmb = reinterpret_cast<VMemMapBlock*>(managementBase);
			
			if (ctx->availableMemory < pages * ShdMem::FRAME_SIZE || vmmb->availablePages < pages) {
				return nullptr;
			}

			size_t index = 0;

			if (pages == 0) {
				return nullptr;
			}
			else if (pages == 1) {
				index = ctx->storedBlocks - 1;
				vmmb += index;
			}
			else {
				for (++index, ++vmmb;; ++index, ++vmmb) {
					if (index >= ctx->storedBlocks || vmmb->availablePages < pages) {
						--index;
						--vmmb;
						break;
					}
				}
			}

			if constexpr (useHint) {
				if (hintPtr != nullptr && (reinterpret_cast<uint64_t>(hintPtr) % ShdMem::FRAME_SIZE) == 0) {
					const uint64_t hint = reinterpret_cast<uint64_t>(hintPtr);
					size_t indexOffset = 0;
					unsigned int fitFound = 0;

					for (size_t i = 0; i <= index; ++i) {
						VMemMapBlock* newBlock = vmmb - i;

						if (newBlock->virtualStart <= hint
							&& hint + pages * ShdMem::FRAME_SIZE
							<= newBlock->virtualStart + newBlock->availablePages * ShdMem::FRAME_SIZE
						) {
							indexOffset = i;
							fitFound = 1;
							break;
						}
						else if (newBlock->virtualStart < vmmb->virtualStart && newBlock->virtualStart >= hint) {
							if (hint + pages * ShdMem::FRAME_SIZE
								<= newBlock->virtualStart + newBlock->availablePages * ShdMem::FRAME_SIZE
							) {
								indexOffset = i;
								fitFound = 1;
								continue;
							}
						}
					}

					if (fitFound && vmmb->availablePages > pages) {
						vmmb -= indexOffset;
						index -= indexOffset;

						if (vmmb->virtualStart <= hint) {
							VMemMapBlock prevBlock = {
								.virtualStart = vmmb->virtualStart,
								.availablePages = (hint - vmmb->virtualStart) / ShdMem::FRAME_SIZE
							};
							VMemMapBlock nextBlock = {
								.virtualStart = hint + pages * ShdMem::FRAME_SIZE,
								.availablePages = (
									vmmb->virtualStart
									+ vmmb->availablePages * ShdMem::FRAME_SIZE
									- (hint + pages * ShdMem::FRAME_SIZE)) / ShdMem::FRAME_SIZE
							};

							if (prevBlock.availablePages == 0) {
								return AllocateHintCore<privilege>(vmmb, index, pages);
							}
							else if (nextBlock.availablePages == 0) {
								if (MapOnDemand(hintPtr, pages, privilege) != StatusCode::SUCCESS) {
									return nullptr;
								}

								*vmmb = prevBlock;
								SortVirtualMemoryMap(vmmb, ctx->storedBlocks - index);
								ctx->availableMemory -= pages * ShdMem::FRAME_SIZE;
								
								return hintPtr;
							}
							else {
								if (ctx->availableBlockMemory == 0) {
									if (ExpandVirtualMemoryMap(managementBase, ctx->storedBlocks) != StatusCode::SUCCESS) {
										return nullptr;
									}
									ctx->availableBlockMemory += ShdMem::FRAME_SIZE;
								}

								if (MapOnDemand(hintPtr, pages, privilege) != StatusCode::SUCCESS) {
									return nullptr;
								}

								VMemMapBlock* splitResidue = reinterpret_cast<VMemMapBlock*>(managementBase) + ctx->storedBlocks++;

								if (prevBlock.availablePages > nextBlock.availablePages) {
									*vmmb = prevBlock;
									*splitResidue = nextBlock;
								}
								else {
									*vmmb = nextBlock;
									*splitResidue = prevBlock;
								}

								SortVirtualMemoryMap(vmmb, ctx->storedBlocks - index);
								RSortVirtualMemoryMap(splitResidue, ctx->storedBlocks);
								ctx->availableMemory -= pages * ShdMem::FRAME_SIZE;
								ctx->availableBlockMemory -= sizeof(VMemMapBlock);

								return hintPtr;
							}
						}
						else {
							return AllocateHintCore<privilege>(vmmb, index, pages);
						}
					}
				}
			}

			void* pagesStart = reinterpret_cast<void*>(
				vmmb->virtualStart + (vmmb->availablePages -= pages) * ShdMem::FRAME_SIZE
			);
			const bool remove = vmmb->availablePages == 0;

			if (contiguous) {
				void* physical_pages = PhysicalMemory::AllocatePages(pages);

				if (physical_pages == nullptr) {
					vmmb->availablePages += pages;
					return nullptr;
				}

				for (size_t i = 0; i < pages; ++i) {
					const uint64_t physicalAddress = PhysicalMemory::FilterAddress(
						reinterpret_cast<uint8_t*>(physical_pages) + i * ShdMem::FRAME_SIZE
					);
					const uint64_t virtualAddress = reinterpret_cast<uint64_t>(pagesStart) + i * ShdMem::FRAME_SIZE;

					if (MapPage<true>(physicalAddress, virtualAddress, privilege) != StatusCode::SUCCESS) {
						PhysicalMemory::FreePages(physical_pages, pages);
						vmmb->availablePages += pages;
						return nullptr;
					}
				}
			}
			else if (MapOnDemand(pagesStart, pages, privilege) != StatusCode::SUCCESS) {
				vmmb->availablePages += pages;
				return nullptr;
			}

			SortVirtualMemoryMap(vmmb, ctx->storedBlocks - index);

			if (remove) {
				--ctx->storedBlocks;
				ctx->availableBlockMemory += sizeof(VMemMapBlock);

				if (ctx->availableBlockMemory % ShdMem::FRAME_SIZE == 0 && ctx->availableBlockMemory > ShdMem::FRAME_SIZE) {
					ctx->availableBlockMemory -= ShdMem::FRAME_SIZE;
					const uint64_t linearAddress = managementBase + ctx->availableBlockMemory;
					ShdMem::VirtualAddress blockMemoryAddress = ShdMem::ParseVirtualAddress(linearAddress);
					PTE* blockMemoryPTE = GetPTEAddress(
						blockMemoryAddress.PML4_offset,
						blockMemoryAddress.PDPT_offset,
						blockMemoryAddress.PD_offset,
						blockMemoryAddress.PT_offset
					);
					PhysicalMemory::Free(reinterpret_cast<void*>(*blockMemoryPTE & ShdMem::PTE_ADDRESS));
					*blockMemoryPTE = 0;
					__asm__ volatile("invlpg (%0)" :: "r"(linearAddress));
				}
			}

			ctx->availableMemory -= pages * ShdMem::FRAME_SIZE;
			return pagesStart;
		}

		template<AccessPrivilege privilege> static inline StatusCode FreeCore(void* ptr, uint64_t pages) {
			constexpr uint64_t managementBase = privilege == AccessPrivilege::HIGH
				? VirtualMemoryLayout::KernelHeapManagement.start
				: (VirtualMemoryLayout::UserVMemManagement.start
					+ (sizeof(*userContext) + sizeof(VMemMapBlock) - 1) / sizeof(VMemMapBlock));

			MemoryContext* ctx = privilege == AccessPrivilege::HIGH ? &kernelContext : userContext;

			if (pages == 0) {
				return StatusCode::SUCCESS;
			}
			
			uint64_t address = reinterpret_cast<uint64_t>(ptr);

			if constexpr (privilege == AccessPrivilege::LOW) {
				if (address > VirtualMemoryLayout::UserMemory.start + VirtualMemoryLayout::UserMemory.limit
					|| address < VirtualMemoryLayout::UserMemory.start
					|| address + pages * ShdMem::FRAME_SIZE < address
				) {
					return StatusCode::INVALID_PARAMETER;
				}
			}

			for (size_t i = 0; i < pages; ++i, address += ShdMem::FRAME_SIZE) {
				ShdMem::VirtualAddress mapping = ShdMem::ParseVirtualAddress(address);
				PML4E* pml4e = GetPML4EAddress(mapping.PML4_offset);
				PDPTE* pdpte = GetPDPTEAddress(mapping.PML4_offset, mapping.PDPT_offset);
				PDE* pde = GetPDEAddress(mapping.PML4_offset, mapping.PDPT_offset, mapping.PD_offset);
				PTE* pte = GetPTEAddress(mapping.PML4_offset, mapping.PDPT_offset, mapping.PD_offset, mapping.PT_offset);

				if constexpr (privilege == AccessPrivilege::LOW) {
					if ((*pml4e & ShdMem::PML4E_PRESENT) == 0
						|| (*pdpte & ShdMem::PDPTE_PRESENT) == 0
						|| (*pde & ShdMem::PDE_PRESENT) == 0
						|| (*pte == 0)
					) {
						return StatusCode::INVALID_PARAMETER;
					}
				}

				if ((*pte & ShdMem::PTE_PRESENT) != 0) {
					void* pageAddress = reinterpret_cast<void*>(*pte & ShdMem::PTE_ADDRESS);
					if (PhysicalMemory::Free(pageAddress) != PhysicalMemory::StatusCode::SUCCESS) {
						return StatusCode::OUT_OF_MEMORY;
					}
					__asm__ volatile("invlpg (%0)" :: "r"(address));
				}
				else if ((*pte & VirtualMemory::NP_ON_DEMAND) != 0) {
					/// TODO: do stuff with the swap file
				}
				*pte = 0;
			}

			VMemMapBlock vmmb = {
				.virtualStart = reinterpret_cast<uint64_t>(ptr),
				.availablePages = pages
			};

			if (ctx->availableBlockMemory == 0) {
				auto status = ExpandVirtualMemoryMap(managementBase, ctx->storedBlocks);
				if (status != StatusCode::SUCCESS) {
					return status;
				}
				ctx->availableBlockMemory += ShdMem::FRAME_SIZE;
			}

			VMemMapBlock* blockPtr = reinterpret_cast<VMemMapBlock*>(managementBase) + ctx->storedBlocks++;
			*blockPtr = vmmb;
			ctx->availableBlockMemory -= sizeof(VMemMapBlock);
			ctx->availableMemory += pages * ShdMem::FRAME_SIZE;
			RSortVirtualMemoryMap(blockPtr, ctx->storedBlocks);

			return StatusCode::SUCCESS;
		}
    }

	void UpdateSecondaryRecursiveMapping(void* newAddress) {
		static constexpr auto secondaryMapping = ShdMem::ParseVirtualAddress(VirtualMemoryLayout::SecondaryRecursiveMapping.start);
		PML4E* const secondaryPML4 = GetPML4EAddress(secondaryMapping.PML4_offset);

		*secondaryPML4 = ShdMem::PML4E_XD
			| (PhysicalMemory::FilterAddress(newAddress) & ShdMem::PML4E_ADDRESS)
			| ShdMem::PML4E_READWRITE
			| ShdMem::PML4E_PRESENT;

		__asm__ volatile("invlpg (%0)" :: "r"(GetPML4Address<false>()));
	}

	StatusCode Setup() {
		// make the NULL memory page reserved and unusable, and allocate the DMA PML4E and PDPTE
		if (PhysicalMemory::QueryDMAAddress(0) == PhysicalMemory::StatusCode::FREE) {
			if (VirtualMemory::AllocateDMA(1) == nullptr) {
				return StatusCode::INTERNAL_ERROR;
			}
		}
		else {
			MapPage(0, 0, AccessPrivilege::MEDIUM);
		}

		*GetPTEAddress(0, 0, 0, 0) = 0;
		__asm__ volatile("invlpg (%0)" :: "r"((uint64_t)0));
		
		// set up identity paging for the DMA zone, and allocate all necessary pages tables it needs
		// even if they are not mapping to anything yet
		for (size_t i = ShdMem::Layout::DMAZone.start;
			i < ShdMem::Layout::DMAZone.start + ShdMem::Layout::DMAZone.limit;
			i += ShdMem::FRAME_SIZE
		) {
			if (PhysicalMemory::QueryDMAAddress(i) == PhysicalMemory::StatusCode::ALLOCATED) {
				MapPage(i, i, AccessPrivilege::MEDIUM);
			}
			else {
				auto mapping = ShdMem::ParseVirtualAddress(i);
				if ((*GetPDPTEAddress(mapping.PML4_offset, mapping.PDPT_offset) & ShdMem::PDPTE_PRESENT) == 0) {
					void* page = PhysicalMemory::Allocate();
					if (page == nullptr) {
						return StatusCode::OUT_OF_MEMORY;
					}
					*GetPDPTEAddress(mapping.PML4_offset, mapping.PDPT_offset) =
						(PhysicalMemory::FilterAddress(page) & ShdMem::PDPTE_ADDRESS)
						| ShdMem::PDPTE_READWRITE
						| ShdMem::PDPTE_PRESENT;

					PDE* pd = GetPDAddress(mapping.PML4_offset, mapping.PDPT_offset);
					__asm__ volatile("invlpg (%0)" :: "r"(pd));
					ShdMem::ZeroPage(pd);
				}

				if ((*GetPDEAddress(mapping.PML4_offset, mapping.PDPT_offset, mapping.PD_offset) & ShdMem::PDE_PRESENT) == 0) {
					void* page = PhysicalMemory::Allocate();
					if (page == nullptr) {
						return StatusCode::OUT_OF_MEMORY;
					}
					*GetPDEAddress(mapping.PML4_offset, mapping.PDPT_offset, mapping.PD_offset) =
						(PhysicalMemory::FilterAddress(page) & ShdMem::PDE_ADDRESS)
						| ShdMem::PDE_READWRITE
						| ShdMem::PDE_PRESENT;

					PTE* pt = GetPTAddress(mapping.PML4_offset, mapping.PDPT_offset, mapping.PD_offset);
					__asm__ volatile("invlpg (%0)" :: "r"(pt));
					ShdMem::ZeroPage(pt);
				}
			}
		}

		// set up kernel heap
		void* basePage = PhysicalMemory::Allocate();
		if (basePage == nullptr) {
			return StatusCode::OUT_OF_MEMORY;
		}

		auto status = MapPage(
			reinterpret_cast<uint64_t>(basePage),
			VirtualMemoryLayout::KernelHeapManagement.start,
			AccessPrivilege::HIGH
		);
		if (status != StatusCode::SUCCESS) {
			return status;
		}

		kernelContext.availableBlockMemory = ShdMem::FRAME_SIZE - sizeof(VMemMapBlock);
		kernelContext.storedBlocks = 1;

		VMemMapBlock* kernelHeapBlockPtr = reinterpret_cast<VMemMapBlock*>(VirtualMemoryLayout::KernelHeapManagement.start);
		kernelHeapBlockPtr->virtualStart = VirtualMemoryLayout::KernelHeap.start;
		kernelHeapBlockPtr->availablePages = kernelContext.availableMemory / ShdMem::FRAME_SIZE;
		
		return status;
	}

	void* DeriveNewFreshCR3() {
		void* CR3 = PhysicalMemory::Allocate();

		if (CR3 == nullptr) {
			return nullptr;
		}
		
		PML4E* vroot = static_cast<PML4E*>(MapGeneralPages(CR3, 1, ShdMem::PML4E_PRESENT | ShdMem::PML4E_READWRITE));

		if (vroot == nullptr) {
			PhysicalMemory::Free(CR3);
			return nullptr;
		}

		// clear entire page
		ShdMem::ZeroPage(vroot);

		static constexpr uint16_t SECONDARY_PML4_INDEX =
			ShdMem::ParseVirtualAddress(VirtualMemoryLayout::SecondaryRecursiveMapping.start).PML4_offset;
		static constexpr uint16_t PRIMARY_PML4_INDEX =
			ShdMem::ParseVirtualAddress(ShdMem::Layout::RecursiveMemoryMapping.start).PML4_offset;

		vroot[SECONDARY_PML4_INDEX] = vroot[PRIMARY_PML4_INDEX] = 
			(PhysicalMemory::FilterAddress(CR3) & ShdMem::PML4E_ADDRESS)
			| ShdMem::PML4E_PRESENT
			| ShdMem::PML4E_READWRITE;

		auto status = VirtualMemory::UnmapGeneralPages(vroot, 1);

		if (status != VirtualMemory::StatusCode::SUCCESS) {
			PhysicalMemory::Free(CR3);
			return nullptr;
		}

		UpdateSecondaryRecursiveMapping(CR3);

		// copy shared kernel memory
		for (uint16_t i = 256; i < SECONDARY_PML4_INDEX; ++i) {
			GetPML4Address<false>()[i] = GetPML4Address<true>()[i];
		}

		// share DMA pages
		static constexpr auto dma_mapping = ShdMem::ParseVirtualAddress(ShdMem::Layout::DMAZone.start);
		static constexpr auto dma_end_mapping = ShdMem::ParseVirtualAddress(
			ShdMem::Layout::DMAZone.start + ShdMem::Layout::DMAZone.limit
		);

		static_assert(dma_mapping.PML4_offset == dma_end_mapping.PML4_offset);
		static_assert(dma_mapping.PDPT_offset == dma_end_mapping.PDPT_offset);
		static_assert(dma_mapping.PD_offset == 0);
		
		static constexpr uint16_t dma_pds = dma_end_mapping.PD_offset - dma_mapping.PD_offset;

		static_assert(dma_pds == 8);

		void* phys_first_pml4e = PhysicalMemory::Allocate();

		if (phys_first_pml4e == nullptr) {
			PhysicalMemory::Free(CR3);
			return nullptr;
		}

		*GetPML4EAddress<false>(dma_mapping.PML4_offset) =
			(PhysicalMemory::FilterAddress(phys_first_pml4e) & ShdMem::PML4E_ADDRESS)
			| ShdMem::PML4E_PRESENT
			| ShdMem::PML4E_READWRITE;

		void* phys_first_pdpt = PhysicalMemory::Allocate();

		if (phys_first_pdpt == nullptr) {
			PhysicalMemory::Free(phys_first_pml4e);
			PhysicalMemory::Free(CR3);
			return nullptr;
		}

		auto* first_pdpt = GetPDPTEAddress<false>(dma_mapping.PML4_offset, dma_mapping.PDPT_offset);
		__asm__ volatile("invlpg (%0)" :: "r"(first_pdpt));

		*first_pdpt	=
			(PhysicalMemory::FilterAddress(phys_first_pdpt) & ShdMem::PDPTE_ADDRESS)
			| ShdMem::PDPTE_PRESENT
			| ShdMem::PDPTE_READWRITE;

		for (size_t i = 0; i < dma_pds; ++i) {
			*GetPDPTEAddress<false>(dma_mapping.PML4_offset, dma_mapping.PDPT_offset + i) =
				*GetPDPTEAddress<true>(dma_mapping.PML4_offset, dma_mapping.PDPT_offset + i);
		}

		/// FIXME: starting from here, a failure will cause a memory leak
		/// How to fix: create a method to completely free an entire page table

		// setup kernel stack and kernel stack guard
		status = MapOnDemand<false>(
			reinterpret_cast<void*>(VirtualMemoryLayout::KernelStack.start),
			(VirtualMemoryLayout::KernelStack.limit - ShdMem::PAGE_SIZE) / ShdMem::PAGE_SIZE,
			AccessPrivilege::HIGH
		);

		if (status != StatusCode::SUCCESS) {
			PhysicalMemory::Free(phys_first_pml4e);
			PhysicalMemory::Free(phys_first_pdpt);
			PhysicalMemory::Free(CR3);
			return nullptr;
		}

		auto stack_guard_mapping = ShdMem::ParseVirtualAddress(VirtualMemoryLayout::KernelStackGuard.start);
		PTE* stack_guard_pte = GetPTEAddress<false>(
			stack_guard_mapping.PML4_offset,
			stack_guard_mapping.PDPT_offset,
			stack_guard_mapping.PD_offset,
			stack_guard_mapping.PT_offset
		);
		*stack_guard_pte = 0;
		__asm__ volatile("invlpg (%0)" :: "r"(VirtualMemoryLayout::KernelStackGuard.start));

		void* stack_top = PhysicalMemory::Allocate();

		if (stack_top == nullptr) {
			PhysicalMemory::Free(phys_first_pml4e);
			PhysicalMemory::Free(phys_first_pdpt);
			PhysicalMemory::Free(CR3);
			return nullptr;
		}

		void* stack_reserve = PhysicalMemory::Allocate();

		if (stack_reserve == nullptr) {
			PhysicalMemory::Free(stack_top);
			PhysicalMemory::Free(phys_first_pml4e);
			PhysicalMemory::Free(phys_first_pdpt);
			PhysicalMemory::Free(CR3);
			return nullptr;
		}

		status = MapPage<false>(
			reinterpret_cast<uint64_t>(stack_top),
			VirtualMemoryLayout::KernelStackReserve.start - ShdMem::PAGE_SIZE,
			AccessPrivilege::HIGH
		);

		if (status != StatusCode::SUCCESS) {
			PhysicalMemory::Free(stack_reserve);
			PhysicalMemory::Free(stack_top);
			PhysicalMemory::Free(phys_first_pml4e);
			PhysicalMemory::Free(phys_first_pdpt);
			PhysicalMemory::Free(CR3);
			return nullptr;
		}

		status = MapPage<false>(
			reinterpret_cast<uint64_t>(stack_reserve),
			VirtualMemoryLayout::KernelStackReserve.start,
			AccessPrivilege::HIGH
		);

		if (status != StatusCode::SUCCESS) {
			PhysicalMemory::Free(stack_top);
			PhysicalMemory::Free(stack_reserve);
			PhysicalMemory::Free(phys_first_pml4e);
			PhysicalMemory::Free(phys_first_pdpt);
			PhysicalMemory::Free(CR3);
			return nullptr;
		}

		// set up user memory management structures
		void* base_page = PhysicalMemory::Allocate();
		
		if (base_page == nullptr) {
			PhysicalMemory::Free(stack_reserve);
			PhysicalMemory::Free(stack_top);
			PhysicalMemory::Free(phys_first_pml4e);
			PhysicalMemory::Free(phys_first_pdpt);
			PhysicalMemory::Free(CR3);
			return nullptr;
		}

		status = MapPage<false>(
			reinterpret_cast<uint64_t>(base_page),
			reinterpret_cast<uint64_t>(userContext),
			AccessPrivilege::HIGH
		);

		if (status != StatusCode::SUCCESS) {
			PhysicalMemory::Free(base_page);
			PhysicalMemory::Free(stack_reserve);
			PhysicalMemory::Free(stack_top);
			PhysicalMemory::Free(phys_first_pml4e);
			PhysicalMemory::Free(phys_first_pdpt);
			PhysicalMemory::Free(CR3);
			return nullptr;
		}

		void* vbase_page = MapGeneralPages(base_page, 1, ShdMem::PTE_PRESENT | ShdMem::PTE_READWRITE);

		if (vbase_page == nullptr) {
			PhysicalMemory::Free(base_page);
			PhysicalMemory::Free(stack_reserve);
			PhysicalMemory::Free(stack_top);
			PhysicalMemory::Free(phys_first_pml4e);
			PhysicalMemory::Free(phys_first_pdpt);
			PhysicalMemory::Free(CR3);
			return nullptr;
		}

		MemoryContext* newUserContext = static_cast<MemoryContext*>(vbase_page);

		newUserContext->availableMemory = VirtualMemoryLayout::UserMemory.limit - VirtualMemoryLayout::UserStack.limit;
		newUserContext->availableBlockMemory =
			ShdMem::PAGE_SIZE
			- sizeof(VMemMapBlock)
			- (VirtualMemoryLayout::UserVMemManagement.start - reinterpret_cast<uint64_t>(newUserContext));
		newUserContext->storedBlocks = 1;
		
		VMemMapBlock* userMemBlockPtr = reinterpret_cast<VMemMapBlock*>(
			reinterpret_cast<uint64_t>(newUserContext) + sizeof(MemoryContext)
		);
		userMemBlockPtr->virtualStart = VirtualMemoryLayout::UserMemory.start;
		userMemBlockPtr->availablePages = newUserContext->availableMemory / ShdMem::FRAME_SIZE;

		UnmapGeneralPages(vbase_page, 1);

		return CR3;
	}

	void* AllocateDMA(uint64_t pages) {
		void* const allocated = PhysicalMemory::AllocateDMA(pages);
		if (allocated == nullptr) {
			return nullptr;
		}
		
		uint64_t address = reinterpret_cast<uint64_t>(allocated);

		for (size_t i = 0; i < pages; ++i, address += ShdMem::FRAME_SIZE) {
			if (MapPage(address, address, AccessPrivilege::MEDIUM) != StatusCode::SUCCESS) {
				return nullptr;
			}
		}

		return allocated;
	}

	void* AllocateKernelHeap(uint64_t pages, bool contiguous) {
		return AllocateCore<AccessPrivilege::HIGH>(pages, nullptr, contiguous);
	}

	void* AllocateUserPages(uint64_t pages) {
		return AllocateCore<AccessPrivilege::LOW>(pages, nullptr);
	}

	void* AllocateUserPagesAt(uint64_t pages, void* ptr) {
		return AllocateCore<AccessPrivilege::LOW, true>(pages, ptr);
	}

	StatusCode FreeDMA(void* ptr, uint64_t pages) {
		auto status = PhysicalMemory::FreeDMA(ptr, pages);
		if (status != PhysicalMemory::StatusCode::SUCCESS) {
			return StatusCode::INVALID_PARAMETER;
		}

		uint64_t address = reinterpret_cast<uint64_t>(ptr);

		for (size_t i = 0; i < pages; ++i, address += ShdMem::FRAME_SIZE) {
			ShdMem::VirtualAddress mapping = ShdMem::ParseVirtualAddress(address);
			PTE* pte = GetPTEAddress(mapping.PML4_offset, mapping.PDPT_offset, mapping.PD_offset, mapping.PT_offset);
			*pte = 0;
		}

		return StatusCode::SUCCESS;
	}

	StatusCode FreeKernelHeap(void* ptr, uint64_t pages) {
		return FreeCore<AccessPrivilege::HIGH>(ptr, pages);
	}

	StatusCode FreeUserPages(void* ptr, uint64_t pages) {
		return FreeCore<AccessPrivilege::LOW>(ptr, pages);
	}

	StatusCode ChangeMappingFlags(void* _ptr, uint64_t flags, uint64_t pages) {
		uint64_t ptr = reinterpret_cast<uint64_t>(_ptr);

		for (size_t i = 0; i < pages; ++i, ptr += ShdMem::PAGE_SIZE) {
			ShdMem::VirtualAddress mapping = ShdMem::ParseVirtualAddress(ptr);
			
			PML4E* pml4e = GetPML4EAddress(mapping.PML4_offset);
			PDPTE* pdpte = GetPDPTEAddress(mapping.PML4_offset, mapping.PDPT_offset);
			PDE* pde = GetPDEAddress(mapping.PML4_offset, mapping.PDPT_offset, mapping.PD_offset);
			PTE* pte = GetPTEAddress(mapping.PML4_offset, mapping.PDPT_offset, mapping.PD_offset, mapping.PT_offset);

			if ((*pml4e & ShdMem::PML4E_PRESENT) == 0) {
				return StatusCode::INVALID_PARAMETER;
			}
			else if ((*pdpte & ShdMem::PDPTE_PRESENT) == 0) {
				return StatusCode::INVALID_PARAMETER;
			}
			else if ((*pde & ShdMem::PDE_PRESENT) == 0) {
				return StatusCode::INVALID_PARAMETER;
			}
			
			*pte = (*pte & (ShdMem::PTE_ADDRESS)) | (flags & ~ShdMem::PTE_ADDRESS);
			__asm__ volatile("invlpg (%0)" :: "r"(ptr));
		}

		return StatusCode::SUCCESS;
	}

	void* MapGeneralPages(void* pageAddress, size_t pages, uint64_t flags) {
		constexpr uint64_t GP_PAGES = VirtualMemoryLayout::GeneralMapping.limit / ShdMem::PAGE_SIZE;

		if (pages == 0) {
			return nullptr;
		}

		uint64_t address = VirtualMemoryLayout::GeneralMapping.start;

		size_t found = 0;
		uint64_t start = 0;

		for (size_t i = 0; i < GP_PAGES; ++i, address += ShdMem::PAGE_SIZE) {
			auto mapping = ShdMem::ParseVirtualAddress(address);

			PDPTE* pdpte = GetPDPTEAddress(mapping.PML4_offset, mapping.PDPT_offset);

			if ((*pdpte & ShdMem::PDPTE_PRESENT) == 0) {
				void* page = PhysicalMemory::Allocate();

				if (page == nullptr) {
					return nullptr;
				}

				*pdpte = (PhysicalMemory::FilterAddress(page) & ShdMem::PDPTE_ADDRESS)
					| ShdMem::PDPTE_READWRITE
					| ShdMem::PDPTE_PRESENT;

				PDE* pd = GetPDAddress(mapping.PML4_offset, mapping.PDPT_offset);
				ShdMem::ZeroPage(pd);
			}

			PDE* pde = GetPDEAddress(mapping.PML4_offset, mapping.PDPT_offset, mapping.PD_offset);

			if ((*pde & ShdMem::PDE_PRESENT) == 0) {
				void* page = PhysicalMemory::Allocate();
				
				if (page == nullptr) {
					return nullptr;
				}

				*pde = (PhysicalMemory::FilterAddress(page) & ShdMem::PDE_ADDRESS)
					| ShdMem::PDE_READWRITE
					| ShdMem::PDE_PRESENT;

				PTE* pt = GetPTAddress(mapping.PML4_offset, mapping.PDPT_offset, mapping.PD_offset);
				ShdMem::ZeroPage(pt);
			}

			PTE* pte = GetPTEAddress(mapping.PML4_offset, mapping.PDPT_offset, mapping.PD_offset, mapping.PT_offset);

			if ((*pte & ShdMem::PTE_PRESENT) == 0) {
				if (found == 0) {
					start = address;
				}

				if (++found == pages) {
					address = start;

					uint64_t physical_address = reinterpret_cast<uint64_t>(pageAddress);

					for (size_t j = 0; j < pages; ++j, address += ShdMem::PAGE_SIZE, physical_address += ShdMem::PAGE_SIZE) {
						mapping = ShdMem::ParseVirtualAddress(address);
						pte = GetPTEAddress(mapping.PML4_offset, mapping.PDPT_offset, mapping.PD_offset, mapping.PT_offset);

						*pte = (PhysicalMemory::FilterAddress(physical_address) & ShdMem::PTE_ADDRESS)
							| flags
							| ShdMem::PTE_PRESENT;

						__asm__ volatile("invlpg (%0)" :: "r"(address));
					}

					return reinterpret_cast<void*>(start + (reinterpret_cast<uint64_t>(pageAddress) % ShdMem::PAGE_SIZE));
				}
			}
			else {
				found = 0;
				start = 0;
			}
		}

		return nullptr;
	}

	StatusCode UnmapGeneralPages(void* vpage, size_t pages) {
		if (pages == 0) {
			return StatusCode::SUCCESS;
		}

		uint64_t address = reinterpret_cast<uint64_t>(vpage);

		for (size_t i = 0; i < pages; ++i, address += ShdMem::PAGE_SIZE) {
			auto mapping = ShdMem::ParseVirtualAddress(address);

			PML4E* pml4e = GetPML4EAddress(mapping.PML4_offset);
			if ((*pml4e & ShdMem::PML4E_PRESENT) == 0) {
				return StatusCode::INVALID_PARAMETER;
			}

			PDPTE* pdpte = GetPDPTEAddress(mapping.PML4_offset, mapping.PDPT_offset);
			if ((*pdpte & ShdMem::PDPTE_PRESENT) == 0) {
				return StatusCode::INVALID_PARAMETER;
			}

			PDE* pde = GetPDEAddress(mapping.PML4_offset, mapping.PDPT_offset, mapping.PD_offset);
			if ((*pde & ShdMem::PDE_PRESENT) == 0) {
				return StatusCode::INVALID_PARAMETER;
			}

			PTE* pte = GetPTEAddress(mapping.PML4_offset, mapping.PDPT_offset, mapping.PD_offset, mapping.PT_offset);
			*pte = 0;

			__asm__ volatile("invlpg (%0)" :: "r"(address));
		}

		return StatusCode::SUCCESS;
	}
}
