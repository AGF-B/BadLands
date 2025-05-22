#pragma once

#define LEGACY_EXPORT extern "C"

namespace VirtualMemory {
    LEGACY_EXPORT void kernel_gdt_setup(void);
}
