/* SPDX-License-Identifier: GPL-3.0-only
 *
 * Copyright (C) 2026 Alexandre Boissiere
 * This file is part of the BadLands operating system.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation, version 3.
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <https://www.gnu.org/licenses/>.
 */

OUTPUT_ARCH(i386:x86-64)

ENTRY(KernelEntry)

SECTIONS {
    . = 0x0000008000000000;
    .userembedded ALIGN(CONSTANT(MAXPAGESIZE)) : {
        userembedded_start = .;
        *(.userembedded*)
        . = ALIGN(CONSTANT(MAXPAGESIZE));
        userembedded_end = .;
    }
    . = 0xffff800000000000;
    .text ALIGN(CONSTANT(MAXPAGESIZE)) : {
        *(.text*)
    }
    .data ALIGN(CONSTANT(MAXPAGESIZE)) : {
        *(.data*)
    }
    .rodata ALIGN(CONSTANT(MAXPAGESIZE)) : {
        *(.rodata*)
    }
    .rdata ALIGN(CONSTANT(MAXPAGESIZE)) : {
        *(.rdata*)
    }
    .bss ALIGN(CONSTANT(MAXPAGESIZE)) : {
        *(.bss*)
        *(COMMON)
    }
    .init_array ALIGN(CONSTANT(MAXPAGESIZE)) : {
        kernel_init_array_start = .;
        *(.init_array*)
        kernel_init_array_end = .;
    }
    /DISCARD/ : {
        *(.reloc*)
        *(.idata*)
        *(.pdata*)
        *(.xdata*)
        *(.note*)
        *(.comment*)
        *(.eh_frame*)
    }
}