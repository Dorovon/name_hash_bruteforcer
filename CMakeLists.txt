cmake_minimum_required(VERSION 3.1...3.28)

project(bruteforcer VERSION 1.0 LANGUAGES CXX)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(bruteforcer src/main.cpp)
target_sources(bruteforcer PRIVATE src/main.cpp)
target_sources(bruteforcer PRIVATE src/util.cpp)
install(TARGETS bruteforcer DESTINATION .)

if (UNIX)
target_compile_options(bruteforcer PRIVATE -Wall)
endif()

if (WIN32)
target_compile_options(bruteforcer PRIVATE /W4)
endif()
