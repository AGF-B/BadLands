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

#include <cstddef>
#include <cstdint>

#include <shared/memory/defs.hpp>
#include <shared/memory/layout.hpp>

#include <acpi/Interface.hpp>
#include <acpi/tables.hpp>
#include <interrupts/Panic.hpp>
#include <mm/Utils.hpp>
#include <mm/VirtualMemory.hpp>
#include <screen/Log.hpp>

namespace ACPI {
    namespace {
        static XSDT* xsdt = nullptr;
        static size_t xsdt_entries = 0;
    }

    static size_t required_pages(void* _start, size_t length) {
        constexpr auto& PAGE_SIZE = Shared::Memory::PAGE_SIZE;

        const uint64_t start = reinterpret_cast<uint64_t>(_start);

        return ((start + length) / PAGE_SIZE) - (start / PAGE_SIZE) + 1;
    }

    void Initialize() {
        Log::puts("[ACPI] Initializing ACPI platform...\n\r");

        void* physical_rsdp = *reinterpret_cast<void**>(
            Shared::Memory::Layout::OsLoaderData.start + Shared::Memory::Layout::OsLoaderDataOffsets.AcpiRSDP
        );

        RSDP* rsdp = static_cast<RSDP*>(VirtualMemory::MapGeneralPages(physical_rsdp, 1));

        if (rsdp == nullptr) {
            Panic::PanicShutdown("ACPI (RSDP MAPPING FAILED)\n\r");
        }
        else if (Utils::memcmp(rsdp->Signature, "RSD PTR ", 8) != 0) {
            Panic::PanicShutdown("ACPI (INVALID RSDP)\n\r");
        }
        else if (rsdp->Revision < 2) {
            Panic::PanicShutdown("ACPI (UNSUPPORTED RSDP)\n\r");
        }

        Log::printf("[ACPI] Valid RSDP found at %#.16llx, temporarily mapped at %#.16llx\n\r", physical_rsdp, rsdp);

        void* physical_xsdt = reinterpret_cast<void*>(rsdp->XsdtAddress);

        if (!VirtualMemory::UnmapGeneralPages(rsdp, 1).IsSuccess()) {
            Panic::PanicShutdown("ACPI (COULD NOT UNMAP RSDP)\n\r");
        }

        xsdt = static_cast<XSDT*>(VirtualMemory::MapGeneralPages(physical_xsdt, 1));

        if (xsdt == nullptr) {
            Panic::PanicShutdown("ACPI (XSDT FIRST MAPPING FAILED)\n\r");
        }
        else if (Utils::memcmp(xsdt->hdr.Signature, "XSDT", 4) != 0) {
            Panic::PanicShutdown("ACPI (INVALID FIRST XSDT)\n\r");
        }

        size_t xsdt_pages = required_pages(physical_xsdt, xsdt->hdr.Length);

        if (!VirtualMemory::UnmapGeneralPages(xsdt, 1).IsSuccess()) {
            Panic::PanicShutdown("ACPI (COULD NOT UNMAP FIRST XSDT)\n\r");
        }

        xsdt = static_cast<XSDT*>(VirtualMemory::MapGeneralPages(physical_xsdt, xsdt_pages));

        if (xsdt == nullptr) {
            Panic::PanicShutdown("ACPI (XSDT SECOND MAPPING FAILED)\n\r");
        }
        else if (Utils::memcmp(xsdt->hdr.Signature, "XSDT", 4) != 0) {
            Panic::PanicShutdown("ACPI (INVALID SECOND XSDT)\n\r");
        }

        xsdt_entries = (xsdt->hdr.Length - sizeof(xsdt->hdr)) / sizeof(void*);

        Log::printf("[ACPI] Valid XSDT found at %#.16llx : %#.16llx\n\r", physical_xsdt, xsdt);
        
        Log::printf("[ACPI] Initialization done\n\r");
    }

    void* FindTable(const char* signature) {
        Header** entries = reinterpret_cast<Header**>(xsdt + 1);

        for (size_t i = 0; i < xsdt_entries; ++i, ++entries) {
            Header* mapped = static_cast<Header*>(VirtualMemory::MapGeneralPages(*entries, 1));

            if (mapped != nullptr) {
                if (Utils::memcmp(mapped->Signature, signature, 4) == 0) {
                    void* table_address = *entries;
                    VirtualMemory::UnmapGeneralPages(mapped, 1);
                    return table_address;
                }

                VirtualMemory::UnmapGeneralPages(mapped, 1);
            }
        }

        return nullptr;
    }

    void* MapTable(void* physical_address) {
        Header* mapped = static_cast<Header*>(VirtualMemory::MapGeneralPages(physical_address, 1));

        if (mapped == nullptr) {
            return nullptr;
        }

        size_t pages = required_pages(mapped, mapped->Length);

        if (!VirtualMemory::UnmapGeneralPages(mapped, 1).IsSuccess()) {
            return nullptr;
        }

        return VirtualMemory::MapGeneralPages(physical_address, pages);
    }

    Success UnmapTable(void* address) {
        Header* mapped = static_cast<Header*>(address);

        size_t pages = required_pages(address, mapped->Length);

        return VirtualMemory::UnmapGeneralPages(address, pages);
    }
}
