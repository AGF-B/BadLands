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

#include <shared/LockGuard.hpp>
#include <shared/Response.hpp>

#include <crypto/crc.hpp>

#include <devices/Block/Device.hpp>

#include <fs/Bridge.hpp>
#include <fs/FileSystem.hpp>

#include <kern/math.hpp>
#include <kern/memory.hpp>
#include <kern/string.hpp>

#include <mm/Heap.hpp>
#include <mm/Utils.hpp>

#include <screen/Log.hpp>

#include <exports.hpp>

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
        using GUID = Devices::Block::GUID;
        
        uint64_t signature;
        uint32_t revision;
        uint32_t header_size;
        mutable uint32_t header_crc32;
        uint32_t reserved;
        uint64_t my_lba;
        uint64_t alternate_lba;
        uint64_t first_usable_lba;
        uint64_t last_usable_lba;
        GUID     disk_guid;
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

    struct GPTPartitionEntry {
        using GUID = Devices::Block::GUID;

        GUID partition_type_guid;
        GUID unique_partition_guid;
        uint64_t starting_lba;
        uint64_t ending_lba;
        uint64_t attributes;
        uint16_t partition_name[36];
    };

    // check that the structure is correctly packed and has the expected size, i.e., can memcpy directly to/from it
    static_assert(sizeof(GPTPartitionEntry) == 128);

    class GPTPartitionFetcher {
    private:
        using Interface = GPT::Interface;

        const GPT* gpt;
        const size_t blockSize;

        Interface* const interface;

        size_t currentPartition     = 0;
        size_t cachedPartitionIndex = 0;
        size_t cachedPartitions;
        size_t cachedBlocks         = 0;
        kern::unique_ptr<uint8_t[]> cached;

        Success UpdateCache(size_t partitionIndex) {
            if (partitionIndex >= gpt->partition_entry_count) {
                return Failure();
            }

            const size_t cache_lba = gpt->partition_entries_lba + partitionIndex * gpt->partition_entry_size / blockSize;

            if (!interface->ReadBlocks(cache_lba, cachedBlocks, cached.get()).IsSuccess()) {
                return Failure();
            }

            cachedPartitionIndex = (cache_lba - gpt->partition_entries_lba) * blockSize / gpt->partition_entry_size;

            return Success();
        }

        bool IsPartitionFullyWithinCache(size_t partitionIndex) const {
            const size_t partition_end = partitionIndex * gpt->partition_entry_size;
            const size_t cache_end = cachedPartitionIndex * gpt->partition_entry_size + cachedBlocks * blockSize;

            return partition_end <= cache_end;
        }

    public:
        GPTPartitionFetcher(const GPT* gpt, size_t blockSize, Interface* interface, size_t cachedPartitions)
            : gpt{gpt}, blockSize{blockSize}, interface{interface}, cachedPartitions{cachedPartitions} {}

        Success Initialize() {
            size_t cache_size = cachedPartitions * gpt->partition_entry_size;

            if (cache_size == 0) {
                cache_size = blockSize;
            }

            cachedBlocks = (cache_size + blockSize - 1) / blockSize;
            cachedPartitions = cachedBlocks * blockSize / gpt->partition_entry_size;

            const size_t cache_lba = gpt->partition_entries_lba;

            cached = kern::make_unique<uint8_t[]>(cachedBlocks * blockSize);

            if (!cached) {
                return Failure();
            }

            if (!interface->ReadBlocks(cache_lba, cachedBlocks, cached.get()).IsSuccess()) {
                return Failure();
            }

            return Success();
        }

        Success Reset() {
            currentPartition = 0,
            cachedPartitionIndex = 0;
            return Initialize();
        }

        bool HasNext() const {
            return currentPartition < gpt->partition_entry_count;
        }
        
        Optional<GPTPartitionEntry> Next() {
            if (!HasNext()) {
                return Optional<GPTPartitionEntry>();
            }

            if (currentPartition >= cachedPartitionIndex + cachedPartitions || !IsPartitionFullyWithinCache(currentPartition)) {
                if (!UpdateCache(currentPartition).IsSuccess()) {
                    return Optional<GPTPartitionEntry>();
                }
            }

            const size_t partition_offset = currentPartition - cachedPartitionIndex;
            const GPTPartitionEntry* entry = reinterpret_cast<const GPTPartitionEntry*>(cached.get() + partition_offset * gpt->partition_entry_size);
            currentPartition++;

            return Optional<GPTPartitionEntry>(*entry);
        }
    };

    constexpr void ByteToHex(uint8_t byte, char& high, char& low) {
        static constexpr char hex_digits[] = "0123456789ABCDEF";

        high = hex_digits[byte >> 4];
        low = hex_digits[byte & 0x0F];
    }

    constexpr char* EmplaceHexByte(uint8_t byte, char* ptr) {
        ByteToHex(byte, *ptr, *(ptr + 1));
        return ptr + 2;
    }
}

