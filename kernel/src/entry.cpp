#include <cstdint>

#include <shared/efi/efi.h>
#include <shared/memory/layout.hpp>

#include <interrupts/idt.hpp>
#include <interrupts/Panic.hpp>

#include <mm/gdt.hpp>
#include <mm/Heap.hpp>
#include <mm/PhysicalMemory.hpp>
#include <mm/VirtualMemory.hpp>

#include <screen/Log.hpp>

#define LEGACY_EXPORT extern "C"

extern uint8_t kernel_init_array_start[];
extern uint8_t kernel_init_array_end[];

namespace {
    static const EFI_RUNTIME_SERVICES* rtServices;

    static void _kernel_ctx_init() {
        const size_t initialize_count = (
            kernel_init_array_end - kernel_init_array_start
        ) / sizeof(void(*)(void));

        for (size_t i = 0; i < initialize_count; ++i) {
            reinterpret_cast<void(**)(void)>(
                kernel_init_array_start
            )[i]();
        }

        rtServices = reinterpret_cast<EFI_RUNTIME_SERVICES*>(
            Shared::Memory::Layout::OsLoaderData.start + Shared::Memory::Layout::OsLoaderDataOffsets.RTServices
        );
    }

    static inline void SetupPhysicalMemory() {
        auto status = PhysicalMemory::Setup();
        if (status != PhysicalMemory::StatusCode::SUCCESS) {
            if (status == PhysicalMemory::StatusCode::OUT_OF_MEMORY) {
                Panic::PanicShutdown(rtServices, "PMM INITIALIZATION FAILED (OUT OF MEMORY)\n\r");
            }
            else {
                Panic::PanicShutdown(rtServices, "PMM INITIALIZATION FAILED (UNKNOWN REASON)\n\r");
            }
        }
    }

    static inline void SetupVirtualMemory() {
        auto status = VirtualMemory::Setup();
        if (status != VirtualMemory::StatusCode::SUCCESS) {
            Panic::PanicShutdown(rtServices, "VMM INITIALIZATION FAILED\n\r");
        }
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

    auto ptr    = Heap::Allocate(1);
    auto ptr2   = Heap::Allocate(17);

    __asm__ volatile("mov %0, %%r15" :: "r"(ptr));
    __asm__ volatile("mov %0, %%r14" :: "r"(ptr2));

    Heap::Free(ptr);

    while (1) {
        __asm__ volatile("hlt");
    }
}
