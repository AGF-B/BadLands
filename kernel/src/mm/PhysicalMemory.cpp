#include <cpuid.h>
#include <cstdint>

#include <shared/efi/efi.h>
#include <shared/memory/defs.hpp>
#include <shared/memory/layout.hpp>

#include <mm/PhysicalMemory.hpp>
#include <mm/VirtualMemory.hpp>
#include <mm/VirtualMemoryLayout.hpp>

namespace ShdMem = Shared::Memory;
namespace VML = ShdMem::Layout;

struct MemMapBlock {
	uint64_t physicalAddress;
	uint64_t availablePages;
};

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
}

namespace {
	static uint64_t availableBlockMemory = 0;
	static uint64_t availableMemory = 0;
	static uint64_t storedBlocks = 0;

	static uint8_t* DMA_bitmap = nullptr;

	static constexpr uint64_t DMA_PAGES			= (VML::DMAZone.limit / ShdMem::FRAME_SIZE);
	static constexpr uint64_t DMA_BITMAP_SIZE	= (DMA_PAGES / 8);
}

uint64_t PhysicalMemory::FilterAddress(uint64_t address) {
	return _filter_address_handler(address);
}

uint64_t PhysicalMemory::FilterAddress(void* address) {
	return _filter_address_handler(reinterpret_cast<uint64_t>(address));
}

PhysicalMemory::StatusCode PhysicalMemory::Setup() {
	const uint64_t mmapSize = *reinterpret_cast<uint64_t*>(VML::OsLoaderData.start + VML::OsLoaderDataOffsets.MmapSize);
	const uint64_t descriptorSize = *reinterpret_cast<uint64_t*>(VML::OsLoaderData.start + VML::OsLoaderDataOffsets.MmapDescSize);
	uint8_t* const mmap = reinterpret_cast<uint8_t*>(VML::OsLoaderData.start + VML::OsLoaderDataOffsets.Mmap);
	const size_t descriptorCount = mmapSize / descriptorSize;

	DMA_bitmap = reinterpret_cast<uint8_t*>(VML::OsLoaderData.start + VML::OsLoaderDataOffsets.DMABitMap);

	MemMapBlock* currentBlockPtr = reinterpret_cast<MemMapBlock*>(VirtualMemoryLayout::PhysicalMemoryMap.start);

	for (size_t i = 0; i < descriptorCount; ++i) {
		EFI_MEMORY_DESCRIPTOR* descriptor = reinterpret_cast<EFI_MEMORY_DESCRIPTOR*>(mmap + i * descriptorSize);

		if (descriptor->Type == EfiConventionalMemory || descriptor->Type == EfiLoaderCode || descriptor->Type == EfiLoaderData
			|| descriptor->Type == EfiBootServicesCode || descriptor->Type == EfiBootServicesData
		) {
			if (descriptor->PhysicalStart < VML::DMAZone.limit) {
				int64_t endDMAOffset =
                    descriptor->PhysicalStart
                    + ShdMem::FRAME_SIZE * descriptor->NumberOfPages
                    - VML::DMAZone.limit;

				if (endDMAOffset <= 0) {
					continue;
				}

				descriptor->NumberOfPages = endDMAOffset / ShdMem::FRAME_SIZE;
				descriptor->PhysicalStart = VML::DMAZone.limit;
			}

			if (descriptor->NumberOfPages == 0) {
				continue;
			}
			else if (availableBlockMemory == 0) {
				uint64_t pageAddress = descriptor->PhysicalStart + ShdMem::FRAME_SIZE * (--descriptor->NumberOfPages);

				ShdMem::VirtualAddress mapping = ShdMem::ParseVirtualAddress(currentBlockPtr);

				ShdMem::PML4E* pml4e = VirtualMemory::GetPML4EAddress(mapping.PML4_offset);

				if ((*pml4e & ShdMem::PML4E_PRESENT) == 0) {
					// use the last page and try again
					*pml4e = (FilterAddress(pageAddress) & ShdMem::PML4E_ADDRESS)
						| ShdMem::PML4E_READWRITE
						| ShdMem::PML4E_PRESENT;

					ShdMem::PDPTE* pdpt = VirtualMemory::GetPDPTAddress(mapping.PML4_offset);
					ShdMem::ZeroPage(pdpt);

					if (descriptor->NumberOfPages == 0) {
						continue;
					}
					pageAddress = descriptor->PhysicalStart + ShdMem::FRAME_SIZE * (--descriptor->NumberOfPages);
				}

				ShdMem::PDPTE* pdpte = VirtualMemory::GetPDPTEAddress(mapping.PML4_offset, mapping.PDPT_offset);

				if ((*pdpte & ShdMem::PDPTE_PRESENT) == 0) {
					*pdpte = (FilterAddress(pageAddress) & ShdMem::PDPTE_ADDRESS)
						| ShdMem::PDPTE_READWRITE
						| ShdMem::PDPTE_PRESENT;

					ShdMem::PDE* pd = VirtualMemory::GetPDAddress(mapping.PML4_offset, mapping.PDPT_offset);
					ShdMem::ZeroPage(pd);

					if (descriptor->NumberOfPages == 0) {
						continue;
					}
					pageAddress = descriptor->PhysicalStart + ShdMem::FRAME_SIZE * (--descriptor->NumberOfPages);
				}

				ShdMem::PDE* pde = VirtualMemory::GetPDEAddress(mapping.PML4_offset, mapping.PDPT_offset, mapping.PD_offset);

				if ((*pde & ShdMem::PDE_PRESENT) == 0) {
					*pde = (FilterAddress(pageAddress) & ShdMem::PDE_ADDRESS)
						| ShdMem::PDE_READWRITE
						| ShdMem::PDE_PRESENT;
					
					ShdMem::PTE* pt = VirtualMemory::GetPTAddress(mapping.PML4_offset, mapping.PDPT_offset, mapping.PD_offset);
					ShdMem::ZeroPage(pt);

					if (descriptor->NumberOfPages == 0) {
						continue;
					}
					pageAddress = descriptor->PhysicalStart + ShdMem::FRAME_SIZE * (--descriptor->NumberOfPages);
				}

				ShdMem::PTE* pte = VirtualMemory::GetPTEAddress(
                    mapping.PML4_offset,
                    mapping.PDPT_offset,
                    mapping.PD_offset,
                    mapping.PT_offset
                );
				*pte = ShdMem::PTE_XD
					| (FilterAddress(pageAddress) & ShdMem::PTE_ADDRESS)
					| ShdMem::PTE_READWRITE
					| ShdMem::PTE_PRESENT;

				if (descriptor->NumberOfPages == 0) {
					continue;
				}

				availableBlockMemory += ShdMem::FRAME_SIZE;
			}

			currentBlockPtr->physicalAddress = descriptor->PhysicalStart;
			currentBlockPtr->availablePages = descriptor->NumberOfPages;
			++currentBlockPtr;
			++storedBlocks;
			availableMemory += ShdMem::FRAME_SIZE * descriptor->NumberOfPages;
			availableBlockMemory -= sizeof(MemMapBlock);
		}
	}

	if (currentBlockPtr == reinterpret_cast<MemMapBlock*>(VirtualMemoryLayout::PhysicalMemoryMap.start)) {
		// not enough memory to setup the Physical Memory Manager (PMM)
		return StatusCode::OUT_OF_MEMORY;
	}

	DMA_bitmap[0] |= 1; // reserve the first DMA page to make NULL pointers invalid.

	return StatusCode::SUCCESS;
}

