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

namespace Devices {
    namespace Block {
        class Device : public FS::File {
        private:
            static inline size_t nextDeviceId = 0;

            Interface* const interface;
            size_t deviceId;

            Device(Interface* interface, size_t deviceId) : FS::File{nullptr}, interface{interface}, deviceId{deviceId} {}

        public:
            static Optional<Device*> AddDevice(Interface* interface);

            virtual FS::Response<size_t> Read(size_t offset, size_t count, uint8_t* buffer) final;
            virtual FS::Response<size_t> Write(size_t offset, size_t count, const uint8_t* buffer) final;

            virtual void Destroy() final;
        };
    }
}
