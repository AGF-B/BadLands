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

#include <interrupts/IDT.hpp>
#include <interrupts/Panic.hpp>

#include <sched/Self.hpp>
#include <sched/Dispatcher.hpp>

#include <screen/Log.hpp>

namespace Scheduling {
    struct SwitchResult {
        void* CR3;
        void* RSP;
    };

    extern "C" void SCHEDULER_IRQ_HANDLER();
    extern "C" void SCHEDULER_SOFT_IRQ_HANDLER();

    void InitializeDispatcher() {
        __asm__ volatile("cli");

        // Setup timer IRQ handler for scheduling
        Self().GetTimer().ReattachIRQ(&SCHEDULER_IRQ_HANDLER);
        Interrupts::ForceIRQHandler(Interrupts::SOFTWARE_YIELD_IRQ, reinterpret_cast<void*>(&SCHEDULER_SOFT_IRQ_HANDLER));

        Log::printfSafe("[CPU %llu] Scheduler Initialized\n\r", Self().GetID());
        
        __asm__ volatile("sti");
    }

    static void Reschedule(SwitchResult* result, void* stack_context, UnattachedSelf& self) {
        auto* task = self.GetTaskManager().TaskSwitch(stack_context);

        if (task != nullptr) {
            result->CR3 = task->CR3;
            result->RSP = task->StackPointer;
        }
    }

    extern "C" void SCHEDULER_IRQ_DISPATCHER(SwitchResult* result, void* stack_context, bool is_timer_irq) {
        if (result != nullptr) {
            result->CR3 = nullptr;
            result->RSP = nullptr;
            
            auto& self = Self();

            if (is_timer_irq) {
                auto& timer = self.GetTimer();
                timer.SignalIRQ();
                timer.SendEOI();

                if (timer.GetCountMillis() % 10 == 0) {
                    Reschedule(result, stack_context, self);
                }
            }
            else {
                Reschedule(result, stack_context, self);
            }
        }
    }
}