uint64_t PhysicalMemory::QueryMemoryUsage() {
	return availableMemory;
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
	if (storedBlocks == 0 || availableMemory < ShdMem::FRAME_SIZE) {
		return nullptr;
	}

	MemMapBlock* mmb = reinterpret_cast<MemMapBlock*>(VirtualMemoryLayout::PhysicalMemoryMap.start) + storedBlocks - 1;

	void* page = reinterpret_cast<void*>(mmb->physicalAddress + ShdMem::FRAME_SIZE * (--mmb->availablePages));
	
	while (mmb->availablePages == 0) {
		--storedBlocks;
		availableBlockMemory += sizeof(MemMapBlock);
		--mmb;
	}
	availableMemory -= ShdMem::FRAME_SIZE;

	return page;
}

void* PhysicalMemory::AllocatePages(uint64_t pages) {
	if (pages == 0 || storedBlocks == 0 || availableMemory < ShdMem::FRAME_SIZE * pages) {
		return nullptr;
	}

	MemMapBlock* const mmb_end = reinterpret_cast<MemMapBlock*>(VirtualMemoryLayout::PhysicalMemoryMap.start) + storedBlocks - 1;
	MemMapBlock* mmb = mmb_end;

	for (size_t i = 0; i < storedBlocks; ++i) {
		if (mmb->availablePages >= pages) {
			break;
		}
		--mmb;
	}

	mmb->availablePages -= pages;

	void* page = reinterpret_cast<void*>(mmb->physicalAddress + ShdMem::FRAME_SIZE * (mmb->availablePages));

	availableMemory -= ShdMem::FRAME_SIZE * pages;

	if (mmb == mmb_end) {
		while (mmb->availablePages == 0) {
			--storedBlocks;
			availableBlockMemory += sizeof(MemMapBlock);
			--mmb;
		}
	}

	return page;
}

