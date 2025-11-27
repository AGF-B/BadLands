#pragma once

#include <cstdint>

#include <shared/Lock.hpp>
#include <shared/LockGuard.hpp>

#include <sched/TaskContext.hpp>

namespace Scheduling {
    class TaskManager {
    private:
        struct Task {
            const bool blockable;
            bool blocked;
            uint64_t id;
            Task* prev;
            Task* next;
            TaskContext context;
        };

        mutable Utils::Lock modify_lock;

        uint64_t switches = 0;
        Task* head = nullptr;
        uint64_t task_count = 0;

        Task* FindTask(uint64_t task_id) const;
    
    public:
        uint64_t GetTaskCount() const;
        uint64_t AddTask(const TaskContext& context, bool blockable = true);
        void RemoveTask(uint64_t task_id);
        void BlockTask(uint64_t task_id) const;
        void UnblockTask(uint64_t task_id) const;
        TaskContext* TaskSwitch(void* stack_context);
    };
}