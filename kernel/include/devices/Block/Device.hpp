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

#include <shared/Response.hpp>

#include <devices/Block/Interface.hpp>

#include <fs/IFNode.hpp>

#include <kern/memory.hpp>

/// TODO: Fix race conditions on destruction of partitions and devices.

namespace Devices {
    namespace Block {
        struct GUID {
            uint8_t data[16];
        };

        class Device;

        class Partition : public FS::File {
        private:
            kern::shared_ptr<Interface> interface;

            const size_t deviceId;
            const size_t partitionId;

            const uint64_t firstBlock;
            const uint64_t blocksCount;

            const GUID typeGUID;
            const GUID uniqueGUID;

        public:
            class Queries {
            public:
                enum : size_t {
                    INVALID,
                    GET_PARTITION_TYPE_GUID,
                    GET_PARTITION_UNIQUE_GUID
                };
                
                decltype(INVALID) value;

                inline constexpr Queries(size_t v) {
                    switch (v) {
                        case 0: value = INVALID; break;
                        case 1: value = GET_PARTITION_TYPE_GUID; break;
                        case 2: value = GET_PARTITION_UNIQUE_GUID; break;
                        default: value = INVALID; break;
                    }
                }

                inline constexpr operator decltype(INVALID) () const {
                    return value;
                }

                inline constexpr bool operator==(const decltype(INVALID)& other) const {
                    return value == other;
                }

                inline constexpr bool operator!=(const decltype(INVALID)& other) const {
                    return value != other;
                }
            };

            inline Partition(
                const kern::shared_ptr<Interface>& interface,
                size_t deviceId,
                size_t partitionId,
                uint64_t firstBlock,
                uint64_t blocksCount,
                const GUID& typeGUID,
                const GUID& uniqueGUID
            )
                : FS::File{nullptr}, interface{interface}, deviceId{deviceId},
                partitionId{partitionId}, firstBlock{firstBlock}, blocksCount{blocksCount},
                typeGUID{typeGUID}, uniqueGUID{uniqueGUID} {}

            inline Partition()
                : FS::File{nullptr}, interface{}, deviceId{0},
                partitionId{0}, firstBlock{0}, blocksCount{0},
                typeGUID{0}, uniqueGUID{0} {}

            inline constexpr size_t GetDeviceId() const { return deviceId; }
            inline constexpr size_t GetPartitionId() const { return partitionId; }
            size_t GetNameLength() const;
            kern::unique_ptr<char[]> GetName() const;

            virtual FS::Response<size_t> Read(size_t offset, size_t count, uint8_t* buffer) final;
            virtual FS::Response<size_t> Write(size_t offset, size_t count, const uint8_t* buffer) final;

            virtual FS::Status Query(const FS::QueryInfo& info) final;

            virtual void Unregister() final;

            void Eject();

            ~Partition() = default;
        };

        class Device : public FS::File {
        private:
            static inline size_t nextDeviceId = 0;

            kern::shared_ptr<Interface> interface;
            size_t deviceId;

            kern::unique_ptr<kern::shared_ptr<Partition>[]> partitions{};
            size_t partitionsCount{0};

            GUID diskGUID;
            bool hasShortGUID{false};

            inline constexpr void SetGUID(const GUID& diskGUID, bool isShortGUID) {
                this->diskGUID = diskGUID;
                hasShortGUID = isShortGUID;
            }

        public:
            class Queries {
            public:
                enum : size_t {
                    INVALID,
                    GET_DISK_GUID,
                    GET_DISK_PARTITION_COUNT
                };
                
                decltype(INVALID) value;

                inline constexpr Queries(size_t v) {
                    switch (v) {
                        case 0: value = INVALID; break;
                        case 1: value = GET_DISK_GUID; break;
                        case 2: value = GET_DISK_PARTITION_COUNT; break;
                        default: value = INVALID; break;
                    }
                }

                inline constexpr operator decltype(INVALID) () const {
                    return value;
                }

                inline constexpr bool operator==(const decltype(INVALID)& other) const {
                    return value == other;
                }

                inline constexpr bool operator!=(const decltype(INVALID)& other) const {
                    return value != other;
                }
            };

            Device(const kern::shared_ptr<Interface>& interface, size_t deviceId)
                : FS::File{nullptr}, interface{interface}, deviceId{deviceId} {}

            static kern::shared_ptr<Device> AddDevice(const kern::shared_ptr<Interface>& interface);

            inline constexpr size_t GetDeviceId() const { return deviceId; }
            size_t GetNameLength() const;
            kern::unique_ptr<char[]> GetName() const;

            virtual FS::Response<size_t> Read(size_t offset, size_t count, uint8_t* buffer) final;
            virtual FS::Response<size_t> Write(size_t offset, size_t count, const uint8_t* buffer) final;

            virtual FS::Status Query(const FS::QueryInfo& info) final;

            virtual void Unregister() final;

            void Eject();

            ~Device() = default;
        };
    }
}
