// SPDX-License-Identifier: GPL-3.0-only
//
// Copyright (C) 2026 Alexandre Boissiere
// This file is part of the BadLands operating system.
//
// This program is free software: you can redistribute it and/or modify it under the terms of the
// GNU General Public License as published by the Free Software Foundation, version 3.
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
// without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with this program.
// If not, see <https://www.gnu.org/licenses/>. 

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