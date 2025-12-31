#include <cstddef>
#include <cstdint>

#include <new>

#include <shared/Response.hpp>

#include <devices/USB/HID/Keyboard.hpp>

#include <mm/Heap.hpp>

namespace Devices::USB::HID {
    Keyboard::Report* Keyboard::Report::Release() {
        if (items != nullptr) {
            Item* current = items;

            while (current != nullptr) {
                Item* next = current->next;
                Heap::Free(current);
                current = next;
            }
        }
        Report* const returned = next;

        items = nullptr;        
        next = nullptr;

        return returned;
    }

    Success Keyboard::Report::AddItem(const Item& item) {
        auto* const raw = Heap::Allocate(sizeof(Item));

        if (raw == nullptr) {
            return Failure();
        }

        Item* newItem = new (raw) Item(item);

        newItem->offset = items == nullptr ? 0 : (items->offset + items->size * items->count);
        newItem->next = items;
        items = newItem;

        return Success();
    }

    Keyboard::ReportCollection* Keyboard::ReportCollection::Release() {
        Report** pReportClasses[] {
            &inputReports,
            &outputReports
        };

        for (Report** pReportClass : pReportClasses) {
            Report*& reportClass = *pReportClass;

            if (reportClass != nullptr) {
                Report* current = reportClass;

                while (current != nullptr) {
                    Report* next = current->Release();
                    Heap::Free(current);
                    current = next;
                }

                reportClass = nullptr;
            }
        }

        ReportCollection* const returned = next;

        parent = nullptr;
        next = nullptr;

        return returned;
    }

    Keyboard::Report* Keyboard::ReportCollection::GetReport(uint32_t reportID, bool input) {
        Report* current = input ? inputReports : outputReports;

        while (current != nullptr) {
            if (current->id == reportID) {
                return current;
            }

            current = current->next;
        }

        return nullptr;
    }

    Optional<Keyboard::Report*> Keyboard::ReportCollection::AddReport(uint32_t reportID, bool input) {
        auto* const raw = Heap::Allocate(sizeof(Report));

        if (raw == nullptr) {
            return Optional<Report*>();
        }

        Report* newReport = new (raw) Report;

        newReport->id = reportID;
        newReport->next = input ? inputReports : outputReports;

        if (input) {
            inputReports = newReport;
        }
        else {
            outputReports = newReport;
        }

        return Optional<Report*>(newReport);
    }

    void Keyboard::Release() {
        ReportCollection* current = collections;

        while (current != nullptr) {
            ReportCollection* next = current->Release();
            Heap::Free(current);
            current = next;
        }

        collections = nullptr;
    }

    bool Keyboard::IsUsageSupported(uint32_t page, uint32_t usage) {
        static constexpr uint32_t GENERIC_DESKTOP_CONTROLS  = 0x01;
        static constexpr uint32_t GENERIC_KEYBOARD          = 0x06;
        static constexpr uint32_t KEYBOARD_KEYPAD           = 0x07;
        static constexpr uint32_t LEDS                      = 0x08;

        if (page == GENERIC_DESKTOP_CONTROLS) {
            return usage == GENERIC_KEYBOARD;
        }
        else if (page == KEYBOARD_KEYPAD || page == LEDS) {
            return true;
        }

        return false;
    }

    bool Keyboard::IsReportSupported(uint32_t reportID, bool input) {
        ReportCollection* currentCollection = collections;

        while (currentCollection != nullptr) {
            Report* currentReport = input ? currentCollection->inputReports : currentCollection->outputReports;

            while (currentReport != nullptr) {
                if (currentReport->id == reportID) {
                    return true;
                }

                currentReport = currentReport->next;
            }

            currentCollection = currentCollection->next;
        }

        return false;
    }

    Success Keyboard::AddReportItem(const HIDState& state, const IOConfiguration& config, bool input) {
        if (config.variable && state.globalState.reportSize != 1) {
            return Failure();
        }
        else if (currentCollection == nullptr) {
            return Failure();
        }

        Report* report = currentCollection->GetReport(state.globalState.reportID, input);

        if (report == nullptr) {
            auto const report_wrapper = currentCollection->AddReport(state.globalState.reportID, input);

            if (!report_wrapper.HasValue()) {
                return Failure();
            }

            report = report_wrapper.GetValue();
        }

        return report->AddItem(Item{
            .isConstant         = config.constant,
            .usage_page         = state.globalState.usagePage,
            .usage_minimum      = state.localState.usageMinimum,
            .usage_maximum      = state.localState.usageMaximum,
            .logical_minimum    = state.globalState.logicalMinimum,
            .logical_maximum    = state.globalState.logicalMaximum,
            .offset             = 0,
            .size               = state.globalState.reportSize,
            .count              = state.globalState.reportCount,
            .next               = nullptr
        });
    }

    Success Keyboard::AddInput(const HIDState& state, const IOConfiguration& config) {
        return AddReportItem(state, config, true);
    }

    Success Keyboard::AddOutput(const HIDState& state, const IOConfiguration& config) {
        return AddReportItem(state, config, false);
    }

    Success Keyboard::StartCollection([[maybe_unused]] const HIDState& state, CollectionType type) {
        if (type != CollectionType::Application) {
            return Failure();
        }

        auto* const raw = Heap::Allocate(sizeof(ReportCollection));

        if (raw == nullptr) {
            return Failure();
        }

        ReportCollection* newCollection = new (raw) ReportCollection;

        newCollection->parent = currentCollection;
        newCollection->next = collections;
        collections = newCollection;
        currentCollection = newCollection;

        return Success();
    }

    Success Keyboard::EndCollection() {
        if (currentCollection == nullptr) {
            return Failure();
        }

        currentCollection = currentCollection->parent;

        return Success();
    }
}
