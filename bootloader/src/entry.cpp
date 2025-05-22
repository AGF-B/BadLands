#include <efi/efi_misc.hpp>
#include <shared/efi/efi.h>

#include <shared/memory/defs.hpp>
#include <shared/memory/layout.hpp>

#include <loader/acpi_check.hpp>
#include <loader/basic_graphics.hpp>
#include <loader/kernel_loader.hpp>
#include <loader/loader_info.hpp>
#include <loader/paging.hpp>
#include <loader/pci.hpp>
#include <loader/psf_font.hpp>
#include <loader/system_config.hpp>

#include <ldstdio.hpp>

#include <cpuid.h>

namespace ShdMem = Shared::Memory;

#define LEGACY_EXPORT extern "C"
#define EFI_2_00_SYSTEM_TABLE_REVISION ((2<<16) | (00))

LEGACY_EXPORT int32_t EfiLoaderSetup(uint32_t* CpuIdCommand, uint8_t* PhysicalAddressWidth);

EFI_SYSTEM_TABLE* EFI::sys;

static PagingInformation PI;
static PML4E* pml4;
static KernelLocInfo KernelLI;
static LoaderInfo ldInfo;
static ShdMem::Layout::DMAZoneInfo* pmi;
static EfiMemoryMap mmap;

// very basic stack for the kernel, aligned on an 8-byte boundary, 4KB
static uint64_t temporary_stack[512] = { 0 };

