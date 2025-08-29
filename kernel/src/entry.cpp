#include <cstdint>

#include <shared/efi/efi.h>
#include <shared/memory/layout.hpp>

#include <acpi/Interface.hpp>

#include <devices/PS2/Controller.hpp>
#include <devices/PS2/Keyboard.hpp>

#include <fs/VFS.hpp>

#include <interrupts/APIC.hpp>
#include <interrupts/idt.hpp>
#include <interrupts/Panic.hpp>
#include <interrupts/RuntimeSvc.hpp>

#include <mm/gdt.hpp>
#include <mm/Heap.hpp>
#include <mm/PhysicalMemory.hpp>
#include <mm/VirtualMemory.hpp>

#include <screen/Log.hpp>

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

    static inline void SetupPS2Keyboard() {
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

        auto statusCode = Devices::PS2::InitializeKeyboard();

        if (statusCode != Devices::PS2::StatusCode::SUCCESS) {
            Log::puts("[PS/2] Keyboard initialization failed\n\r");
            Log::puts("[PS/2] No keyboard input will be provided until a USB keyboard is connected\n\r");
            return;
        }

        Interrupts::RegisterIRQ(0x40, &Devices::PS2::PS2_IRQ1_Handler, false);

        APIC::SetupIRQ(Devices::PS2::PS2_PORT1_ISA_IRQ_VECTOR, {
            .InterruptVector = 0x40,
            .Delivery = APIC::IRQDeliveryMode::FIXED,
            .DestinationMode = APIC::IRQDestinationMode::Logical,
            .Polarity = APIC::IRQPolarity::ACTIVE_HIGH,
            .Trigger = APIC::IRQTrigger::EDGE,
            .Masked = false,
            .Destination = APIC::GetLAPICLogicalID()
        });

        Log::puts("[PS/2] Keyboard IRQ configured\n\r");
        Log::puts("[PS/2] Keyboard input enabled\n\r");
        Log::puts("[PS/2] Initialization done\n\r");
    }
}

#include <cpuid.h>

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
    Log::puts("VFS Initialized\n\r");

    ACPI::Initialize();

    APIC::Initialize();

    APIC::SetupLocalAPIC();

    SetupPS2Keyboard();

    __asm__ volatile("sti");

    while (1) {
        __asm__ volatile("hlt");
    }
}
