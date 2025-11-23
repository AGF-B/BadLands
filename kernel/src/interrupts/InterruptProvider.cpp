#include <cstdint>

#include <interrupts/InterruptProvider.hpp>

namespace Interrupts {
    InterruptTrampoline::InterruptTrampoline(decltype(handler) handler) : handler{handler} {}

    void InterruptTrampoline::HandleIRQ(void* stack_pointer, uint64_t error_code) {
        if (handler != nullptr) {
            handler(stack_pointer, error_code);
        }
    }
}
