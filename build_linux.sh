mkdir -p build/linux
cd build/linux
cmake ../..
cmake --build . --config Release
cmake --install . --prefix=../..
