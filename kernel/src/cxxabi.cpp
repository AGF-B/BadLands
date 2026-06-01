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

// Stubs for C++ ABI symbols required by the compiler when static-duration
// objects with non-trivial destructors are present. The kernel never exits,
// so destructor registration is a no-op.

void* __dso_handle __attribute__((visibility("hidden"))) = nullptr;

extern "C" int __cxa_atexit(void (*)(void*), void*, void*) {
    return 0;
}