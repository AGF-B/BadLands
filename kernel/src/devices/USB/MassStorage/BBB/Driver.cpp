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

#include <new>

#include <shared/Debug.hpp>
#include <shared/Lock.hpp>
#include <shared/LockGuard.hpp>
#include <shared/Response.hpp>

#include <devices/Storage/SCSI/Driver.hpp>
#include <devices/USB/MassStorage/BBB/Driver.hpp>
#include <devices/USB/xHCI/Device.hpp>

#include <mm/Heap.hpp>
#include <mm/Paging.hpp>
#include <mm/PhysicalMemory.hpp>
#include <mm/Utils.hpp>
#include <mm/VirtualMemory.hpp>

#include <sched/Self.hpp>

#include <screen/Log.hpp>

namespace Devices::USB::MassStorage::BBB {
    Driver::Driver(
        xHCI::Device& device,
        const Driver::StorageInfo& storage_info,
        const Driver::EndpointsInfo& endpoints_info,
        const Driver::IOBufferInfo& io_buffer_info
    ) :
        MassStorage::Driver{device},
        storage_info{storage_info},
        endpoints_info{endpoints_info},
        io_buffer{io_buffer_info.pointer},
        phys_io_buffer{io_buffer_info.physical_pointer}
    { }

    Success Driver::SendNormalBuffer(uint32_t length, uint8_t endpoint, bool is_input) {
        static constexpr uint64_t COMPLETION_TIMEOUT_MS = 1000;
        static constexpr auto TRANSFER_STATUS_PREDICATE = [](void* arg) {
            const Driver* const drv = reinterpret_cast<Driver*>(arg);
            return drv->transfer_complete.load();
        };

        auto* const endpoint_ring = GetEndpointTransferRing(endpoint, is_input);

        if (endpoint_ring == nullptr) {
            return Failure();
        }
        
        const auto init_trb = xHCI::NormalTRB::Create({
            .bufferPointer = phys_io_buffer,
            .transferLength = length,
            .tdSize = 0,
            .interrupterTarget = 0,
            .cycle = endpoint_ring->GetCycle(),
            .evaluateNextTRB = false,
            .interruptOnShortPacket = true,
            .noSnoop = false,
            .chain = false,
            .interruptOnCompletion = true,
            .immediateData = false,
            .blockEventInterrupt = false
        });

        last_sent_trb = endpoint_ring->Enqueue(init_trb);

        RingDoorbell(endpoint * 2 + (is_input ? 1 : 0));

        const bool result = Self().SpinWaitMillsFor(COMPLETION_TIMEOUT_MS, TRANSFER_STATUS_PREDICATE, this);   

        last_sent_trb = nullptr;

        transfer_complete.store(false);

        return Success(result);
    }

