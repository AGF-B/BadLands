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

#include <sched/Self.hpp>

#include <screen/Log.hpp>

namespace {
    static constexpr size_t PAGE_SIZE       = Shared::Memory::PAGE_SIZE;
    static constexpr size_t BUFFER_SIZE     = 0x2000 * sizeof(char);
    static constexpr size_t BUFFER_PAGES    = BUFFER_SIZE / PAGE_SIZE;

    static_assert(BUFFER_SIZE % PAGE_SIZE == 0);

    struct CommandString {
        const char* command;
        size_t length;
    };

    class CommandContext {
    private:
        static constexpr size_t DEFAULT_PATH_BUFFER_SIZE = 256;

        char* displayedPath = nullptr;
        size_t displayedPathLength = 0;
        size_t displayedPathCapacity = 0;
        FS::IFNode* currentDirectory = nullptr;

    public:
        static Optional<CommandContext> Create() {
            char* displayedPath = static_cast<char*>(Heap::Allocate(DEFAULT_PATH_BUFFER_SIZE));

            if (displayedPath == nullptr) {
                return Optional<CommandContext>();
            }

            Utils::memset(displayedPath, 0, DEFAULT_PATH_BUFFER_SIZE);
            Utils::memcpy(displayedPath, "//", 2);

            auto rootResponse = Kernel::Exports.vfs->Open(FS::DirectoryEntry {
                .NameLength = 2,
                .Name = "//"
            });

            if (rootResponse.CheckError()) {
                Heap::Free(displayedPath);
                return Optional<CommandContext>();
            }

            CommandContext context;

            context.displayedPath = displayedPath;
            context.displayedPathLength = 3;
            context.displayedPathCapacity = DEFAULT_PATH_BUFFER_SIZE;
            context.currentDirectory = rootResponse.GetValue();

            return Optional(context);
        }

        const char* GetDisplayedPath() const {
            return displayedPath;
        }

        FS::IFNode* GetCurrentDirectory() const {
            return currentDirectory;
        }

        void OpenSubdirectory(FS::IFNode* newDirectory, const char* addedPath, size_t addedPathLength) {
            if (newDirectory != nullptr && addedPath != nullptr && addedPathLength > 0) {
                currentDirectory = newDirectory;

                // +1 for the '/' character
                if (displayedPathLength + addedPathLength + 1 > displayedPathCapacity) {
                    size_t newCapacity = displayedPathCapacity + addedPathLength * 5;

                    char* newBuffer = static_cast<char*>(Heap::Allocate(newCapacity));

                    if (newBuffer != nullptr) {
                        Utils::memset(newBuffer, 0, newCapacity);
                        Utils::memcpy(newBuffer, displayedPath, displayedPathLength);
                        Heap::Free(displayedPath);
                        displayedPath = newBuffer;
                        displayedPathCapacity = newCapacity;
                    }
                    else {
                        Panic::PanicShutdown("[SHELL] Failed to allocate memory for command context path buffer expansion\n\r");
                    }
                }

                Utils::memcpy(displayedPath + displayedPathLength - 1, addedPath, addedPathLength);

                displayedPathLength += addedPathLength + 1;     // +1 for the '/' character
                displayedPath[displayedPathLength - 2] = '/';   // see below for the explanation of the -2
                displayedPath[displayedPathLength - 1] = '\0';  // -1 because length already accounts for null-terminator
            }
        }

        void OpenParentDirectory() {
            if (currentDirectory != nullptr) {
                FS::DirectoryEntry filename;

                auto parentResponse = Kernel::Exports.vfs->OpenParent(FS::DirectoryEntry {
                    .NameLength = displayedPathLength - 1,
                    .Name = displayedPath
                }, filename);

                if (parentResponse.CheckError()) {
                    Log::putsSafe("[SHELL] Failed to open parent directory\n\r");
                }
                else {
                    currentDirectory = parentResponse.GetValue();
                }

                if (filename.NameLength > 0) {
                    // We are not in the root directory, so we can trim the displayed path
                    displayedPathLength -= filename.NameLength + 1; // +1 for the '/' character
                    displayedPath[displayedPathLength - 1] = '\0'; // -1 because length already accounts for null-terminator
                }
            }            
        }
    };

