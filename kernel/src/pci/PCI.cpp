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

#include <cstdint>

#include <shared/memory/defs.hpp>
#include <shared/memory/layout.hpp>

#include <devices/USB/xHCI/Controller.hpp>
#include <interrupts/APIC.hpp>
#include <interrupts/Panic.hpp>
#include <mm/VirtualMemory.hpp>
#include <screen/Log.hpp>

#pragma pack(push)
#pragma pack(1)

struct PCI_CS {
    uint16_t    VendorID;
    uint16_t    DeviceID;
    uint16_t    Command;
    uint16_t    Status;
    uint8_t     RevisionID;
    uint8_t     ProgrammingInterface;
    uint8_t     SubclassCode;
    uint8_t     BaseClassCode;
    uint8_t     CacheLineSize;
    uint8_t     LatencyTimer;
    uint8_t     HeaderType;
    uint8_t     BIST;
    union {
        uint8_t Raw[8];
        struct {
            union {
                uint32_t BaseAddressRegisters[6];
                uint64_t XBaseAddressRegisters[3];
            };
            uint32_t CardbusCISPointer;
            uint16_t SubsystemVendorID;
            uint16_t SubsystemID;
            uint32_t ExpansionROMBaseAddress;
        } Type0;
        struct {
            union {
                struct {
                    uint32_t    BAR0;
                    uint32_t    BAR1;
                } BARS;
                uint64_t XBAR0;
            };
            uint8_t     PrimaryBusNumber;
            uint8_t     SecondaryBusNumber;
            uint8_t     SubordinateBusNumber;
            uint8_t     SecondaryLatencyTimer;
            uint8_t     IOBase;
            uint8_t     IOLimit;
            uint16_t    SecondaryStatus;
            uint16_t    MemoryBase;
            uint16_t    MemoryLimit;
            uint16_t    PrefetchableMemoryBase;
            uint16_t    PrefetchableMemoryLimit;
            uint32_t    PrefetchableBaseUpper32;
            uint32_t    PrefetchableLimitUpper32;
            uint16_t    IOBaseUpper16;
            uint16_t    IOLimitUpper16;
        } Type1;
    } TypeSpecificData1;
    uint8_t CapabilitiesPointer;
    union {
        uint8_t Raw[7];
        struct {
            uint8_t     Reserved[3];
            uint32_t    ExpansionROMBaseAddress;
        } Type1;
    } TypeSpecificData2;
    uint8_t InterruptLine;
    uint8_t InterruptPin;
    union {
        struct {
            uint8_t Min_Gnt;
            uint8_t Max_Lat;
        } Type0;
        struct {
            uint16_t BridgeControl;
        } Type1;
    } TypeSpecificData3;
};

struct PCI_Capability {
    uint8_t ID;
    uint8_t NextPointer;
};

struct MSI_Capability {
    uint8_t ID;
    uint8_t NextPointer;
    uint16_t MessageControl;
};

struct MSIx32 {
    uint8_t ID;
    uint8_t NextPointer;
    uint16_t MessageControl;
    uint32_t MessageAddress;
    uint16_t MessageData;
};

struct MSIx64 {
    uint8_t ID;
    uint8_t NextPointer;
    uint16_t MessageControl;
    uint32_t MessageAddress;
    uint32_t MessageUpperAddress;
    uint16_t MessageData;
};

namespace PCI {
    void Enumerate() {
        uint8_t* ECAM_CS = *reinterpret_cast<uint8_t**>(
            Shared::Memory::Layout::OsLoaderData.start + Shared::Memory::Layout::OsLoaderDataOffsets.PCIeECAM0
        );

        for (size_t bus = 0; bus < 256; ++bus) {
            for (size_t device = 0; device < 32; ++device) {
                PCI_CS* phys_device_ecam = reinterpret_cast<PCI_CS*>(
                    ECAM_CS + ((bus) << 20 | (device) << 15)
                );

                constexpr uint64_t flags = Shared::Memory::PTE_READWRITE | Shared::Memory::PTE_UNCACHEABLE;
                PCI_CS* device_ecam = reinterpret_cast<PCI_CS*>(
                    VirtualMemory::MapGeneralPages(phys_device_ecam, 1, flags)
                );

                if (device_ecam == nullptr) {
                    Panic::PanicShutdown("COULD NOT RESERVE PAGE FOR DEVICE ECAM\n\r");
                }

                if (device_ecam->VendorID != 0xFFFF) {
                    if (device_ecam->BaseClassCode == 12 && device_ecam->SubclassCode == 3) {
                        if (device_ecam->ProgrammingInterface == 0x30) {
                            Log::printfSafe("Found USB xHCI controller at bus=%u,device=%u\n\r", bus, device);

                            auto* ptr = Devices::USB::xHCI::Controller::Initialize(
                                bus,
                                device,
                                0,
                                device_ecam
                            );

                            if (ptr == nullptr) {
                                Log::putsSafe("USB xHCI controller initialization failed\n\r");
                            }
                        }
                    }
                }

                if ((device_ecam->HeaderType & 0x80) != 0) {
                    for (size_t function = 0; function < 8; ++function) {
                        PCI_CS* phys_function_ecam = reinterpret_cast<PCI_CS*>(
                            ECAM_CS + ((bus) << 20 | (device) << 15 | (function) << 12)
                        );
                        PCI_CS* function_ecam = reinterpret_cast<PCI_CS*>(
                            VirtualMemory::MapGeneralPages(phys_function_ecam, 1, flags)
                        );

                        if (function_ecam == nullptr) {
                            Panic::PanicShutdown("COULD NOT RESERVE PAGE FOR FUNCTION ECAM\n\r");
                        }

                        if (function_ecam->VendorID != 0xFFFF) {
                            Log::printfSafe(
                                "\tFunction found (class=%u,subclass=%u,pi=%u,bus=%u,device=%u,function=%u)\n\r",
                                function_ecam->BaseClassCode,
                                function_ecam->SubclassCode,
                                function_ecam->ProgrammingInterface,
                                bus,
                                device,
                                function
                            );
                        }

                        VirtualMemory::UnmapGeneralPages(function_ecam, 1);
                    }
                }

                VirtualMemory::UnmapGeneralPages(device_ecam, 1);
            }
        }
    }
}

#pragma pack(pop)