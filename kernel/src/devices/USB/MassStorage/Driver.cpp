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

#include <shared/Debug.hpp>

#include <devices/USB/MassStorage/Driver.hpp>
#include <devices/USB/MassStorage/BBB/Driver.hpp>
#include <devices/USB/xHCI/Device.hpp>
#include <devices/USB/xHCI/TRB.hpp>

#include <screen/Log.hpp>

namespace Devices::USB::MassStorage {
    Driver::Driver(xHCI::Device& device) : USB::Driver{device} { }

    Optional<USB::Driver*> Driver::Create(xHCI::Device& device, uint8_t configurationValue, const xHCI::Device::FunctionDescriptor* function) {        
        static constexpr uint8_t BBB_PROTOCOL = 0x50;
        
        switch (function->functionProtocol) {
            case BBB_PROTOCOL:
                return BBB::Driver::Create(device, configurationValue, function);
            default:
                if constexpr (Debug::DEBUG_USB_ERRORS) {
                    Log::printfSafe("[USB] Unsupported Mass Storage protocol 0x%0.2hhx\r\n", function->functionProtocol);
                }

                return Optional<USB::Driver*>();
        }
    }
}
