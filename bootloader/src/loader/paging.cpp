#include <cstdint>

#include <efi/efi_misc.hpp>

#include <loader/paging.hpp>

#include <shared/efi/efi.h>
#include <shared/graphics/basic.hpp>
#include <shared/memory/defs.hpp>
#include <shared/memory/layout.hpp>

#include <ldstdio.hpp>
#include <ldstdlib.hpp>

namespace ShdMem = Shared::Memory;

namespace {
    struct SimpleMMAP {
        size_t size;
        size_t desc_size;
        EFI_MEMORY_DESCRIPTOR* mmap;
    };

    static inline constexpr uint64_t FilterAddress(uint64_t address, const PagingInformation& PI) {
        return (address & (((uint64_t)1 << PI.MAXPHYADDR) - 1)) & ~0xFFF;
    }

    static inline constexpr PML4E MakePML4E(uint64_t address, const PagingInformation& PI) {
        return FilterAddress(address, PI) | ShdMem::PML4E_READWRITE | ShdMem::PML4E_PRESENT;
    }

    static inline constexpr PDPTE MakePDPTE(uint64_t address, const PagingInformation& PI) {
        return FilterAddress(address, PI) | ShdMem::PDPTE_READWRITE | ShdMem::PDPTE_PRESENT;
    }

    static inline constexpr PDE MakePDE(uint64_t address, const PagingInformation& PI) {
        return FilterAddress(address, PI) | ShdMem::PDE_READWRITE | ShdMem::PDE_PRESENT;
    }

    static inline constexpr PTE MakePTE(uint64_t address, const PagingInformation& PI, bool XD) {
        return (XD ? ShdMem::PTE_XD : 0)
            | FilterAddress(address, PI)
            | ShdMem::PTE_READWRITE
            | ShdMem::PTE_PRESENT;
    }

    static inline SimpleMMAP GetSimpleMMAP(void) {
        SimpleMMAP smmap = {
            .size = 0,
            .desc_size = 0,
            .mmap = nullptr
        };

        UINTN mmap_key;
        UINT32 desc_ver;

        EFI::sys->BootServices->GetMemoryMap(&smmap.size, smmap.mmap, &mmap_key, &smmap.desc_size, &desc_ver);
        smmap.size += 2 * smmap.desc_size;
        EFI::sys->BootServices->AllocatePool(EfiLoaderData, smmap.size, reinterpret_cast<VOID**>(&smmap.mmap));
        EFI::sys->BootServices->GetMemoryMap(&smmap.size, smmap.mmap, &mmap_key, &smmap.desc_size, &desc_ver);

        return smmap;
    }

    static inline void UpdateRemapRVA(ShdMem::VirtualAddress& remap_rva) {
        if (++remap_rva.PD_offset >= ShdMem::PD_ENTRIES) {
            remap_rva.PD_offset = 0;
            if (++remap_rva.PDPT_offset >= ShdMem::PDPT_ENTRIES) {
                remap_rva.PDPT_offset = 0;
                ++remap_rva.PML4_offset;
            }
        }
    }

    static inline void FullUpdateRemapRVA(ShdMem::VirtualAddress& remap_rva) {
        if (++remap_rva.PT_offset >= ShdMem::PT_ENTRIES) {
            remap_rva.PT_offset = 0;
            UpdateRemapRVA(remap_rva);
        }
    }

