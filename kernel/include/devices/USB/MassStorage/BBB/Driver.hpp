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

#include <shared/Lock.hpp>
#include <shared/Response.hpp>
#include <shared/SimpleAtomic.hpp>

#include <devices/Storage/Controller.hpp>
#include <devices/Storage/Driver.hpp>
#include <devices/USB/Driver.hpp>
#include <devices/USB/MassStorage/Driver.hpp>
#include <devices/USB/xHCI/Device.hpp>

namespace Devices {
    namespace USB {
        namespace MassStorage {
            namespace BBB {
                class Driver : public MassStorage::Driver, public Storage::Controller {
                private:
                    struct StorageInfo {
                        size_t max_lun = 0;
                        Storage::Driver** drivers = nullptr;
                    };

                    struct EndpointsInfo {
                        uint8_t bulkIn;
                        uint8_t bulkOut;
                    };

                    struct IOBufferInfo {
                        void* pointer;
                        void* physical_pointer;
                    };

                    struct CBW {
                        uint32_t signature;
                        uint32_t tag;
                        uint32_t dataTransferLength;
                        uint8_t flags;
                        uint8_t lun;
                        uint8_t commandLength;
                        uint8_t commandBlock[16];

                        static constexpr uint32_t SIGNATURE             = 0x43425355;
                        static constexpr size_t   MAX_COMMAND_LENGTH    = 16;
                        static constexpr size_t   SIZE                  = 31;
                        
                        inline static constexpr CBW Create(const CommandPayload& payload, uint32_t tag) {
                            CBW cbw;

                            cbw.signature = CBW::SIGNATURE;
                            cbw.tag = tag;
                            cbw.dataTransferLength = payload.dataLength;
                            cbw.flags = (payload.isInputTransfer && payload.dataLength > 0) ? 0x80 : 0x00;
                            cbw.lun = payload.lun;
                            cbw.commandLength = static_cast<uint8_t>(payload.commandLength);

                            for (size_t i = 0; i < payload.commandLength && i < MAX_COMMAND_LENGTH; ++i) {
                                cbw.commandBlock[i] = payload.commandBuffer[i];
                            }

                            return cbw;
                        }
                    };

                    struct CSW {
                        uint32_t signature;
                        uint32_t tag;
                        uint32_t dataResidue;
                        uint8_t status;

                        static constexpr uint32_t SIGNATURE = 0x53425355;
                        static constexpr size_t   SIZE      = 13;
                    };

                    static constexpr size_t MAX_DATA_TRANSFER_LENGTH = 2 * 1024 * 1024; // 2MB, size of the IO buffer

                    StorageInfo storage_info;
                    EndpointsInfo endpoints_info;
                    uint32_t current_tag = 0;
                    void* const io_buffer;
                    void* const phys_io_buffer;
                    const xHCI::TRB* volatile last_sent_trb = nullptr;
                    xHCI::TransferEventTRB last_transfer_result{};
                    Utils::SimpleAtomic<bool> transfer_complete{false};
                    Utils::Lock driver_lock;

                    Driver(
                        xHCI::Device& device,
                        const StorageInfo& storage_info,
                        const EndpointsInfo& endpoints_info,
                        const IOBufferInfo& io_buffer_info
                    );

                    Success ResetRecovery();
                    Success SendNormalBuffer(uint32_t length, uint8_t endpoint, bool is_input);

                public:
                    static Optional<USB::Driver*> Create(xHCI::Device& device, uint8_t configurationValue, const xHCI::Device::FunctionDescriptor* function);

                    virtual const xHCI::TRB* GetAwaitingTRB() const final;
                    virtual void HandleEvent(const xHCI::TransferEventTRB& trb) final;
                    virtual Success PostInitialization() final;
                    virtual void Release() final;

                    virtual inline constexpr size_t GetMaxCommandLength() const final { return CBW::MAX_COMMAND_LENGTH; }
                    virtual inline constexpr size_t GetMaxDataTransferLength() const final { return MAX_DATA_TRANSFER_LENGTH; }
                    virtual Success SendCommand(const CommandPayload& payload) final;
                };
            }
        }
    }
}