namespace Devices::Block {
    kern::static_string Partition::GetFSBridgeName() const {
        // name is guid of the partition in string form, e.g., "12345678-1234-1234-1234-1234567890AB"
        const size_t guid_string_length = 36;

        auto name = kern::make_static_string(guid_string_length);

        if (name) {
            char* ptr = name.get();

            const uint8_t* guid_data = uniqueGUID.data;

            // first 4 bytes in little-endian
            for (size_t i = 0; i < 4; ++i) {
                const uint8_t byte = guid_data[3 - i];
                ptr = EmplaceHexByte(byte, ptr);
            }

            *ptr++ = '-';

            // next 2 bytes in little-endian
            for (size_t i = 0; i < 2; ++i) {
                const uint8_t byte = guid_data[5 - i];
                ptr = EmplaceHexByte(byte, ptr);
            }

            *ptr++ = '-';

            // next 2 bytes in little-endian
            for (size_t i = 0; i < 2; ++i) {
                const uint8_t byte = guid_data[7 - i];
                ptr = EmplaceHexByte(byte, ptr);
            }

            *ptr++ = '-';

            // next 2 bytes in big-endian
            for (size_t i = 8; i < 10; ++i) {
                const uint8_t byte = guid_data[i];
                ptr = EmplaceHexByte(byte, ptr);
            }

            *ptr++ = '-';

            // last 6 bytes in big-endian
            for (size_t i = 10; i < 16; ++i) {
                const uint8_t byte = guid_data[i];
                ptr = EmplaceHexByte(byte, ptr);
            }
        }

        return name;
    }

    kern::static_string Partition::GetName() const {
        // name format is "bdev{deviceId}-{partitionId}"
        const size_t root_size = 4;
        const size_t device_id_size = kern::log(deviceId, 10) + 1;
        const size_t partition_id_size = kern::log(partitionId, 10) + 1;
        const size_t name_length = root_size + device_id_size + 1 + partition_id_size;

        auto name = kern::make_static_string(name_length);

        if (name) {
            char* ptr = name.get();

            Utils::memcpy(ptr, "bdev", 4);

            ptr += 4;

            const size_t device_id_length = kern::log(deviceId, 10) + 1;

            for (size_t i = 0, devId = deviceId; i < device_id_length; ++i, devId /= 10) {
                ptr[device_id_length - 1 - i] = '0' + (devId % 10);
            }

            ptr += device_id_length;
            *ptr++ = '-';

            const size_t partition_id_length = kern::log(partitionId, 10) + 1;

            for (size_t i = 0, partId = partitionId; i < partition_id_length; ++i, partId /= 10) {
                ptr[partition_id_length - 1 - i] = '0' + (partId % 10);
            }
        }

        return name;
    }

    void Partition::TryMountFilesystem(const kern::shared_ptr<Partition>& self) {
        // try to mount a filesystem on the partition, if it fails, it's not a problem, the partition can still be used as a raw block device

        auto filesystem = FileSystem::AutoDetect(kern::static_pointer_cast<FS::IFNode>(self));

        if (filesystem) {
            const auto bridge = kern::make_shared<FS::Bridge>(owner, std::move(filesystem));

            if (bridge) {
                const auto bridge_name = GetFSBridgeName();

                if (bridge_name) {
                    const FS::DirectoryEntry entry{
                        .NameLength = bridge_name.size(),
                        .Name = bridge_name.get()
                    };

                    const auto generic_bridge = kern::static_pointer_cast<FS::IFNode>(bridge);

                    if (Kernel::Exports.partitionInterface->AddNode(entry, generic_bridge) == FS::Status::SUCCESS) {
                        filesystemBridge = kern::static_pointer_cast<FS::IFNode>(bridge);
                    }
                    else {
                        Log::putsSafe("[DEV] Failed to add filesystem bridge to filesystem");
                    }
                }
                else {
                    Log::putsSafe("[DEV] Failed to allocate filesystem bridge name");
                }
            }
            else {
                Log::putsSafe("[DEV] Failed to allocate filesystem bridge");
            }
        }
        else {
            Log::putsSafe("[DEV] No filesystem detected on partition");
        }
    }
    