    static inline void prepareRemap(PML4E* pml4, const PagingInformation& PI, void* _addr, uint64_t pages) {
        ShdMem::VirtualAddress remap_rva = ShdMem::ParseVirtualAddress(_addr);
        uint64_t required_PTs = (pages + ShdMem::PT_ENTRIES - 1) / ShdMem::PT_ENTRIES;

        for (size_t i = 0; i < required_PTs; ++i) {
            PML4E* pml4e = pml4 + remap_rva.PML4_offset;
            PDPTE* pdpt = nullptr;

            if ((*pml4e & ShdMem::PML4E_PRESENT) == 0) {
                EFI::sys->BootServices->AllocatePages(
                    AllocateAnyPages,
                    EfiUnusableMemory,
                    1,
                    reinterpret_cast<EFI_PHYSICAL_ADDRESS*>(&pdpt)
                );
                EFI::sys->BootServices->SetMem(pdpt, ShdMem::PAGE_SIZE, 0);
                *pml4e = MakePML4E(reinterpret_cast<uint64_t>(pdpt), PI);
            }
            else {
                pdpt = reinterpret_cast<PDPTE*>(*pml4e & ShdMem::PML4E_ADDRESS);
            }

            PDPTE* pdpte = pdpt + remap_rva.PDPT_offset;
            PDE* pd = nullptr;

            if ((*pdpte & ShdMem::PDPTE_PRESENT) == 0) {
                EFI::sys->BootServices->AllocatePages(
                    AllocateAnyPages,
                    EfiUnusableMemory,
                    1,
                    reinterpret_cast<EFI_PHYSICAL_ADDRESS*>(&pd)
                );
                EFI::sys->BootServices->SetMem(pd, ShdMem::PAGE_SIZE, 0);
                *pdpte = MakePDPTE(reinterpret_cast<uint64_t>(pd), PI);
            }
            else {
                pd = reinterpret_cast<PDE*>(*pdpte & ShdMem::PDPTE_ADDRESS);
            }

            PDE* pde = pd + remap_rva.PD_offset;
            PTE* pt = nullptr;

            if ((*pde & ShdMem::PDE_PRESENT) == 0) {
                EFI::sys->BootServices->AllocatePages(
                    AllocateAnyPages,
                    EfiUnusableMemory,
                    1,
                    reinterpret_cast<EFI_PHYSICAL_ADDRESS*>(&pt)
                );
                EFI::sys->BootServices->SetMem(pt, ShdMem::PAGE_SIZE, 0);
                *pde = MakePDE(reinterpret_cast<uint64_t>(pt), PI);
            }

            UpdateRemapRVA(remap_rva);
        }
    }

    static inline void DirectRemap(
        size_t pages,
        PML4E* pml4,
        ShdMem::VirtualAddress& remap_rva,
        uint64_t physical_start,
        unsigned int execute_disable,
        const PagingInformation& PI
    ) {
        for (size_t i = 0; i < pages; ++i) {
            PML4E* pml4e = pml4 + remap_rva.PML4_offset;
            PDPTE* pdpte = reinterpret_cast<PDPTE*>(*pml4e & ShdMem::PML4E_ADDRESS) + remap_rva.PDPT_offset;
            PDE* pde = reinterpret_cast<PDE*>(*pdpte & ShdMem::PDPTE_ADDRESS) + remap_rva.PD_offset;
            PTE* pte = reinterpret_cast<PTE*>(*pde & ShdMem::PDE_ADDRESS) + remap_rva.PT_offset;

            *pte = MakePTE(physical_start + i * ShdMem::PAGE_SIZE, PI, execute_disable);

            FullUpdateRemapRVA(remap_rva);
        }
    }

    static unsigned int PAT_enable = 0;

