#include <sched/Self.hpp>

#include <sched/Dispatcher.hpp>

namespace Scheduling {
    struct SwitchResult {
        void* CR3;
        void* RSP;
    };

    extern "C" void SCHEDULER_IRQ_HANDLER();

    void InitializeDispatcher() {
        Self::Get().GetTimer().ReattachIRQ(&SCHEDULER_IRQ_HANDLER);
    }

    extern "C" void SCHEDULER_IRQ_DISPATCHER(SwitchResult* result, void* stack_context) {
        if (result != nullptr) {
            result->CR3 = nullptr;
            result->RSP = nullptr;
            
            auto& self = Self::Get();

            self.GetTimer().SignalIRQ();
            self.GetTimer().SendEOI();

            if (self.GetTimer().GetCountMillis() % 10 == 0) {
                auto* task = self.TaskSwitch(stack_context);

                if (task != nullptr) {
                    result->CR3 = task->CR3;
                    result->RSP = task->StackPointer;
                }
            }
        }
    }
}