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

namespace Devices {
    namespace Storage {
        class Controller {
        public:
            struct CommandPayload {
                size_t          commandLength;
                const uint8_t*  commandBuffer;
                size_t          dataLength;
                uint8_t*        dataBuffer;
                bool            isInputTransfer;
                uint8_t         lun;
            };

            virtual size_t GetMaxCommandLength() const = 0;
            virtual size_t GetMaxDataTransferLength() const = 0;        
            virtual Success SendCommand(const CommandPayload& payload) = 0;
        };
    }
}