    static inline void IndirectRemap(
        PML4E* pml4,
        ShdMem::VirtualAddress& remap_rva,
        EFI_MEMORY_TYPE mem_type,
        uint64_t& current_source,
        unsigned int execute_disable,
        const PagingInformation& PI
    ) {
        PML4E* pml4e = pml4 + remap_rva.PML4_offset;
        PDPTE* pdpt = nullptr;

        if ((*pml4e & ShdMem::PML4E_PRESENT) == 0) {            
            EFI::sys->BootServices->AllocatePages(
                AllocateAnyPages,
                mem_type,
                1,
                reinterpret_cast<EFI_PHYSICAL_ADDRESS*>(&pdpt)
            );
            EFI::sys->BootServices->SetMem(pdpt, ShdMem::PAGE_SIZE, 0);
            *pml4e = MakePML4E(reinterpret_cast<uint64_t>(pdpt), PI);
        }
        else {
            pdpt = reinterpret_cast<PDPTE*>(*pml4e & ShdMem::PML4E_ADDRESS);
        }

        PDPTE* pdpte = pdpt + remap_rva.PDPT_offset;
        PDE* pd = nullptr;

        if ((*pdpte & ShdMem::PDPTE_PRESENT) == 0) {
            EFI::sys->BootServices->AllocatePages(
                AllocateAnyPages,
                mem_type,
                1,
                reinterpret_cast<EFI_PHYSICAL_ADDRESS*>(&pd)
            );
            EFI::sys->BootServices->SetMem(pd, ShdMem::PAGE_SIZE, 0);
            *pdpte = MakePDPTE(reinterpret_cast<uint64_t>(pd), PI);
        }
        else {
            pd = reinterpret_cast<PDE*>(*pdpte & ShdMem::PDPTE_ADDRESS);
        }

        PDE* pde = pd + remap_rva.PD_offset;
        PTE* pt = nullptr;

        if ((*pde & ShdMem::PDE_PRESENT) == 0) {
            EFI::sys->BootServices->AllocatePages(
                AllocateAnyPages,
                mem_type,
                1,
                reinterpret_cast<EFI_PHYSICAL_ADDRESS*>(&pt)
            );
            EFI::sys->BootServices->SetMem(pt, ShdMem::PAGE_SIZE, 0);
            *pde = MakePDE(reinterpret_cast<uint64_t>(pt), PI);
        }
        else {
            pt = reinterpret_cast<PTE*>(*pde & ShdMem::PDE_ADDRESS);
        }

        PTE* pte = pt + remap_rva.PT_offset;
        *pte = MakePTE(current_source, PI, execute_disable);
        *pte |= PAT_enable ? ShdMem::PTE_PAT : 0;

        current_source += ShdMem::PAGE_SIZE;

        FullUpdateRemapRVA(remap_rva);
    }

    static void* MakeshiftMalloc(EfiMemoryMap& mmap, size_t pages) {
        const size_t desc_num = mmap.mmap_size / mmap.desc_size;

        for (size_t i = 0; i < desc_num; ++i) {
            EFI_MEMORY_DESCRIPTOR* mm_descriptor = reinterpret_cast<EFI_MEMORY_DESCRIPTOR*>(
                reinterpret_cast<uint8_t*>(mmap.mmap) + i * mmap.desc_size
            );

            if (mm_descriptor->Type == EfiConventionalMemory && mm_descriptor->NumberOfPages >= pages) {
                mm_descriptor->NumberOfPages -= pages;

                void* range_start = reinterpret_cast<void*>(
                    mm_descriptor->PhysicalStart
                        + mm_descriptor->NumberOfPages * ShdMem::PAGE_SIZE
                );
                
                EFI_MEMORY_DESCRIPTOR newBlock = {
                    .Type = EfiLoaderData,
                    ._padding = 0,
                    .PhysicalStart = reinterpret_cast<EFI_PHYSICAL_ADDRESS>(range_start),
                    .VirtualStart = 0, // don't care about this for now
                    .NumberOfPages = pages,
                    .Attribute = mm_descriptor->Attribute
                };

                // append new block at the end
                *reinterpret_cast<EFI_MEMORY_DESCRIPTOR*>(
                    reinterpret_cast<uint8_t*>(mmap.mmap) + desc_num * mmap.desc_size
                ) = newBlock;
                mmap.mmap_size += mmap.desc_size;

                return range_start;
            }
        }

        return nullptr;
    }
}

