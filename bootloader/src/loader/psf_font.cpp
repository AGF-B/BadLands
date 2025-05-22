#include <cstdint>

#include <efi/efi_datatypes.h>
#include <efi/efi_fs.hpp>
#include <efi/efi_image_services.hpp>
#include <efi/efi_misc.hpp>

#include <ldstdio.hpp>

#include <shared/efi/efi.h>
#include <shared/memory/defs.hpp>

#include <loader/paging.hpp>
#include <loader/psf_font.hpp>

namespace ShdMem = Shared::Memory;

namespace {
    static const CHAR16* font_file_path = u"\\EFI\\BOOT\\psf_font.psf\0";
}

const void* Loader::LoadFont(EFI_HANDLE ImageHandle, PML4E* pml4, const PagingInformation& PI) {
    EFI_LOADED_IMAGE_PROTOCOL* efi_lip = EFI::getLoadedImageProtocol(ImageHandle);
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* efi_sfsp = EFI::getDeviceSFSP(ImageHandle, efi_lip->DeviceHandle);
    EFI_FILE_PROTOCOL* efi_root_fsp = EFI::openDeviceVolume(efi_sfsp);
    EFI_FILE_PROTOCOL* efi_font_fsp = EFI::openReadOnlyFile(efi_root_fsp, const_cast<CHAR16*>(font_file_path));

    if (efi_font_fsp == nullptr) {
        Loader::puts(u"TTY font file was either not found, or no suitable protocol was found to locate/open it\n\r");
        EFI::Terminate();
    }

    EFI_FILE_INFO* FontInfo = EFI::getFileInfo(efi_font_fsp);
    UINT64 FontSize = FontInfo->FileSize;
    EFI::sys->BootServices->FreePool(FontInfo);

    size_t requiredPages = (FontSize + ShdMem::PAGE_SIZE - 1) /  ShdMem::PAGE_SIZE;

    void* FontBuffer = nullptr;
    EFI::sys->BootServices->AllocatePages(
        AllocateAnyPages,
        EfiUnusableMemory,
        requiredPages,
        reinterpret_cast<EFI_PHYSICAL_ADDRESS*>(&FontBuffer)
    );

    if (efi_font_fsp->Read(efi_font_fsp, &FontSize, FontBuffer) != EFI_SUCCESS) {
        Loader::puts(u"Error reading font file\n\r");
        EFI::Terminate();
    }
    efi_font_fsp->Close(efi_font_fsp);
    
    Loader::MapPSFFont(pml4, FontBuffer, FontSize, PI);

    return FontBuffer;
}
