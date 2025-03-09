cmake_minimum_required(VERSION 3.22)

project(EFI_BOOTX64)

add_executable(EFI_BOOTX64
    "bootloader/src/entry.cpp"
    "bootloader/src/efi/efi_misc.cpp"
    "bootloader/src/ldstdio/printf.cpp"
    "bootloader/src/ldstdio/puts.cpp"
    "bootloader/src/ldstdio/readkey.cpp"
    "bootloader/src/ldstdio/vsnprintf.cpp"
    "bootloader/src/ldstdlib/itoa.cpp"
    "bootloader/src/ldstdlib/memory.cpp"
    "bootloader/src/loader/basic_graphics.cpp"
    "bootloader/src/loader/paging.cpp"
    "bootloader/src/loader/system_config.cpp"
)

set_target_properties(EFI_BOOTX64 PROPERTIES OUTPUT_NAME "BOOTX64.EFI")
target_include_directories(EFI_BOOTX64 PRIVATE bootloader/include)
target_include_directories(EFI_BOOTX64 PRIVATE common/include)
target_compile_options(EFI_BOOTX64 PRIVATE
    -mabi=ms
    -Wall
	-Wextra
	-Wpedantic
	-fms-extensions
	-mno-red-zone
	-ffreestanding
	-nostdlib
	-Ofast
    -mgeneral-regs-only
)
target_link_options(EFI_BOOTX64 PRIVATE
    -Wl,--subsystem,10
    -Wl,-e,EfiEntry
    -nolibc
    -nostdlib
)
target_compile_features(EFI_BOOTX64 PRIVATE cxx_std_23)