EfiMemoryMap Loader::GetEfiMemoryMap() {
    EfiMemoryMap efi_mmap = {
        .mmap_size  = 0,
        .mmap       = nullptr,
        .mmap_key   = 0,
        .desc_size  = 0,
        .desc_ver   = 0
    };
    EFI::sys->BootServices->GetMemoryMap(
        &efi_mmap.mmap_size,
        efi_mmap.mmap,
        &efi_mmap.mmap_key,
        &efi_mmap.desc_size,
        &efi_mmap.desc_ver
    );
    efi_mmap.mmap_size += 2 * efi_mmap.desc_size;
    // another +2 because mapping the loader info requires memory allocations that may fragment the memory map
    EFI::sys->BootServices->AllocatePool(
        EfiLoaderData,
        efi_mmap.mmap_size + 2 * efi_mmap.desc_size,
        reinterpret_cast<VOID**>(&efi_mmap.mmap)
    );
    EFI::sys->BootServices->GetMemoryMap(
        &efi_mmap.mmap_size,
        efi_mmap.mmap,
        &efi_mmap.mmap_key,
        &efi_mmap.desc_size,
        &efi_mmap.desc_ver
    );
    return efi_mmap;
}

PML4E* Loader::SetupBasicPaging(const PagingInformation& PI) {
    // maps PML4 onto itself
    PML4E* pml4 = nullptr;

    EFI::sys->BootServices->AllocatePages(
        AllocateAnyPages,
        EfiUnusableMemory,
        1,
        reinterpret_cast<EFI_PHYSICAL_ADDRESS*>(&pml4)
    );
    EFI::sys->BootServices->SetMem(pml4, ShdMem::PAGE_SIZE, 0);

    if ((uint64_t)pml4 % ShdMem::PAGE_SIZE != 0) {
        return nullptr; // ensures the PML4 is aligned on a 4 KiB boundary
    }

    PML4E* pml4e = pml4 + ShdMem::Layout::PAGING_LOOP_MASK;
    *pml4e = MakePML4E((uint64_t)pml4, PI) | ShdMem::PML4E_XD;
    return pml4;
}

void Loader::PrepareEFIRemap(PML4E* pml4, const PagingInformation& PI) {
    SimpleMMAP smmap = GetSimpleMMAP();

    uint64_t efi_services_required_pages = 0;
    uint64_t efi_acpi_required_pages = 0;

    size_t desc_num = smmap.size / smmap.desc_size;

    for (size_t i = 0; i < desc_num; ++i) {
        EFI_MEMORY_DESCRIPTOR* current_descriptor = reinterpret_cast<EFI_MEMORY_DESCRIPTOR*>(
            reinterpret_cast<uint8_t*>(smmap.mmap) + i * smmap.desc_size
        );

        if (current_descriptor->Type == EfiRuntimeServicesCode || current_descriptor->Type == EfiRuntimeServicesData) {
            efi_services_required_pages += current_descriptor->NumberOfPages;
        }
        else if (current_descriptor->Type == EfiACPIMemoryNVS) {
            efi_acpi_required_pages += current_descriptor->NumberOfPages;
        }
    }

    if (efi_services_required_pages * ShdMem::PAGE_SIZE >= ShdMem::Layout::EfiRuntimeServices.limit) {
        Loader::puts(u"Not enough memory to map all runtime services.\n\r");
        EFI::Terminate();
    }
    else if (efi_acpi_required_pages * ShdMem::PAGE_SIZE >= ShdMem::Layout::AcpiNvs.limit) {
        Loader::puts(u"Not enough memory to map all ACPI memory.\n\r");
        EFI::Terminate();
    }

    prepareRemap(
        pml4,
        PI,
        reinterpret_cast<void*>(ShdMem::Layout::EfiRuntimeServices.start),
        efi_services_required_pages
    );
    prepareRemap(
        pml4,
        PI,
        reinterpret_cast<void*>(ShdMem::Layout::AcpiNvs.start),
        efi_acpi_required_pages
    );

    EFI::sys->BootServices->FreePool(smmap.mmap);
}

