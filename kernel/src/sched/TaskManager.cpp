#include <cstdint>

#include <mm/Heap.hpp>

#include <sched/TaskContext.hpp>
#include <sched/TaskManager.hpp>

namespace Scheduling {
    uint64_t TaskManager::GetTaskCount() const {
        return task_count;
    }

    bool TaskManager::AddTask(const TaskContext& context) {
        if (context.CR3 == nullptr || context.InstructionPointer == nullptr || context.StackPointer == nullptr) {
            return false;
        }
        
        Task* new_task = static_cast<Task*>(
            Heap::Allocate(sizeof(Task))
        );

        if (new_task == nullptr) {
            return false;
        }

        new_task->id = task_count++;
        new_task->context = context;

        if (head == nullptr) {
            head = new_task;
            new_task->prev = new_task;
            new_task->next = new_task;
        }
        else {
            Task* tail = head->prev;

            tail->next = new_task;
            new_task->prev = tail;
            new_task->next = head;
            head->prev = new_task;
        }

        return true;
    }

    TaskContext* TaskManager::TaskSwitch(void* stack_context) {
        if (head == nullptr) {
            return nullptr;
        }

        auto* next = head->next;

        if (last != nullptr) {
            head->context.StackPointer = stack_context;
        }

        if (next == last) {
            return nullptr; // signal no change in task
        }

        last = next;
        head = next;
        return &head->context;
    }
}
