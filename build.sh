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

if [[ $1 == "--clean" ]]; then
    rm -rf build/*
    echo "Done"
elif [[ $1 == "--kernel" ]]; then
    source build_kernel.sh $@ &
    wait

    cd build
    mv kernel/kernel.img kernel.img
else
    source build_loader.sh $@ &
    source build_kernel.sh $@ &
    wait

    cd build
    mv kernel/kernel.img kernel.img

    strip -R .idata -R .pdata -R .xdata BOOTX64.EFI
fi
