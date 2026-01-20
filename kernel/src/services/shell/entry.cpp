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

#include <cstddef>
#include <cstdint>

#include <exports.hpp>

#include <shared/memory/defs.hpp>

#include <devices/KeyboardDispatcher/Converter.hpp>
#include <devices/KeyboardDispatcher/Keypacket.hpp>
#include <devices/KeyboardDispatcher/Keycodes.h>
#include <devices/KeyboardDispatcher/Multiplexer.hpp>

#include <fs/IFNode.hpp>
#include <fs/VFS.hpp>

#include <interrupts/Panic.hpp>

#include <mm/Heap.hpp>
#include <mm/Utils.hpp>

#include <screen/Log.hpp>

namespace {
    static constexpr size_t PAGE_SIZE       = Shared::Memory::PAGE_SIZE;
    static constexpr size_t BUFFER_SIZE     = 0x2000 * sizeof(char);
    static constexpr size_t BUFFER_PAGES    = BUFFER_SIZE / PAGE_SIZE;

    static_assert(BUFFER_SIZE % PAGE_SIZE == 0);

    char* InputBuffer;
    size_t InputBufferPosition = 0;
    bool InputBufferOverflow = false;
}

namespace Services {
    namespace Shell {
        void Entry() {
            Log::putsSafe("[SHELL] Initializing shell...\n\r");

            static constexpr char keyboardBufferPath[] = "//Devices/keyboard";
            static constexpr FS::DirectoryEntry keyboardBufferEntry {
                .NameLength = sizeof(keyboardBufferPath) - 1,
                .Name = keyboardBufferPath
            };

            auto response = Kernel::Exports.vfs->Open(keyboardBufferEntry);

            if (response.CheckError()) {
                Log::printfSafe("Error code: %d\n\r", static_cast<int>(response.GetError()));
                Panic::PanicShutdown("(SHELL) COULD NOT OPEN KEYBOARD BUFFER\n\r");
            }

            auto keyboardBuffer = response.GetValue();

            InputBuffer = static_cast<char*>(Heap::Allocate(BUFFER_PAGES * PAGE_SIZE));

            if (InputBuffer == nullptr) {
                Panic::PanicShutdown("(SHELL) COULD NOT START KERNEL SHELL\n\r");
            }

            Log::putsSafe("[SHELL] Kernel shell initialized\n\r");

            Log::putsSafe("> ");

            while (true) {                
                __asm__ volatile("hlt");

                Devices::KeyboardDispatcher::BasicKeyPacket packet;

                auto v = keyboardBuffer->Read(0, sizeof(packet), reinterpret_cast<uint8_t*>(&packet));

                if (v.CheckError()) {
                    Log::putsSafe("Error reading\n\r");
                }
                else {
                    if (v.GetValue() > 0) {
                        auto vpkt = Devices::KeyboardDispatcher::GetVirtualKeyPacket(packet);

                        if (vpkt.flags & Devices::KeyboardDispatcher::FLAG_KEY_PRESSED) {
                            bool shift = (vpkt.flags & Devices::KeyboardDispatcher::FLAG_LEFT_SHIFT) ||
                                         (vpkt.flags & Devices::KeyboardDispatcher::FLAG_RIGHT_SHIFT);

                            bool control = (vpkt.flags & Devices::KeyboardDispatcher::FLAG_LEFT_CONTROL) ||
                                           (vpkt.flags & Devices::KeyboardDispatcher::FLAG_RIGHT_CONTROL);

                            bool alt = (vpkt.flags & Devices::KeyboardDispatcher::FLAG_LEFT_ALT) ||
                                       (vpkt.flags & Devices::KeyboardDispatcher::FLAG_RIGHT_ALT);
                            
                            if (control || alt) {
                                continue;
                            }

                            char shift_offset = shift ? 'A' - 'a' : 0;

                            char c = '\0';

                            switch (vpkt.keycode) {
                                case VK_A: c = 'a' + shift_offset; break;
                                case VK_B: c = 'b' + shift_offset; break;
                                case VK_C: c = 'c' + shift_offset; break;
                                case VK_D: c = 'd' + shift_offset; break;
                                case VK_E: c = 'e' + shift_offset; break;
                                case VK_F: c = 'f' + shift_offset; break;
                                case VK_G: c = 'g' + shift_offset; break;
                                case VK_H: c = 'h' + shift_offset; break;
                                case VK_I: c = 'i' + shift_offset; break;
                                case VK_J: c = 'j' + shift_offset; break;
                                case VK_K: c = 'k' + shift_offset; break;
                                case VK_L: c = 'l' + shift_offset; break;
                                case VK_M: c = 'm' + shift_offset; break;
                                case VK_N: c = 'n' + shift_offset; break;
                                case VK_O: c = 'o' + shift_offset; break;
                                case VK_P: c = 'p' + shift_offset; break;
                                case VK_Q: c = 'q' + shift_offset; break;
                                case VK_R: c = 'r' + shift_offset; break;
                                case VK_S: c = 's' + shift_offset; break;
                                case VK_T: c = 't' + shift_offset; break;
                                case VK_U: c = 'u' + shift_offset; break;
                                case VK_V: c = 'v' + shift_offset; break;
                                case VK_W: c = 'w' + shift_offset; break;
                                case VK_X: c = 'x' + shift_offset; break;
                                case VK_Y: c = 'y' + shift_offset; break;
                                case VK_Z: c = 'z' + shift_offset; break;
                                case VK_0: c = shift ? ')' : '0'; break;
                                case VK_1: c = shift ? '!' : '1'; break;
                                case VK_2: c = shift ? '@' : '2'; break;
                                case VK_3: c = shift ? '#' : '3'; break;
                                case VK_4: c = shift ? '$' : '4'; break;
                                case VK_5: c = shift ? '%' : '5'; break;
                                case VK_6: c = shift ? '^' : '6'; break;
                                case VK_7: c = shift ? '&' : '7'; break;
                                case VK_8: c = shift ? '*' : '8'; break;
                                case VK_9: c = shift ? '(' : '9'; break;
                                case VK_SPACE: c = ' '; break;
                                case VK_RETURN: c = '\n'; break;
                                case VK_BACK:
                                    InputBufferPosition = (InputBufferPosition > 0) ? InputBufferPosition - 1 : 0;
                                    Log::putcSafe('\b');
                                    break;
                                default: break;
                            }

                            if (c != '\0' && c != '\n') {
                                Log::putcSafe(c);

                                if (InputBufferPosition < BUFFER_SIZE - 1) {
                                    InputBuffer[InputBufferPosition++] = c;
                                }
                                else {
                                    InputBufferOverflow = true;
                                }
                            }
                            else if (c == '\n') {
                                Log::putsSafe("\n\r");

                                if (InputBufferOverflow) {
                                    Log::putsSafe("[SHELL] Command too long.\n\r");
                                    InputBufferOverflow = false;
                                }
                                else {
                                    if (Utils::memcmp(InputBuffer, "clear", 5) == 0 && InputBufferPosition == 5) {
                                        Log::clear();
                                    }
                                    else {
                                        Log::putsSafe("[SHELL] Unknown command: ");
                                        Log::putsSafe(InputBuffer);
                                        Log::putsSafe("\n\r");
                                    }
                                }

                                Log::putsSafe("> ");

                                InputBufferPosition = 0;
                            }
                        }
                    }
                }
            }
        }
    }
}