    typedef void (*ExecuteCallback)(const CommandString&, CommandContext&);
    typedef Devices::KeyboardDispatcher::VirtualKeyPacket KeyPacket;

    class InputBuffer final {
    private:
        char* buffer    = nullptr;
        size_t capacity = 0;
        size_t position = 0;

        ExecuteCallback executeCallback = nullptr;
        CommandContext& context;

        void AppendChar(char c) {
            if (!IsOverflowing()) {
                buffer[position++] = c;
            }
        }

        CommandString GetCommandString() const {
            return CommandString {
                .command = buffer,
                .length = position
            };
        }
        
    public:
        explicit InputBuffer(size_t size, CommandContext& context) : capacity{size}, context{context} {
            buffer = static_cast<char*>(Heap::Allocate(size));

            if (buffer != nullptr) {
                Utils::memset(buffer, 0, size);
                capacity = size;
            }
        }

        ~InputBuffer() {
            if (buffer != nullptr) {
                Heap::Free(buffer);
            }
        }

        bool IsValid() const {
            return buffer != nullptr;
        }

        bool IsOverflowing() const {
            // Reserve one character for null-terminator
            return position >= capacity - 1;
        }

        void OnBackspace() {
            if (position > 0) {
                buffer[--position] = '\0';
                Log::putcSafe('\b');
            }
        }

        void SetExecuteCallback(const ExecuteCallback& callback) {
            executeCallback = callback;
        }

        void DisplayPrompt() const {
            Log::putsSafe(context.GetDisplayedPath());
            Log::putsSafe("> ");
        }

        void OnKeyEvent(const KeyPacket& pkt) {
            if (pkt.flags & Devices::KeyboardDispatcher::FLAG_KEY_PRESSED) {
                bool shift = (pkt.flags & Devices::KeyboardDispatcher::FLAG_LEFT_SHIFT) ||
                                (pkt.flags & Devices::KeyboardDispatcher::FLAG_RIGHT_SHIFT);

                bool control = (pkt.flags & Devices::KeyboardDispatcher::FLAG_LEFT_CONTROL) ||
                                (pkt.flags & Devices::KeyboardDispatcher::FLAG_RIGHT_CONTROL);

                bool alt = (pkt.flags & Devices::KeyboardDispatcher::FLAG_LEFT_ALT) ||
                            (pkt.flags & Devices::KeyboardDispatcher::FLAG_RIGHT_ALT);
                
                if (control || alt) {
                    return;
                }

                char shift_offset = shift ? 'A' - 'a' : 0;

                char c = '\0';

                switch (pkt.keycode) {
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
                    case VK_OEM_PERIOD: c = shift ? '>' : '.'; break;
                    case VK_OEM_COMMA: c = shift ? '<' : ','; break;
                    case VK_OEM_2: c = shift ? '?' : '/'; break; 
                    case VK_SPACE: c = ' '; break;
                    case VK_RETURN: c = '\n'; break;
                    case VK_BACK:
                        OnBackspace();
                        break;
                    default: break;
                }

                if (c != '\0' && c != '\n') {
                    AppendChar(c);
                    Log::putcSafe(c);
                }
                else if (c == '\n') {
                    Log::putsSafe("\n\r");

                    if (IsOverflowing()) {
                        Log::putsSafe("[SHELL] Command too long.\n\r");
                    }
                    else {
                        if (executeCallback != nullptr) {
                            executeCallback(GetCommandString(), context);
                        }
                    }

                    Clear();

                    DisplayPrompt();
                }
            }
        }

        void Clear() {
            if (position != 0) {
                Utils::memset(buffer, 0, position);
                position = 0;
            }
        }
    };

