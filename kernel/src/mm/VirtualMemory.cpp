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

#include <shared/Response.hpp>
#include <shared/memory/defs.hpp>

#include <mm/Paging.hpp>
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
        static inline Success MapPage(
			uint64_t _physicalAddress,
			uint64_t _virtualAddress,
			AccessPrivilege privilege,
			bool huge = false
		) {
            if (_physicalAddress % ShdMem::FRAME_SIZE != 0 || _virtualAddress % ShdMem::FRAME_SIZE != 0) {
                return Failure();
            }

            const auto mapping = ShdMem::ParseVirtualAddress(_virtualAddress);

            const auto pml4e = Paging::GetPML4EAddress(mapping, usePrimary);
			const auto pml4e_info = Paging::GetPML4EInfo(pml4e);

            if (!pml4e_info.present) {
                void* page = PhysicalMemory::Allocate();

                if (page == nullptr) {
                    return Failure();
                }

				Paging::SetPML4EInfo(pml4e, {
					.present = 1,
					.readWrite = 1,
					.userMode = privilege != AccessPrivilege::HIGH,
					.address = PhysicalMemory::FilterAddress(page)
				});

                const auto pdpt = Paging::GetPDPTAddress(mapping, usePrimary);
                Paging::InvalidatePage(pdpt);
                ShdMem::ZeroPage(pdpt);
            }

            const auto pdpte = Paging::GetPDPTEAddress(mapping, usePrimary);
			const auto pdpte_info = Paging::GetPDPTEInfo(pdpte);

            if (!pdpte_info.present) {
                void* page = PhysicalMemory::Allocate();

                if (page == nullptr) {
                    return Failure();
                }
				
				Paging::SetPDPTEInfo(pdpte, {
					.present = 1,
					.readWrite = 1,
					.userMode = privilege != AccessPrivilege::HIGH,
					.address = PhysicalMemory::FilterAddress(page)
				});

                const auto pd = Paging::GetPDAddress(mapping, usePrimary);
                Paging::InvalidatePage(pd);
                ShdMem::ZeroPage(pd);
            }

            const auto pde = Paging::GetPDEAddress(mapping, usePrimary);
			const auto pde_info = Paging::GetPDEInfo(pde);

			if (huge) {
				*pde = (PhysicalMemory::FilterAddress(_physicalAddress) & ShdMem::PDE_ADDRESS)
					| (privilege == AccessPrivilege::LOW ? ShdMem::PDE_USERMODE : 0)
					| ShdMem::PDE_PAGE_SIZE
					| ShdMem::PDE_READWRITE
					| ShdMem::PDE_PRESENT;

				Paging::SetPDEInfo(pde, {
					.present = 1,
					.readWrite = 1,
					.userMode = privilege == AccessPrivilege::LOW,
					.pageSize = 1,
					.address = PhysicalMemory::FilterAddress(_physicalAddress)
				});
				
				Paging::InvalidatePage(reinterpret_cast<void*>(_virtualAddress));
			}
			else {
				if (!pde_info.present) {
					void* page = PhysicalMemory::Allocate();

					if (page == nullptr) {
						return Failure();
					}

					Paging::SetPDEInfo(pde, {
						.present = 1,
						.readWrite = 1,
						.userMode = privilege != AccessPrivilege::HIGH,
						.address = PhysicalMemory::FilterAddress(page)
					});
					const auto pt = Paging::GetPTAddress(mapping, usePrimary);
					Paging::InvalidatePage(pt);
					ShdMem::ZeroPage(pt);
				}

				const auto pte = Paging::GetPTEAddress(mapping, usePrimary);

				*pte = (PhysicalMemory::FilterAddress(_physicalAddress) & ShdMem::PTE_ADDRESS)
					| (privilege == AccessPrivilege::LOW ? ShdMem::PTE_USERMODE : 0)
					| ShdMem::PTE_READWRITE
					| ShdMem::PTE_PRESENT;

				Paging::SetPTEInfo(pte, {
					.present = 1,
					.readWrite = 1,
					.userMode = privilege == AccessPrivilege::LOW,
					.address = PhysicalMemory::FilterAddress(_physicalAddress)
				});

				Paging::InvalidatePage(reinterpret_cast<void*>(_virtualAddress));
			}

            return Success();
        }

        template<bool usePrimary = true>
		static inline Success MapOnDemand(const void* _address, uint64_t pages, AccessPrivilege privilege) {
			const uint8_t* address = static_cast<const uint8_t*>(_address);

			for (size_t i = 0; i < pages; ++i) {
				const auto mapping = ShdMem::ParseVirtualAddress(address);

				const auto pml4e = Paging::GetPML4EAddress(mapping, usePrimary);
				const auto pml4e_info = Paging::GetPML4EInfo(pml4e);

				if (!pml4e_info.present) {
					void* page = PhysicalMemory::Allocate();

					if (page == nullptr) {
						return Failure();
					}

					Paging::SetPML4EInfo(pml4e, {
						.present = 1,
						.readWrite = 1,
						.userMode = privilege != AccessPrivilege::HIGH,
						.address = PhysicalMemory::FilterAddress(page)
					});

					const auto pdpt = Paging::GetPDPTAddress(mapping, usePrimary);
					Paging::InvalidatePage(pdpt);
					ShdMem::ZeroPage(pdpt);
				}

				const auto pdpte = Paging::GetPDPTEAddress(mapping, usePrimary);
				const auto pdpte_info = Paging::GetPDPTEInfo(pdpte);

				if (!pdpte_info.present) {
					void* page = PhysicalMemory::Allocate();

					if (page == nullptr) {
						return Failure();
					}

					Paging::SetPDPTEInfo(pdpte, {
						.present = 1,
						.readWrite = 1,
						.userMode = privilege != AccessPrivilege::HIGH,
						.address = PhysicalMemory::FilterAddress(page)
					});

					const auto pd = Paging::GetPDAddress(mapping, usePrimary);
					Paging::InvalidatePage(pd);
					ShdMem::ZeroPage(pd);
				}

				const auto pde = Paging::GetPDEAddress(mapping, usePrimary);
				const auto pde_info = Paging::GetPDEInfo(pde);

				if (!pde_info.present) {
					void* page = PhysicalMemory::Allocate();

					if (page == nullptr) {
						return Failure();
					}

					Paging::SetPDEInfo(pde, {
						.present = 1,
						.readWrite = 1,
						.userMode = privilege != AccessPrivilege::HIGH,
						.address = PhysicalMemory::FilterAddress(page)
					});

					const auto pt = Paging::GetPTAddress(mapping, usePrimary);
					Paging::InvalidatePage(pt);
					ShdMem::ZeroPage(pt);
				}

				const auto pte = Paging::GetPTEAddress(mapping, usePrimary);

				*pte = NP_ON_DEMAND
					| (privilege == AccessPrivilege::LOW ? NP_USERMODE : 0)
					| NP_READWRITE;

				Paging::InvalidatePage(reinterpret_cast<const void*>(address));

				address += ShdMem::FRAME_SIZE;
			}

			return Success();
		}

        static inline Success ExpandVirtualMemoryMap(uint64_t start, uint64_t blocks) {
			void* page = PhysicalMemory::Allocate();
			if (page == nullptr) {
				return Failure();
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

			if (!MapOnDemand(pagesStart, pages, AccessPrivilege::LOW).IsSuccess()) {
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
								if (!MapOnDemand(hintPtr, pages, privilege).IsSuccess()) {
									return nullptr;
								}

								*vmmb = prevBlock;
								SortVirtualMemoryMap(vmmb, ctx->storedBlocks - index);
								ctx->availableMemory -= pages * ShdMem::FRAME_SIZE;
								
								return hintPtr;
							}
							else {
								if (ctx->availableBlockMemory == 0) {
									if (!ExpandVirtualMemoryMap(managementBase, ctx->storedBlocks).IsSuccess()) {
										return nullptr;
									}
									ctx->availableBlockMemory += ShdMem::FRAME_SIZE;
								}

								if (!MapOnDemand(hintPtr, pages, privilege).IsSuccess()) {
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

			if (!MapOnDemand(pagesStart, pages, privilege).IsSuccess()) {
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
					const auto blockMemoryAddress = ShdMem::ParseVirtualAddress(linearAddress);

					const auto blockMemoryPTE = Paging::GetPTEAddress(blockMemoryAddress);

					PhysicalMemory::Free(reinterpret_cast<void*>(Paging::GetPTEInfo(blockMemoryPTE).address));
					Paging::UnmapPTE(blockMemoryPTE);
					Paging::InvalidatePage(reinterpret_cast<void*>(linearAddress));
				}
			}

			ctx->availableMemory -= pages * ShdMem::FRAME_SIZE;
			return pagesStart;
		}

		template<AccessPrivilege privilege> static inline Success FreeCore(void* ptr, uint64_t pages) {
			constexpr uint64_t managementBase = privilege == AccessPrivilege::HIGH
				? VirtualMemoryLayout::KernelHeapManagement.start
				: (VirtualMemoryLayout::UserVMemManagement.start
					+ (sizeof(*userContext) + sizeof(VMemMapBlock) - 1) / sizeof(VMemMapBlock));

			MemoryContext* ctx = privilege == AccessPrivilege::HIGH ? &kernelContext : userContext;

			if (pages == 0) {
				return Success();
			}
			
			uint64_t address = reinterpret_cast<uint64_t>(ptr);

			if constexpr (privilege == AccessPrivilege::LOW) {
				if (address > VirtualMemoryLayout::UserMemory.start + VirtualMemoryLayout::UserMemory.limit
					|| address < VirtualMemoryLayout::UserMemory.start
					|| address + pages * ShdMem::FRAME_SIZE < address
				) {
					return Failure();
				}
			}

			for (size_t i = 0; i < pages; ++i, address += ShdMem::FRAME_SIZE) {
				const auto mapping = ShdMem::ParseVirtualAddress(address);

				const auto pml4e = Paging::GetPML4EAddress(mapping);
				const auto pdpte = Paging::GetPDPTEAddress(mapping);
				const auto pde 	 = Paging::GetPDEAddress(mapping);
				const auto pte 	 = Paging::GetPTEAddress(mapping);

				const auto pml4e_info = Paging::GetPML4EInfo(pml4e);
				const auto pdpte_info = Paging::GetPDPTEInfo(pdpte);
				const auto pde_info   = Paging::GetPDEInfo(pde);
				const auto pte_info   = Paging::GetPTEInfo(pte);

				if constexpr (privilege == AccessPrivilege::LOW) {
					if (!pml4e_info.present || !pdpte_info.present || !pde_info.present || *pte == 0) {
						return Failure();
					}
				}

				if (pde_info.present) {
					if (pde_info.pageSize) {
						constexpr uint64_t pagesPerHugePage = ShdMem::PDE_COVERAGE / ShdMem::FRAME_SIZE;

						void* const pageAddress = reinterpret_cast<void*>(pde_info.address);

						if (!PhysicalMemory::Free2MB(pageAddress).IsSuccess()) {
							return Failure();
						}

						Paging::UnmapPDE(pde);
						Paging::InvalidatePage(reinterpret_cast<void*>(address));

						i += pagesPerHugePage - 1;
						address += (pagesPerHugePage - 1) * ShdMem::FRAME_SIZE;
					}
					else {
						if (pte_info.present) {
							void* pageAddress = reinterpret_cast<void*>(pte_info.address);

							if (!PhysicalMemory::Free(pageAddress).IsSuccess()) {
								return Failure();
							}

							Paging::UnmapPTE(pte);
							Paging::InvalidatePage(reinterpret_cast<void*>(address));
						}
						else if ((*pte & VirtualMemory::NP_ON_DEMAND) != 0) {
							/// TODO: do stuff with the swap file
						}
						
					}
				}
			}

			VMemMapBlock vmmb = {
				.virtualStart = reinterpret_cast<uint64_t>(ptr),
				.availablePages = pages
			};

			if (ctx->availableBlockMemory == 0) {
				auto status = ExpandVirtualMemoryMap(managementBase, ctx->storedBlocks);

				if (!status.IsSuccess()) {
					return Failure();
				}

				ctx->availableBlockMemory += ShdMem::FRAME_SIZE;
			}

			VMemMapBlock* blockPtr = reinterpret_cast<VMemMapBlock*>(managementBase) + ctx->storedBlocks++;
			*blockPtr = vmmb;
			ctx->availableBlockMemory -= sizeof(VMemMapBlock);
			ctx->availableMemory += pages * ShdMem::FRAME_SIZE;
			RSortVirtualMemoryMap(blockPtr, ctx->storedBlocks);

			return Success();
		}
    }

	Success Setup() {
		// make the NULL memory page reserved and unusable, and allocate the DMA PML4E and PDPTE
		if (PhysicalMemory::QueryDMAAddress(0) == PhysicalMemory::StatusCode::FREE) {
			if (VirtualMemory::AllocateDMA(1) == nullptr) {
				return Failure();
			}
		}
		else {
			MapPage(0, 0, AccessPrivilege::MEDIUM);
		}

		static constexpr auto nullMapping = ShdMem::ParseVirtualAddress(static_cast<uint64_t>(0));

		Paging::UnmapPTE(Paging::GetPTEAddress(nullMapping));
		
		// set up identity paging for the DMA zone, and allocate all necessary pages table it needs
		// even if they are not mapping to anything yet

		static constexpr auto dmaStart = ShdMem::Layout::DMAZone.start;
		static constexpr auto dmaEnd = ShdMem::Layout::DMAZone.start + ShdMem::Layout::DMAZone.limit;

		for (size_t i = dmaStart; i < dmaEnd; i += ShdMem::FRAME_SIZE) {
			if (PhysicalMemory::QueryDMAAddress(i) == PhysicalMemory::StatusCode::ALLOCATED) {
				MapPage(i, i, AccessPrivilege::MEDIUM);
			}
			else {
				const auto mapping = ShdMem::ParseVirtualAddress(i);

				const auto pdpte 		= Paging::GetPDPTEAddress(mapping);
				const auto pdpte_info 	= Paging::GetPDPTEInfo(pdpte);

				const auto pd 			= Paging::GetPDAddress(mapping);
				const auto pde 			= Paging::GetPDEAddress(mapping);

				const auto pt 			= Paging::GetPTAddress(mapping);

				if (!pdpte_info.present) {
					void* page = PhysicalMemory::Allocate();
					
					if (page == nullptr) {
						return Failure();
					}

					Paging::SetPDPTEInfo(pdpte, {
						.present = 1,
						.readWrite = 1,
						.userMode = 1,
						.address = PhysicalMemory::FilterAddress(page)
					});

					Paging::InvalidatePage(pd);
					ShdMem::ZeroPage(pd);
				}

				const auto pde_info = Paging::GetPDEInfo(pde);

				if (!pde_info.present) {
					void* page = PhysicalMemory::Allocate();

					if (page == nullptr) {
						return Failure();
					}
					
					Paging::SetPDEInfo(pde, {
						.present = 1,
						.readWrite = 1,
						.userMode = 1,
						.address = PhysicalMemory::FilterAddress(page)
					});

					Paging::InvalidatePage(pt);
					ShdMem::ZeroPage(pt);
				}
			}
		}

		// set up kernel heap
		void* basePage = PhysicalMemory::Allocate();
		if (basePage == nullptr) {
			return Failure();
		}

		if (!MapPage(
			reinterpret_cast<uint64_t>(basePage),
			VirtualMemoryLayout::KernelHeapManagement.start,
			AccessPrivilege::HIGH
		).IsSuccess()) {
			return Failure();
		}

		kernelContext.availableBlockMemory = ShdMem::FRAME_SIZE - sizeof(VMemMapBlock);
		kernelContext.storedBlocks = 1;

		VMemMapBlock* kernelHeapBlockPtr = reinterpret_cast<VMemMapBlock*>(VirtualMemoryLayout::KernelHeapManagement.start);
		kernelHeapBlockPtr->virtualStart = VirtualMemoryLayout::KernelHeap.start;
		kernelHeapBlockPtr->availablePages = kernelContext.availableMemory / ShdMem::FRAME_SIZE;
		
		return Success();
	}

	void* DeriveNewFreshCR3() {
		void* const CR3 = PhysicalMemory::Allocate();

		if (CR3 == nullptr) {
			return nullptr;
		}
		
		if (!Paging::CreateSecondaryRecursiveMapping(CR3).IsSuccess()) {
			PhysicalMemory::Free(CR3);
			return nullptr;
		}

		/// FIXME: starting from here, a failure will cause a memory leak
		/// How to fix: create a method to completely free an entire page table

		// setup kernel stack and kernel stack guard
		if (!MapOnDemand<false>(
			reinterpret_cast<void*>(VirtualMemoryLayout::KernelStack.start),
			(VirtualMemoryLayout::KernelStack.limit - ShdMem::PAGE_SIZE) / ShdMem::PAGE_SIZE,
			AccessPrivilege::HIGH
		).IsSuccess()) {
			Paging::FreeSecondaryRecursiveMapping();
			return nullptr;
		}

		const auto stack_guard_address = reinterpret_cast<void*>(VirtualMemoryLayout::KernelStackGuard.start);
		const auto stack_guard_mapping = ShdMem::ParseVirtualAddress(stack_guard_address);
		const auto stack_guard_pte = Paging::GetPTEAddress(stack_guard_mapping, false);
		Paging::UnmapPTE(stack_guard_pte);
		Paging::InvalidatePage(stack_guard_address);

		void* stack_top = PhysicalMemory::Allocate();

		if (stack_top == nullptr) {
			Paging::FreeSecondaryRecursiveMapping();
			return nullptr;
		}

		void* stack_reserve = PhysicalMemory::Allocate();

		if (stack_reserve == nullptr) {
			PhysicalMemory::Free(stack_top);
			Paging::FreeSecondaryRecursiveMapping();
			return nullptr;
		}

		if (!MapPage<false>(
			reinterpret_cast<uint64_t>(stack_top),
			VirtualMemoryLayout::KernelStackReserve.start - ShdMem::PAGE_SIZE,
			AccessPrivilege::HIGH
		).IsSuccess()) {
			PhysicalMemory::Free(stack_reserve);
			PhysicalMemory::Free(stack_top);
			Paging::FreeSecondaryRecursiveMapping();
			return nullptr;
		}

		if (!MapPage<false>(
			reinterpret_cast<uint64_t>(stack_reserve),
			VirtualMemoryLayout::KernelStackReserve.start,
			AccessPrivilege::HIGH
		).IsSuccess()) {
			PhysicalMemory::Free(stack_reserve);
			Paging::FreeSecondaryRecursiveMapping();
			return nullptr;
		}

		// set up user memory management structures
		void* base_page = PhysicalMemory::Allocate();
		
		if (base_page == nullptr) {
			Paging::FreeSecondaryRecursiveMapping();
			return nullptr;
		}

		if (!MapPage<false>(
			reinterpret_cast<uint64_t>(base_page),
			reinterpret_cast<uint64_t>(userContext),
			AccessPrivilege::HIGH
		).IsSuccess()) {
			PhysicalMemory::Free(base_page);
			Paging::FreeSecondaryRecursiveMapping();
			return nullptr;
		}

		void* vbase_page = MapGeneralPages(base_page, 1, ShdMem::PTE_PRESENT | ShdMem::PTE_READWRITE);

		if (vbase_page == nullptr) {
			Paging::FreeSecondaryRecursiveMapping();
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
			if (!MapPage(address, address, AccessPrivilege::MEDIUM).IsSuccess()) {
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

	Success FreeDMA(void* ptr, uint64_t pages) {
		if (!PhysicalMemory::FreeDMA(ptr, pages).IsSuccess()) {
			return Failure();
		}

		uint64_t address = reinterpret_cast<uint64_t>(ptr);

		for (size_t i = 0; i < pages; ++i, address += ShdMem::FRAME_SIZE) {
			const auto mapping = ShdMem::ParseVirtualAddress(address);
			const auto pte = Paging::GetPTEAddress(mapping);
			Paging::UnmapPTE(pte);
			Paging::InvalidatePage(reinterpret_cast<void*>(address));
		}

		return Success();
	}

	Success FreeKernelHeap(void* ptr, uint64_t pages) {
		return FreeCore<AccessPrivilege::HIGH>(ptr, pages);
	}

	Success FreeUserPages(void* ptr, uint64_t pages) {
		return FreeCore<AccessPrivilege::LOW>(ptr, pages);
	}

	Success ChangeMappingFlags(void* _ptr, uint64_t flags, uint64_t pages) {		
		uint64_t ptr = reinterpret_cast<uint64_t>(_ptr);

		for (size_t i = 0; i < pages; ++i, ptr += ShdMem::PTE_COVERAGE) {
			const ShdMem::VirtualAddress mapping = ShdMem::ParseVirtualAddress(ptr);
			
			const auto pml4e = Paging::GetPML4EAddress(mapping);
			const auto pdpte = Paging::GetPDPTEAddress(mapping);
			const auto pde = Paging::GetPDEAddress(mapping);
			const auto pte = Paging::GetPTEAddress(mapping);

			const auto pml4e_info = Paging::GetPML4EInfo(pml4e);
			const auto pdpte_info = Paging::GetPDPTEInfo(pdpte);
			const auto pde_info = Paging::GetPDEInfo(pde);

			if (!pml4e_info.present || !pdpte_info.present || !pde_info.present) {
				return Failure();
			}
			else if (pde_info.pageSize) {
				*pde = (*pde & (ShdMem::PDE_ADDRESS)) | (flags & ~ShdMem::PDE_ADDRESS);
			}
			else {			
				*pte = (*pte & (ShdMem::PTE_ADDRESS)) | (flags & ~ShdMem::PTE_ADDRESS);
			}

			Paging::InvalidatePage(reinterpret_cast<void*>(ptr));
		}

		return Success();
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

			const auto pdpte = Paging::GetPDPTEAddress(mapping);
			const auto pdpte_info = Paging::GetPDPTEInfo(pdpte);

			if (!pdpte_info.present) {
				void* page = PhysicalMemory::Allocate();

				if (page == nullptr) {
					return nullptr;
				}

				*pdpte = (PhysicalMemory::FilterAddress(page) & ShdMem::PDPTE_ADDRESS)
					| ShdMem::PDPTE_READWRITE
					| ShdMem::PDPTE_PRESENT;

				const auto pd = Paging::GetPDAddress(mapping);
				ShdMem::ZeroPage(pd);
			}

			const auto pde = Paging::GetPDEAddress(mapping);
			const auto pde_info = Paging::GetPDEInfo(pde);

			if (!pde_info.present) {
				void* page = PhysicalMemory::Allocate();
				
				if (page == nullptr) {
					return nullptr;
				}

				*pde = (PhysicalMemory::FilterAddress(page) & ShdMem::PDE_ADDRESS)
					| ShdMem::PDE_READWRITE
					| ShdMem::PDE_PRESENT;

				const auto pt = Paging::GetPTAddress(mapping);
				ShdMem::ZeroPage(pt);
			}

			auto pte = Paging::GetPTEAddress(mapping);
			const auto pte_info = Paging::GetPTEInfo(pte);

			if (!pte_info.present) {
				if (found == 0) {
					start = address;
				}

				if (++found == pages) {
					address = start;

					uint64_t physical_address = reinterpret_cast<uint64_t>(pageAddress);

					for (size_t j = 0; j < pages; ++j, address += ShdMem::PAGE_SIZE, physical_address += ShdMem::PAGE_SIZE) {
						mapping = ShdMem::ParseVirtualAddress(address);
						pte = Paging::GetPTEAddress(mapping);

						*pte = (PhysicalMemory::FilterAddress(physical_address) & ShdMem::PTE_ADDRESS)
							| flags
							| ShdMem::PTE_PRESENT;

						Paging::InvalidatePage(reinterpret_cast<void*>(address));
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

	Success UnmapGeneralPages(void* vpage, size_t pages) {
		if (pages == 0) {
			return Success();
		}

		uint64_t address = reinterpret_cast<uint64_t>(vpage);

		for (size_t i = 0; i < pages; ++i, address += ShdMem::PAGE_SIZE) {
			auto mapping = ShdMem::ParseVirtualAddress(address);

			const auto pml4e	= Paging::GetPML4EAddress(mapping);
			const auto pdpte 	= Paging::GetPDPTEAddress(mapping);
			const auto pde 		= Paging::GetPDEAddress(mapping);
			const auto pte 		= Paging::GetPTEAddress(mapping);

			const auto pml4e_info 	= Paging::GetPML4EInfo(pml4e);
			const auto pdpte_info 	= Paging::GetPDPTEInfo(pdpte);
			const auto pde_info 	= Paging::GetPDEInfo(pde);

			if (!pml4e_info.present || !pdpte_info.present || !pde_info.present) {
				return Failure();
			}

			Paging::UnmapPTE(pte);
			Paging::InvalidatePage(reinterpret_cast<void*>(address));
		}

		return Success();
	}
}