void Loader::RemapRuntimeServices(PML4E* pml4, EFI_MEMORY_DESCRIPTOR* rt_desc, const PagingInformation& PI) {
    static uint64_t current_remap = ShdMem::Layout::EfiRuntimeServices.start;

    if (rt_desc != nullptr) {
        rt_desc->VirtualStart = current_remap;
        ShdMem::VirtualAddress remap_rva = ShdMem::ParseVirtualAddress(current_remap);

        DirectRemap(
            rt_desc->NumberOfPages,
            pml4,
            remap_rva,
            rt_desc->PhysicalStart,
            rt_desc->Type == EfiRuntimeServicesData,
            PI
        );

        current_remap += rt_desc->NumberOfPages * ShdMem::PAGE_SIZE;
    }
}

void Loader::RemapACPINVS(PML4E* pml4, EFI_MEMORY_DESCRIPTOR* acpi_desc, const PagingInformation& PI) {
    static uint64_t current_remap = ShdMem::Layout::AcpiNvs.start;

    if (acpi_desc != nullptr) {
        acpi_desc->VirtualStart = current_remap;
        ShdMem::VirtualAddress remap_rva = ShdMem::ParseVirtualAddress(current_remap);

        DirectRemap(
            acpi_desc->NumberOfPages,
            pml4,
            remap_rva,
            acpi_desc->PhysicalStart,
            0,
            PI
        );

        current_remap += acpi_desc->NumberOfPages * ShdMem::PAGE_SIZE;
    }
}

void Loader::MapKernel(PML4E* pml4, void* _source, void* _dest, size_t size, const PagingInformation& PI) {
    size_t pages = (size + ShdMem::PAGE_SIZE - 1) / ShdMem::PAGE_SIZE;

    uint64_t current_src = (uint64_t)_source;
    ShdMem::VirtualAddress map_rva = ShdMem::ParseVirtualAddress(_dest);

    for (size_t i = 0; i < pages; ++i) {
        IndirectRemap(pml4, map_rva, EfiUnusableMemory, current_src, 0, PI);
    }
}

void Loader::MapLoader(PML4E* pml4, const PagingInformation& PI) {
    SimpleMMAP smmap = GetSimpleMMAP();

    size_t desc_num = smmap.size / smmap.desc_size;

    for (size_t i = 0; i < desc_num; ++i) {
        EFI_MEMORY_DESCRIPTOR* current_descriptor = reinterpret_cast<EFI_MEMORY_DESCRIPTOR*>(
            reinterpret_cast<uint8_t*>(smmap.mmap) + i * smmap.desc_size
        );
        
        if (current_descriptor->Type == EfiLoaderCode || current_descriptor->Type == EfiLoaderData) {
            ShdMem::VirtualAddress remap_rva = ShdMem::ParseVirtualAddress(
                current_descriptor->PhysicalStart
            );

            uint64_t current_src = current_descriptor->PhysicalStart;
            
            for (size_t i = 0; i < current_descriptor->NumberOfPages; ++i) {
                IndirectRemap(
                    pml4,
                    remap_rva,
                    EfiLoaderData,
                    current_src,
                    current_descriptor->Type == EfiLoaderData,
                    PI
                );
            }
        }
    }

    EFI::sys->BootServices->FreePool(smmap.mmap);
}