    void OnExecute(const CommandString& cmd, CommandContext& context) {
        const char* const cmd_string = cmd.command;

        if (cmd.length > 0) {
            if (cmd.length == 5 && Utils::memcmp(cmd_string, "clear", 5) == 0) {
                Log::clear();
            }
            else if (cmd.length == 4 && Utils::memcmp(cmd_string, "list", 4) == 0) {
                static constexpr size_t ELEMENTS_PER_BLOCK = 64;

                FS::DirectoryEntry* buffer = static_cast<FS::DirectoryEntry*>(
                    Heap::Allocate(ELEMENTS_PER_BLOCK * sizeof(FS::DirectoryEntry))
                );

                if (buffer == nullptr) {
                    Log::putsSafe("[SHELL] Failed to allocate memory for directory entries\n\r");
                    return;
                }

                FS::Response<size_t> response(0);

                do {                
                    response = context.GetCurrentDirectory()->List(buffer, ELEMENTS_PER_BLOCK);

                    if (response.CheckError()) {
                        Log::printfSafe("[SHELL] Failed to list current directory (error %d)\n\r", static_cast<int>(response.GetError()));
                        Heap::Free(buffer);
                        return;
                    }

                    size_t count = response.GetValue();

                    for (size_t i = 0; i < count; ++i) {
                        const FS::DirectoryEntry& entry = buffer[i];
                        
                        for (size_t j = 0; j < entry.NameLength; ++j) {
                            Log::putcSafe(entry.Name[j]);
                        }
                        Log::putsSafe("\n\r");
                    }
                } while (response.GetValue() == ELEMENTS_PER_BLOCK);

                Heap::Free(buffer);
            }
            else if (cmd.length >= 4 && Utils::memcmp(cmd_string, "goto", 4) == 0) {
                if (cmd.length <= 5) {
                    Log::putsSafe("[SHELL] No path provided for goto command\n\r");
                }
                else if (cmd_string[4] != ' ') {
                    Log::putsSafe("[SHELL] Invalid syntax for goto command\n\r");
                }
                else {
                    const char* path = cmd_string + 5;
                    size_t path_length = cmd.length - 5;

                    if (path_length == 1 && path[0] == '.') {
                        return;
                    }
                    else if (path_length == 2 && path[0] == '.' && path[1] == '.') {
                        context.OpenParentDirectory();
                        return;
                    }

                    FS::DirectoryEntry entry {
                        .NameLength = path_length,
                        .Name = path
                    };

                    auto response = context.GetCurrentDirectory()->Find(entry);

                    if (response.CheckError()) {
                        if (response.GetError() == FS::Status::NOT_FOUND) {
                            Log::putsSafe("[SHELL] No such directory exists\n\r");
                        }
                        else {
                            Log::putsSafe("[SHELL] Failed to find directory entry for goto command\n\r");
                        }
                    }
                    else {
                        FS::IFNode* node = response.GetValue();

                        if (node == nullptr || !node->IsDirectory()) {
                            Log::putsSafe("[SHELL] The specified path is not a valid directory\n\r");
                        }
                        else {
                            context.OpenSubdirectory(node, path, path_length);
                        }
                    }
                }
            }
            else {
                Log::putsSafe("[SHELL] Unknown command: ");
                Log::putsSafe(cmd_string);
                Log::putsSafe("\n\r");
            }
        }
    }
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

            const auto response = Kernel::Exports.vfs->Open(keyboardBufferEntry);

            if (response.CheckError()) {
                Log::printfSafe("Error code: %d\n\r", static_cast<int>(response.GetError()));
                Panic::PanicShutdown("(SHELL) COULD NOT OPEN KEYBOARD BUFFER\n\r");
            }

            const auto keyboardBuffer = response.GetValue();

            Optional<CommandContext> context_wrapper = CommandContext::Create();

            if (!context_wrapper.HasValue()) {
                Panic::PanicShutdown("(SHELL) COULD NOT CREATE COMMAND CONTEXT\n\r");
            }

            CommandContext context = context_wrapper.GetValue();

            InputBuffer inputBuffer(BUFFER_SIZE, context);

            if (!inputBuffer.IsValid()) {
                Panic::PanicShutdown("(SHELL) COULD NOT START KERNEL SHELL\n\r");
            }

            inputBuffer.SetExecuteCallback(OnExecute);

            Log::putsSafe("[SHELL] Kernel shell initialized\n\r");
            inputBuffer.DisplayPrompt();

            while (true) {
                Devices::KeyboardDispatcher::BasicKeyPacket packet;

                const auto v = keyboardBuffer->Read(0, sizeof(packet), reinterpret_cast<uint8_t*>(&packet));

                if (v.CheckError()) {
                    Log::putsSafe("Error reading\n\r");
                }
                else {
                    if (v.GetValue() > 0) {
                        inputBuffer.OnKeyEvent(Devices::KeyboardDispatcher::GetVirtualKeyPacket(packet));
                    }
                }

                Self().Yield();
            }
        }
    }
}