    Optional<USB::Driver*> Driver::Create(xHCI::Device& device, uint8_t configurationValue, const xHCI::Device::FunctionDescriptor* function) {
        Log::printfSafe("[BBB] Attempting to initialize bulk-only transport mass storage controller (configuration value: %d)\n\r", configurationValue);
        
        if (!SendRequest(device, 0x21, 0xFF, 0, function->interfaces->interfaceNumber, 0, nullptr, nullptr).IsSuccess()) {
            if constexpr (Debug::DEBUG_BBB_ERRORS) {
                Log::printfSafe("[BBB] Failed to perform bulk-only reset on mass storage controller\r\n");
            }

            return Optional<USB::Driver*>();
        }

        uint8_t max_lun = 0;

        xHCI::TRB::CompletionCode completion_code = xHCI::TRB::CompletionCode::Invalid;

        if (!SendRequest(device, 0xA1, 0xFE, 0, function->interfaces->interfaceNumber, 1, &max_lun, &completion_code).IsSuccess()) {
            if (completion_code == xHCI::TRB::CompletionCode::StallError) {
                max_lun = 0;
            }
            else {
                if constexpr (Debug::DEBUG_BBB_ERRORS) {
                    Log::printfSafe("[BBB] Failed to get max LUN from mass storage controller\r\n");
                }

                return Optional<USB::Driver*>();
            }
        } 

        if (!SetConfiguration(device, configurationValue).IsSuccess()) {
            if constexpr (Debug::DEBUG_BBB_ERRORS) {
                Log::printfSafe("[BBB] Could not configure endpoint 0x%0.2hhx\n\r", configurationValue);
            }

            return Optional<USB::Driver*>();
        }

        auto* config_interface = function->interfaces;

        uint8_t bulk_in_ep  = 0;
        uint8_t bulk_out_ep = 0;

        while (config_interface != nullptr) {
            for (size_t i = 0; i < config_interface->endpointsNumber; ++i) {
                const auto& endpoint = config_interface->endpoints[i];
                
                if (!ConfigureEndpoint(device, endpoint).IsSuccess()) {
                    if constexpr (Debug::DEBUG_BBB_ERRORS) {
                        Log::printfSafe("[BBB] Could not configure endpoint 0x%0.2hhx\n\r", endpoint.endpointAddress);
                    }

                    return Optional<USB::Driver*>();
                }

                if constexpr (Debug::DEBUG_BBB_INFO) {
                    Log::printfSafe("[BBB] Configured endpoint 0x%0.2hhx\n\r", endpoint.endpointAddress);
                }

                if (endpoint.endpointType == xHCI::EndpointType::BulkIn && bulk_in_ep == 0) {
                    bulk_in_ep = endpoint.endpointAddress;
                }
                else if (endpoint.endpointType == xHCI::EndpointType::BulkOut && bulk_out_ep == 0) {
                    bulk_out_ep = endpoint.endpointAddress;
                }
            }

            config_interface = config_interface->next;
        }

        if (bulk_in_ep == 0 || bulk_out_ep == 0) {
            if constexpr (Debug::DEBUG_BBB_ERRORS) {
                Log::putsSafe("[BBB] Could not find required bulk IN and OUT endpoints\n\r");
            }

            return Optional<USB::Driver*>();
        }

        void* phys_io_buffer = PhysicalMemory::Allocate2MB();

        if (phys_io_buffer == nullptr) {
            if constexpr (Debug::DEBUG_BBB_ERRORS) {
                Log::putsSafe("[BBB] Failed to allocate physical memory for I/O buffer\n\r");
            }

            return Optional<USB::Driver*>();
        }

        void* io_buffer = VirtualMemory::MapGeneralPages(
            phys_io_buffer,
            1,
            Shared::Memory::PDE_PAGE_SIZE
                | Shared::Memory::PDE_UNCACHEABLE
                | Shared::Memory::PDE_READWRITE
                | Shared::Memory::PDE_PRESENT
        );

        if (io_buffer == nullptr) {
            PhysicalMemory::Free2MB(phys_io_buffer);

            if constexpr (Debug::DEBUG_BBB_ERRORS) {
                Log::putsSafe("[BBB] Failed to map virtual memory for I/O buffer\n\r");
            }

            return Optional<USB::Driver*>();
        }

        StorageInfo storage_info = {
            .max_lun = max_lun,
            .drivers = static_cast<Storage::Driver**>(Heap::Allocate(sizeof(Storage::Driver*) * (max_lun + 1)))
        };

        if (storage_info.drivers == nullptr) {
            if constexpr (Debug::DEBUG_BBB_ERRORS) {
                Log::printfSafe("[BBB] Failed to allocate memory for storage drivers (max LUN: %d)\n\r", max_lun);
            }

            VirtualMemory::UnmapGeneralPages(io_buffer, 1);
            PhysicalMemory::Free2MB(phys_io_buffer);
            
            return Optional<USB::Driver*>();
        }

        for (size_t i = 0; i <= max_lun; ++i) {
            storage_info.drivers[i] = nullptr;
        }

        Driver* driver = static_cast<Driver*>(Heap::Allocate(sizeof(Driver)));

        if (driver == nullptr) {
            if constexpr (Debug::DEBUG_BBB_ERRORS) {
                Log::printfSafe("[BBB] Failed to allocate memory for bulk-only transport driver\n\r");
            }

            VirtualMemory::UnmapGeneralPages(io_buffer, 1);
            PhysicalMemory::Free2MB(phys_io_buffer);
            Heap::Free(storage_info.drivers);

            return Optional<USB::Driver*>();
        }

        new (driver) Driver(
            device,
            storage_info,
            { .bulkIn = bulk_in_ep, .bulkOut = bulk_out_ep },
            { .pointer = io_buffer, .physical_pointer = phys_io_buffer }
        );

        static constexpr uint8_t TRANSPARENT_SCSI_USB_SUBCLASS = 0x06;

        for (size_t i = 0; i <= max_lun; ++i) {
            switch (function->functionSubClass) {
                case TRANSPARENT_SCSI_USB_SUBCLASS: {
                    const auto scsi_driver_wrapper = Storage::SCSI::Driver::Create(*driver, i);

                    if (!scsi_driver_wrapper.HasValue()) {
                        if constexpr (Debug::DEBUG_BBB_ERRORS) {
                            Log::printfSafe("[BBB] Failed to create SCSI driver for LUN %d\n\r", i);
                        }
                    }
                    else {
                        storage_info.drivers[i] = scsi_driver_wrapper.GetValue();
                    }

                    break;
                }
                default: {
                    if constexpr (Debug::DEBUG_BBB_INFO) {
                        Log::printfSafe("[BBB] Unsupported mass storage subclass 0x%0.2hhx for LUN %d\n\r", function->functionSubClass, i);
                    }

                    break;
                }
            }
        }

        for (size_t i = 0; i <= max_lun; ++i) {
            if (storage_info.drivers[i] != nullptr) {
                return Optional<USB::Driver*>(driver);
            }
        }

        if constexpr (Debug::DEBUG_BBB_ERRORS) {
            Log::putsSafe("[BBB] No compatible storage drivers could be created for any of the LUNs\n\r");
        }

        driver->Release();
        Heap::Free(driver);

        return Optional<USB::Driver*>();
    }

