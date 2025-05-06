mkdir build\win
cd build\win
cmake ../..
cmake --build . --config Release
cmake --install . --prefix=../..
