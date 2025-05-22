mkdir -p build
cd build
cmake -D CMAKE_C_COMPILER="x86_64-w64-mingw32-gcc" -D CMAKE_CXX_COMPILER="x86_64-w64-mingw32-g++" ../bootloader/
make -j4 EFI_BOOTX64

