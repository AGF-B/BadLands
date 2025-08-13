cmake_minimum_required(VERSION 3.30)

project(KERNEL_IMG)

set(CMAKE_ASM_NASM_OBJECT_FORMAT win64)
enable_language(ASM_NASM)

add_library(KERNEL_IMG_ASM STATIC
    "src/interrupts/core/page_error.asm"
    "src/interrupts/CoreDumpSetup.asm"
    "src/mm/gdt.asm"
)

add_executable(KERNEL_IMG
    "src/fs/IFNode.cpp"
    "src/fs/NPFS.cpp"
    "src/fs/VFS.cpp"
    "src/interrupts/core/PageFault.cpp"
    "src/interrupts/idt.cpp"
    "src/interrupts/CoreDump.cpp"
    "src/interrupts/Panic.cpp"
    "src/mm/Heap.cpp"
    "src/mm/PhysicalMemory.cpp"
    "src/mm/Utils.cpp"
    "src/mm/VirtualMemory.cpp"
    "src/screen/Framebuffer.cpp"
    "src/screen/Log.cpp"
    "src/entry.cpp"
)

set_target_properties(KERNEL_IMG PROPERTIES OUTPUT_NAME "kernel.img")
target_include_directories(KERNEL_IMG PRIVATE include)
target_include_directories(KERNEL_IMG PRIVATE ../common/include)

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
)

target_link_options(KERNEL_IMG PRIVATE
    -Wl,-e,KernelEntry
    -Wl,-T,../../kernel/linker.ld
    -Wl,--build-id=none
    -s
    -z noexecstack
    -nolibc
    -nostdlib
    -flto
)

target_compile_features(KERNEL_IMG PRIVATE cxx_std_23)
target_link_libraries(KERNEL_IMG PRIVATE KERNEL_IMG_ASM -static)