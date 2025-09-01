#!/bin/bash

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