void Loader::RemapGOP(
    PML4E* pml4,
    Shared::Graphics::BasicGraphics& BasicGFX,
    const PagingInformation& PI
) {
    if (BasicGFX.FBSIZE > ShdMem::Layout::EfiGopFramebuffer.limit) {
        Loader::puts(u"Not enough memory to map the framebuffer\n\r");
        EFI::Terminate();
    }

    size_t pages = (BasicGFX.FBSIZE + ShdMem::PAGE_SIZE - 1) / ShdMem::PAGE_SIZE;

    uint64_t current_src = reinterpret_cast<uint64_t>(BasicGFX.FBADDR);
    ShdMem::VirtualAddress remap_rva = ShdMem::ParseVirtualAddress(
        ShdMem::Layout::EfiGopFramebuffer.start
    );

    PAT_enable = 1;
    for (size_t i = 0; i < pages; ++i) {
        IndirectRemap(pml4, remap_rva, EfiUnusableMemory, current_src, 1, PI);
    }
    PAT_enable = 0;

    BasicGFX.FBADDR = reinterpret_cast<uint32_t*>(ShdMem::Layout::EfiGopFramebuffer.start);

    // map back buffer

    if (EFI::sys->BootServices->AllocatePages(
        AllocateAnyPages,
        EfiUnusableMemory,
        pages,
        reinterpret_cast<EFI_PHYSICAL_ADDRESS*>(&current_src)
    ) != EFI_SUCCESS) {
        Loader::puts(u"Failed to allocate back buffer for framebuffer\n\r");
        EFI::Terminate();
    } 

    ShdMem::VirtualAddress backbuffer_rva = ShdMem::ParseVirtualAddress(
        ShdMem::Layout::ScreenBackBuffer.start
    );

    for (size_t i = 0; i < pages; ++i) {
        IndirectRemap(pml4, backbuffer_rva, EfiUnusableMemory, current_src, 1, PI);
    }
}

void Loader::MapPSFFont(PML4E* pml4, void*& pcf_font, size_t size, const PagingInformation& PI) {
    if (size > ShdMem::Layout::OsLoaderFont.limit) {
        Loader::puts(u"PCF Font too large to fit in memory\n\r");
        EFI::Terminate();
    }

    size_t pages = (size + ShdMem::PAGE_SIZE - 1) / ShdMem::PAGE_SIZE;

    uint64_t current_src = reinterpret_cast<uint64_t>(pcf_font);    
    ShdMem::VirtualAddress remap_rva = ShdMem::ParseVirtualAddress(
        ShdMem::Layout::OsLoaderFont.start
    );

    for (size_t i = 0; i < pages; ++i) {
        IndirectRemap(pml4, remap_rva, EfiUnusableMemory, current_src, 1, PI);
    }

    pcf_font = reinterpret_cast<void*>(ShdMem::Layout::OsLoaderFont.start);
}