PhysicalMemory::StatusCode PhysicalMemory::FreeDMA(void* ptr, uint64_t pages) {
	const uint64_t address = reinterpret_cast<uint64_t>(ptr);

	if (address % ShdMem::FRAME_SIZE != 0 || address >= VML::DMAZone.limit
		|| address + ShdMem::FRAME_SIZE * pages > VML::DMAZone.limit || pages > DMA_PAGES
	) {
		return StatusCode::INVALID_PARAMETER;
	}

	const uint64_t firstPage = address / ShdMem::FRAME_SIZE;

	for (size_t p = firstPage; p < firstPage + pages; ++p) {
		size_t x = p / 8;
		size_t y = p % 8;

		DMA_bitmap[x] &= ~(1 << y);
	}

	return StatusCode::SUCCESS;
}

PhysicalMemory::StatusCode PhysicalMemory::Free(void* ptr) {
	const uint64_t address = reinterpret_cast<uint64_t>(ptr);

	MemMapBlock* blockPtr = reinterpret_cast<MemMapBlock*>(VirtualMemoryLayout::PhysicalMemoryMap.start) + storedBlocks;

	if (availableBlockMemory == 0) {
		if (storedBlocks * sizeof(MemMapBlock) >= VirtualMemoryLayout::PhysicalMemoryMap.limit) {
			return StatusCode::OUT_OF_MEMORY;
		}

		ShdMem::VirtualAddress mapping = ShdMem::ParseVirtualAddress(blockPtr);

		ShdMem::PML4E* pml4e = VirtualMemory::GetPML4EAddress(mapping.PML4_offset);

		if ((*pml4e & ShdMem::PML4E_PRESENT) == 0) {
			void* _page = Allocate();

			if (_page == nullptr) {
				return StatusCode::OUT_OF_MEMORY;
			}

			*pml4e = (FilterAddress(_page) & ShdMem::PML4E_ADDRESS)
				| ShdMem::PML4E_READWRITE
				| ShdMem::PML4E_PRESENT;

			ShdMem::PDPTE* pdpt = VirtualMemory::GetPDPTAddress(mapping.PML4_offset);
			ShdMem::ZeroPage(pdpt);
		}

		ShdMem::PDPTE* pdpte = VirtualMemory::GetPDPTEAddress(mapping.PML4_offset, mapping.PDPT_offset);

		if ((*pdpte & ShdMem::PDPTE_PRESENT) == 0) {
			void* _page = Allocate();

			if (_page == nullptr) {
				return StatusCode::OUT_OF_MEMORY;
			}

			*pdpte = (FilterAddress(_page) & ShdMem::PDPTE_ADDRESS)
				| ShdMem::PDPTE_READWRITE
				| ShdMem::PDPTE_PRESENT;

			ShdMem::PDE* pd = VirtualMemory::GetPDAddress(mapping.PML4_offset, mapping.PDPT_offset);
			ShdMem::ZeroPage(pd);
		}

		ShdMem::PDE* pde = VirtualMemory::GetPDEAddress(mapping.PML4_offset, mapping.PDPT_offset, mapping.PDPT_offset);

		if ((*pde & ShdMem::PDE_PRESENT) == 0) {
			void* _page = Allocate();

			if (_page == nullptr) {
				return StatusCode::OUT_OF_MEMORY;
			}

			*pde = (FilterAddress(_page) & ShdMem::PDE_ADDRESS)
				| ShdMem::PDE_READWRITE
				| ShdMem::PDE_PRESENT;

			ShdMem::PTE* pt = VirtualMemory::GetPTAddress(mapping.PML4_offset, mapping.PDPT_offset, mapping.PD_offset);
			ShdMem::ZeroPage(pt);
		}

		ShdMem::PTE* pte = VirtualMemory::GetPTEAddress(mapping.PML4_offset, mapping.PDPT_offset, mapping.PD_offset, mapping.PT_offset);
		void* _page = Allocate();
		availableBlockMemory += ShdMem::FRAME_SIZE;

		if (_page == nullptr) {
			*pte = ShdMem::PTE_XD
				| (FilterAddress(ptr) & ShdMem::PTE_ADDRESS)
				| ShdMem::PTE_READWRITE
				| ShdMem::PTE_PRESENT;
			return StatusCode::SUCCESS;
		}

		*pte = ShdMem::PTE_XD
			| (FilterAddress(_page) & ShdMem::PTE_ADDRESS)
			| ShdMem::PTE_READWRITE
			| ShdMem::PTE_PRESENT;
	}

	blockPtr->availablePages = 1;
	blockPtr->physicalAddress = address;
	++storedBlocks;
	availableBlockMemory -= sizeof(MemMapBlock);
	availableMemory += ShdMem::FRAME_SIZE;
	return StatusCode::SUCCESS;
}

