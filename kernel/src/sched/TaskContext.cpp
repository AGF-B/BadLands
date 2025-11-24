#include <cstdint>

#include <shared/memory/defs.hpp>

#include <interrupts/Panic.hpp>

#include <mm/Heap.hpp>
#include <mm/VirtualMemory.hpp>
#include <mm/VirtualMemoryLayout.hpp>
#include <sched/TaskContext.hpp>

namespace Scheduling {
    TaskContext TaskContext::Create(void* InstructionPointer) {
        return TaskContext {
            .CR3 = nullptr,
            .InstructionPointer = InstructionPointer,
            .StackPointer = nullptr
        };
    }

    void TaskContext::Destroy() {
        Panic::PanicShutdown("TASK DESTRUCTION NOT IMPLEMENTED YET\n\r");
    }

    void* KernelTaskContext::SetupTaskPages() {
        void* CR3 = VirtualMemory::DeriveNewFreshCR3();

        if (CR3 == nullptr) {
            return nullptr;
        }

        return CR3;
    }

    void* KernelTaskContext::SetupTaskContext(void* InstructionPointer, uint64_t argument) {
        static constexpr auto mapping = Shared::Memory::ParseVirtualAddress(
            VirtualMemoryLayout::KernelStackReserve.start
        );

        auto* context_pte = VirtualMemory::GetPTEAddress<false>(
            mapping.PML4_offset,
            mapping.PDPT_offset,
            mapping.PD_offset,
            mapping.PT_offset
        );

        void* physical_frame = reinterpret_cast<void*>(*context_pte & Shared::Memory::PTE_ADDRESS);
        void* mapped_frame = VirtualMemory::MapGeneralPages(physical_frame, 1, 
            Shared::Memory::PTE_PRESENT | Shared::Memory::PTE_READWRITE
        );

        if (mapped_frame == nullptr) {
            return nullptr;
        }

        struct TaskInterruptContext {
            uint64_t R15;
            uint64_t R14;
            uint64_t R13;
            uint64_t R12;
            uint64_t R11;
            uint64_t R10;
            uint64_t R9;
            uint64_t R8;
            uint64_t RDI;
            uint64_t RSI;
            uint64_t RBP;
            uint64_t RBX;
            uint64_t RDX;
            uint64_t RCX;
            uint64_t RAX;
            void* RIP;
            uint64_t CS;
            uint64_t RFLAGS;
            void* RSP;
            uint64_t SS;
        } *const task_context = static_cast<TaskInterruptContext*>(mapped_frame);

        task_context->R15 = 0;
        task_context->R14 = 0;
        task_context->R13 = 0;
        task_context->R12 = 0;
        task_context->R11 = 0;
        task_context->R10 = 0;
        task_context->R9  = 0;
        task_context->R8  = 0;
        task_context->RDI = 0;
        task_context->RSI = 0;
        task_context->RBP = 0;
        task_context->RBX = 0;
        task_context->RDX = 0;
        task_context->RCX = argument;
        task_context->RAX = 0;
        task_context->RIP = InstructionPointer;
        task_context->CS = 0x08;
        task_context->RFLAGS = 0x200; // interrupts enabled
        task_context->RSP = reinterpret_cast<void*>(VirtualMemoryLayout::KernelStackReserve.start);
        task_context->SS = 0x10;

        VirtualMemory::UnmapGeneralPages(mapped_frame, 1);

        return reinterpret_cast<void*>(VirtualMemoryLayout::KernelStackReserve.start);
    }

    Optional<KernelTaskContext> KernelTaskContext::Create(void* InstructionPointer, uint64_t argument) {
        /// FIXME: make a method to completely free a PML4 in case of failure
        /// current status: leaking memory on every failure
        
        void* CR3 = VirtualMemory::DeriveNewFreshCR3();
        if (CR3 == nullptr) {
            return Optional<KernelTaskContext>();
        }

        void* KernelStackPointer = SetupTaskContext(InstructionPointer, argument);
        if (KernelStackPointer == nullptr) {
            return Optional<KernelTaskContext>();
        }

        KernelTaskContext context;

        context.CR3 = CR3;
        context.InstructionPointer = InstructionPointer;
        context.StackPointer = KernelStackPointer;

        return Optional(context);
    }
}
