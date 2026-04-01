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

mkdir -p simulation
cd simulation

if [ ! -f disk.img ]; then
    dd if=/dev/zero of=disk.img bs=4M count=12
    parted -s disk.img mklabel gpt mkpart primary fat32 1MiB 36MiB set 1 esp on
    mkfs.fat -F 32 --offset 2048 -h 2048 disk.img 35840
    mmd -i disk.img@@1M ::/EFI
    mmd -i disk.img@@1M ::/EFI/BOOT 
fi

mcopy -o -Q -i disk.img@@1M ../build/BOOTX64.EFI ::/EFI/BOOT
mcopy -o -Q -i disk.img@@1M ../build/kernel.img ::/EFI/BOOT
mcopy -o -Q -i disk.img@@1M ../assets/psf_font.psf ::/EFI/BOOT
