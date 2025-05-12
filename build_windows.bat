mkdir build\win
cd build/win
cmake -DCMAKE_CXX_FLAGS="-Isrc" ../..
cmake --build . --config Release
cmake --install . --prefix=../..
cd ../..
