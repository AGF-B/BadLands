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

#include <bit>
#include <new>

#include <shared/Debug.hpp>
#include <shared/Response.hpp>

#include <devices/Storage/Controller.hpp>
#include <devices/Storage/Driver.hpp>
#include <devices/Block/Device.hpp>
#include <devices/Storage/SCSI/Driver.hpp>

#include <mm/Heap.hpp>

#include <screen/Log.hpp>

namespace Devices::Storage::SCSI {
    Optional<Driver::CapacityInformation> Driver::ReadCapacity10() {
        const uint8_t command_buffer[10] = { 0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        
        struct {
            uint32_t blocksCount;
            uint32_t blockSize;
        } raw_response;

        const Controller::CommandPayload payload = {
            .commandLength = sizeof(command_buffer),
            .commandBuffer = command_buffer,
            .dataLength = sizeof(raw_response),
            .dataBuffer = reinterpret_cast<uint8_t*>(&raw_response),
            .isInputTransfer = true,
            .lun = lun
        };

        if (!controller.SendCommand(payload).IsSuccess()) {
            if constexpr (Debug::DEBUG_SCSI_ERRORS) {
                Log::printfSafe("[SCSI] Failed to send READ CAPACITY(10) command for LUN %d\n\r", lun);
            }

            return Optional<CapacityInformation>();
        }

        return Optional(CapacityInformation {
            .blocksCount = std::byteswap(raw_response.blocksCount),
            .blockSize = std::byteswap(raw_response.blockSize)
        });
    }

    Optional<Driver::CapacityInformation> Driver::ReadCapacity16() {
        const uint8_t command_buffer[16] = { 0x9E, 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 12, 0, 0 };

        uint8_t raw_response[12] = { 0 };

        const Controller::CommandPayload payload = {
            .commandLength = sizeof(command_buffer),
            .commandBuffer = command_buffer,
            .dataLength = sizeof(raw_response),
            .dataBuffer = raw_response,
            .isInputTransfer = true,
            .lun = lun
        };

        if (!controller.SendCommand(payload).IsSuccess()) {
            if constexpr (Debug::DEBUG_SCSI_ERRORS) {
                Log::printfSafe("[SCSI] Failed to send READ CAPACITY(16) command for LUN %d\n\r", lun);
            }

            return Optional<CapacityInformation>();
        }

        return Optional(CapacityInformation {
            .blocksCount = std::byteswap(*reinterpret_cast<const uint64_t*>(raw_response)),
            .blockSize = std::byteswap(*reinterpret_cast<const uint32_t*>(raw_response + 8))
        });
    }

    Optional<Driver::CapacityInformation> Driver::ReadCapacity() {
        static constexpr uint64_t CAPACITY_10_OVERFLOW = 0xFFFFFFFF;
        
        auto capacity10 = ReadCapacity10();

        if (!capacity10.HasValue()) {
            return Optional<CapacityInformation>();
        }

        if (capacity10.GetValue().blocksCount == CAPACITY_10_OVERFLOW && controller.GetMaxDataTransferLength() >= 16) {
            use_extended_methods = true;
            return ReadCapacity16();
        }

        return capacity10;
    }

    Success Driver::SendReadCommand(uint64_t startBlock, uint64_t blocksCount, uint8_t* buffer) {
        const uint64_t transferSize = blocksCount * capacity.blockSize;

        if (transferSize > controller.GetMaxDataTransferLength()) {
            if constexpr (Debug::DEBUG_SCSI_ERRORS) {
                Log::printfSafe("[SCSI] Read transfer size too large for LUN %d: %d bytes\n\r", lun, transferSize);
            }

            return Failure();
        }

        if (!use_extended_methods) {
            if (blocksCount > 0xFFFF) {
                if constexpr (Debug::DEBUG_SCSI_ERRORS) {
                    Log::printfSafe("[SCSI] Read blocks count too large for READ(10) command for LUN %d: %d blocks\n\r", lun, blocksCount);
                }

                return Failure();
            }

            const uint8_t command_buffer[10] = {
                0x28,
                0,
                static_cast<uint8_t>(startBlock >> 24),
                static_cast<uint8_t>(startBlock >> 16),
                static_cast<uint8_t>(startBlock >> 8),
                static_cast<uint8_t>(startBlock),
                0,
                static_cast<uint8_t>(blocksCount >> 8),
                static_cast<uint8_t>(blocksCount),
                0
            };

            const Controller::CommandPayload payload = {
                .commandLength = sizeof(command_buffer),
                .commandBuffer = command_buffer,
                .dataLength = transferSize,
                .dataBuffer = buffer,
                .isInputTransfer = true,
                .lun = lun
            };

            if (!controller.SendCommand(payload).IsSuccess()) {
                if constexpr (Debug::DEBUG_SCSI_ERRORS) {
                    Log::printfSafe("[SCSI] Failed to send READ(10) command for LUN %d\n\r", lun);
                }

                return Failure();
            }
        }
        else {
            if (blocksCount > 0xFFFFFFFF) {
                if constexpr (Debug::DEBUG_SCSI_ERRORS) {
                    Log::printfSafe("[SCSI] Read blocks count too large for READ(16) command for LUN %d: %d blocks\n\r", lun, blocksCount);
                }

                return Failure();
            }

            const uint8_t command_buffer[16] = {
                0x88,
                0,
                static_cast<uint8_t>(startBlock >> 56),
                static_cast<uint8_t>(startBlock >> 48),
                static_cast<uint8_t>(startBlock >> 40),
                static_cast<uint8_t>(startBlock >> 32),
                static_cast<uint8_t>(startBlock >> 24),
                static_cast<uint8_t>(startBlock >> 16),
                static_cast<uint8_t>(startBlock >> 8),
                static_cast<uint8_t>(startBlock),
                static_cast<uint8_t>(blocksCount >> 24),
                static_cast<uint8_t>(blocksCount >> 16),
                static_cast<uint8_t>(blocksCount >> 8),
                static_cast<uint8_t>(blocksCount),
                0,
                0
            };

            const Controller::CommandPayload payload = {
                .commandLength = sizeof(command_buffer),
                .commandBuffer = command_buffer,
                .dataLength = transferSize,
                .dataBuffer = buffer,
                .isInputTransfer = true,
                .lun = lun
            };

            if (!controller.SendCommand(payload).IsSuccess()) {
                if constexpr (Debug::DEBUG_SCSI_ERRORS) {
                    Log::printfSafe("[SCSI] Failed to send READ(16) command for LUN %d\n\r", lun);
                }

                return Failure();
            }
        }

        return Success();
    }

    Success Driver::SendWriteCommand(uint64_t startBlock, uint64_t blocksCount, const uint8_t* buffer) {
        const uint64_t transferSize = blocksCount * capacity.blockSize;

        if (transferSize > controller.GetMaxDataTransferLength()) {
            if constexpr (Debug::DEBUG_SCSI_ERRORS) {
                Log::printfSafe("[SCSI] Read transfer size too large for LUN %d: %d bytes\n\r", lun, transferSize);
            }

            return Failure();
        }

        if (!use_extended_methods) {
            if (blocksCount > 0xFFFF) {
                if constexpr (Debug::DEBUG_SCSI_ERRORS) {
                    Log::printfSafe("[SCSI] Write blocks count too large for WRITE(10) command for LUN %d: %d blocks\n\r", lun, blocksCount);
                }

                return Failure();
            }

            const uint8_t command_buffer[10] = {
                0x2A,
                0,
                static_cast<uint8_t>(startBlock >> 24),
                static_cast<uint8_t>(startBlock >> 16),
                static_cast<uint8_t>(startBlock >> 8),
                static_cast<uint8_t>(startBlock),
                0,
                static_cast<uint8_t>(blocksCount >> 8),
                static_cast<uint8_t>(blocksCount),
                0
            };

            const Controller::CommandPayload payload = {
                .commandLength = sizeof(command_buffer),
                .commandBuffer = command_buffer,
                .dataLength = transferSize,
                .dataBuffer = const_cast<uint8_t*>(buffer),
                .isInputTransfer = false,
                .lun = lun
            };

            if (!controller.SendCommand(payload).IsSuccess()) {
                if constexpr (Debug::DEBUG_SCSI_ERRORS) {
                    Log::printfSafe("[SCSI] Failed to send WRITE(10) command for LUN %d\n\r", lun);
                }

                return Failure();
            }
        }
        else {
            if (blocksCount > 0xFFFFFFFF) {
                if constexpr (Debug::DEBUG_SCSI_ERRORS) {
                    Log::printfSafe("[SCSI] Write blocks count too large for WRITE(16) command for LUN %d: %d blocks\n\r", lun, blocksCount);
                }

                return Failure();
            }

            const uint8_t command_buffer[16] = {
                0x8A,
                0,
                static_cast<uint8_t>(startBlock >> 56),
                static_cast<uint8_t>(startBlock >> 48),
                static_cast<uint8_t>(startBlock >> 40),
                static_cast<uint8_t>(startBlock >> 32),
                static_cast<uint8_t>(startBlock >> 24),
                static_cast<uint8_t>(startBlock >> 16),
                static_cast<uint8_t>(startBlock >> 8),
                static_cast<uint8_t>(startBlock),
                static_cast<uint8_t>(blocksCount >> 24),
                static_cast<uint8_t>(blocksCount >> 16),
                static_cast<uint8_t>(blocksCount >> 8),
                static_cast<uint8_t>(blocksCount),
                0,
                0
            };

            const Controller::CommandPayload payload = {
                .commandLength = sizeof(command_buffer),
                .commandBuffer = command_buffer,
                .dataLength = transferSize,
                .dataBuffer = const_cast<uint8_t*>(buffer),
                .isInputTransfer = false,
                .lun = lun
            };

            if (!controller.SendCommand(payload).IsSuccess()) {
                if constexpr (Debug::DEBUG_SCSI_ERRORS) {
                    Log::printfSafe("[SCSI] Failed to send WRITE(16) command for LUN %d\n\r", lun);
                }

                return Failure();
            }
        }

        return Success();
    }

    Optional<Driver*> Driver::Create(Storage::Controller& controller, uint8_t lun) {
        void* driver_memory = Heap::Allocate(sizeof(Driver));

        if (driver_memory == nullptr) {
            if constexpr (Debug::DEBUG_SCSI_ERRORS) {
                Log::printfSafe("[SCSI] Failed to allocate memory for driver of LUN %d\n\r", lun);
            }

            return Optional<Driver*>();
        }

        Driver* driver = new (driver_memory) Driver(controller, lun);

        return Optional(driver);
    }

    void Driver::Destroy() {
        Heap::Free(this);
    }

    Success Driver::PostInitialization() {
        const auto capacity = ReadCapacity();

        if (!capacity.HasValue()) {
            if constexpr (Debug::DEBUG_SCSI_ERRORS) {
                Log::printfSafe("[SCSI] Failed to read capacity information for LUN %d\n\r", lun);
            }

            return Failure();
        }        

        this->capacity = capacity.GetValue();
        
        if (this->capacity.blocksCount == 0 || this->capacity.blockSize == 0) {
            if constexpr (Debug::DEBUG_SCSI_ERRORS) {
                Log::printfSafe("[SCSI] Invalid capacity information read for LUN %d: blocks count = %d, block size = %d\n\r", lun, this->capacity.blocksCount, this->capacity.blockSize);
            }

            return Failure();
        }

        Block::Device::AddDevice(this);

        return Success();
    }

    Success Driver::ReadBlocks(uint64_t startBlock, uint64_t blocksCount, uint8_t* buffer) {
        uint64_t transferSize = blocksCount * capacity.blockSize;
        const uint64_t maxAtomicTransferSize = controller.GetMaxDataTransferLength();

        while (transferSize > maxAtomicTransferSize) {
            uint64_t blocksToTransfer = maxAtomicTransferSize / capacity.blockSize;

            if (!SendReadCommand(startBlock, blocksToTransfer, buffer).IsSuccess()) {
                return Failure();
            }

            startBlock += blocksToTransfer;
            buffer += blocksToTransfer * capacity.blockSize;
            transferSize -= blocksToTransfer * capacity.blockSize;
        }

        if (transferSize > 0) {
            const uint64_t remainingBlocksCount = (transferSize + capacity.blockSize - 1) / capacity.blockSize;

            if (!SendReadCommand(startBlock, remainingBlocksCount, buffer).IsSuccess()) {
                return Failure();
            }
        }

        return Success();
    }

    Success Driver::WriteBlocks(uint64_t startBlock, uint64_t blocksCount, const uint8_t* buffer) {
        uint64_t transferSize = blocksCount * capacity.blockSize;
        const uint64_t maxAtomicTransferSize = controller.GetMaxDataTransferLength();

        while (transferSize > maxAtomicTransferSize) {
            uint64_t blocksToTransfer = maxAtomicTransferSize / capacity.blockSize;

            if (!SendWriteCommand(startBlock, blocksToTransfer, buffer).IsSuccess()) {
                return Failure();
            }

            startBlock += blocksToTransfer;
            buffer += blocksToTransfer * capacity.blockSize;
            transferSize -= blocksToTransfer * capacity.blockSize;
        }

        if (transferSize > 0) {
            const uint64_t remainingBlocksCount = (transferSize + capacity.blockSize - 1) / capacity.blockSize;

            if (!SendWriteCommand(startBlock, remainingBlocksCount, buffer).IsSuccess()) {
                return Failure();
            }
        }

        return Success();
    }
}
