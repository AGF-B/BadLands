#include <cstdint>

#include <shared/efi/efi.h>
#include <shared/memory/layout.hpp>

#include <fs/VFS.hpp>

#include <GRM/ResourceManager.hpp>

#include <interrupts/idt.hpp>
#include <interrupts/Panic.hpp>

#include <mm/gdt.hpp>
#include <mm/Heap.hpp>
#include <mm/PhysicalMemory.hpp>
#include <mm/Utils.hpp>
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
        if (VirtualMemory::Setup() != VirtualMemory::StatusCode::SUCCESS) {
            Panic::PanicShutdown(rtServices, "VMM INITIALIZATION FAILED\n\r");
        }
    }

    static inline void SetupHeap() {
        if (!Heap::Create()) {
            Panic::PanicShutdown(rtServices, "KERNEL HEAP CREATION FAILED\n\r");
        }
    }

    static inline VFS* SetupVFS() {
        VFS* vfs = static_cast<VFS*>(Heap::Allocate(sizeof(VFS)));

        if (vfs == nullptr) {
            Panic::PanicShutdown(rtServices, "VFS MEMORY ALLOCATION FAILED\n\r");
        }
        else if (!VFS::Construct(vfs)) {
            Panic::PanicShutdown(rtServices, "VFS INITIALIZATION FAILED\n\r");
        }

        return vfs;
    }
}

#include <new>

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

    while (1) {
        __asm__ volatile("hlt");
    }
}
