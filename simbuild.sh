cd simulation

if [ ! -f disk.img ]; then
    dd if=/dev/zero of=disk.img bs=4M count=12
    sgdisk -o -n=1:2048:73728 -t=1:EF00 disk.img
    mkdosfs -F 32 --offset 2048 disk.img 36864
    mmd -i disk.img@@1M ::/EFI
    mmd -i disk.img@@1M ::/EFI/BOOT 
fi

mcopy -o -Q -i disk.img@@1M ../build/BOOTX64.EFI ::/EFI/BOOT
mcopy -o -Q -i disk.img@@1M ../build/kernel.img ::/EFI/BOOT
mcopy -o -Q -i disk.img@@1M psf_font.psf ::/EFI/BOOT