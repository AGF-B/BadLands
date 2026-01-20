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

#include <devices/USB/HID/Device.hpp>

namespace Devices {
    namespace USB {
        namespace HID {
            class Keyboard final : public InterfaceDevice {
            private:
                struct Item {
                    bool isConstant             = false;
                    uint32_t usage_page         = 0;
                    uint32_t usage_minimum      = 0;
                    uint32_t usage_maximum      = 0;
                    uint32_t logical_minimum    = 0;
                    uint32_t logical_maximum    = 0;
                    uint32_t offset             = 0;
                    uint32_t size               = 0;
                    uint32_t count              = 0;
                    Item* next                  = nullptr;
                };

                struct Report {
                    uint32_t id         = 0;
                    Item* items         = nullptr;
                    Report* next        = nullptr;

                    Report* Release();

                    Success AddItem(const Item& item);
                    size_t GetReportSize() const;
                };

                struct ReportCollection {
                    ReportCollection* next = nullptr;
                    ReportCollection* parent = nullptr;

                    Report* inputReports = nullptr;
                    Report* outputReports = nullptr;

                    ReportCollection* Release();

                    Report* GetReport(uint32_t reportID, bool input);
                    Optional<Report*> AddReport(uint32_t reportID, bool input);
                };

                ReportCollection* collections       = nullptr;
                ReportCollection* currentCollection = nullptr;

                mutable size_t max_report_size = 0;

                static inline constexpr size_t BITMAP_SIZE          = 256;
                static inline constexpr size_t BITMAP_ENTRY_BITS    = 64;
                static inline constexpr size_t BITMAP_ENTRIES       = BITMAP_SIZE / BITMAP_ENTRY_BITS;

                static inline constexpr size_t KEY_REPEAT_DELAY_MS  = 500;
                static inline constexpr size_t KEY_REPEAT_RATE_MS   = 50;

                uint64_t current_keys_bitmap[BITMAP_ENTRIES] = { 0 };
                uint64_t previous_keys_bitmap[BITMAP_ENTRIES] = { 0 };

                uint64_t keys_press_timestamp[BITMAP_SIZE] = { 0 };
                uint64_t keys_repeat_timestamp[BITMAP_SIZE] = { 0 };

                Success AddReportItem(const HIDState& state, const IOConfiguration& config, bool input);
                
                bool                    KeyStateChanged(uint32_t usage) const;
                void                    SetKeyPressed(uint32_t usage);
                void                    RollBitmapsOver();
                static bool             RepeatableUsage(uint32_t usage);
                uint8_t                 GetKeypoint(uint32_t usage) const;
                void                    HandleKeyUsage(uint32_t usage, bool pressed);
                uint8_t                 GetKeyFlags() const;  
                void                    OnKeyPressed(uint32_t usage, uint8_t flags, uint64_t timestamp);
                void                    OnKeyReleased(uint32_t usage, uint8_t flags);
                void                    OnKeyHeld(uint32_t usage, uint8_t flags, uint64_t timestamp);
                void                    UpdateKeys(uint8_t report_id, const uint8_t* data, size_t length);

            public:
                inline Keyboard() : InterfaceDevice(DeviceClass::KEYBOARD) { }

                virtual void Release() override;

                virtual bool IsUsageSupported(uint32_t page, uint32_t usage) override;
                virtual bool IsReportSupported(uint32_t reportID, bool input) override;

                virtual size_t GetMaxReportSize() const override;

                virtual Success AddInput(const HIDState& state, const IOConfiguration& config) override;
                virtual Success AddOutput(const HIDState& state, const IOConfiguration& config) override;
                virtual Success StartCollection(const HIDState& state, CollectionType type) override;
                virtual Success EndCollection() override;

                virtual void HandleReport(uint8_t report_id, const uint8_t* data, size_t length) override;
            };
        }
    }
}
