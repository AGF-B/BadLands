#pragma once

#include <cstdint>

#include <sched/TaskContext.hpp>

namespace Scheduling {
    class TaskManager {
    private:
        struct Task {
            uint64_t id;
            Task* prev;
            Task* next;
            TaskContext context;
        };

        Task* last = nullptr;
        Task* head = nullptr;
        uint64_t task_count = 0;
    
    public:
        uint64_t GetTaskCount() const;
        bool AddTask(const TaskContext& context);
        void BlockTask(uint64_t task_id);
        void UnblockTask(uint64_t task_id);
        void RemoveTask(uint64_t task_id);
        TaskContext* TaskSwitch(void* stack_context);
    };
}