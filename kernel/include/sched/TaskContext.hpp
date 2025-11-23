#pragma once

#include <shared/Response.hpp>

namespace Scheduling {
    struct TaskContext {
        void* CR3;
        void* InstructionPointer;
        void* StackPointer;

        static TaskContext Create(void* InstructionPointer);
    };

    struct KernelTaskContext : public TaskContext {
    private:
        static void* SetupTaskPages();
        static void* SetupTaskContext(void* InstructionPointer, uint64_t argument);

    public:
        static Optional<KernelTaskContext> Create(void* InstructionPointer, uint64_t argument = 0);
    };
}
