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

#include <screen/Log.hpp>

namespace {
    static constexpr size_t PAGE_SIZE       = Shared::Memory::PAGE_SIZE;
    static constexpr size_t BUFFER_SIZE     = 0x2000 * sizeof(char);
    static constexpr size_t BUFFER_PAGES    = BUFFER_SIZE / PAGE_SIZE;

    static_assert(BUFFER_SIZE % PAGE_SIZE == 0);

    char* InputBuffer;
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
                        char c = '\0';

                        switch (vpkt.keycode) {
                            case VK_A: c = 'a'; break;
                            case VK_B: c = 'b'; break;
                            case VK_C: c = 'c'; break;
                            case VK_D: c = 'd'; break;
                            case VK_E: c = 'e'; break;
                            case VK_F: c = 'f'; break;
                            case VK_G: c = 'g'; break;
                            case VK_H: c = 'h'; break;
                            case VK_I: c = 'i'; break;
                            case VK_J: c = 'j'; break;
                            case VK_K: c = 'k'; break;
                            case VK_L: c = 'l'; break;
                            case VK_M: c = 'm'; break;
                            case VK_N: c = 'n'; break;
                            case VK_O: c = 'o'; break;
                            case VK_P: c = 'p'; break;
                            case VK_Q: c = 'q'; break;
                            case VK_R: c = 'r'; break;
                            case VK_S: c = 's'; break;
                            case VK_T: c = 't'; break;
                            case VK_U: c = 'u'; break;
                            case VK_V: c = 'v'; break;
                            case VK_W: c = 'w'; break;
                            case VK_X: c = 'x'; break;
                            case VK_Y: c = 'y'; break;
                            case VK_Z: c = 'z'; break;
                            case VK_SPACE: c = ' '; break;
                            case VK_RETURN: c = '\n'; break;
                            case VK_BACK: c = '\b'; break;
                            default: break;
                        }

                        Log::putcSafe(c);
                    }
                }
            }
        }
    }
}
