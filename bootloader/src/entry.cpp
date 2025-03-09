#include <efi/efi_misc.hpp>
#include <efi/efi.h>

#include <loader/basic_graphics.hpp>
#include <loader/loader_info.hpp>
#include <loader/paging.hpp>
#include <loader/system_config.hpp>

#include <ldstdio.hpp>

#define LEGACY_EXPORT extern "C"

EFI_SYSTEM_TABLE* EFI::sys;

LEGACY_EXPORT void puts(const CHAR16* s) {
	EFI::sys->ConOut->OutputString(EFI::sys->ConOut, const_cast<CHAR16*>(s));
}

LEGACY_EXPORT EFIAPI int EfiEntry(EFI_HANDLE handle, EFI_SYSTEM_TABLE* _sys) {
    EFI::sys = _sys;
    EFI::sys->ConOut->ClearScreen(EFI::sys->ConOut);
    
    EFI_SYSTEM_CONFIGURATION sysconf;
    Loader::DetectSystemConfiguration(&sysconf);

    BasicGraphics BasicGFX = Loader::LoadGraphics();

    for (size_t r = 0; r < BasicGFX.ResY; ++r) {
        for (size_t c = 0; c < BasicGFX.ResX; ++c) {
            BasicGFX.FBADDR[r * BasicGFX.PPSL + c] = 0xFF0000;
        }
    }

    EfiMemoryMap mmap = Loader::GetEfiMemoryMap();

    EFI_STATUS Status = EFI::sys->BootServices->ExitBootServices(handle, mmap.mmap_key);
    if (Status != EFI_SUCCESS) {
        Loader::puts(u"Could not exit boot services.\n\r");
        EFI::Terminate();
    }

    Status = EFI::sys->RuntimeServices->SetVirtualAddressMap(mmap.mmap_size, mmap.desc_size, mmap.desc_ver, mmap.mmap);
    if (Status != EFI_SUCCESS) {
        EFI::sys->RuntimeServices->ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, nullptr);
    }
    
    for (size_t r = BasicGFX.ResY / 2; r < BasicGFX.ResY; ++r) {
        for (size_t c = 0; c < BasicGFX.ResX; ++c) {
            BasicGFX.FBADDR[r * BasicGFX.PPSL + c] = 0x00FFFF;
        }
    }

    while (1);

    return 0;
}
