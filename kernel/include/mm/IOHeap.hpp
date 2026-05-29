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

#include <cstddef>

#include <shared/Response.hpp>

#include <mm/MemoryProvider.hpp>

class IOHeap {
private:
    struct Metadata {
        uint32_t padding;
        uint32_t size;
        Metadata* next;
    };

    static inline Metadata* head = nullptr;

    static void Coalesce(Metadata* current);

public:
    static Success  Create();
    static void*    Allocate(size_t size);
    static void*    Allocate(size_t size, size_t alignment);
    static void     Free(void* ptr);
};

static_assert(MemoryProvider<IOHeap>);