void Loader::SetupLoaderInfo(
    PML4E* pml4,
    const LoaderInfo& ldInfo,
    const PagingInformation& PI,
    EfiMemoryMap& mmap
) {
    const size_t total_size = sizeof(ldInfo)
        + sizeof(mmap.mmap_size)
        + sizeof(mmap.desc_size)
        + mmap.mmap_size;

    // Loader font is already mapped, so the corresponding PML4, PDPT and PD have already been allocated and mapped
    // since we have 32MB - 512KB, we need at most 32 / 2 - 1 = 16 - 1 = 15 page tables
    // the real number of pages we need is required_pages + required_pages / PT_ENTRIES
    // notice we don't need to round up the division result here, however,
    // we need to add 0x80 to take into account the 512KB of space reserved to the TTY font.

    const size_t required_pages = (total_size + ShdMem::PAGE_SIZE - 1) / ShdMem::PAGE_SIZE;
    const size_t total_pages = required_pages + (required_pages + 0x80) / ShdMem::PT_ENTRIES;

    if (total_pages * ShdMem::PAGE_SIZE > ShdMem::Layout::OsLoaderData.limit) {
        // unsolvable error, restart machine
        EFI::sys->RuntimeServices->ResetSystem(EfiResetCold, EFI_BUFFER_TOO_SMALL, 0, nullptr);
    }

    uint8_t* ptr = static_cast<uint8_t*>(MakeshiftMalloc(mmap, total_pages));
    if (ptr == nullptr) {
        // unsolable error, restart machine
        EFI::sys->RuntimeServices->ResetSystem(EfiResetCold, EFI_OUT_OF_RESOURCES, 0, nullptr);
    }

    *(reinterpret_cast<ShdMem::Layout::DMAZoneInfo*>(
        ptr + ShdMem::Layout::OsLoaderDataOffsets.DMABitMap)) = ldInfo.dmaInfo;
    
    *(reinterpret_cast<Shared::Graphics::BasicGraphics*>(
        ptr + ShdMem::Layout::OsLoaderDataOffsets.GFXData)) = ldInfo.gfxData;
    
    *(reinterpret_cast<EFI_RUNTIME_SERVICES**>(
        ptr + ShdMem::Layout::OsLoaderDataOffsets.RTServices)) = ldInfo.rtServices;

    *(reinterpret_cast<EFI_PHYSICAL_ADDRESS*>(
        ptr + ShdMem::Layout::OsLoaderDataOffsets.PCIeECAM0)) = ldInfo.PCIe_ECAM_0;
    
    *(reinterpret_cast<uint64_t*>(
        ptr + ShdMem::Layout::OsLoaderDataOffsets.AcpiRevision)) = ldInfo.AcpiRevision;
    
    *(reinterpret_cast<EFI_PHYSICAL_ADDRESS*>(
        ptr + ShdMem::Layout::OsLoaderDataOffsets.AcpiRSDP
    )) = reinterpret_cast<EFI_PHYSICAL_ADDRESS>(ldInfo.RSDP);

    *(reinterpret_cast<uint64_t*>(
        ptr + ShdMem::Layout::OsLoaderDataOffsets.MmapSize)) = mmap.mmap_size;
    
    *(reinterpret_cast<uint64_t*>(
        ptr + ShdMem::Layout::OsLoaderDataOffsets.MmapDescSize)) = mmap.desc_size;

    Loader::memcpy(
        ptr + ShdMem::Layout::OsLoaderDataOffsets.Mmap,
        mmap.mmap,
        mmap.mmap_size
    );

    const auto origin_ptr = ptr;
    ptr += ShdMem::Layout::OsLoaderDataOffsets.Mmap + mmap.mmap_size;

    uint64_t offset = reinterpret_cast<uint64_t>(origin_ptr) % ShdMem::PAGE_SIZE;
    if (offset != 0) {
        offset += ShdMem::PAGE_SIZE - offset;
    }

    uint64_t current_source = reinterpret_cast<uint64_t>(origin_ptr);
    ShdMem::VirtualAddress remap_rva = ShdMem::ParseVirtualAddress(
        ShdMem::Layout::OsLoaderData.start
    );

    for (size_t i = 0; i < required_pages; ++i) {
        PML4E* pml4e = pml4 + remap_rva.PML4_offset;
        PDPTE* pdpt  = reinterpret_cast<PDPTE*>(*pml4e & ShdMem::PML4E_ADDRESS);
        PDPTE* pdpte = pdpt + remap_rva.PDPT_offset;
        PDE* pd = reinterpret_cast<PDE*>(*pdpte & ShdMem::PDPTE_ADDRESS);
        PDE* pde = pd + remap_rva.PD_offset;

        PTE* pt = nullptr;

        if ((*pde & ShdMem::PDE_PRESENT) == 0) {
            pt = reinterpret_cast<PTE*>(ptr);
            ptr += ShdMem::PAGE_SIZE;
            Loader::memset(pt, 0, ShdMem::PAGE_SIZE);
            *pde = MakePDE(reinterpret_cast<uint64_t>(pt), PI);
        }
        else {
            pt = reinterpret_cast<PTE*>(*pde & ShdMem::PDE_ADDRESS);
        }

        PTE* pte = pt + remap_rva.PT_offset;
        *pte = MakePTE(current_source, PI, true);

        current_source += ShdMem::PAGE_SIZE;
        FullUpdateRemapRVA(remap_rva);
    }
}