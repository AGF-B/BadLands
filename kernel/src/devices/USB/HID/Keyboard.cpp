#include <cstddef>
#include <cstdint>

#include <new>

#include <shared/Response.hpp>

#include <devices/KeyboardDispatcher/Keycodes.h>
#include <devices/KeyboardDispatcher/Keypacket.hpp>
#include <devices/USB/HID/Keyboard.hpp>

#include <sched/Self.hpp>

#include <exports.hpp>

#include <mm/Heap.hpp>
#include <mm/Utils.hpp>

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

    size_t Keyboard::Report::GetReportSize() const {
        if (items == nullptr) {
            return 0;
        }

        const size_t size_in_bits = items->offset + items->size * items->count;
        return (size_in_bits + 7) / 8;
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

    size_t Keyboard::GetMaxReportSize() const {
        if (max_report_size == 0) {        
            size_t max_size = 0;

            ReportCollection* currentCollection = collections;

            while (currentCollection != nullptr) {
                Report* currentReport = currentCollection->inputReports;

                while (currentReport != nullptr) {
                    size_t report_size = currentReport->GetReportSize();

                    if (report_size > max_size) {
                        max_size = report_size;
                    }

                    currentReport = currentReport->next;
                }

                currentReport = currentCollection->outputReports;

                while (currentReport != nullptr) {
                    size_t report_size = currentReport->GetReportSize();

                    if (report_size > max_size) {
                        max_size = report_size;
                    }

                    currentReport = currentReport->next;
                }

                currentCollection = currentCollection->next;
            }

            max_report_size = max_size;
        }

        return max_report_size;
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

    bool Keyboard::KeyStateChanged(uint32_t usage) const {
        if (usage >= BITMAP_SIZE) {
            return false;
        }

        const size_t entry_index = usage / BITMAP_ENTRY_BITS;
        const size_t entry_bit = usage % BITMAP_ENTRY_BITS;

        const uint64_t mask = static_cast<uint64_t>(1) << entry_bit;

        const uint64_t current_state = current_keys_bitmap[entry_index] & mask;
        const uint64_t previous_state = previous_keys_bitmap[entry_index] & mask;

        return current_state != previous_state;
    }

    void Keyboard::SetKeyPressed(uint32_t usage) {
        if (usage >= BITMAP_SIZE) {
            return;
        }

        const size_t entry_index = usage / BITMAP_ENTRY_BITS;
        const size_t entry_bit = usage % BITMAP_ENTRY_BITS;

        const uint64_t mask = static_cast<uint64_t>(1) << entry_bit;

        current_keys_bitmap[entry_index] |= mask;
    }

    void Keyboard::RollBitmapsOver() {
        for (size_t i = 0; i < BITMAP_ENTRIES; ++i) {
            previous_keys_bitmap[i] = current_keys_bitmap[i];
            current_keys_bitmap[i] = 0;
        }
    }

    bool Keyboard::RepeatableUsage(uint32_t usage) {
        // if alphanumeric
        static constexpr uint32_t USAGE_A = 0x04;
        static constexpr uint32_t USAGE_0 = 0x27;

        if (usage >= USAGE_A && usage <= USAGE_0) {
            return true;
        }

        // if in second range repeatable
        static constexpr uint32_t USAGE_DEL     = 0x2A;
        static constexpr uint32_t USAGE_SLASH   = 0x38;

        if (usage >= USAGE_DEL && usage <= USAGE_SLASH) {
            return true;
        }

        // if in third repeatable range

        static constexpr uint32_t USAGE_RETURN      = 0x28;
        static constexpr uint32_t USAGE_PAGE_UP     = 0x4B;
        static constexpr uint32_t USAGE_PAGE_DOWN   = 0x4E;
        static constexpr uint32_t DEL_FWD           = 0x4C;
        static constexpr uint32_t ARROW_RIGHT       = 0x4F;
        static constexpr uint32_t ARROW_UP          = 0x52;

        if (usage == USAGE_RETURN || usage == USAGE_PAGE_UP || usage == USAGE_PAGE_DOWN || usage == DEL_FWD) {
            return true;
        }
        else if (usage >= ARROW_RIGHT && usage <= ARROW_UP) {
            return true;
        }

        // repeatable keypad range
        static constexpr uint32_t USAGE_KEYPAD_ENTER    = 0x58;
        static constexpr uint32_t USAGE_KEYPAD_NON_US   = 0x64;
        static constexpr uint32_t USAGE_KEYPAD_EQUAL    = 0x67;

        if (usage >= USAGE_KEYPAD_ENTER && usage <= USAGE_KEYPAD_NON_US) {
            return true;
        }
        else if (usage == USAGE_KEYPAD_EQUAL) {
            return true;
        }

        return false;
    }

    uint8_t Keyboard::GetKeyFlags() const {
        static constexpr uint8_t LEFT_CONTROL_USAGE     = 0xE0;
        static constexpr uint8_t LEFT_SHIFT_USAGE       = 0xE1;
        static constexpr uint8_t LEFT_ALT_USAGE         = 0xE2;
        static constexpr uint8_t LEFT_GUI_USAGE         = 0xE3;
        static constexpr uint8_t RIGHT_CONTROL_USAGE    = 0xE4;
        static constexpr uint8_t RIGHT_SHIFT_USAGE      = 0xE5;
        static constexpr uint8_t RIGHT_ALT_USAGE        = 0xE6;
        static constexpr uint8_t RIGHT_GUI_USAGE        = 0xE7;

        static constexpr size_t FLAGS_COUNT = 8;

        static constexpr uint8_t USAGES[FLAGS_COUNT] = {
            LEFT_CONTROL_USAGE,
            LEFT_SHIFT_USAGE,
            LEFT_ALT_USAGE,
            LEFT_GUI_USAGE,
            RIGHT_CONTROL_USAGE,
            RIGHT_SHIFT_USAGE,
            RIGHT_ALT_USAGE,
            RIGHT_GUI_USAGE
        };

        static constexpr uint8_t FLAGS[FLAGS_COUNT] = {
            KeyboardDispatcher::FLAG_LEFT_CONTROL,
            KeyboardDispatcher::FLAG_LEFT_SHIFT,
            KeyboardDispatcher::FLAG_LEFT_ALT,
            KeyboardDispatcher::FLAG_LEFT_GUI,
            KeyboardDispatcher::FLAG_RIGHT_CONTROL,
            KeyboardDispatcher::FLAG_RIGHT_SHIFT,
            KeyboardDispatcher::FLAG_RIGHT_ALT,
            KeyboardDispatcher::FLAG_RIGHT_GUI
        };

        uint8_t flags = 0;

        for (size_t i = 0; i < FLAGS_COUNT; ++i) {
            const size_t entry_index = USAGES[i] / BITMAP_ENTRY_BITS;
            const size_t entry_bit = USAGES[i] % BITMAP_ENTRY_BITS;

            const uint64_t mask = static_cast<uint64_t>(1) << entry_bit;

            if (current_keys_bitmap[entry_index] & mask) {
                flags |= FLAGS[i];
            }
        }

        return flags;
    }

    uint8_t Keyboard::GetKeypoint(uint32_t usage) const {
        static constexpr uint8_t KEYPOINT_TABLE[0x100] = {
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // No Event, POST Fail, ErrorRollOver, ErrorUndefined
            KEYPOINT(5,  1),    KEYPOINT(6,  6),     KEYPOINT(6,  4),     KEYPOINT(5,  3),     // A, B, C, D
            KEYPOINT(4,  3),    KEYPOINT(5,  4),     KEYPOINT(5,  5),     KEYPOINT(5,  6),     // E, F, G, H
            KEYPOINT(4,  8),    KEYPOINT(5,  7),     KEYPOINT(5,  8),     KEYPOINT(5,  9),     // I, J, K, L
            KEYPOINT(6,  8),    KEYPOINT(6,  7),     KEYPOINT(4,  9),     KEYPOINT(4, 10),     // M, N, O, P
            KEYPOINT(4,  1),    KEYPOINT(4,  4),     KEYPOINT(5,  2),     KEYPOINT(4,  5),     // Q, R, S, T
            KEYPOINT(4,  7),    KEYPOINT(6,  5),     KEYPOINT(4,  2),     KEYPOINT(6,  3),     // U, V, W, X
            KEYPOINT(4,  6),    KEYPOINT(6,  2),     KEYPOINT(3,  1),     KEYPOINT(3,  2),     // Y, Z, 1, 2
            KEYPOINT(3,  3),    KEYPOINT(3,  4),     KEYPOINT(3,  5),     KEYPOINT(3,  6),     // 3, 4, 5, 6
            KEYPOINT(3,  7),    KEYPOINT(3,  8),     KEYPOINT(3,  9),     KEYPOINT(3, 10),     // 7, 8, 9, 0
            KEYPOINT(5, 13),    KEYPOINT(2,  0),     KEYPOINT(3, 13),     KEYPOINT(4,  0),     // ENTER, ESCAPE, BACKSPACE, TAB
            KEYPOINT(7,  6),    KEYPOINT(3, 11),     KEYPOINT(3, 12),     KEYPOINT(4, 11),     // SPACE, -, =, [
            KEYPOINT(4, 12),    KEYPOINT(4, 13),     KEYPOINT(0,  0),     KEYPOINT(5, 10),     // ], \, Non-US #, ;
            KEYPOINT(5, 11),    KEYPOINT(3,  0),     KEYPOINT(6,  9),     KEYPOINT(6, 10),     // ', `, COMMA, .
            KEYPOINT(6, 11),    KEYPOINT(5,  0),     KEYPOINT(2,  1),     KEYPOINT(2,  2),     // /, CAPS LOCK, F1, F2
            KEYPOINT(2,  3),    KEYPOINT(2,  4),     KEYPOINT(2,  5),     KEYPOINT(2,  6),     // F3, F4, F5, F6
            KEYPOINT(2,  7),    KEYPOINT(2,  8),     KEYPOINT(2,  9),     KEYPOINT(2, 10),     // F7, F8, F9, F10
            KEYPOINT(2, 11),    KEYPOINT(2, 12),     KEYPOINT(1, 13),     KEYPOINT(1, 14),     // F11, F12, PRINT SCREEN, SCROLL LOCK
            KEYPOINT(1, 15),    KEYPOINT(2, 13),     KEYPOINT(2, 14),     KEYPOINT(2, 15),     // PAUSE, INSERT, HOME, PAGE UP
            KEYPOINT(3, 14),    KEYPOINT(3, 15),     KEYPOINT(4, 14),     KEYPOINT(7, 14),     // DELETE, END, PAGE DOWN, RIGHT ARROW
            KEYPOINT(7, 12),    KEYPOINT(7, 13),     KEYPOINT(6, 13),     KEYPOINT(3, 16),     // LEFT ARROW, DOWN ARROW, UP ARROW, NUM LOCK
            KEYPOINT(3, 17),    KEYPOINT(3, 18),     KEYPOINT(3, 19),     KEYPOINT(5, 19),     // KEYPAD /, KEYPAD *, KEYPAD -, KEYPAD +
            KEYPOINT(7, 19),    KEYPOINT(6, 16),     KEYPOINT(6, 17),     KEYPOINT(6, 18),     // KEYPAD ENTER, KEYPAD 1, KEYPAD 2, KEYPAD 3
            KEYPOINT(5, 16),    KEYPOINT(5, 17),     KEYPOINT(5, 18),     KEYPOINT(4, 16),     // KEYPAD 4, KEYPAD 5, KEYPAD 6, KEYPAD 7
            KEYPOINT(4, 17),    KEYPOINT(4, 18),     KEYPOINT(7, 16),     KEYPOINT(7, 18),     // KEYPAD 8, KEYPAD 9, KEYPAD 0, KEYPAD .
            KEYPOINT(0,  0),    KEYPOINT(7, 10),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // Non-US \, APPLICATION, POWER, KEYPAD =
            KEYPOINT(1,  1),    KEYPOINT(1,  2),     KEYPOINT(1,  3),     KEYPOINT(1,  4),     // F13, F14, F15, F16
            KEYPOINT(1,  5),    KEYPOINT(1,  6),     KEYPOINT(1,  7),     KEYPOINT(1,  8),     // F17, F18, F19, F20
            KEYPOINT(1,  9),    KEYPOINT(1, 10),     KEYPOINT(1, 11),     KEYPOINT(1, 12),     // F21, F22, F23, F24
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,   0),    KEYPOINT(0,  0),     // EXECUTE, HELP, MENU, SELECT
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // STOP, AGAIN, UNDO, CUT
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // COPY, PASTE, FIND, MUTE
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // VOLUME UP, VOLUME DOWN, LOCKING CAPS LOCK, LOCKING NUM LOCK
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // LOCKING SCROLL LOCK, KEYPAD COMMA, KEYPAD EQUAL, INTL1
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // INTL2, INTL3, INTL4, INTL5
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // INTL6, INTL7, INTL8, INTL9
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // LANG1, LANG2, LANG3, LANG4
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // LANG5, LANG6, LANG7, LANG8
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // LANG9, ALT ERASE, SYS REQ, CANCEL
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // CLEAR, PRIOR, RETURN, SEPARATOR
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // OUT, OPER, CLEAR/AGAIN, CRSEL/PROPS
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // EXSEL, RESERVED, RESERVED, RESERVED
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // RESERVED, RESERVED, RESERVED, RESERVED
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // RESERVED, RESERVED, RESERVED, RESERVED
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // KEYPAD 00, KEYPAD 000, THOUSANDS SEPARATOR, DECIMAL SEPARATOR
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // CURRENCY UNIT, CURRENCY SUB-UNIT, KEYPAD (, KEYPAD )
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // KEYPAD {, KEYPAD }, KEYPAD TAB, KEYPAD BACKSPACE
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // KEYPAD A, KEYPAD B, KEYPAD C, KEYPAD D
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // KEYPAD E, KEYPAD F, KEYPAD XOR, KEYPAD ^
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // KEYPAD %, KEYPAD <, KEYPAD >, KEYPAD &
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // KEYPAD &&, KEYPAD |, KEYPAD ||, KEYPAD :
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // KEYPAD #, KEYPAD SPACE, KEYPAD @, KEYPAD !
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // KEYPAD MEM STORE, KEYPAD MEM RECALL, KEYPAD MEM CLEAR, KEYPAD MEM ADD
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // KEYPAD MEM SUBTRACT, KEYPAD MULTIPLY, KEYPAD DIVIDE, KEYPAD PLUS/MINUS
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // KEYPAD CLEAR, KEYPAD CLEAR ENTRY, KEYPAD BIN, KEYPAD OCTAL
            KEYPOINT(0,  0),    KEYPOINT(0,  0),     KEYPOINT(0,  0),     KEYPOINT(0,  0),     // KEYPAD DECIMAL, KEYPAD HEXADECIMAL, RESERVED, RESERVED
            KEYPOINT(7,  0),    KEYPOINT(6,  0),     KEYPOINT(7,  2),     KEYPOINT(7,  1),     // LCTRL, LSHIFT, LALT, LGUI
            KEYPOINT(7, 11),    KEYPOINT(6, 12),     KEYPOINT(7,  8),     KEYPOINT(7,  9)      // RCTRL, RSHIFT, RALT, RGUI
        };

        if (usage < 0x100) {
            return KEYPOINT_TABLE[usage];
        }

        return VK_INVALID;
    }

    void Keyboard::HandleKeyUsage(uint32_t usage, bool pressed) {
        if (pressed) {
            SetKeyPressed(usage);
        }
    }

    void Keyboard::OnKeyPressed(uint32_t usage, uint8_t flags, uint64_t timestamp) {
        const uint8_t keypoint = GetKeypoint(usage);

        if (keypoint != VK_INVALID) {
            if (usage < BITMAP_SIZE && RepeatableUsage(usage)) {
                keys_press_timestamp[usage] = timestamp;
            }

            KeyboardDispatcher::BasicKeyPacket packet {
                .scancode = static_cast<uint8_t>(usage),
                .keypoint = keypoint,
                .flags    = static_cast<uint16_t>(flags | KeyboardDispatcher::FLAG_KEY_PRESSED),
            };

            Kernel::Exports.keyboardMultiplexerInterface->Write(
                0,
                sizeof(packet),
                reinterpret_cast<uint8_t*>(&packet)
            );
        }
    }

    void Keyboard::OnKeyReleased(uint32_t usage, uint8_t flags) {
        const uint8_t keypoint = GetKeypoint(usage);

        if (keypoint != VK_INVALID) {
            if (usage < BITMAP_SIZE) {
                keys_press_timestamp[usage] = 0;
                keys_repeat_timestamp[usage] = 0;
            }

            KeyboardDispatcher::BasicKeyPacket packet {
                .scancode = static_cast<uint8_t>(usage),
                .keypoint = keypoint,
                .flags    = static_cast<uint16_t>(flags & ~KeyboardDispatcher::FLAG_KEY_PRESSED),
            };

            Kernel::Exports.keyboardMultiplexerInterface->Write(
                0,
                sizeof(packet),
                reinterpret_cast<uint8_t*>(&packet)
            );
        }
    }

    void Keyboard::OnKeyHeld(uint32_t usage, uint8_t flags, uint64_t timestamp) {
        if (usage < BITMAP_SIZE) {
            const uint64_t press_time = keys_press_timestamp[usage];
            const uint64_t repeat_time = keys_repeat_timestamp[usage];

            if (press_time != 0) {
                const uint64_t time_since_press = timestamp - press_time;

                if (time_since_press >= KEY_REPEAT_DELAY_MS) {
                    const uint64_t time_since_repeat = timestamp - repeat_time;

                    if (time_since_repeat >= KEY_REPEAT_RATE_MS) {
                        keys_repeat_timestamp[usage] = timestamp;
                        
                        const uint8_t keypoint = GetKeypoint(usage);

                        if (keypoint != VK_INVALID) {
                            KeyboardDispatcher::BasicKeyPacket packet {
                                .scancode = static_cast<uint8_t>(usage),
                                .keypoint = keypoint,
                                .flags    = static_cast<uint16_t>(flags | KeyboardDispatcher::FLAG_KEY_PRESSED),
                            };

                            Kernel::Exports.keyboardMultiplexerInterface->Write(
                                0,
                                sizeof(packet),
                                reinterpret_cast<uint8_t*>(&packet)
                            );
                        }
                    }
                }
            }
        }
    }

    void Keyboard::UpdateKeys(uint8_t report_id, const uint8_t* data, size_t length) {
        ReportCollection* currentCollection = collections;

        while (currentCollection != nullptr) {
            Report* report = currentCollection->GetReport(report_id, true);

            if (report != nullptr) {
                if (report->GetReportSize() > length) {
                    return;
                }

                Item* currentItem = report->items;

                while (currentItem != nullptr) {
                    if (!currentItem->isConstant) {
                        if (currentItem->size == 1) {
                            for (size_t j = 0; j < currentItem->count; ++j) {
                                const size_t bit_index = currentItem->offset + j;
                                const size_t byte_index = bit_index / 8;
                                const size_t bit_in_byte = bit_index % 8;

                                const bool pressed = (data[byte_index] & (1 << bit_in_byte)) != 0;

                                const uint32_t usage = currentItem->usage_minimum + j;

                                HandleKeyUsage(usage, pressed);
                            }
                        }
                        else if (currentItem->size == 8) {
                            for (size_t j = 0; j < currentItem->count; ++j) {
                                const size_t byte_index = (currentItem->offset / 8) + j;
                                const uint8_t value = data[byte_index];

                                HandleKeyUsage(value, value != 0);
                            }
                        }
                    }

                    currentItem = currentItem->next;
                }
            }

            currentCollection = currentCollection->next;
        };

        const uint8_t flags = GetKeyFlags();

        const uint64_t timestamp = Self().GetTimer().GetCountMillis();

        for (size_t i = 0; i < BITMAP_SIZE; ++i) {
            const size_t entry_index = i / BITMAP_ENTRY_BITS;
            const size_t entry_bit = i % BITMAP_ENTRY_BITS;

            const uint64_t mask = static_cast<uint64_t>(1) << entry_bit;

            const bool pressed = (current_keys_bitmap[entry_index] & mask) != 0;

            if (KeyStateChanged(i)) {
                if (pressed) {
                    OnKeyPressed(i, flags, timestamp);
                }
                else {
                    OnKeyReleased(i, flags);
                }
            }
            else {
                if (pressed && RepeatableUsage(i)) {
                    OnKeyHeld(i, flags, timestamp);
                }
            }
        }
    }

    void Keyboard::HandleReport(uint8_t report_id, const uint8_t* data, size_t length) {
        UpdateKeys(report_id, data, length);
        RollBitmapsOver();
    }
}
