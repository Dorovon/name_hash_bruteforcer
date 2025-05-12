mkdir -p build/linux
cd build/linux
cmake -DCMAKE_CXX_FLAGS="-Isrc" ../..
cmake --build . --config Release
cmake --install . --prefix=../..
