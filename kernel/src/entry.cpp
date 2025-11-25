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
        uint32_t status = 0;

        Log::puts("[PS/2] Initializing PS/2 platform...\n\r");

        APIC::MaskIRQ(Devices::PS2::PS2_PORT1_ISA_IRQ_VECTOR);

        if ((status = Devices::PS2::InitializePS2Controller()) != 0) {
            Log::puts("[PS/2] Controller initialization failed\n\r");
            return;
        }

        Log::puts("[PS/2] Controller initialized\n\r");

        if ((status = Devices::PS2::IdentifyPS2Port1()) > 0xFFFF) {
            Log::puts("[PS/2] Identify failed for device on port 1\n\r");
            return;
        }

        auto statusCode = Devices::PS2::InitializeKeyboard(keyboardMultiplexer);

        if (statusCode != Devices::PS2::StatusCode::SUCCESS) {
            Log::puts("[PS/2] Keyboard initialization failed\n\r");
            Log::puts("[PS/2] No keyboard input will be provided until a USB keyboard is connected\n\r");
            return;
        }

        const int vector = Interrupts::ReserveInterrupt();

        if (vector < 0) {
            Log::puts("[PS/2] Coult not reserve an interrupt for the keyboard\n\r");
            Log::puts("[PS/2] No keyboard input will be provided\n\r");
            return;
        }

        // Interrupts::RegisterIRQ(vector, &Devices::PS2::PS2_IRQ1_Handler, false);

        // APIC::SetupIRQ(Devices::PS2::PS2_PORT1_ISA_IRQ_VECTOR, {
        //     .InterruptVector = static_cast<uint8_t>(vector),
        //     .Delivery = APIC::IRQDeliveryMode::FIXED,
        //     .DestinationMode = APIC::IRQDestinationMode::Logical,
        //     .Polarity = APIC::IRQPolarity::ACTIVE_HIGH,
        //     .Trigger = APIC::IRQTrigger::EDGE,
        //     .Masked = false,
        //     .Destination = APIC::GetLAPICLogicalID()
        // });

        // Log::puts("[PS/2] Keyboard IRQ configured\n\r");
        // Log::puts("[PS/2] Keyboard input enabled\n\r");
        // Log::puts("[PS/2] Initialization done\n\r");

        Log::puts("[PS/2] Services configured but disabled\n\r");
    }
}

Kernel::KernelExports Kernel::Exports = {
    .vfs = nullptr,
    .deviceInterface = nullptr
};

static int tid_g = 0;

void f() {
    static int i = 0;
    while (true) {
        Log::putsSafe("Task 1\n\r");
        ++i;
        if (i == 2000000000) {
            Self::Get().RemoveTask(tid_g);
        }
    }
}

void g() {
    while (true) {
        Log::putsSafe("Task 2\n\r");
    }
}

static Interrupts::InterruptTrampoline apic_timer_handler(
    [](void* stack, uint64_t error_code) {
        static uint64_t i = 0;
        i += 1;

        APIC::SendEOI();

        if (i % 1000 == 0) {
            Log::printfSafe("APIC ticks: %llu\n\r", i);
        }
    }
);

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

    auto* keyboardBuffer = Devices::KeyboardDispatcher::Initialize(deviceInterface);

    //SetupPS2Keyboard(keyboardBuffer);

    PIT::Initialize();

    /// TODO: make the APIC::Initialize method create the list of processors in Self
    auto taskF = Scheduling::KernelTaskContext::Create(&f);
    if (!taskF.HasValue()) {
        Panic::PanicShutdown("COULD NOT CREATE TASK 1");
    }

    auto tf = taskF.GetValue();

    Self::Get().AddTask(tf);

    auto taskG = Scheduling::KernelTaskContext::Create(&g);
    if (!taskG.HasValue()) {
        Panic::PanicShutdown("COULD NOT CREATE TASK 2");
    }

    auto tg = taskG.GetValue();

    Self::Get().AddTask(tg);
    

    //Log::puts("Initializing scheduler...\n\r");

    //Scheduling::InitializeDispatcher(&tman);

    __asm__ volatile("sti");

    //Services::Shell::Entry();

    //PCI::Enumerate();

    while (1) {
        __asm__ volatile("hlt");
    }
}
