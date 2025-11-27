#include <cstdint>

#include <new>

#include <mm/Heap.hpp>

#include <sched/TaskContext.hpp>
#include <sched/TaskManager.hpp>

namespace Scheduling {
    uint64_t TaskManager::GetTaskCount() const {
        return task_count;
    }

    TaskManager::Task* TaskManager::FindTask(uint64_t task_id) const {
        auto* ptr = head;
        auto* const loop_ref = ptr;

        if (ptr != nullptr) {
            if (ptr->id == task_id) {
                return ptr;
            }

            ptr = ptr->next;

            while (ptr != loop_ref && ptr->id != task_id) {
                ptr = ptr->next;
            }

            if (ptr != loop_ref) {
                return ptr;
            }
        }

        return nullptr;
    }

    uint64_t TaskManager::AddTask(const TaskContext& context, bool blockable) {        
        if (context.CR3 == nullptr || context.InstructionPointer == nullptr || context.StackPointer == nullptr) {
            return 0;
        }
        
        Task* new_task = static_cast<Task*>(
            Heap::Allocate(sizeof(Task))
        );

        if (new_task == nullptr) {
            return 0;
        }

        Utils::LockGuard _{modify_lock};

        new (new_task) Task {
            .blockable = blockable,
            .blocked = false,
            .id = ++task_count, // id = number of tasks added since reset
            .prev = nullptr,
            .next = nullptr,
            .context = context
        };

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

        return new_task->id;
    }

    void TaskManager::RemoveTask(uint64_t task_id) {
        Utils::LockGuard _{modify_lock};

        auto* task = FindTask(task_id);

        // refuse to delete task if it is the only one left
        if (!(task == head && task->next == task)) {
            if (head == task) {
                head = task->next;
            }

            task->prev = task->next;
            task->context.Destroy();
            Heap::Free(task);
        }
    }

    void TaskManager::BlockTask(uint64_t task_id) const {
        Utils::LockGuard _{modify_lock};

        auto* task = FindTask(task_id);

        if (task != nullptr && task->blockable) {
            task->blocked = true;
        }
    }

    void TaskManager::UnblockTask(uint64_t task_id) const {
        Utils::LockGuard _{modify_lock};
        
        auto* task = FindTask(task_id);

        if (task != nullptr) {
            task->blocked = false;
        }
    }

    TaskContext* TaskManager::TaskSwitch(void* stack_context) {
        if (head == nullptr) {
            return nullptr;
        }
        else if (!modify_lock.trylock()) {
            return nullptr; // if can't change task, stay in current task
        }

        auto* next = head->next;
        auto* const current = head;

        while (next != current && next->blocked) {
            next = next->next;
        }

        if (switches++ != 0) {
            current->context.StackPointer = stack_context;
        }
        else {
            modify_lock.unlock();
            return &current->context;
        }

        head = next;

        modify_lock.unlock();

        if (next == current) {
            return nullptr; // signal no change in task
        }

        return &head->context;
    }
}
