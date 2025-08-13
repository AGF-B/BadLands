#!/bin/bash

if [[ $1 == "--clean" ]]; then
    rm -rf build/*
    echo "Done"
else
    source build_loader.sh $@ &
    source build_kernel.sh $@ &
    wait

    cd build
    mv kernel/kernel.img kernel.img

    strip -R .idata -R .pdata -R .xdata BOOTX64.EFI
fi
