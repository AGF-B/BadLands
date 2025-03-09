mkdir -p build
cd build
cmake -D CMAKE_C_COMPILER="x86_64-w64-mingw32-gcc" -D CMAKE_CXX_COMPILER="x86_64-w64-mingw32-g++" ..
make -j4
strip -R .idata -R .pdata -R .xdata BOOTX64.EFI