    const xHCI::TRB* Driver::GetAwaitingTRB() const {
        return last_sent_trb;
    }

    void Driver::HandleEvent(const xHCI::TransferEventTRB& trb) {
        last_transfer_result = trb;
        transfer_complete.store(true);
    }

    Success Driver::PostInitialization() {
        for (size_t i = 0; i <= storage_info.max_lun; ++i) {
            if (storage_info.drivers[i] != nullptr) {
                if (!storage_info.drivers[i]->PostInitialization().IsSuccess()) {
                    if constexpr (Debug::DEBUG_BBB_ERRORS) {
                        Log::printfSafe("[BBB] Post-initialization failed for storage driver of LUN %d\n\r", i);
                    }

                    return Failure();
                }
            }
        }

        return Success();
    }

    void Driver::Release() {
        if (storage_info.drivers != nullptr) {
            for (size_t i = 0; i <= storage_info.max_lun; ++i) {
                if (storage_info.drivers[i] != nullptr) {
                    storage_info.drivers[i]->Destroy();
                }
            }

            Heap::Free(storage_info.drivers);
            storage_info.drivers = nullptr;
        }

        if (io_buffer != nullptr) {
            Optional<void*> phys_io_buffer = Paging::GetPhysicalAddress(io_buffer);

            VirtualMemory::UnmapGeneralPages(io_buffer, 1);

            if (phys_io_buffer.HasValue()) {
                PhysicalMemory::Free2MB(phys_io_buffer.GetValue());
            }
        }
    }

    Success Driver::SendCommand(const CommandPayload& payload) {
        ContextBusy busy{this};

        if (!busy.IsValid()) {
            return Failure();
        }

        if (payload.commandLength > GetMaxCommandLength()
            || (payload.dataBuffer == nullptr && payload.dataLength > 0)
            || payload.dataLength > GetMaxDataTransferLength()
        ) {
            return Failure();
        }
        else if (payload.commandLength == 0) {
            return Success();
        }
        else if (payload.commandBuffer == nullptr) {
            return Failure();
        }

        Utils::LockGuard _{driver_lock};

        const CBW cbw = CBW::Create(payload, current_tag++);
        Utils::memcpy(io_buffer, &cbw, cbw.SIZE);

        if (!SendNormalBuffer(cbw.SIZE, endpoints_info.bulkOut, false).IsSuccess()) {
            if constexpr (Debug::DEBUG_BBB_ERRORS) {
                Log::putsSafe("[BBB] Failed to send CBW\r\n");
            }
            while (1);
            return Failure();
        }

        if (payload.dataLength > 0) {
            if (payload.isInputTransfer) {
                if (!SendNormalBuffer(payload.dataLength, endpoints_info.bulkIn, true).IsSuccess()) {
                    if constexpr (Debug::DEBUG_BBB_ERRORS) {
                        Log::putsSafe("[BBB] Failed to receive data buffer\r\n");
                    }

                    return Failure();
                }

                Utils::memcpy(payload.dataBuffer, io_buffer, payload.dataLength);
            }
            else {
                Utils::memcpy(io_buffer, payload.dataBuffer, payload.dataLength);

                if (!SendNormalBuffer(payload.dataLength, endpoints_info.bulkOut, false).IsSuccess()) {
                    if constexpr (Debug::DEBUG_BBB_ERRORS) {
                        Log::putsSafe("[BBB] Failed to send data buffer\r\n");
                    }

                    return Failure();
                }
            }
        }

        CSW csw;

        if (!SendNormalBuffer(csw.SIZE, endpoints_info.bulkIn, true).IsSuccess()) {
            if constexpr (Debug::DEBUG_BBB_ERRORS) {
                Log::putsSafe("[BBB] Failed to receive CSW\r\n");
            }

            return Failure();
        }

        Utils::memcpy(&csw, io_buffer, csw.SIZE);

        if (csw.signature != CSW::SIGNATURE) {
            if constexpr (Debug::DEBUG_BBB_ERRORS) {
                Log::putsSafe("[BBB] Invalid CSW signature received for command\r\n");
            }

            return Failure();
        }
        else if (csw.tag != cbw.tag) {
            if constexpr (Debug::DEBUG_BBB_ERRORS) {
                Log::putsSafe("[BBB] Mismatching tag in CSW received for command\r\n");
            }

            return Failure();
        }
        else if (csw.status != 0) {
            if constexpr (Debug::DEBUG_BBB_ERRORS) {
                Log::printfSafe("[BBB] Command failed with status 0x%0.2hhx\r\n", csw.status);
            }

            return Failure();
        }

        return Success();
    }
}