    FS::Response<size_t> Partition::Read(size_t offset, size_t count, uint8_t* buffer) {
        const uint64_t blockSize    = interface->GetBlockSize();
        const uint64_t startBlock   = offset / blockSize;
        const uint64_t endBlock     = (offset + count + blockSize - 1) / blockSize;
        const uint64_t blocksToRead = endBlock - startBlock;

        if (blocksToRead == 0) {
            return {0};
        }
        else if (offset % blockSize != 0 || count % blockSize != 0) {
            return {FS::Status::INVALID_PARAMETER};
        }
        else if (endBlock > blocksCount) {
            return {FS::Status::OUT_OF_BOUNDS};
        }
        else if (!interface->ReadBlocks(firstBlock + startBlock, blocksToRead, buffer).IsSuccess()) {
            return {FS::Status::DEVICE_ERROR};
        }

        return {blocksToRead * blockSize};
    }

    FS::Response<size_t> Partition::Write(size_t offset, size_t count, const uint8_t* buffer) {
        const uint64_t blockSize        = interface->GetBlockSize();
        const uint64_t startBlock       = offset / blockSize;
        const uint64_t endBlock         = (offset + count + blockSize - 1) / blockSize;
        const uint64_t blocksToWrite    = endBlock - startBlock;

        if (blocksToWrite == 0) {
            return {0};
        }
        else if (offset % blockSize != 0 || count % blockSize != 0) {
            return {FS::Status::INVALID_PARAMETER};
        }
        else if (endBlock > blocksCount) {
            return {FS::Status::OUT_OF_BOUNDS};
        }
        else if (!interface->WriteBlocks(firstBlock + startBlock, blocksToWrite, buffer).IsSuccess()) {
            return {FS::Status::DEVICE_ERROR};
        }

        return {blocksToWrite * blockSize};
    }

    FS::Status Partition::Query(const FS::QueryInfo& info) {
        const Queries query(info.queryId);

        switch (query) {
        case Queries::INVALID:
            return FS::Status::INVALID_PARAMETER;
        
        case Queries::GET_PARTITION_TYPE_GUID:
            if (info.queryResultSize < sizeof(GUID) || info.queryResult == nullptr) {
                return FS::Status::INVALID_PARAMETER;
            }
            Utils::memcpy(info.queryResult, typeGUID.data, sizeof(GUID));

            return FS::Status::SUCCESS;

        case Queries::GET_PARTITION_UNIQUE_GUID:
            if (info.queryResultSize < sizeof(GUID) || info.queryResult == nullptr) {
                return FS::Status::INVALID_PARAMETER;
            }
            Utils::memcpy(info.queryResult, uniqueGUID.data, sizeof(GUID));

            return FS::Status::SUCCESS;

        default:
            return FS::Status::INVALID_PARAMETER;
        }
    }

    void Partition::Unregister() {
        // Signals the partition has been removed from the filesystem
        // The partition will be destroyed once all handles to it are closed
        // Partitions are already removed from filesystem on device unregistration
        // Therefore, this method is a no-op
    }

    void Partition::Eject() {
        // Removes the partition from the filesystem if it is not already removed

        if (!ShouldBeRemoved()) {
            const auto name = GetName();

            if (name) {
                Kernel::Exports.deviceInterface->Remove({
                    .NameLength = name.size(),
                    .Name = name.get()
                });
            }
        }

        if (!filesystemBridge->ShouldBeRemoved()) {
            const auto bridge_name = GetFSBridgeName();

            if (bridge_name) {
                Kernel::Exports.partitionInterface->Remove({
                    .NameLength = bridge_name.size(),
                    .Name = bridge_name.get()
                });
            }
        }

        filesystemBridge = {};
    }

