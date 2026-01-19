#pragma once

#include <shared/Response.hpp>

namespace ACPI {
    void Initialize();
    void* FindTable(const char* signature);
    void* MapTable(void* physical_address);
    Success UnmapTable(void* address);
}
