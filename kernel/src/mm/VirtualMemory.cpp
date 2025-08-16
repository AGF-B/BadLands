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
		static inline void* AllocateCore(uint64_t pages, [[maybe_unused]] void* hintPtr) {
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

			if (MapOnDemand(pagesStart, pages, privilege) != StatusCode::SUCCESS) {
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

	StatusCode Setup() {
		// set up identity paging for the DMA zone
		for (size_t i = ShdMem::Layout::DMAZone.start;
			i < ShdMem::Layout::DMAZone.start + ShdMem::Layout::DMAZone.limit;
			i += ShdMem::FRAME_SIZE
		) {
			if (PhysicalMemory::QueryDMAAddress(i) == PhysicalMemory::StatusCode::ALLOCATED) {
				MapPage(i, i, AccessPrivilege::MEDIUM);
			}
		}

		// make the NULL memory page reserved and unusable
		if (PhysicalMemory::QueryDMAAddress(0) == PhysicalMemory::StatusCode::FREE) {
			static_cast<void>(PhysicalMemory::AllocateDMA(1));
		}
		*GetPTEAddress(0, 0, 0, 0) = 0;
		__asm__ volatile("invlpg (%0)" :: "r"((uint64_t)0));

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

		// setup a temporary main core dump in case a setup failure happens
		basePage = PhysicalMemory::Allocate();
		if (basePage == nullptr) {
			return StatusCode::OUT_OF_MEMORY;
		}
		
		status = MapPage(
			reinterpret_cast<uint64_t>(basePage),
			VirtualMemoryLayout::MainCoreDump.start,
			AccessPrivilege::HIGH
		);
		
		return status;
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

	void* AllocateKernelHeap(uint64_t pages) {
		return AllocateCore<AccessPrivilege::HIGH>(pages, nullptr);
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

	void* MapGeneralPage(void* pageAddress) {
		constexpr uint64_t GP_PAGES = VirtualMemoryLayout::GeneralMapping.limit / ShdMem::PAGE_SIZE;

		uint64_t address = VirtualMemoryLayout::GeneralMapping.start;

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
				*pte = (PhysicalMemory::FilterAddress(pageAddress) & ShdMem::PTE_ADDRESS)
					| ShdMem::PTE_READWRITE
					| ShdMem::PTE_PRESENT;

				__asm__ volatile("invlpg (%0)" :: "r"(address));
				return reinterpret_cast<void*>(address);
			}
		}

		return nullptr;
	}

	StatusCode UnmapGeneralPage(void* vpage) {
		auto mapping = ShdMem::ParseVirtualAddress(vpage);

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

		__asm__ volatile("invlpg (%0)" :: "r"(vpage));
		return StatusCode::SUCCESS;
	}
}
