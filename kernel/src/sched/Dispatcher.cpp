#include <sched/Self.hpp>
#include <sched/Dispatcher.hpp>

#include <screen/Log.hpp>

namespace Scheduling {
    struct SwitchResult {
        void* CR3;
        void* RSP;
    };

    extern "C" void SCHEDULER_IRQ_HANDLER();

    void InitializeDispatcher() {
        __asm__ volatile("cli");
        Self().GetTimer().ReattachIRQ(&SCHEDULER_IRQ_HANDLER);
        Log::printfSafe("[CPU %llu] Scheduler Initialized\n\r", Self().GetID());
        __asm__ volatile("sti");
    }

    extern "C" void SCHEDULER_IRQ_DISPATCHER(SwitchResult* result, void* stack_context) {
        if (result != nullptr) {
            result->CR3 = nullptr;
            result->RSP = nullptr;
            
            auto& self = Self();
            auto& timer = self.GetTimer();

            timer.SignalIRQ();
            timer.SendEOI();

            if (timer.GetCountMillis() % 10 == 0) {
                auto* task = self.GetTaskManager().TaskSwitch(stack_context);

                if (task != nullptr) {
                    result->CR3 = task->CR3;
                    result->RSP = task->StackPointer;
                }
            }
        }
    }
}