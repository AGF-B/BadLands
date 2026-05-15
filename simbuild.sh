#!/bin/bash

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

    root_partition_uuid=$(uuidgen)

    sgdisk -Z disk.img \
        -n 1:2048:73727 \
        -u 1:R \
        -t 1:ef00 \
        -c 1:EFI \
        -n 2:73728 \
        -u 2:$root_partition_uuid \
        -t 2:0700 \
        -c 2:ROOT

    echo "root=$root_partition_uuid" > boot.cfg
    
    mkfs.fat -F 32 --offset 2048 -h 2048 disk.img 35840

    mmd -i disk.img@@1M ::/EFI
    mmd -i disk.img@@1M ::/EFI/BOOT
fi

mcopy -o -Q -i disk.img@@1M ../build/BOOTX64.EFI ::/EFI/BOOT
mcopy -o -Q -i disk.img@@1M ../build/kernel.img ::/EFI/BOOT
mcopy -o -Q -i disk.img@@1M ../assets/psf_font.psf ::/EFI/BOOT
mcopy -o -Q -i disk.img@@1M boot.cfg ::/EFI/BOOT