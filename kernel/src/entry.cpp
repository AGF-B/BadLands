#include <cstdint>

#include <exports.hpp>

#include <shared/efi/efi.h>
#include <shared/memory/layout.hpp>

#include <acpi/Interface.hpp>

#include <devices/KeyboardDispatcher/Converter.hpp>
#include <devices/KeyboardDispatcher/Keypacket.hpp>
#include <devices/KeyboardDispatcher/Multiplexer.hpp>
#include <devices/PS2/Controller.hpp>
#include <devices/PS2/Keyboard.hpp>

#include <fs/VFS.hpp>

#include <interrupts/APIC.hpp>
#include <interrupts/IDT.hpp>
#include <interrupts/InterruptProvider.hpp>
#include <interrupts/Panic.hpp>
#include <interrupts/PIT.hpp>
#include <interrupts/RuntimeSvc.hpp>

#include <mm/gdt.hpp>
#include <mm/Heap.hpp>
#include <mm/PhysicalMemory.hpp>
#include <mm/VirtualMemory.hpp>

#include <pci/PCI.hpp>

#include <sched/Dispatcher.hpp>
#include <sched/Self.hpp>
#include <sched/TaskContext.hpp>
#include <sched/TaskManager.hpp>

#include <screen/Log.hpp>

#include <services/shell.hpp>

#define LEGACY_EXPORT extern "C"

extern uint8_t kernel_init_array_start[];
extern uint8_t kernel_init_array_end[];

namespace {
    static void _kernel_ctx_init() {
        const size_t initialize_count = (
            kernel_init_array_end - kernel_init_array_start
        ) / sizeof(void(*)(void));

        for (size_t i = 0; i < initialize_count; ++i) {
            reinterpret_cast<void(**)(void)>(
                kernel_init_array_start
            )[i]();
        }

        Runtime::Initialize();
    }

    static inline void SetupPhysicalMemory() {
        auto status = PhysicalMemory::Setup();
        if (status != PhysicalMemory::StatusCode::SUCCESS) {
            if (status == PhysicalMemory::StatusCode::OUT_OF_MEMORY) {
                Panic::PanicShutdown("PMM INITIALIZATION FAILED (OUT OF MEMORY)\n\r");
            }
            else {
                Panic::PanicShutdown("PMM INITIALIZATION FAILED (UNKNOWN REASON)\n\r");
            }
        }
    }

    static inline void SetupVirtualMemory() {
        if (VirtualMemory::Setup() != VirtualMemory::StatusCode::SUCCESS) {
            Panic::PanicShutdown("VMM INITIALIZATION FAILED\n\r");
        }
    }

    static inline void SetupHeap() {
        if (!Heap::Create()) {
            Panic::PanicShutdown("KERNEL HEAP CREATION FAILED\n\r");
        }
    }

    static inline VFS* SetupVFS() {
        VFS* vfs = static_cast<VFS*>(Heap::Allocate(sizeof(VFS)));

        if (vfs == nullptr) {
            Panic::PanicShutdown("VFS MEMORY ALLOCATION FAILED\n\r");
        }
        else if (!VFS::Construct(vfs)) {
            Panic::PanicShutdown("VFS INITIALIZATION FAILED\n\r");
        }

        return vfs;
    }

    static inline void SetupPS2Keyboard(FS::IFNode* keyboardMultiplexer) {
        Log::putsSafe("[PS/2] Initializing PS/2 platform...\n\r");

        if (!Devices::PS2::InitializeController().IsSuccess()) {
            Log::putsSafe("[PS/2] Controller initialization failed\n\r");
            return;
        }

        Log::putsSafe("[PS/2] Controller initialized\n\r");

        const auto identity = Devices::PS2::IdentifyPort1();

        if (!identity.HasValue()) {
            Log::putsSafe("[PS/2] Identify failed for device on port 1\n\r");
            return;
        }

        auto statusCode = Devices::PS2::InitializeKeyboard(keyboardMultiplexer);

        if (statusCode != Devices::PS2::StatusCode::SUCCESS) {
            Log::putsSafe("[PS/2] Keyboard initialization failed\n\r");
            Log::putsSafe("[PS/2] No keyboard input will be provided until a USB keyboard is connected\n\r");
            return;
        }

        Log::putsSafe("[PS/2] Keyboard input enabled\n\r");
        Log::putsSafe("[PS/2] Initialization done\n\r");

        __asm__ volatile("sti");
    }
}

Kernel::KernelExports Kernel::Exports = {
    .vfs = nullptr,
    .deviceInterface = nullptr,
    .keyboardMultiplexerInterface = nullptr
};

void BootProcessorInit() {
    auto* const keyboardMultiplexer = Devices::KeyboardDispatcher::Initialize(Kernel::Exports.deviceInterface);

    Kernel::Exports.keyboardMultiplexerInterface = keyboardMultiplexer;

    SetupPS2Keyboard(keyboardMultiplexer);

    //PCI::Enumerate();

    Services::Shell::Entry();

    while (1) {
        __asm__ volatile("hlt");
    }
}

LEGACY_EXPORT void KernelEntry() {
    __asm__ volatile("cli");

    _kernel_ctx_init();
    
    VirtualMemory::kernel_gdt_setup();
    Interrupts::kernel_idt_setup();

    Log::Setup();
    Log::puts("Kernel Log Enabled\n\r");

    SetupPhysicalMemory();
    Log::puts("PMM Initialized\n\r");

    SetupVirtualMemory();
    Log::puts("VMM Initialized\n\r");

    SetupHeap();
    Log::puts("KERNEL HEAP Initialized\n\r");

    VFS* vfs = SetupVFS();
    Kernel::Exports.vfs = vfs;
    Log::puts("VFS Initialized\n\r");

    Log::puts("[ENTRY] Creating VFS system hierarchy...\n\r");

    static constexpr FS::DirectoryEntry RootEntry = { .NameLength = 2, .Name = "//" };
    static constexpr FS::DirectoryEntry DeviceEntry = { .NameLength = 7, .Name = "Devices" };

    auto response = vfs->Open(RootEntry);

    if (response.CheckError()) {
        Panic::PanicShutdown("[ENTRY] Could not open VFS root to create system hierarchy\n\r");
    }

    auto root = response.GetValue();

    auto status = root->Create(DeviceEntry, FS::FileType::DIRECTORY);

    if (status != FS::Status::SUCCESS) {
        Panic::PanicShutdown("[ENTRY] Could not create VFS device interface\n\r");
    }

    response = root->Find(DeviceEntry);
    root->Close();

    if (response.CheckError()) {
        Panic::PanicShutdown("[ENTRY] Could not open VFS device interface\n\r");
    }

    auto deviceInterface = response.GetValue();
    Kernel::Exports.deviceInterface = deviceInterface;

    Log::puts("[ENTRY] VFS system hierarchy created\n\r");

    ACPI::Initialize();

    APIC::Initialize();

    APIC::SetupLocalAPIC();

    PIT::Initialize();

    __asm__ volatile("sti");

    Self().GetTimer().Initialize();

    auto initTask = Scheduling::KernelTaskContext::Create(reinterpret_cast<void*>(&BootProcessorInit));
    if (!initTask.HasValue()) {
        Panic::PanicShutdown("COULD NOT CREATE INIT TASK\n\r");
    }

    Self().GetTaskManager().AddTask(initTask.GetValue(), false);

    Scheduling::InitializeDispatcher();

    while (1) {
        __asm__ volatile("hlt");
    }
}