LEGACY_EXPORT EFIAPI int EfiEntry(EFI_HANDLE handle, EFI_SYSTEM_TABLE* _sys) {
    EFI::sys = _sys;
    EFI::sys->ConOut->ClearScreen(EFI::sys->ConOut);

    Loader::puts(u"=== BadLands loader ===\n\r");
    
    if (EFI::sys->BootServices->Hdr.Revision < EFI_2_00_SYSTEM_TABLE_REVISION) {
        Loader::puts(u"UEFI firmware revision should be 2.0 or later.\n\r");
        EFI::Terminate();
    }

    uint32_t CpuIdCommand;

    int32_t init_status = EfiLoaderSetup(&CpuIdCommand, &PI.MAXPHYADDR);

    if (init_status == -1) {
        Loader::printf(u"LOADER PANIC: CPUID DOES NOT SUPPORT COMMAND 0x%.8x.\n\r", CpuIdCommand);
        EFI::Terminate();
    }
    else if (init_status == -2) {
        Loader::printf(u"LOADER PANIC: CPU DOES NOT SUPPORT NXE PROTECTION.\n\r");
        EFI::Terminate();
    }
    else if (init_status == -3) {
        Loader::printf(u"LOADER PANIC: CPU DOES NOT SUPPORT PAT REPROGRAMMING.\n\r");
        EFI::Terminate();
    }

    pml4 = Loader::SetupBasicPaging(PI);
    KernelLI = Loader::LoadKernel(handle, pml4, PI);
    ldInfo.gfxData = Loader::LoadGraphics();
    Loader::LoadFont(handle, pml4, PI);

    Loader::PrepareEFIRemap(pml4, PI);
    Loader::RemapGOP(pml4, ldInfo.gfxData, PI);
    
    EFI_SYSTEM_CONFIGURATION sysconf;
    Loader::DetectSystemConfiguration(&sysconf);

    if (sysconf.ACPI_20 == nullptr) {
        Loader::puts(u"LOADER PANIC: SYSTEM DOES NOT SUPPORT ACPI 2.0 OR LATER.\n\r");
        EFI::Terminate();
    }
    else {
        ldInfo.AcpiRevision = 2;
        ldInfo.RSDP = sysconf.ACPI_20;
    }

    ldInfo.PCIe_ECAM_0 = Loader::LocatePCI(sysconf);

    EFI::sys->BootServices->AllocatePages(
        AllocateAnyPages,
        EfiLoaderData,
        (sizeof(ShdMem::Layout::DMAZoneInfo) + ShdMem::PAGE_SIZE - 1) / ShdMem::PAGE_SIZE,
        reinterpret_cast<EFI_PHYSICAL_ADDRESS*>(&pmi)
    );
    EFI::sys->BootServices->SetMem(pmi, sizeof(ShdMem::Layout::DMAZoneInfo), 0);

    Loader::MapLoader(pml4, PI);
    mmap = Loader::GetEfiMemoryMap();

    Loader::CheckACPI(sysconf, mmap);

    EFI_STATUS Status = EFI::sys->BootServices->ExitBootServices(handle, mmap.mmap_key);
    if (Status != EFI_SUCCESS) {
        Loader::puts(u"Could not exit boot services.\n\r");
        EFI::Terminate();
    }

    UINTN desc_num = mmap.mmap_size / mmap.desc_size;

    EFI_MEMORY_DESCRIPTOR* previous_descriptor = nullptr;

    for (size_t i = 0; i < desc_num; ++i) {
        EFI_MEMORY_DESCRIPTOR* current_descriptor = reinterpret_cast<EFI_MEMORY_DESCRIPTOR*>(
            reinterpret_cast<uint8_t*>(mmap.mmap) + i * mmap.desc_size
        );

        if (current_descriptor->PhysicalStart < ShdMem::Layout::DMAZone.limit) {
            constexpr size_t DMAEndPage = ShdMem::Layout::DMAZone.limit / ShdMem::PAGE_SIZE;

            const size_t start_page = current_descriptor->PhysicalStart / ShdMem::PAGE_SIZE;
            const size_t unbounded_end_page = start_page + current_descriptor->NumberOfPages;
            const size_t end_page =
                unbounded_end_page <= DMAEndPage ? unbounded_end_page : DMAEndPage;

            if (current_descriptor->Type != EfiConventionalMemory) {
                for (size_t page = start_page; page < end_page; ++page) {
                    const size_t byte = page / 8;
                    const size_t bit = page % 8;
                    pmi->bitmap[byte] |= 1 << bit;
                }
            }
            else {
                for (size_t page = start_page; page < end_page; ++page) {
                    const size_t byte = page / 8;
                    const size_t bit = page % 8;
                    pmi->bitmap[byte] &= ~(1 << bit);
                }
            }
        }

        if (current_descriptor->Type == EfiRuntimeServicesCode || current_descriptor->Type == EfiRuntimeServicesData) {
            Loader::RemapRuntimeServices(pml4, current_descriptor, PI);
        }
        else if (current_descriptor->Type == EfiACPIMemoryNVS) {
            Loader::RemapACPINVS(pml4, current_descriptor, PI);
        }
        
        size_t j = i;
        EFI_MEMORY_DESCRIPTOR* ptr = current_descriptor;
        while (ptr->PhysicalStart < previous_descriptor->PhysicalStart && previous_descriptor != nullptr) {
            EFI_MEMORY_DESCRIPTOR temp = *ptr;
            *ptr = *previous_descriptor;
            *previous_descriptor = temp;
            ptr = previous_descriptor;
            previous_descriptor = j-- >= 1 ? reinterpret_cast<EFI_MEMORY_DESCRIPTOR*>(
                reinterpret_cast<uint8_t*>(mmap.mmap) + j * mmap.desc_size
            ) : nullptr;
        }

        previous_descriptor = current_descriptor;
    }

    Status = EFI::sys->RuntimeServices->SetVirtualAddressMap(mmap.mmap_size, mmap.desc_size, mmap.desc_ver, mmap.mmap);
    if (Status != EFI_SUCCESS) {
        EFI::sys->RuntimeServices->ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, nullptr);
    }

    __asm__ volatile("mov %0, %%rsp\n\tmov %1, %%rbp"
        :: "r"(temporary_stack + 511),"r"(temporary_stack + 511));

    ldInfo.dmaInfo = *pmi;
    ldInfo.rtServices = EFI::sys->RuntimeServices;
    Loader::SetupLoaderInfo(pml4, ldInfo, PI, mmap);

    __asm__ volatile("mov %0, %%cr3" :: "r"(pml4) : "memory");

    __asm__ volatile("mov %0, %%rax" :: "m"(KernelLI.EntryPoint));
    __asm__ volatile("callq *%rax");
    __asm__ volatile("jmp .");

    while (1) {
        __asm__ volatile("hlt");
    }

    return 0;
}
