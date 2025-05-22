mkdir -p build/kernel
cd build/kernel
cmake ../../kernel/
make -j4 KERNEL_IMG