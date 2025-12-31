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

                Success AddReportItem(const HIDState& state, const IOConfiguration& config, bool input);

            public:
                inline Keyboard() : InterfaceDevice(DeviceClass::KEYBOARD) { }

                virtual void Release();

                virtual bool IsUsageSupported(uint32_t page, uint32_t usage);
                virtual bool IsReportSupported(uint32_t reportID, bool input);

                virtual Success AddInput(const HIDState& state, const IOConfiguration& config);
                virtual Success AddOutput(const HIDState& state, const IOConfiguration& config);
                virtual Success StartCollection(const HIDState& state, CollectionType type);
                virtual Success EndCollection();
            };
        }
    }
}
