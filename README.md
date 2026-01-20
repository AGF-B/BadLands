# BadLands Operating System

This is an experimental operating system I am currently working on. It is written in C++ with some C and x86-64 assembly code.
The operating system targets UEFI-compatible x86-64 platforms.

## Dependencies

The listed versions are the ones I am currently using, feel free to use older or newer versions if they work.

### To build from source

- ``cmake`` version 3.30+
- ``x86_64-w64-mingw32-gcc`` and ``x86_64-w64-mingw32-g++`` version "13-win32" for the bootloader
- ``NASM`` for assembly code
- ``gcc`` and ``g++`` version 14 or 15 for the kernel
- ``ld`` for linking
- ``strip`` to make executables smaller

### To run the QEMU simulation

- ``qemu-system-x86_64`` | tested on version 10.1.93 (v10.2.0-rc3)
- ``sgdisk`` : GPT fdisk (sgdisk) version 1.0.10
- ``mkdosfs`` : mkfs.fat 4.2 (2021-01-31)
- ``mmd``, ``mcopy`` : Mtools version 4.0.48

## Compilation

The build scripts must be executed from the root of the repository, they will create the output directories automatically.

To compile the entire system, it is enough to run the following command:

```bash
./build.sh
```

If it fails, first check the file has execute permissions and add them if necessary before trying again.

If you only desire to compile the kernel without its bootloader, pass the ``--kernel`` flag to the build script:

```bash
./build.sh --kernel
```

If you want to clean up the build directory, pass the ``--clean`` flag to the build script:

```bash
./build.sh --clean
```

## Running the QEMU simulation

To run the QEMU simulation, you first need to build the system disk image. In order to do so, place yourself at the root of the repository and run the following command:

```bash
./simbuild.sh
```

If it fails, check again that the script has execute permissions

To run QEMU with the disk image, simply go to the ``simulation`` directory that was just created by the previous command and run:

```bash
./qemusim.sh
```

Again, check it has execute permissions in case of failure. If any other error appears, check that you have an adequate QEMU version. If it still does not work, do not hesitate to build QEMU from source (which is what has been done for the development of this project).

## Features

- [x] Linear Framebuffer  (UEFI - GOP)
- [x] Execute Disable Protection
- [x] Huge Pages
- [x] Caching Strategies
- [x] Runtime Interrupt Mapping
- [x] IO Heap
- [x] UEFI
  - [x] GOP Framebuffer
  - [x] Runtime Services Remapping
  - [x] Power Reset
  - [x] Time Services
- [ ] ACPI
  - [x] ACPI Table Memory Mapping
  - [x] MADT Parsing
  - [x] MCFG Parsing
  - [x] Locating all PCIe regions
  - [ ] AML Parsing
- [x] APIC / IOxAPIC
  - [x] APIC Timer
  - [x] Logical Processor Addressing
  - [x] IOxAPIC Interface
  - [x] IOxAPIC Interrupt Overrides
- [x] PCIe
  - [x] Generic PCIe Interfaces
  - [x] PCIe MSI Support
  - [x] PCIe Bus Enumeration
- [x] xHCI Controller
  - [x] xHCI detection through PCIe enumeration
  - [x] xHCI Initialization
  - [x] xHCI Port Update Events
  - [x] xHCI Devices Driver Events
  - [x] Generic xHCI Device Driver
- [x] USB - xHCI
  - [x] Device Initialization
  - [x] Device Addressing
  - [x] Device Descriptor Parsing
  - [x] Device Configuration Descriptors Parsing
- [ ] HID
  - [x] HID Report Parsing
  - [x] HID Report Events
  - [ ] Better handling of usages as per the HUT
- [ ] Keyboard
  - [x] PS/2 Keyboard (all scan code sets)
  - [ ] USB Keyboard
    - [x] Basic support
    - [ ] Vendor-defined collections skipping
    - [x] N-key rollover
    - [x] Modifier keys
    - [ ] LEDs
  - [ ] Keyboard Multiplexer
    - [x] Key Events
    - [ ] LEDs Dispatch
- [ ] Generic USB Devices
  - [ ] Mass Storage
  - [ ] Hubs
  - [ ] Ethernet Modules
- [ ] Non-USB Storage Drivers
  - [ ] ATA/IDE
  - [ ] ATA/SATA
  - [ ] NVMe
- [ ] File Systems
  - [x] Virtual File System
  - [x] Non-Persistent FS (NPFS)
  - [ ] FAT12 / FAT16 / FAT32
  - [ ] NTFS
  - [ ] Ext2
- Scheduling
  - [x] Task Scheduling
  - [x] Task Contexts
  - [x] Task Blocking/Unblocking
  - [ ] Processes
- [ ] User Space
  - [x] Switching to ring 3
  - [ ] Syscalls
  - [ ] Executable Loading
  - [ ] Process Spawning From User Space