    kern::shared_ptr<Device> Device::AddDevice(const kern::shared_ptr<Interface>& interface) {
        if (!interface) {
            return {};
        }

        auto boot_sector = kern::make_unique<uint8_t[]>(interface->GetBlockSize());

        if (!boot_sector) {
            return {};
        }

        if (!interface->ReadBlocks(0, 1, boot_sector.get()).IsSuccess()) {
            return {};
        }

        auto mbr = MBR::FromBytes(boot_sector.get());

        kern::shared_ptr<Device> device = kern::make_shared<Device>(interface, nextDeviceId++);

        if (!device) {
            Log::putsSafe("[DEV] Failed to allocate block device");
            return {};
        }

        const auto device_name = device->GetName();
        
        if (!device_name) {
            Log::putsSafe("[DEV] Failed to allocate block device name");
            return {};
        }

        if (Kernel::Exports.deviceInterface->AddNode({
            .NameLength = device_name.size(),
            .Name = device_name.get()
        }, kern::static_pointer_cast<FS::IFNode>(device)) != FS::Status::SUCCESS) {
            Log::putsSafe("[DEV] Failed to add block device to filesystem");
            return {};
        }

        // from now on, an error is not fatal, it just prevents partitions from being loaded.

        if (mbr->IsProtective()) {            
            auto gpt_sector = kern::make_unique<uint8_t[]>(interface->GetBlockSize());
            
            if (!gpt_sector) {
                return device;
            }

            static constexpr size_t PRIMARY_GPT_LBA = 1;

            if (!interface->ReadBlocks(PRIMARY_GPT_LBA, 1, gpt_sector.get()).IsSuccess()) {
                return device;
            }

            auto gpt = reinterpret_cast<const GPT*>(gpt_sector.get());

            if (!gpt->IsValid(PRIMARY_GPT_LBA, interface->GetBlockSize(), interface.get())) {
                return device;
            }

            device->SetGUID(gpt->disk_guid, false);

            auto partition_fetcher = GPTPartitionFetcher(gpt, interface->GetBlockSize(), interface.get(), 32);
            
            if (!partition_fetcher.Initialize().IsSuccess()) {
                Log::putsSafe("Failed to initialize GPT partition fetcher");
                return device;
            }

            size_t partition_count = 0;

            while (partition_fetcher.HasNext()) {
                auto entryOpt = partition_fetcher.Next();

                if (!entryOpt.HasValue()) {
                    Log::putsSafe("Failed to fetch GPT partition entry");
                    return device;
                }

                const auto& entry = entryOpt.GetValue();

                if (entry.starting_lba == 0 && entry.ending_lba == 0) {
                    continue;
                }
                else if (entry.starting_lba < gpt->first_usable_lba
                    || entry.ending_lba > gpt->last_usable_lba
                    || entry.starting_lba > entry.ending_lba
                ) {
                    continue;
                }
                else {
                    ++partition_count;
                }
            }

            if (!partition_fetcher.Reset().IsSuccess()) {
                Log::putsSafe("Failed to reset GPT partition fetcher");
                return device;
            }

            device->partitions = kern::make_unique<kern::shared_ptr<Partition>[]>(partition_count);
            device->partitionsCount = partition_count;

            size_t current_partition = 0;

            while (partition_fetcher.HasNext()) {
                auto entryOpt = partition_fetcher.Next();

                if (!entryOpt.HasValue()) {
                    Log::putsSafe("Failed to fetch GPT partition entry");
                    return device;
                }

                const auto& entry = entryOpt.GetValue();

                if (entry.starting_lba == 0 && entry.ending_lba == 0) {
                    continue;
                }
                else if (entry.starting_lba < gpt->first_usable_lba
                    || entry.ending_lba > gpt->last_usable_lba
                    || entry.starting_lba > entry.ending_lba
                ) {
                    continue;
                }

                const uint64_t partition_first_block = entry.starting_lba;
                const uint64_t partition_block_count = entry.ending_lba - entry.starting_lba + 1;

                auto& partition = device->partitions[current_partition];

                partition = kern::make_shared<Partition>(
                    interface,
                    device->GetDeviceId(),
                    current_partition++,
                    partition_first_block,
                    partition_block_count,
                    entry.partition_type_guid,
                    entry.unique_partition_guid
                );

                if (!partition) {
                    Log::putsSafe("[DEV] Failed to allocate block device partition");
                    continue;
                }

                const auto partition_name = partition->GetName();

                if (!partition_name) {
                    Log::putsSafe("[DEV] Failed to allocate block device partition name");
                    continue;
                }
                else {
                    if (Kernel::Exports.deviceInterface->AddNode({
                        .NameLength = partition_name.size(),
                        .Name = partition_name.get()
                    }, static_pointer_cast<FS::IFNode>(partition)) != FS::Status::SUCCESS) {
                        Log::putsSafe("[DEV] Failed to add block device partition to filesystem");
                    }
                }

                partition->TryMountFilesystem(partition);
            }
        }
        else {
            /// TODO: handle MBR partitions
        }

        return device;
    }

