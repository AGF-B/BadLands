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

#include <shared/Response.hpp>

#include <devices/Block/Device.hpp>

#include <kern/memory.hpp>

#include <mm/Heap.hpp>
#include <mm/Utils.hpp>

#include <screen/Log.hpp>

namespace {
    struct MBR {
        static constexpr size_t BOOT_CODE_LENGTH = 440;
        static constexpr size_t DISK_SIGNATURE_OFFSET = BOOT_CODE_LENGTH;
        static constexpr size_t PARTITION_ENTRIES_OFFSET = DISK_SIGNATURE_OFFSET + sizeof(uint32_t) + 2;
        static constexpr size_t PARTITION_ENTRY_SIZE = 16;
        static constexpr size_t SIGNATURE_OFFSET = 510;

        uint8_t boot_code[BOOT_CODE_LENGTH];

        uint32_t disk_signature;
        
        struct PartitionEntry {
            uint8_t boot_indicator;
            uint8_t start_chs[3];
            uint8_t partition_type;
            uint8_t end_chs[3];
            uint32_t start_lba;
            uint32_t lba_count;
        } partitions[4];

        uint16_t signature;
    };

    kern::unique_ptr<MBR> ParseMBR(const uint8_t* data) {
        auto mbr = kern::make_unique<MBR>();

        if (!mbr) {
            return kern::unique_ptr<MBR>();
        }

        Utils::memcpy(&mbr->boot_code, data, MBR::BOOT_CODE_LENGTH);
        
        mbr->disk_signature = *reinterpret_cast<const uint32_t*>(data + MBR::DISK_SIGNATURE_OFFSET);

        for (size_t i = 0; i < 4; ++i) {
            const size_t offset = MBR::PARTITION_ENTRIES_OFFSET + i * MBR::PARTITION_ENTRY_SIZE;
            
            mbr->partitions[i].boot_indicator = data[offset];
            Utils::memcpy(&mbr->partitions[i].start_chs, &data[offset + 1], 3);
            mbr->partitions[i].partition_type = data[offset + 4];
            Utils::memcpy(&mbr->partitions[i].end_chs, &data[offset + 5], 3);
            mbr->partitions[i].start_lba = *reinterpret_cast<const uint32_t*>(&data[offset + 8]);
            mbr->partitions[i].lba_count = *reinterpret_cast<const uint32_t*>(&data[offset + 12]);
        }

        mbr->signature = *reinterpret_cast<const uint16_t*>(&data[MBR::SIGNATURE_OFFSET]);

        if (mbr->signature != 0xAA55) {
            return kern::unique_ptr<MBR>();
        }

        return mbr;
    }
}

namespace Devices::Block {
    Optional<Device*> Device::AddDevice(Interface* interface) {
        if (interface == nullptr) {
            return Optional<Device*>();
        }

        auto boot_sector = kern::make_unique<uint8_t[]>(interface->GetBlockSize());

        if (!boot_sector) {
            return Optional<Device*>();
        }

        if (!interface->ReadBlocks(0, 1, boot_sector.get()).IsSuccess()) {
            return Optional<Device*>();
        }

        auto mbr = ParseMBR(boot_sector.get());

        
    }

    FS::Response<size_t> Device::Read(size_t offset, size_t count, uint8_t* buffer) {
        const uint64_t blockSize = interface->GetBlockSize();
        const uint64_t startBlock = offset / blockSize;
        const uint64_t endBlock = (offset + count + blockSize - 1) / blockSize;
        const uint64_t blocksCount = endBlock - startBlock;

        if (blocksCount == 0) {
            return FS::Response<size_t>(0);
        }
        else if (offset % blockSize != 0 || count % blockSize != 0) {
            return FS::Response<size_t>(FS::Status::INVALID_PARAMETER);
        }
        else if (endBlock > interface->GetBlocksCount()) {
            return FS::Response<size_t>(FS::Status::OUT_OF_BOUNDS);
        }
        else if (!interface->ReadBlocks(startBlock, blocksCount, buffer).IsSuccess()) {
            return FS::Response<size_t>(FS::Status::DEVICE_ERROR);
        }

        return FS::Response<size_t>(blocksCount * blockSize);
    }

    FS::Response<size_t> Device::Write(size_t offset, size_t count, const uint8_t* buffer) {
        const uint64_t blockSize = interface->GetBlockSize();
        const uint64_t startBlock = offset / blockSize;
        const uint64_t endBlock = (offset + count + blockSize - 1) / blockSize;
        const uint64_t blocksCount = endBlock - startBlock;

        if (blocksCount == 0) {
            return FS::Response<size_t>(0);
        }
        else if (offset % blockSize != 0 || count % blockSize != 0) {
            return FS::Response<size_t>(FS::Status::INVALID_PARAMETER);
        }
        else if (endBlock > interface->GetBlocksCount()) {
            return FS::Response<size_t>(FS::Status::OUT_OF_BOUNDS);
        }
        else if (!interface->WriteBlocks(startBlock, blocksCount, buffer).IsSuccess()) {
            return FS::Response<size_t>(FS::Status::DEVICE_ERROR);
        }

        return FS::Response<size_t>(blocksCount * blockSize);
    }
}