PhysicalMemory::StatusCode PhysicalMemory::FreePages(void* ptr, uint64_t pages) {
	if (pages == 0) {
		return StatusCode::INVALID_PARAMETER;
	}

	const uint64_t address = reinterpret_cast<uint64_t>(ptr);

	MemMapBlock* blockPtr = reinterpret_cast<MemMapBlock*>(VirtualMemoryLayout::PhysicalMemoryMap.start) + storedBlocks;

	if (availableBlockMemory == 0) {
		if (storedBlocks * sizeof(MemMapBlock) >= VirtualMemoryLayout::PhysicalMemoryMap.limit) {
			return StatusCode::OUT_OF_MEMORY;
		}

		ShdMem::VirtualAddress mapping = ShdMem::ParseVirtualAddress(blockPtr);

		ShdMem::PML4E* pml4e = VirtualMemory::GetPML4EAddress(mapping.PML4_offset);

		if ((*pml4e & ShdMem::PML4E_PRESENT) == 0) {
			void* _page = Allocate();

			if (_page == nullptr) {
				return StatusCode::OUT_OF_MEMORY;
			}

			*pml4e = (FilterAddress(_page) & ShdMem::PML4E_ADDRESS)
				| ShdMem::PML4E_READWRITE
				| ShdMem::PML4E_PRESENT;

			ShdMem::PDPTE* pdpt = VirtualMemory::GetPDPTAddress(mapping.PML4_offset);
			ShdMem::ZeroPage(pdpt);
		}

		ShdMem::PDPTE* pdpte = VirtualMemory::GetPDPTEAddress(mapping.PML4_offset, mapping.PDPT_offset);

		if ((*pdpte & ShdMem::PDPTE_PRESENT) == 0) {
			void* _page = Allocate();

			if (_page == nullptr) {
				return StatusCode::OUT_OF_MEMORY;
			}

			*pdpte = (FilterAddress(_page) & ShdMem::PDPTE_ADDRESS)
				| ShdMem::PDPTE_READWRITE
				| ShdMem::PDPTE_PRESENT;

			ShdMem::PDE* pd = VirtualMemory::GetPDAddress(mapping.PML4_offset, mapping.PDPT_offset);
			ShdMem::ZeroPage(pd);
		}

		ShdMem::PDE* pde = VirtualMemory::GetPDEAddress(mapping.PML4_offset, mapping.PDPT_offset, mapping.PDPT_offset);

		if ((*pde & ShdMem::PDE_PRESENT) == 0) {
			void* _page = Allocate();

			if (_page == nullptr) {
				return StatusCode::OUT_OF_MEMORY;
			}

			*pde = (FilterAddress(_page) & ShdMem::PDE_ADDRESS)
				| ShdMem::PDE_READWRITE
				| ShdMem::PDE_PRESENT;

			ShdMem::PTE* pt = VirtualMemory::GetPTAddress(mapping.PML4_offset, mapping.PDPT_offset, mapping.PD_offset);
			ShdMem::ZeroPage(pt);
		}

		ShdMem::PTE* pte = VirtualMemory::GetPTEAddress(mapping.PML4_offset, mapping.PDPT_offset, mapping.PD_offset, mapping.PT_offset);
		void* _page = Allocate();
		availableBlockMemory += ShdMem::FRAME_SIZE;

		if (_page == nullptr) {
			*pte = ShdMem::PTE_XD
				| (FilterAddress(ptr) & ShdMem::PTE_ADDRESS)
				| ShdMem::PTE_READWRITE
				| ShdMem::PTE_PRESENT;
			--pages;
		}

		*pte = ShdMem::PTE_XD
			| (FilterAddress(_page) & ShdMem::PTE_ADDRESS)
			| ShdMem::PTE_READWRITE
			| ShdMem::PTE_PRESENT;
	}

	blockPtr->availablePages = pages;
	blockPtr->physicalAddress = address;
	++storedBlocks;
	availableBlockMemory -= sizeof(MemMapBlock);
	availableMemory += ShdMem::FRAME_SIZE * pages;
	return StatusCode::SUCCESS;
}
