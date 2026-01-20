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

#include <cstdint>

#include <efi/efi_datatypes.h>

namespace ExecutableFormat {
    struct ELF_PROGRAM_HEADER {
        uint32_t SegmentType;
        uint32_t Flags;
        uint64_t FileOffset;
        uint64_t SegmentVirtualAddress;
        uint64_t SegmentPhysicalAddress;
        uint64_t SegmentFileSize;
        uint64_t SegmentMemorySize;
        uint64_t Alignment;
    };
    
    struct ELF_SECTION_HEADER {
        uint32_t SectionNameOffset;
        uint32_t SectionType;
        uint64_t SectionFlags;
        uint64_t SectionVirtualAddress;
        uint64_t FileOffset;
        uint64_t Size;
        uint32_t LinkedSectionIndex;
        uint32_t SectionInfo;
        uint64_t SectionAlignment;
        uint64_t SectionEntrySize;
    };
    
    struct ELF_HEADER {
        uint32_t Magic;
        uint8_t Format;
        uint8_t Endianness;
        uint8_t Version1;
        uint8_t ABI;
        uint8_t ABIVersion;
        uint8_t Reserved[7];
        uint16_t Type;
        uint16_t MArch;
        uint32_t Version2;
        void (*EntryPoint)();
        uint64_t ProgramHeaderTableOffset;
        uint64_t SectinoHeaderTableOffset;
        uint32_t ArchFlags;
        uint16_t HeaderSize;
        uint16_t ProgramHeaderSize;
        uint16_t ProgramHeadersCount;
        uint16_t SectionHeaderSize;
        uint16_t SectionHeadersCount;
        uint16_t SectionNamesEntryIndex;
    };
}

struct KernelLocInfo {
    void (*EntryPoint)();
};

typedef uint64_t PML4E;
struct PagingInformation;

#include <loader/paging.hpp>

namespace Loader {
   KernelLocInfo LoadKernel(EFI_HANDLE ImageHandle, PML4E* pml4, const PagingInformation& PI);
}
