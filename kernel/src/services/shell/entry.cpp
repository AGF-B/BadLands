#include <cstddef>
#include <cstdint>

#include <exports.hpp>

#include <shared/memory/defs.hpp>

#include <devices/KeyboardDispatcher/Converter.hpp>
#include <devices/KeyboardDispatcher/Keypacket.hpp>
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
            Log::puts("[SHELL] Initializing shell...\n\r");

            static constexpr char keyboardBufferPath[] = "//Devices/keyboard";
            static constexpr FS::DirectoryEntry keyboardBufferEntry {
                .NameLength = sizeof(keyboardBufferPath) - 1,
                .Name = keyboardBufferPath
            };

            auto response = Kernel::Exports.vfs->Open(keyboardBufferEntry);

            if (response.CheckError()) {
                Log::printf("Error code: %d\n\r", static_cast<int>(response.GetError()));
                Panic::PanicShutdown("(SHELL) COULD NOT OPEN KEYBOARD BUFFER\n\r");
            }

            auto keyboardBuffer = response.GetValue();

            InputBuffer = static_cast<char*>(Heap::Allocate(BUFFER_PAGES * PAGE_SIZE));

            if (InputBuffer == nullptr) {
                Panic::PanicShutdown("(SHELL) COULD NOT START KERNEL SHELL\n\r");
            }

            Log::puts("[SHELL] Kernel shell initialized\n\r");

            while (true) {
                Devices::KeyboardDispatcher::BasicKeyPacket packet;

                auto v = keyboardBuffer->Read(0, sizeof(packet), reinterpret_cast<uint8_t*>(&packet));

                if (v.CheckError()) {
                    Log::puts("Error reading\n\r");
                }
                else {
                    if (v.GetValue() > 0) {
                        auto vpkt = Devices::KeyboardDispatcher::GetVirtualKeyPacket(packet);
                        Log::printf("Key: 0x%.2hhx, 0x%.2hhx, %s\n\r", vpkt.keypoint, vpkt.keycode, vpkt.flags != 0 ? "PRESSED" : "RELEASED");
                    }
                }
            }
        }
    }
}
