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

#include <devices/USB/Driver.hpp>
#include <devices/USB/xHCI/Device.hpp>

namespace Devices {
    namespace USB {
        namespace MassStorage {
            class Driver : public USB::Driver{
            protected:
                Driver(xHCI::Device& device);

            public:
                static inline constexpr uint8_t GetClassCode() { return 0x08; }

                static Optional<USB::Driver*> Create(xHCI::Device& device, uint8_t configurationValue, const xHCI::Device::FunctionDescriptor* function);
            };
        }
    }
}
