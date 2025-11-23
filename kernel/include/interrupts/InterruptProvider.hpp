#pragma once

#include <cstdint>

namespace Interrupts {
    class InterruptProvider {
    public:
        virtual void HandleIRQ(void* stack, uint64_t error_code) = 0;
    };

    class InterruptTrampoline : public InterruptProvider {
    private:
        void (*handler)(void*,uint64_t) = nullptr;

    public:
        InterruptTrampoline(decltype(handler) handler);
        void HandleIRQ(void* stack, uint64_t error_code);
    };
}
