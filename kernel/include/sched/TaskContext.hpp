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

#include <shared/Response.hpp>

namespace Scheduling {
    struct TaskContext {
        void* CR3;
        void* InstructionPointer;
        void* StackPointer;

        static TaskContext Create(void* InstructionPointer);
        void Destroy();
    };

    struct KernelTaskContext : public TaskContext {
    private:
        static void* SetupTaskPages();
        static void* SetupTaskContext(void* InstructionPointer, uint64_t argument);

    public:
        static Optional<KernelTaskContext> Create(void* InstructionPointer, uint64_t argument = 0);
    };
}