    kern::static_string Device::GetName() const {
        // name format is "bdev{deviceId}"
        const size_t root_size = 4;
        const size_t device_id_size = kern::log(deviceId, 10) + 1;
        const size_t name_length = root_size + device_id_size;

        auto name = kern::make_static_string(name_length);

        if (name) {
            char* ptr = name.get();

            Utils::memcpy(ptr, "bdev", 4);

            ptr += 4;

            const size_t device_id_length = kern::log(deviceId, 10) + 1;

            for (size_t i = 0, devId = deviceId; i < device_id_length; ++i, devId /= 10) {
                ptr[device_id_length - 1 - i] = '0' + (devId % 10);
            }
        }

        return name;
    }

    FS::Response<size_t> Device::Read(size_t offset, size_t count, uint8_t* buffer) {
        const uint64_t blockSize = interface->GetBlockSize();
        const uint64_t startBlock = offset / blockSize;
        const uint64_t endBlock = (offset + count + blockSize - 1) / blockSize;
        const uint64_t blocksCount = endBlock - startBlock;

        if (blocksCount == 0) {
            return {0};
        }
        else if (offset % blockSize != 0 || count % blockSize != 0) {
            return {FS::Status::INVALID_PARAMETER};
        }
        else if (endBlock > interface->GetBlocksCount()) {
            return {FS::Status::OUT_OF_BOUNDS};
        }
        else if (!interface->ReadBlocks(startBlock, blocksCount, buffer).IsSuccess()) {
            return {FS::Status::DEVICE_ERROR};
        }

        return {blocksCount * blockSize};
    }

    FS::Response<size_t> Device::Write(size_t offset, size_t count, const uint8_t* buffer) {
        const uint64_t blockSize = interface->GetBlockSize();
        const uint64_t startBlock = offset / blockSize;
        const uint64_t endBlock = (offset + count + blockSize - 1) / blockSize;
        const uint64_t blocksCount = endBlock - startBlock;

        if (blocksCount == 0) {
            return {0};
        }
        else if (offset % blockSize != 0 || count % blockSize != 0) {
            return {FS::Status::INVALID_PARAMETER};
        }
        else if (endBlock > interface->GetBlocksCount()) {
            return {FS::Status::OUT_OF_BOUNDS};
        }
        else if (!interface->WriteBlocks(startBlock, blocksCount, buffer).IsSuccess()) {
            return {FS::Status::DEVICE_ERROR};
        }

        return {blocksCount * blockSize};
    }

    FS::Status Device::Query(const FS::QueryInfo& info) {
        const Queries query(info.queryId);

        switch (query) {
        case Queries::INVALID:
            return FS::Status::INVALID_PARAMETER;

        case Queries::GET_DISK_GUID:
            if (info.queryResultSize < sizeof(GUID) || info.queryResult == nullptr) {
                return FS::Status::INVALID_PARAMETER;
            }
            Utils::memcpy(info.queryResult, diskGUID.data, sizeof(GUID));

            return FS::Status::SUCCESS;

        case Queries::GET_DISK_PARTITION_COUNT:
            if (info.queryResultSize < sizeof(uint64_t) || info.queryResult == nullptr) {
                return FS::Status::INVALID_PARAMETER;
            }
            *reinterpret_cast<uint64_t*>(info.queryResult) = partitionsCount;

            return FS::Status::SUCCESS;
        
        default:
            return FS::Status::INVALID_PARAMETER;
        }
    }

    void Device::Unregister() {
        // Signals the device has been removed from the filesystem
        // Two cases:
        // Interface already ejected: device will be destroyed after this call and once all handles to it are closed
        // Interface not ejected yet: device will be destroyed once the interface is ejected and all handles to the device are closed
        // Partitions are not removed from filesystem on device unregistration, rather on interface ejection
        // Therefore, this method is a no-op
    }

    void Device::Eject() {
        // Signals the device interface has been ejected
        // Two cases:
        // Device already unregistered: device will be destroyed after this call and once all handles to it are closed
        // Device not unregistered yet: device will be destroyed once it is unregistered and all handles to the device are closed
        // Partitions are removed from filesystem on interface ejection
        // Therefore they are removed in this method

        // eject partitions

        for (size_t i = 0; i < partitionsCount; ++i) {
            if (partitions[i]) {
                partitions[i]->Eject();
            }
        }

        // self-eject

        if (!ShouldBeRemoved()) {
            const auto name = GetName();

            if (name) {
                Kernel::Exports.deviceInterface->Remove({
                    .NameLength = name.size(),
                    .Name = name.get()
                });
            }
        }
    }
}
