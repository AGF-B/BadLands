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

#include <shared/Response.hpp>

#include <devices/Block/Interface.hpp>

#include <fs/IFNode.hpp>

#include <kern/memory.hpp>

namespace Devices {
    namespace Block {
        class Device;

        class Partition : public FS::File {
        private:
            Interface* const interface;

            const size_t deviceId;
            const size_t partitionId;

            const uint64_t firstBlock;
            const uint64_t blocksCount;

            const bool valid;
            bool removed{false};

        public:
            Partition(Interface* interface, size_t deviceId, size_t partitionId, uint64_t firstBlock, uint64_t blocksCount)
                : FS::File{nullptr}, interface{interface}, deviceId{deviceId},
                partitionId{partitionId}, firstBlock{firstBlock}, blocksCount{blocksCount}, valid{true} {}

            Partition() : FS::File{nullptr}, interface{nullptr}, deviceId{0}, partitionId{0}, firstBlock{0}, blocksCount{0}, valid{false} {}

            inline constexpr size_t GetDeviceId() const { return deviceId; }
            inline constexpr size_t GetPartitionId() const { return partitionId; }
            size_t GetNameLength() const;
            kern::unique_ptr<char[]> GetName() const;

            bool IsValid() const { return valid; }

            virtual FS::Response<size_t> Read(size_t offset, size_t count, uint8_t* buffer) final;
            virtual FS::Response<size_t> Write(size_t offset, size_t count, const uint8_t* buffer) final;

            // Called by FS when unregistered, not deallocated as owned by the block device. Only marks partition as removed.
            inline constexpr virtual void Destroy() final { removed = true; }
            
            // Called by block device. Removes partition from FS if not yet removed
            void DestroyPartition();
        };

        class Device : public FS::File {
        private:
            static inline size_t nextDeviceId = 0;

            Interface* const interface;
            size_t deviceId;

            kern::unique_ptr<Partition[]> partitions{};
            size_t partitionsCount{0};

            bool removed{false};

        public:
            Device(Interface* interface, size_t deviceId) : FS::File{nullptr}, interface{interface}, deviceId{deviceId} {}

            static Optional<Device*> AddDevice(Interface* interface);

            inline constexpr size_t GetDeviceId() const { return deviceId; }
            size_t GetNameLength() const;
            kern::unique_ptr<char[]> GetName() const;

            virtual FS::Response<size_t> Read(size_t offset, size_t count, uint8_t* buffer) final;
            virtual FS::Response<size_t> Write(size_t offset, size_t count, const uint8_t* buffer) final;

            // Called by FS when unregistered, not deallocated as owned by the device driver. Only marks device as removed.
            inline constexpr virtual void Destroy() final { removed = true; }

            // Called by the device driver when cleaning up, deallocates the device
            void DestroyDevice();
        };
    }
}
