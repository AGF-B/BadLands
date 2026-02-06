# SPDX-License-Identifier: GPL-3.0-only
#
# Copyright (C) 2026 Alexandre Boissiere
# This file is part of the BadLands operating system.
#
# This program is free software: you can redistribute it and/or modify it under the terms of the
# GNU General Public License as published by the Free Software Foundation, version 3.
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
# without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with this program.
# If not, see <https://www.gnu.org/licenses/>. 

cmake_minimum_required(VERSION 3.30)

project(KERNEL_IMG)

set(CMAKE_ASM_NASM_OBJECT_FORMAT win64)
enable_language(ASM_NASM)

add_library(KERNEL_IMG_ASM OBJECT
    "../common/src/shared/AtomicImpl.asm"
    "src/interrupts/IDTCore.asm"
    "src/interrupts/PIC.asm"
    "src/sched/SchedulerIRQ.asm"
    "src/mm/gdt.asm"
)

add_library(KERNEL_IMG_C OBJECT
    "src/devices/KeyboardDispatcher/KeycodeMap.c"
)

add_library(KERNEL_SVC OBJECT
    "src/services/shell/entry.cpp"
)

add_library(xHCI_USB OBJECT
    "src/devices/USB/Driver.cpp"
    "src/devices/USB/HID/Driver.cpp"
    "src/devices/USB/HID/Keyboard.cpp"
    "src/devices/USB/xHCI/Controller.cpp"
    "src/devices/USB/xHCI/Device.cpp"
    "src/devices/USB/xHCI/Specification.cpp"
    "src/devices/USB/xHCI/TRB.cpp"
)

add_executable(KERNEL_IMG
    "src/acpi/Interface.cpp"
    "src/devices/KeyboardDispatcher/Converter.cpp"
    "src/devices/KeyboardDispatcher/Multiplexer.cpp"
    "src/devices/PS2/Controller.cpp"
    "src/devices/PS2/Keyboard.cpp"
    "src/devices/PS2/Keypoints.cpp"
    "src/fs/IFNode.cpp"
    "src/fs/NPFS.cpp"
    "src/fs/VFS.cpp"
    "src/interrupts/core/PageFault.cpp"
    "src/interrupts/APIC.cpp"
    "src/interrupts/CoreDump.cpp"
    "src/interrupts/IDT.cpp"
    "src/interrupts/InterruptProvider.cpp"
    "src/interrupts/Panic.cpp"
    "src/interrupts/PIT.cpp"
    "src/interrupts/RuntimeSvc.cpp"
    "src/mm/Heap.cpp"
    "src/mm/IOHeap.cpp"
    "src/mm/Paging.cpp"
    "src/mm/PhysicalMemory.cpp"
    "src/mm/Utils.cpp"
    "src/mm/VirtualMemory.cpp"
    "src/pci/MSI.cpp"
    "src/pci/PCI.cpp"
    "src/pci/Interface.cpp"
    "src/sched/Dispatcher.cpp"
    "src/sched/Self.cpp"
    "src/sched/TaskContext.cpp"
    "src/sched/TaskManager.cpp"
    "src/screen/Format.cpp"
    "src/screen/Framebuffer.cpp"
    "src/screen/Log.cpp"
    "src/screen/ThreadSafe.cpp"
    "src/entry.cpp"
)

add_library(KERNEL_EXTENSIONS STATIC
    $<TARGET_OBJECTS:KERNEL_IMG_ASM>
    $<TARGET_OBJECTS:KERNEL_IMG_C>
    $<TARGET_OBJECTS:KERNEL_SVC>
    $<TARGET_OBJECTS:xHCI_USB>
)

set_target_properties(KERNEL_IMG PROPERTIES OUTPUT_NAME "kernel.img")

target_include_directories(KERNEL_IMG PRIVATE include)
target_include_directories(KERNEL_IMG PRIVATE ../common/include)
target_include_directories(KERNEL_IMG_C PRIVATE include)
target_include_directories(KERNEL_IMG_C PRIVATE ../common/include)
target_include_directories(KERNEL_SVC PRIVATE include)
target_include_directories(KERNEL_SVC PRIVATE ../common/include)
target_include_directories(xHCI_USB PRIVATE include)
target_include_directories(xHCI_USB PRIVATE ../common/include)

target_compile_options(KERNEL_IMG PRIVATE
    -mabi=ms
    -Wall
    -Wextra
    -fms-extensions
    -mno-red-zone
    -ffreestanding
    -nostdlib
    -nolibc
    -mgeneral-regs-only
    -D__EFI_STANDALONE__
    -fno-pie
    -mcmodel=large
    -static
    -fno-exceptions
    -fno-rtti
    -flto
    -fno-stack-protector
)

target_compile_options(KERNEL_IMG_C PRIVATE
    -mabi=ms
    -Wall
    -Wextra
    -fms-extensions
    -mno-red-zone
    -ffreestanding
    -nostdlib
    -nolibc
    -mgeneral-regs-only
    -D__EFI_STANDALONE__
    -fno-pie
    -mcmodel=large
    -static
    -fno-exceptions
    -flto
    -std=c2x
    -fno-stack-protector
)
target_compile_options(KERNEL_SVC PRIVATE
    -mabi=ms
    -Wall
    -Wextra
    -fms-extensions
    -mno-red-zone
    -ffreestanding
    -nostdlib
    -nolibc
    -mgeneral-regs-only
    -D__EFI_STANDALONE__
    -fno-pie
    -mcmodel=large
    -static
    -fno-exceptions
    -fno-rtti
    -flto
    -fno-stack-protector
)
target_compile_options(xHCI_USB PRIVATE
    -mabi=ms
    -Wall
    -Wextra
    -fms-extensions
    -mno-red-zone
    -ffreestanding
    -nostdlib
    -nolibc
    -mgeneral-regs-only
    -D__EFI_STANDALONE__
    -fno-pie
    -mcmodel=large
    -static
    -fno-exceptions
    -fno-rtti
    -flto
    -fno-stack-protector
)

target_link_options(KERNEL_IMG PRIVATE
    -Wl,-e,KernelEntry
    -Wl,-T,../../kernel/linker.ld
    -Wl,--build-id=none
    -z noexecstack
    -nolibc
    -nostdlib
    -flto
    #-s
)

target_link_options(KERNEL_EXTENSIONS PRIVATE
    -Wl,--build-id=none
    -z noexecstack
    -nolibc
    -nostdlib
    -flto
    #-s
)

target_compile_features(KERNEL_IMG PRIVATE cxx_std_23)
target_compile_features(KERNEL_SVC PRIVATE cxx_std_23)
target_compile_features(xHCI_USB PRIVATE cxx_std_23)

target_link_libraries(KERNEL_IMG PRIVATE
    KERNEL_EXTENSIONS
    -static
)
