#include <interrupts/PIT.hpp>

#include <sched/TaskManager.hpp>
#include <sched/Dispatcher.hpp>

namespace Scheduling {
    struct SwitchResult {
        void* CR3;
        void* RSP;
    };

    // for MP: have per-CPU processor structures that point to their respective task managers
    // and dispatch to correct manager here
    static TaskManager* taskManager = nullptr;

    extern "C" void SCHEDULER_IRQ_HANDLER();

    void InitializeDispatcher(TaskManager* manager) {
        taskManager = manager;
        PIT::ReattachIRQ(&SCHEDULER_IRQ_HANDLER);
    }

    extern "C" void SCHEDULER_IRQ_DISPATCHER(SwitchResult* result, void* stack_context) {
        if (result != nullptr) {
            result->CR3 = nullptr;
            result->RSP = nullptr;

            PIT::SignalIRQ();
            PIT::SendEOI();

            if (PIT::GetCountMicros() % 1000 == 0) {
                auto* task = taskManager->TaskSwitch(stack_context);

                if (task != nullptr) {
                    result->CR3 = task->CR3;
                    result->RSP = task->StackPointer;
                }
            }
        }
    }
}