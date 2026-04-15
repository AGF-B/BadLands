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
#include <devices/Block/Device.hpp>
#include <devices/Storage/Controller.hpp>
#include <devices/Storage/Driver.hpp>

namespace Devices {
    namespace Storage {
        namespace SCSI {
            class Driver : public Storage::Driver, public Block::Interface {
            private:
                struct CapacityInformation {
                    uint64_t blocksCount;
                    uint64_t blockSize;
                };

                Storage::Controller& controller;
                const uint8_t lun;

                CapacityInformation capacity = {
                    .blocksCount = 0,
                    .blockSize = 0
                };

                bool use_extended_methods = false;

                Block::Device* device{nullptr};

                inline constexpr Driver(Storage::Controller& controller, uint8_t lun) : controller{controller}, lun{lun} { }

                Optional<CapacityInformation> ReadCapacity10();
                Optional<CapacityInformation> ReadCapacity16();
                Optional<CapacityInformation> ReadCapacity();

                Success SendReadCommand(uint64_t startBlock, uint64_t blocksCount, uint8_t* buffer);
                Success SendWriteCommand(uint64_t startBlock, uint64_t blocksCount, const uint8_t* buffer);

            public:
                static Optional<Driver*> Create(Storage::Controller& controller, uint8_t lun);

                virtual void Destroy() final;

                virtual Success PostInitialization() final;

                inline constexpr uint8_t GetLUN() const {
                    return lun;
                }

                virtual inline constexpr uint64_t GetBlocksCount() const final {
                    return capacity.blocksCount;
                }

                virtual inline constexpr uint64_t GetBlockSize() const final {
                    return capacity.blockSize;
                }

                virtual Success ReadBlocks(uint64_t startBlock, uint64_t blocksCount, uint8_t* buffer) final;
                virtual Success WriteBlocks(uint64_t startBlock, uint64_t blocksCount, const uint8_t* buffer) final;
            };
        }
    }
}
