// SPDX-License-Identifier: GPL-3.0-only
//
// Copyright (C) 2026 Alexandre Boissiere
// This file is part of the BadLands operating system.
//
// This program is free software: you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation, version 3.
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
// without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with this program.
// If not, see <https://www.gnu.org/licenses/>. 

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

    inline constexpr MemoryZone UserStack  {
        .start = UserMemory.end() - 0x0000000000200000,
        .limit = 0x0000000000200000
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
        .limit = (Shared::Memory::PML4_ENTRIES / 2 - 5) * Shared::Memory::PML4E_COVERAGE
    };

    inline constexpr MemoryZone SecondaryRecursiveMapping = {
        .start = 0xFFFFFE8000000000,
        .limit = Shared::Memory::PML4E_COVERAGE
    };

    inline constexpr MemoryZone TaskMemory {
        .start = Shared::Memory::Layout::RecursiveMemoryMapping.end(),
        .limit = Shared::Memory::PML4E_COVERAGE
    };
    
    inline constexpr MemoryZone KernelStackGuard = {
        .start = TaskMemory.start,
        .limit = Shared::Memory::PAGE_SIZE
    };

    inline constexpr MemoryZone KernelStack = {
        .start = KernelStackGuard.end(),
        .limit = 0x0000000000100000 - 2 * Shared::Memory::PAGE_SIZE
    };

    inline constexpr MemoryZone KernelStackReserve = {
        .start = KernelStack.end(),
        .limit = Shared::Memory::PAGE_SIZE
    };

    inline constexpr MemoryZone UserVMemManagement = {
        .start = 0xFFFFFF8001100000,
        .limit = Shared::Memory::PML4E_COVERAGE - (0xFFFFFF8001100000 % Shared::Memory::PML4E_COVERAGE)
    };
}
