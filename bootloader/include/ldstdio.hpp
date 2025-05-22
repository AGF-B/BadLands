#pragma once

#include <efi/efi_datatypes.h>
#include <shared/efi/efi.h>

#include <cstdarg>
#include <cwchar>

namespace Loader {
    INTN puts(const CHAR16* s);

    size_t vsnprintf(CHAR16* restrict buffer, size_t bufsz, const CHAR16* restrict format, va_list vilst);
    size_t printf(const CHAR16* restrict format, ...);
}

namespace EFI {
    EFI_INPUT_KEY readkey(void);
};
