#pragma once

#include <efi/efi_datatypes.h>

namespace Loader {
    const void* LoadFont(EFI_HANDLE ImageHandle, PML4E* pml4, const PagingInformation& PI);
}
