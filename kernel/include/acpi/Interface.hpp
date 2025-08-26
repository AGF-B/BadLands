#pragma once

namespace ACPI {
    void Initialize();
    void* FindTable(const char* signature);
    void* MapTable(void* physical_address);
    bool UnmapTable(void* address);
}
