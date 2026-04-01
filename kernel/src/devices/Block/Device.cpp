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

#include <crypto/crc.hpp>

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

            constexpr bool IsZero() const {
                return boot_indicator == 0
                && start_chs[0] == 0 && start_chs[1] == 0 && start_chs[2] == 0
                && partition_type == 0
                && end_chs[0] == 0 && end_chs[1] == 0 && end_chs[2] == 0
                && start_lba == 0
                && lba_count == 0;
            }
        } partitions[4];

        uint16_t signature;

        static constexpr kern::unique_ptr<MBR> FromBytes(const uint8_t* data) {
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

        constexpr bool IsProtective() const {
            int non_empty_partition = -1;

            for (int i = 0; i < 4; ++i) {
                if (!partitions[i].IsZero()) {
                    if (non_empty_partition != -1) {
                        return false;
                    }

                    non_empty_partition = i;
                }
            }

            if (non_empty_partition != -1) {
                const auto& partition = partitions[non_empty_partition];

                static constexpr uint8_t PROTECTIVE_MBR_CHS_HEAD = 2;
                static constexpr uint8_t PROTECTIVE_MBR_PARTITION_TYPE = 0xEE;
                static constexpr uint32_t PROTECTIVE_MBR_FIRST_LBA = 1;

                return partition.start_chs[0] == 0
                    && partition.start_chs[1] == PROTECTIVE_MBR_CHS_HEAD
                    && partition.start_chs[2] == 0
                    && partition.partition_type == PROTECTIVE_MBR_PARTITION_TYPE
                    && partition.start_lba == PROTECTIVE_MBR_FIRST_LBA;
            }

            return false;
        }
    };

    struct GPT {
        uint64_t signature;
        uint32_t revision;
        uint32_t header_size;
        mutable uint32_t header_crc32;
        uint32_t reserved;
        uint64_t my_lba;
        uint64_t alternate_lba;
        uint64_t first_usable_lba;
        uint64_t last_usable_lba;
        uint8_t  disk_guid[16];
        uint64_t partition_entries_lba;
        uint32_t partition_entry_count;
        uint32_t partition_entry_size;
        uint32_t partition_entries_crc32;
        uint32_t reserved2[];

        using Interface = Devices::Block::Interface;

        constexpr bool IsPartitionArrayValid(size_t block_size, Interface* interface) const {
            if (partition_entry_size < 128 || partition_entry_size > block_size) {
                return false;
            }
            else if (partition_entry_count == 0) {
                return false;
            }
            else if (partition_entries_lba < 2 || partition_entries_lba >= interface->GetBlocksCount()) {
                return false;
            }

            static constexpr size_t BATCH_SIZE = 32;

            const size_t batch_array_size = BATCH_SIZE * partition_entry_size;
            const size_t batch_array_blocks = (batch_array_size + block_size - 1) / block_size;
            const size_t batch_count = (partition_entry_count + BATCH_SIZE - 1) / BATCH_SIZE;

            auto batch_buffer = kern::make_unique<uint8_t[]>(batch_array_blocks * block_size);

            if (!batch_buffer) {
                return false;
            }

            auto crc_processor = Crypto::CRC32Engine();

            for (size_t batch_id = 0; batch_id < batch_count; ++batch_id) {
                const size_t batch_offset = batch_id * batch_array_size;
                const size_t remaining_entries = partition_entry_count - batch_offset / partition_entry_size;
                const size_t batch_entries = BATCH_SIZE < remaining_entries ? BATCH_SIZE : remaining_entries;
                const size_t batch_size = batch_entries * partition_entry_size;
                const size_t batch_blocks = (batch_size + block_size - 1) / block_size;
                const size_t batch_lba = partition_entries_lba + batch_id * batch_array_blocks;

                if (!interface->ReadBlocks(batch_lba, batch_blocks, batch_buffer.get()).IsSuccess()) {
                    return false;
                }

                crc_processor.Update(batch_buffer.get(), batch_size);
            }

            const uint32_t expected = partition_entries_crc32;
            const uint32_t actual = crc_processor.Finalize();

            return expected == actual;
        }

        constexpr bool IsValid(size_t lba, size_t block_size, Interface* interface) const {
            static constexpr uint64_t GPT_SIGNATURE = 0x5452415020494645;

            if (signature != GPT_SIGNATURE) {
                return false;
            }
            else if (header_size < 92 || header_size > block_size) {
                return false;
            }
            else if (my_lba != lba) {
                return false;
            }
            
            const uint8_t* ptr = reinterpret_cast<const uint8_t*>(this);

            const uint32_t expected = header_crc32;
            header_crc32 = 0;

            const uint32_t actual = Crypto::CRC32(ptr, header_size);
            header_crc32 = expected;

            if (actual != expected) {
                return false;
            }

            return IsPartitionArrayValid(block_size, interface);
        };
    };
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

        auto mbr = MBR::FromBytes(boot_sector.get());

        if (mbr->IsProtective()) {            
            auto gpt_sector = kern::make_unique<uint8_t[]>(interface->GetBlockSize());
            
            if (!gpt_sector) {
                return Optional<Device*>();
            }

            static constexpr size_t PRIMARY_GPT_LBA = 1;

            if (!interface->ReadBlocks(PRIMARY_GPT_LBA, 1, gpt_sector.get()).IsSuccess()) {
                return Optional<Device*>();
            }

            auto gpt = reinterpret_cast<const GPT*>(gpt_sector.get());

            if (!gpt->IsValid(PRIMARY_GPT_LBA, interface->GetBlockSize(), interface)) {
                return Optional<Device*>();
            }
        }

        return Optional<Device*>();
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
