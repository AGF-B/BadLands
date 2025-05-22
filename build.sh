sh build_loader.sh &
sh build_kernel.sh &
wait

cd build
mv kernel/kernel.img kernel.img

strip -R .idata -R .pdata -R .xdata BOOTX64.EFI
