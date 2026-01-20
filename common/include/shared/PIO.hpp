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

inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %%dx, %%al" : "=a"(ret) : "d"(port));
    return ret;
}

inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("inw %%dx, %%ax" : "=a"(ret) : "d"(port));
    return ret;
}

inline uint32_t indw(uint16_t port) {
    uint32_t ret;
    __asm__ volatile("inl %%dx, %%eax" : "=a"(ret) : "d"(port));
    return ret;
}

inline void outb(uint16_t port, uint8_t data) {
    __asm__ volatile("outb %%al, %%dx" :: "a"(data), "d"(port));
}

inline void outw(uint16_t port, uint16_t data) {
    __asm__ volatile("outw %%ax, %%dx" :: "a"(data), "d"(port));
}

inline void outdw(uint16_t port, uint32_t data) {
    __asm__ volatile("outl %%eax, %%dx" :: "a"(data), "d"(port));
}
