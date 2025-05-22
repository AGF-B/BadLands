#pragma once

#include <cstddef>
#include <cstdint>

#include <shared/memory/layout.hpp>

namespace VirtualMemoryLayout {
    using MemoryZone = Shared::Memory::Layout::MemoryZone;

    inline constexpr MemoryZone UserMemory = {
        .start = Shared::Memory::Layout::DMAZone.start,
        .limit = Shared::Memory::PML4E_COVERAGE - Shared::Memory::Layout::DMAZone.limit
    };

    inline constexpr MemoryZone PhysicalMemoryMap = {
        .start = Shared::Memory::Layout::UnmappedMemoryStart,
        .limit = 0x100000000
    };

    inline constexpr MemoryZone GeneralMapping = {
        .start = PhysicalMemoryMap.end(),
        .limit = Shared::Memory::PML4E_COVERAGE
            - (PhysicalMemoryMap.end() - Shared::Memory::Layout::KernelImage.start)
    };

    inline constexpr MemoryZone KernelHeapManagement = {
        .start = GeneralMapping.end(),
        .limit = Shared::Memory::PML4E_COVERAGE
    };

    inline constexpr MemoryZone KernelHeap = {
        .start = KernelHeapManagement.end(),
        .limit = (Shared::Memory::PML4_ENTRIES / 2 - 4) * Shared::Memory::PML4E_COVERAGE
    };

    inline constexpr MemoryZone SecondaryRecursiveMapping = {
        .start = 0xFFFFFE8000000000,
        .limit = Shared::Memory::PML4E_COVERAGE
    };

    inline constexpr MemoryZone MainCoreDump = {
        .start = 0xFFFFFF8001000000,
        .limit = 0x0000000000008000
    };

    inline constexpr MemoryZone SecondaryCoreDump = {
        .start = 0xFFFFFF8001008000,
        .limit = 0x0000000000008000
    };

    inline constexpr MemoryZone UserVMemManagement = {
        .start = 0xFFFFFF8001100000,
        .limit = Shared::Memory::PML4E_COVERAGE - (0xFFFFFF8001100000 % Shared::Memory::PML4E_COVERAGE)
    };
}
