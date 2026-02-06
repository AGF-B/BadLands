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

#include <devices/USB/xHCI/Device.hpp>
#include <devices/USB/xHCI/TRB.hpp>

namespace Devices {
    namespace USB {
        class Driver {
        private:
            const xHCI::Device& device;

        protected:
            static inline const decltype(xHCI::Device::SendRequest)& SendRequest = xHCI::Device::SendRequest;
            static inline const decltype(xHCI::Device::SetConfiguration)& SetConfiguration = xHCI::Device::SetConfiguration;
            static inline const decltype(xHCI::Device::ConfigureEndpoint)& ConfigureEndpoint = xHCI::Device::ConfigureEndpoint;

            virtual xHCI::TransferRing* GetEndpointTransferRing(uint8_t endpointAddress, bool isIn) const final;
            virtual void RingDoorbell(uint8_t doorbellID) const final;

        public:
            Driver(const xHCI::Device& device);

            virtual const xHCI::TRB* GetAwaitingTRB() const = 0;
            virtual void HandleEvent() = 0;
            virtual Success PostInitialization() = 0;
            virtual void Release() = 0;
        };
    }
}
