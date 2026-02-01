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

#include <interrupts/InterruptProvider.hpp>

class Timer {
public:
    virtual void Initialize() = 0;
    virtual bool IsEnabled() const = 0;
    virtual void Enable() = 0;
    virtual void Disable() = 0;
    virtual void ReattachIRQ(void (*handler)()) = 0;
    virtual void ReleaseIRQ() = 0;
    virtual void SignalIRQ() = 0;
    virtual void SendEOI() const = 0;
    virtual void SetHandler(void (*handler)()) = 0;
    virtual uint64_t GetCountMicros() const = 0;
    virtual uint64_t GetCountMillis() const = 0;
};
